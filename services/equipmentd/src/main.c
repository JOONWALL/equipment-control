#define _POSIX_C_SOURCE 200809L
#define CONN_IDLE_TIMEOUT_MS 60000 //60초 idle → connection close
#define MAX_CONN 1024
#define MODULE_STALE_MS 3000

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

#include "protocol/session.h"
#include "protocol/line_codec.h"
#include "internal/router.h"
#include "internal/device_manager.h"

#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>
#include "internal/connection.h"
#include "internal/connection_table.h"
#include "internal/module_registry.h"
#include "internal/scheduler.h"
#include "internal/request_tracker.h"

typedef struct {
  device_manager_t dev_mgr;
  module_registry_t module_registry;
  int pmc_fd;
  int tmc_fd;

  request_tracker_t tracker;

  uint32_t next_seq;

  job_state_t job_state;
  
} app_ctx_t;

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

static int tracker_has_pending(const request_tracker_t* t){
  if(!t) return 0;

  for(int i = 0; i < REQUEST_TRACKER_MAX; i++){
    if(t->items[i].valid){
      return 1;
    }
  }

  return 0;
}

static int resolve_target_fd(const app_ctx_t* ctx, uint32_t dev){
  if(!ctx) return -1;

  if(dev == 100){
    return ctx->tmc_fd;
  }

  if(dev == 1 || dev == 2){
    return ctx->pmc_fd;
  }

  return -1;
}

static int is_controller_fd(const app_ctx_t* ctx, int fd){
  if(!ctx) return 0;
  return fd == ctx->pmc_fd || fd == ctx->tmc_fd;
}

static void close_connection(int epfd, connection_t* conn){
  if(!conn) return;

  conn->state = CONN_CLOSED;
  epoll_ctl(epfd, EPOLL_CTL_DEL, conn->fd, NULL);
  close(conn->fd);
  unregister_connection(conn);
  free(conn);
}

static void make_parse_error(message_t* out){
  memset(out, 0, sizeof(*out));
  out->role = ROLE_RESP;
  out->type = TYPE_ERROR;
  out->ok = 0;
  out->has_ok = 1;
  out->code = 400;
  out->has_code = 1;
  strncpy(out->msg, "PARSE_ERROR", sizeof(out->msg) - 1);
  out->has_msg = 1;
}

static int handle_one_line(connection_t* conn, app_ctx_t* ctx, const char* line, char* wbuf, size_t wbuf_sz){
  message_t in, out;
  int wn;
  int pr = line_parse(line, &in);

  if(pr != 0){
    make_parse_error(&out);
  } else {
    if(in.role == ROLE_REQ &&
       in.type == TYPE_REGISTER){
      int is_tmc = in.has_dev && in.dev == 100;

      if(!in.has_dev || is_tmc){
        if(is_tmc){
          ctx->tmc_fd = conn->fd;
          printf("[EQD] TMC link attached fd=%d dev=%u\n", conn->fd, in.dev);
        } else {
          ctx->pmc_fd = conn->fd;
          printf("[EQD] PMC link attached fd=%d\n", conn->fd);
        }

        memset(&out, 0, sizeof(out));
        out.role = ROLE_RESP;
        out.type = TYPE_REGISTER;

        if(in.has_seq){
          out.seq = in.seq;
          out.has_seq = 1;
        }

        if(in.has_dev){
          out.dev = in.dev;
          out.has_dev = 1;
        }

        out.ok = 1;
        out.has_ok = 1;

        wn = line_format(&out, wbuf, wbuf_sz);
        if(wn < 0){
          return -1;
        }

        if(write(conn->fd, wbuf, (size_t)wn) < 0){
          return -1;
        }

        return 1;
      }
    }

    {
      pending_request_t* req = 0;

      if(is_controller_fd(ctx, conn->fd) &&
         in.role == ROLE_RESP &&
         in.has_seq &&
         in.has_dev){
        req = request_tracker_find(&ctx->tracker, in.seq, in.dev);
      }

      if(req){
        if(req->client_fd >= 0){
          wn = line_format(&in, wbuf, wbuf_sz);
          if(wn < 0){
            return -1;
          }

          if(write(req->client_fd, wbuf, (size_t)wn) < 0){
            return -1;
          }

          printf("[EQD] relay RESP to fd=%d dev=%u seq=%u\n",
                 req->client_fd, in.dev, in.seq);
        } else {
          printf("[EQD] scheduler got RESP dev=%u seq=%u\n", in.dev, in.seq);
        }

        if(in.type != TYPE_ERROR && in.has_ok && in.ok == 1){
          switch(req->type){
            case TYPE_STOP:
              ctx->job_state = JOB_STOPPED;
              break;

            case TYPE_RESET:
              ctx->job_state = JOB_IDLE;

              if(req->dev == 1){
                ctx->module_registry.pmc_preclean.fault_latched = 0;
              } else if(req->dev == 2){
                ctx->module_registry.pmc_deposition.fault_latched = 0;
              } else if(req->dev == 100){
                ctx->module_registry.tmc.fault_latched = 0;
              }
              break;

            case TYPE_FAULT:
              ctx->job_state = JOB_ERROR;
              break;

            default:
              break;
          }

          printf("[EQD] job state updated to %d by pending_type=%d\n",
                 ctx->job_state, req->type);
        }

        request_tracker_remove(&ctx->tracker, req);
        return 1;
      }
    }

    {
      int rr = router_handle(&ctx->dev_mgr,
                             &ctx->module_registry,
                             ctx->pmc_fd,
                             ctx->tmc_fd,
                             &in,
                             &out);

      if(rr == 2){
        pending_request_t* req = 0;
        int target_fd = -1;

        if(!in.has_dev){
          return -1;
        }

        target_fd = resolve_target_fd(ctx, in.dev);
        if(target_fd < 0){
          return -1;
        }

        if(in.has_seq){
          req = request_tracker_alloc(&ctx->tracker);
          if(!req){
            fprintf(stderr, "[EQD] request tracker full\n");
            return -1;
          }

          req->client_fd = conn->fd;
          req->seq = in.seq;
          req->dev = in.dev;
          req->type = in.type;
          req->from_scheduler = 0;
          req->created_ms = now_ms();

          printf("[EQD] save pending fd=%d dev=%u seq=%u -> target_fd=%d\n",
                 conn->fd, in.dev, in.seq, target_fd);
        }

        wn = line_format(&in, wbuf, wbuf_sz);
        if(wn < 0){
          request_tracker_remove(&ctx->tracker, req);
          return -1;
        }

        if(write(target_fd, wbuf, (size_t)wn) < 0){
          request_tracker_remove(&ctx->tracker, req);
          return -1;
        }

        return 0; // 클라이언트에 즉시 응답 안 함
      }

      if(rr <= 0){
        return 0;
      }
    }
  }

  wn = line_format(&out, wbuf, wbuf_sz);
  if(wn < 0) return -1;

  if(write(conn->fd, wbuf, (size_t)wn) < 0){
    return -1;
  }

  return 1;
}

