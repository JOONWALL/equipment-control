#include "sim.h"
#include "protocol/line_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h> // usleep POSIX 2008에서 폐기라 코드 대체

#define BUF_SIZE 1024

static int connect_tcp(const char* host, int port){
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0) return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

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

int sim_connect(sim_ctx_t* ctx, const char* host, int port){
  ctx->sock = connect_tcp(host, port);
  return (ctx->sock >= 0) ? 0 : -1;
}

sim_cmd_t sim_cmd_from_message(const message_t* msg){
  if(!msg) return SIM_CMD_NONE;

  switch(msg->type){
    case TYPE_START: return SIM_CMD_START;
    case TYPE_STOP: return SIM_CMD_STOP;
    case TYPE_RESET: return SIM_CMD_RESET;
    case TYPE_FAULT: return SIM_CMD_FAULT;
    default: return SIM_CMD_NONE;
  }
}

int sim_handle_message(sim_ctx_t* ctx, const message_t* in, message_t* out){
  if(!ctx || !in || !out) return -1;

  if(in->role != ROLE_REQ) return 0;

  sim_cmd_t cmd = sim_cmd_from_message(in);
  int rc = fsm_handle_command(&ctx->fsm, cmd);

  memset(out, 0, sizeof(*out));
  out->role = ROLE_RESP;
  out->type = in->type;

  if(in->has_seq){
    out->seq = in->seq;
    out->has_seq = 1;
  }

  out->dev = ctx->dev_id;
  out->has_dev = 1;

  if(rc == 0){
    out->ok = 1;
    out->has_ok = 1;
  } else {
    out->ok = 0;
    out->has_ok = 1;
    out->type = TYPE_ERROR;
    out->code = 500;
    out->has_code = 1;
    strcpy(out->msg, "SIM_CMD_FAILED");
    out->has_msg = 1;
  }

  return 1;
}

int main(int argc, char* argv[]){
  int dev_id = 1;
  const char* host = "127.0.0.1";
  int port = 12346;

  for(int i = 1; i < argc; i++){
    if(strcmp(argv[i], "--dev") == 0 && i + 1 < argc){
      dev_id = atoi(argv[++i]);
    }
  }

  sim_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));

  ctx.dev_id = dev_id;

  fsm_init(&ctx.fsm);
  process_model_init(&ctx.model);
  session_init(&ctx.session);

  if(sim_connect(&ctx, host, port) != 0){
    printf("connect failed\n");
    return 1;
  }

  printf("connected to %s:%d (dev=%d)\n", host, port, dev_id);

  // PMC에 SIM 등록
  {
    message_t reg;
    char regbuf[512];
    int reglen;

    memset(&reg, 0, sizeof(reg));
    reg.role = ROLE_EVT;
    reg.type = TYPE_REGISTER;
    reg.dev = ctx.dev_id;
    reg.has_dev = 1;

    reglen = line_format(&reg, regbuf, sizeof(regbuf));
    if(reglen > 0){
      send(ctx.sock, regbuf, reglen, 0);
    }
  }

  char buf[BUF_SIZE];
  char line[BUF_SIZE];
  struct timespec ts = {1, 0};

  while(1){
    int n = recv(ctx.sock, buf, sizeof(buf), MSG_DONTWAIT);
    if(n > 0){
      session_feed(&ctx.session, buf, n);

      while(session_pop_line(&ctx.session, line, sizeof(line)) == 1){
        message_t in, out;

        if(line_parse(line, &in) != 0) continue;

        if(sim_handle_message(&ctx, &in, &out) == 1){
          char outbuf[BUF_SIZE];
          int outlen = line_format(&out, outbuf, sizeof(outbuf));
          if(outlen > 0){
            send(ctx.sock, outbuf, outlen, 0);
          }
        }
      }
    }

    fsm_tick(&ctx.fsm);

    process_model_apply_state(&ctx.model, ctx.fsm.state);
    process_model_update(&ctx.model);

    message_t evt;
    telemetry_build_telemetry_evt(&evt, ctx.dev_id, &ctx.model);

    char outbuf[BUF_SIZE];
    int outlen = line_format(&evt, outbuf, sizeof(outbuf));
    if(outlen > 0){
      send(ctx.sock, outbuf, outlen, 0);
    }

    nanosleep(&ts, NULL);
  }

  return 0;
}