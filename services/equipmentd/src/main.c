#define _POSIX_C_SOURCE 200809L
#define CONN_IDLE_TIMEOUT_MS 60000 //60초 idle → connection close
#define MAX_CONN 1024

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol/session.h"
#include "protocol/line_codec.h"
#include "internal/router.h"

#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>
#include "internal/connection.h"
#include "internal/connection_table.h"

static int make_server(int port){
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) return -1;

  int yes = 1;
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons((uint16_t)port);

  if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
  if(listen(fd, 16) < 0) return -1;
  return fd;
}

static int set_nonblocking(int fd){
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags < 0) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static long long now_ms(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

int main(int argc, char** argv){
  int port = (argc >= 2) ? atoi(argv[1]) : 12345;

  char line[512];
  char wbuf[512];

  int sfd = make_server(port);
  if(sfd < 0){ perror("server"); return 1; }

  if(set_nonblocking(sfd) < 0){
    perror("set_nonblocking sfd");
    close(sfd);
    return 1;
  }

  printf("[equipmentd] listening on %d\n", port);

  int epfd = epoll_create1(0);
  if(epfd < 0){ perror("epoll_create"); return 1; }

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = sfd;

  if(epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) < 0){
    perror("epoll_ctl");
    return 1;
  }

  struct epoll_event events[16];

  while(1){
    //1초마다 깨우는데 이건 timerfd로 보완필요
    int nfds = epoll_wait(epfd, events, 16, 1000);
          if(nfds < 0){
            if(errno == EINTR) continue;
            perror("epoll_wait");
            break;
          }

          for(int i = 0; i < nfds; i++){

            if(events[i].data.fd == sfd){
        printf("[epoll] accept event\n");

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
            perror("set_nonblocking");
            close(cfd);
            continue;
          }

          connection_t* conn = malloc(sizeof(connection_t));
          if(!conn){
            perror("malloc");
            close(cfd);
            continue;
          }

          conn->fd = cfd;
          session_init(&conn->session);
          //connection table 등록
          register_connection(conn);
          //connection 생성 시 timestamp/state 초기화
          conn->state = CONN_OPEN;
          conn->last_active_ms = now_ms();

          struct epoll_event cev;
          cev.events = EPOLLIN;
          cev.data.ptr = conn;

          if(epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev) < 0){
            perror("epoll_ctl client add");
            close(cfd);
            free(conn);
            continue;
          }

          printf("[equipmentd] client connected fd=%d\n", cfd);
        }
      }

      else{
        connection_t* conn = events[i].data.ptr;
        int fd = conn->fd;

        printf("[epoll] client event fd=%d\n", fd);

        while(1){
          char rbuf[1024];
          ssize_t n = read(fd, rbuf, sizeof(rbuf));

          if(n == 0){
            printf("[equipmentd] client closed fd=%d\n", fd);
            
            //close 시 state 갱신
            conn->state = CONN_CLOSED;
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            //free 전에 registry에서 제거
            unregister_connection(conn);

            free(conn);
            break;
          }

          if(n < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
              break;
            }
            perror("read");
            //close 시 state 갱신
            conn->state = CONN_CLOSED;
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            //free 전에 registry에서 제거
            unregister_connection(conn);
            free(conn);
            break;
          }
          
          printf("[read] fd=%d n=%zd\n", fd, n);
          //read 성공 시 timestamp 갱신 (timeout reset)
          conn->last_active_ms = now_ms();

          if(session_feed(&conn->session, rbuf, (size_t)n) != 0){
            fprintf(stderr, "[equipmentd] session overflow\n");
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            //free 전에 registry에서 제거
            unregister_connection(conn);
            free(conn);
            break;
          }

          while(1){
            int got = session_pop_line(&conn->session, line, sizeof(line));
            if(got == 0) break;
            if(got < 0){
              fprintf(stderr, "[equipmentd] line too long\n");
              break;
            }

            printf("[RX line] %s\n", line);

            message_t in, out;
            int pr = line_parse(line, &in);

            if(pr != 0){
              printf("[PARSE ERROR]\n");
              memset(&out, 0, sizeof(out));
              out.role = ROLE_RESP;
              out.type = TYPE_ERROR;
              out.ok = 0; out.has_ok = 1;
              out.code = 400; out.has_code = 1;
              strncpy(out.msg, "PARSE_ERROR", sizeof(out.msg) - 1);
              out.has_msg = 1;
            } else {
              printf("[PARSED] role=%d type=%d\n", in.role, in.type);
              int rr = router_handle(&in, &out);
              if(rr <= 0) continue;
            }

            int wn = line_format(&out, wbuf, sizeof(wbuf));
            if(wn < 0){
              fprintf(stderr, "[equipmentd] format fail\n");
              break;
            }

            printf("[TX line] %s", wbuf);
            //write실패 체크 - 아직 partial write 처리랑 write queue 아님
            ssize_t wr = write(fd, wbuf, (size_t)wn);
            if(wr < 0){
              perror("write");
              conn->state = CONN_CLOSED;
              epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
              close(fd);
              unregister_connection(conn);
              free(conn);
              break;
            }
          }
        }
      }
    }
    //epoll loop가 끝난 뒤, connection table돌면서 타임아웃 검사
    scan_connection_timeout(epfd, now_ms());
  }



  close(sfd);
  close(epfd);
  return 0;
}