static void refresh_module_health(module_registry_t* reg){
  long long now;

  if(!reg) return;

  now = now_ms();

  reg->pmc_preclean.healthy =
      !module_registry_is_stale(&reg->pmc_preclean, now, MODULE_STALE_MS);

  reg->pmc_deposition.healthy =
      !module_registry_is_stale(&reg->pmc_deposition, now, MODULE_STALE_MS);

  reg->tmc.healthy =
      !module_registry_is_stale(&reg->tmc, now, MODULE_STALE_MS);
}

static void run_scheduler(app_ctx_t* ctx){
  scheduler_action_t action;
  message_t req;
  char wbuf[256];
  int wn;
  int pending_valid;
  pending_request_t* req_slot;

  if(!ctx) return;

  refresh_module_health(&ctx->module_registry);

  if(ctx->pmc_fd < 0) return;

  pending_valid = tracker_has_pending(&ctx->tracker);

  printf("[SCHED] job=%s preclean(reg=%d healthy=%d state=%s) deposition(reg=%d healthy=%d state=%s) pending=%d\n",
       job_state_to_string(ctx->job_state),
       ctx->module_registry.pmc_preclean.registered,
       ctx->module_registry.pmc_preclean.healthy,
       ctx->module_registry.pmc_preclean.has_state ? ctx->module_registry.pmc_preclean.current_state : "NONE",
       ctx->module_registry.pmc_deposition.registered,
       ctx->module_registry.pmc_deposition.healthy,
       ctx->module_registry.pmc_deposition.has_state ? ctx->module_registry.pmc_deposition.current_state : "NONE",
       pending_valid);

  scheduler_tick(&ctx->module_registry,
                 pending_valid,
                 &ctx->next_seq,
                 &ctx->job_state,
                 &action);

  if(!action.should_send){
    return;
  }

  req_slot = request_tracker_alloc(&ctx->tracker);
  if(!req_slot){
    fprintf(stderr, "[EQD] request tracker full for scheduler\n");
    return;
  }

  memset(&req, 0, sizeof(req));
  req.role = ROLE_REQ;
  req.type = TYPE_START;
  req.dev = (uint32_t)action.dev;
  req.has_dev = 1;
  req.seq = action.seq;
  req.has_seq = 1;

  req_slot->client_fd = -1;
  req_slot->seq = req.seq;
  req_slot->dev = req.dev;
  req_slot->type = TYPE_START;
  req_slot->from_scheduler = 1;
  req_slot->created_ms = now_ms();

  wn = line_format(&req, wbuf, sizeof(wbuf));
  if(wn < 0){
    request_tracker_remove(&ctx->tracker, req_slot);
    return;
  }

  if(write(ctx->pmc_fd, wbuf, (size_t)wn) < 0){
    request_tracker_remove(&ctx->tracker, req_slot);
    return;
  }

  printf("[EQD] scheduler sent START dev=%u seq=%u\n", req.dev, req.seq);
}

