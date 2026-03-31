#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pmc_connection.h"
#include "pmc_router.h"
#include "protocol/line_codec.h"
#include "protocol/message.h"
#include "alarm_manager.h"

#define MAX_EVENTS 16
#define BUF_SIZE 1024

static int set_nonblocking(int fd){
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags < 0) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int make_server(int port){
  int fd;
  int yes = 1;
  struct sockaddr_in addr;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) return -1;

  if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
    close(fd);
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons((uint16_t)port);

  if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    close(fd);
    return -1;
  }

  if(listen(fd, 16) < 0){
    close(fd);
    return -1;
  }

  return fd;
}

static int connect_tcp(const char* host, int port){
  int fd;
  struct sockaddr_in addr;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) return -1;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);

  if(inet_pton(AF_INET, host, &addr.sin_addr) <= 0){
    close(fd);
    return -1;
  }

  if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    close(fd);
    return -1;
  }

  return fd;
}

static void close_connection(int epfd, pmc_connection_t* conn){
  if(!conn) return;

  epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);
  close(conn->fd);
  pmc_conn_remove(conn);
  free(conn);
}

static int add_epoll_connection(int epfd, pmc_connection_t* conn){
  struct epoll_event ev;

  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.ptr = conn;

  return epoll_ctl(epfd, EPOLL_CTL_ADD, conn->fd, &ev);
}

static int connect_to_eqd(int epfd, const char* host, int port){
  int fd = connect_tcp(host, port);
  if(fd < 0){
    return -1;
  }

  if(set_nonblocking(fd) < 0){
    close(fd);
    return -1;
  }

  pmc_connection_t* conn = malloc(sizeof(*conn));
  if(!conn){
    close(fd);
    return -1;
  }

  memset(conn, 0, sizeof(*conn));
  conn->fd = fd;
  conn->type = PEER_UNKNOWN;   // eqd가 REGISTER 보내면 확정
  conn->registered = 0;
  conn->dev_id = -1;
  session_init(&conn->session);

  pmc_conn_add(conn);

  if(add_epoll_connection(epfd, conn) < 0){
    pmc_conn_remove(conn);
    close(fd);
    free(conn);
    return -1;
  }

  printf("[PMC] connected to EQD %s:%d fd=%d\n", host, port, fd);

  conn->type = PEER_EQD;
  conn->registered = 1;
  conn->dev_id = -1;
  printf("[PMC] EQD registered fd=%d\n", conn->fd);

  {
    message_t reg;
    char outbuf[256];
    int len;

    memset(&reg, 0, sizeof(reg));
    reg.role = ROLE_REQ;
    reg.type = TYPE_REGISTER;
    reg.seq = 1;
    reg.has_seq = 1;

    len = line_format(&reg, outbuf, sizeof(outbuf));
    if(len > 0){
      write(fd, outbuf, (size_t)len);
    }
  }
  return 0;
}

static void handle_connected_peer(int epfd, pmc_connection_t* conn){
  char rbuf[BUF_SIZE];
  ssize_t n;

  while(1){
    n = read(conn->fd, rbuf, sizeof(rbuf));

    if(n == 0){
      printf("[PMC] peer closed fd=%d\n", conn->fd);
      close_connection(epfd, conn);
      break;
    }

    if(n < 0){
      if(errno == EAGAIN || errno == EWOULDBLOCK){
        break;
      }
      perror("read");
      close_connection(epfd, conn);
      break;
    }

    if(session_feed(&conn->session, rbuf, (size_t)n) != 0){
      fprintf(stderr, "[PMC] session overflow fd=%d\n", conn->fd);
      close_connection(epfd, conn);
      break;
    }

    while(1){
      char line[BUF_SIZE];
      message_t msg;
      int got = session_pop_line(&conn->session, line, sizeof(line));
      if(got == 0) break;
      if(got < 0){
        fprintf(stderr, "[PMC] line too long fd=%d\n", conn->fd);
        break;
      }

      if(line_parse(line, &msg) != 0){
        fprintf(stderr, "[PMC] parse error fd=%d line=%s\n", conn->fd, line);
        continue;
      }

      if(!conn->registered){
        int rr = pmc_handle_register(conn, &msg);
        if(rr == 1){
          if(conn->type == PEER_EQD){
            printf("[PMC] fd=%d registered as EQD\n", conn->fd);
          } else if(conn->type == PEER_SIM){
            printf("[PMC] fd=%d registered as SIM dev=%d\n", conn->fd, conn->dev_id);
          }
        } else {
          fprintf(stderr, "[PMC] unregistered peer sent non-register msg fd=%d\n", conn->fd);
        }
        continue;
      }

      if(msg.type == TYPE_REGISTER){
        continue;
      }

      if(pmc_route_message(conn, &msg) != 0){
        fprintf(stderr, "[PMC] route failed fd=%d type=%d\n", conn->fd, msg.type);
      }
    }
  }
}

int main(int argc, char** argv){
  int pmc_port = (argc >= 2) ? atoi(argv[1]) : 12346;
  const char* eqd_host = (argc >= 3) ? argv[2] : "192.168.219.120";
  int eqd_port = (argc >= 4) ? atoi(argv[3]) : 12345;

  int sfd;
  int epfd;
  struct epoll_event ev;
  struct epoll_event events[MAX_EVENTS];

  sfd = make_server(pmc_port);
  if(sfd < 0){
    perror("make_server");
    return 1;
  }

  if(set_nonblocking(sfd) < 0){
    perror("set_nonblocking sfd");
    close(sfd);
    return 1;
  }

  epfd = epoll_create1(0);
  if(epfd < 0){
    perror("epoll_create1");
    close(sfd);
    return 1;
  }

  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.fd = sfd;

  if(epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) < 0){
    perror("epoll_ctl add sfd");
    close(epfd);
    close(sfd);
    return 1;
  }
  
  pmc_alarm_init();

  if(connect_to_eqd(epfd, eqd_host, eqd_port) != 0){
    fprintf(stderr, "[PMC] failed to connect to EQD %s:%d\n", eqd_host, eqd_port);
    close(epfd);
    close(sfd);
    return 1;
  }

  printf("[PMC] listening on %d\n", pmc_port);

  while(1){
    int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if(nfds < 0){
      if(errno == EINTR) continue;
      perror("epoll_wait");
      break;
    }

    for(int i = 0; i < nfds; i++){
      if(events[i].data.fd == sfd){
        while(1){
          int cfd = accept(sfd, NULL, NULL);
          if(cfd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
              break;
            }
            perror("accept");
            break;
          }

          if(set_nonblocking(cfd) < 0){
            perror("set_nonblocking cfd");
            close(cfd);
            continue;
          }

          pmc_connection_t* conn = malloc(sizeof(*conn));
          if(!conn){
            perror("malloc");
            close(cfd);
            continue;
          }

          memset(conn, 0, sizeof(*conn));
          conn->fd = cfd;
          conn->type = PEER_UNKNOWN;
          conn->registered = 0;
          conn->dev_id = -1;
          session_init(&conn->session);

          pmc_conn_add(conn);

          if(add_epoll_connection(epfd, conn) < 0){
            perror("epoll_ctl add client");
            pmc_conn_remove(conn);
            close(cfd);
            free(conn);
            continue;
          }

          printf("[PMC] sim-side client connected fd=%d\n", cfd);
        }
      } else {
        pmc_connection_t* conn = (pmc_connection_t*)events[i].data.ptr;
        handle_connected_peer(epfd, conn);
      }
    }
  }

  close(epfd);
  close(sfd);
  return 0;
}