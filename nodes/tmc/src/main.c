#define _POSIX_C_SOURCE 200809L

#include "protocol/line_codec.h"
#include "protocol/session.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define TMC_DEV_ID 100
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 12345
#define REGISTER_SEQ 1

typedef enum {
  TMC_IDLE = 0,
  TMC_TRANSFERRING,
  TMC_DONE,
  TMC_FAULT
} tmc_state_t;

typedef struct {
  int sock;
  session_t session;
  tmc_state_t state;
  long long transfer_started_ms;
  long long last_status_sent_ms;
} tmc_ctx_t;

static long long now_ms(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static const char* tmc_state_to_string(tmc_state_t state){
  switch(state){
    case TMC_IDLE: return "IDLE";
    case TMC_TRANSFERRING: return "TRANSFERRING";
    case TMC_DONE: return "DONE";
    case TMC_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

static int connect_tcp(const char* host, int port){
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0) return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);

  if(inet_pton(AF_INET, host, &addr.sin_addr) <= 0){
    close(sock);
    return -1;
  }

  if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0){
    close(sock);
    return -1;
  }

  return sock;
}

static int send_message(int sock, const message_t* msg){
  char buf[BUF_SIZE];
  int len;

  if(sock < 0 || !msg) return -1;

  len = line_format(msg, buf, sizeof(buf));
  if(len < 0) return -1;

  if(send(sock, buf, (size_t)len, 0) < 0){
    return -1;
  }

  return 0;
}

static int send_register_req(tmc_ctx_t* ctx){
  message_t msg;

  if(!ctx) return -1;

  memset(&msg, 0, sizeof(msg));
  msg.role = ROLE_REQ;
  msg.type = TYPE_REGISTER;
  msg.dev = TMC_DEV_ID;
  msg.has_dev = 1;
  msg.seq = REGISTER_SEQ;
  msg.has_seq = 1;

  return send_message(ctx->sock, &msg);
}

static int send_status_evt(tmc_ctx_t* ctx){
  message_t msg;

  if(!ctx) return -1;

  memset(&msg, 0, sizeof(msg));
  msg.role = ROLE_EVT;
  msg.type = TYPE_STATUS;
  msg.dev = TMC_DEV_ID;
  msg.has_dev = 1;

  strncpy(msg.state, tmc_state_to_string(ctx->state), sizeof(msg.state) - 1);
  msg.state[sizeof(msg.state) - 1] = '\0';
  msg.has_state = 1;

  if(send_message(ctx->sock, &msg) != 0){
    return -1;
  }

  ctx->last_status_sent_ms = now_ms();
  return 0;
}

static int send_simple_resp(tmc_ctx_t* ctx, const message_t* req, int ok, int code, const char* text){
  message_t resp;

  if(!ctx || !req) return -1;

  memset(&resp, 0, sizeof(resp));
  resp.role = ROLE_RESP;
  resp.type = ok ? req->type : TYPE_ERROR;

  if(req->has_dev){
    resp.dev = req->dev;
    resp.has_dev = 1;
  }

  if(req->has_seq){
    resp.seq = req->seq;
    resp.has_seq = 1;
  }

  resp.ok = ok ? 1 : 0;
  resp.has_ok = 1;

  if(!ok){
    resp.code = code;
    resp.has_code = 1;

    if(text){
      strncpy(resp.msg, text, sizeof(resp.msg) - 1);
      resp.msg[sizeof(resp.msg) - 1] = '\0';
      resp.has_msg = 1;
    }
  }

  return send_message(ctx->sock, &resp);
}

static int send_status_resp(tmc_ctx_t* ctx, const message_t* req){
  message_t resp;

  if(!ctx || !req) return -1;

  memset(&resp, 0, sizeof(resp));
  resp.role = ROLE_RESP;
  resp.type = TYPE_STATUS;

  if(req->has_dev){
    resp.dev = req->dev;
    resp.has_dev = 1;
  }

  if(req->has_seq){
    resp.seq = req->seq;
    resp.has_seq = 1;
  }

  resp.ok = 1;
  resp.has_ok = 1;

  strncpy(resp.state, tmc_state_to_string(ctx->state), sizeof(resp.state) - 1);
  resp.state[sizeof(resp.state) - 1] = '\0';
  resp.has_state = 1;

  return send_message(ctx->sock, &resp);
}

