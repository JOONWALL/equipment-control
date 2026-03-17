#ifndef SIM_H
#define SIM_H

#include "fsm.h"
#include "process_model.h"
#include "telemetry.h"
#include "protocol/session.h"
#include "protocol/message.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int dev_id;
  int sock;

  fsm_t fsm;
  process_model_t model;

  session_t session;
} sim_ctx_t;

int sim_connect(sim_ctx_t* ctx, const char* host, int port);

// REQ → FSM command 변환
sim_cmd_t sim_cmd_from_message(const message_t* msg);

// message 처리
int sim_handle_message(sim_ctx_t* ctx, const message_t* in, message_t* out);

#ifdef __cplusplus
}
#endif

#endif