static void handle_accept_event(int epfd, int sfd){
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

    memset(conn, 0, sizeof(*conn));
    conn->fd = cfd;
    session_init(&conn->session);
    conn->state = CONN_OPEN;
    conn->last_active_ms = now_ms();

    register_connection(conn);

    struct epoll_event cev;
    memset(&cev, 0, sizeof(cev));
    cev.events = EPOLLIN;
    cev.data.ptr = conn;

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev) < 0){
      perror("epoll_ctl client add");
      unregister_connection(conn);
      close(cfd);
      free(conn);
      continue;
    }

    printf("[equipmentd] client connected fd=%d\n", cfd);

  }
}

static void handle_client_event(
  int epfd,
  connection_t* conn,
  app_ctx_t* ctx,
  char* line,
  size_t line_sz,
  char* wbuf,
  size_t wbuf_sz
){
  int fd = conn->fd;

  printf("[epoll] client event fd=%d\n", fd);

  while(1){
    char rbuf[1024];
    ssize_t n = read(fd, rbuf, sizeof(rbuf));

    if(n == 0){
      printf("[equipmentd] client closed fd=%d\n", fd);
      if(fd == ctx->pmc_fd) ctx->pmc_fd = -1;
      if(fd == ctx->tmc_fd) ctx->tmc_fd = -1;
      close_connection(epfd, conn);
      break;
    }

    if(n < 0){
      if(errno == EAGAIN || errno == EWOULDBLOCK){
        break;
      }
      perror("read");
      if(fd == ctx->pmc_fd) ctx->pmc_fd = -1;
      if(fd == ctx->tmc_fd) ctx->tmc_fd = -1;
      close_connection(epfd, conn);
      break;
    }

    printf("[read] fd=%d n=%zd\n", fd, n);
    conn->last_active_ms = now_ms();

    if(session_feed(&conn->session, rbuf, (size_t)n) != 0){
      fprintf(stderr, "[equipmentd] session overflow\n");
      close_connection(epfd, conn);
      break;
    }

    while(1){
      int got = session_pop_line(&conn->session, line, line_sz);
      if(got == 0) break;

      if(got < 0){
        fprintf(stderr, "[equipmentd] line too long\n");
        break;
      }

      printf("[RX line] %s\n", line);

      int hr = handle_one_line(conn, ctx, line, wbuf, wbuf_sz);
      if(hr < 0){
        perror("handle_one_line");
        if(fd == ctx->pmc_fd) ctx->pmc_fd = -1;
        if(fd == ctx->tmc_fd) ctx->tmc_fd = -1;
        close_connection(epfd, conn);
        break;
      }
    }

    // handle_one_line에서 close된 경우 추가 작업 방지
    if(conn->state == CONN_CLOSED){
      break;
    }
  }
}

int main(int argc, char** argv){
  int port = (argc >= 2) ? atoi(argv[1]) : 12345;

  app_ctx_t ctx;

  device_manager_init(&ctx.dev_mgr);
  module_registry_init(&ctx.module_registry);

  ctx.pmc_fd = -1;
  ctx.tmc_fd = -1;

  request_tracker_init(&ctx.tracker);

  ctx.next_seq = 1000;

  ctx.job_state = JOB_IDLE;
  
  char line[512];
  char wbuf[512];

  int sfd = make_server(port);
  if(sfd < 0){
    perror("server");
    return 1;
  }

  if(set_nonblocking(sfd) < 0){
    perror("set_nonblocking sfd");
    close(sfd);
    return 1;
  }

  printf("[equipmentd] listening on %d\n", port);

  int epfd = epoll_create1(0);
  if(epfd < 0){
    perror("epoll_create");
    close(sfd);
    return 1;
  }

  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.fd = sfd;

  if(epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) < 0){
    perror("epoll_ctl");
    close(epfd);
    close(sfd);
    return 1;
  }

  struct epoll_event events[16];

  while(1){
    int nfds = epoll_wait(epfd, events, 16, 1000);
    if(nfds < 0){
      if(errno == EINTR) continue;
      perror("epoll_wait");
      break;
    }

    for(int i = 0; i < nfds; i++){
      if(events[i].data.fd == sfd){
        printf("[epoll] accept event\n");
        handle_accept_event(epfd, sfd);
      } else {
        connection_t* conn = (connection_t*)events[i].data.ptr;
        handle_client_event(epfd, conn, &ctx, line, sizeof(line), wbuf, sizeof(wbuf));
      }
    }
    run_scheduler(&ctx);
    request_tracker_sweep_timeouts(&ctx.tracker, now_ms());
    scan_connection_timeout(epfd, now_ms());
  }

  close(sfd);
  close(epfd);
  return 0;
}