static int handle_req_message(tmc_ctx_t* ctx, const message_t* in){
  if(!ctx || !in) return -1;

  if(in->role != ROLE_REQ){
    return 0;
  }

  if(in->type == TYPE_STATUS){
    return send_status_resp(ctx, in);
  }

  switch(in->type){
    case TYPE_START:
      if(ctx->state == TMC_IDLE || ctx->state == TMC_DONE){
        ctx->state = TMC_TRANSFERRING;
        ctx->transfer_started_ms = now_ms();
        if(send_simple_resp(ctx, in, 1, 0, NULL) != 0) return -1;
        if(send_status_evt(ctx) != 0) return -1;
        return 1;
      }
      return send_simple_resp(ctx, in, 0, 409, "INVALID_STATE_TRANSITION");

    case TYPE_STOP:
      if(ctx->state == TMC_TRANSFERRING){
        ctx->state = TMC_IDLE;
        ctx->transfer_started_ms = 0;
        if(send_simple_resp(ctx, in, 1, 0, NULL) != 0) return -1;
        if(send_status_evt(ctx) != 0) return -1;
        return 1;
      }
      return send_simple_resp(ctx, in, 0, 409, "INVALID_STATE_TRANSITION");

    case TYPE_FAULT:
      ctx->state = TMC_FAULT;
      ctx->transfer_started_ms = 0;
      if(send_simple_resp(ctx, in, 1, 0, NULL) != 0) return -1;
      if(send_status_evt(ctx) != 0) return -1;
      return 1;

    case TYPE_RESET:
      if(ctx->state == TMC_FAULT){
        ctx->state = TMC_IDLE;
        ctx->transfer_started_ms = 0;
        if(send_simple_resp(ctx, in, 1, 0, NULL) != 0) return -1;
        if(send_status_evt(ctx) != 0) return -1;
        return 1;
      }
      return send_simple_resp(ctx, in, 0, 409, "INVALID_STATE_TRANSITION");

    default:
      return send_simple_resp(ctx, in, 0, 404, "UNKNOWN_TYPE");
  }
}

static void tick_state_machine(tmc_ctx_t* ctx){
  long long now;

  if(!ctx) return;

  now = now_ms();

  if(ctx->state == TMC_TRANSFERRING){
    if(ctx->transfer_started_ms > 0 &&
       now - ctx->transfer_started_ms >= 2000){
      ctx->state = TMC_DONE;
      ctx->transfer_started_ms = 0;
      (void)send_status_evt(ctx);
    }
  }

  if(now - ctx->last_status_sent_ms >= 1000){
    (void)send_status_evt(ctx);
  }
}

int main(int argc, char* argv[]){
  const char* host = DEFAULT_HOST;
  int port = DEFAULT_PORT;

  if(argc >= 2){
    host = argv[1];
  }
  if(argc >= 3){
    port = atoi(argv[2]);
  }

  tmc_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));

  ctx.sock = connect_tcp(host, port);
  if(ctx.sock < 0){
    perror("connect_tcp");
    return 1;
  }

  session_init(&ctx.session);
  ctx.state = TMC_IDLE;
  ctx.transfer_started_ms = 0;
  ctx.last_status_sent_ms = 0;

  printf("[TMC] connected to CTC %s:%d fd=%d\n", host, port, ctx.sock);

  if(send_register_req(&ctx) != 0){
    perror("send_register_req");
    close(ctx.sock);
    return 1;
  }

  if(send_status_evt(&ctx) != 0){
    perror("send_status_evt");
    close(ctx.sock);
    return 1;
  }

  while(1){
    char buf[BUF_SIZE];
    char line[BUF_SIZE];
    ssize_t n = recv(ctx.sock, buf, sizeof(buf), MSG_DONTWAIT);

    if(n > 0){
      if(session_feed(&ctx.session, buf, (size_t)n) != 0){
        fprintf(stderr, "[TMC] session overflow\n");
        break;
      }

      while(session_pop_line(&ctx.session, line, sizeof(line)) == 1){
        message_t in;

        if(line_parse(line, &in) != 0){
          continue;
        }

        if(in.role == ROLE_RESP &&
           in.type == TYPE_REGISTER &&
           in.has_ok && in.ok == 1){
          printf("[TMC] CTC register ack ok=1\n");
          continue;
        }

        if(in.has_dev && in.dev != TMC_DEV_ID){
          continue;
        }

        printf("[TMC] RX type=%d role=%d dev=%u has_dev=%d\n",
               in.type, in.role, in.dev, in.has_dev);

        if(handle_req_message(&ctx, &in) < 0){
          fprintf(stderr, "[TMC] handle_req_message failed\n");
          break;
        }
      }
    } else if(n == 0){
      printf("[TMC] connection closed by CTC\n");
      break;
    } else {
      if(errno != EAGAIN && errno != EWOULDBLOCK){
        perror("recv");
        break;
      }
    }

    tick_state_machine(&ctx);

    {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 100 * 1000 * 1000; // 100ms
      nanosleep(&ts, NULL);
    }
  }

  close(ctx.sock);
  return 0;
}