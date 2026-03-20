#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ROLE: direction/semantics
//메세지 방향, 성격 구분 각 요청,응답,비동기 이벤트, 파싱 실패 대비
typedef enum {
  ROLE_REQ = 0,
  ROLE_RESP,
  ROLE_EVT,
  ROLE_UNKNOWN
} msg_role_t;

// TYPE: command/event kind (Phase1 minimal)
//연결 확인, 상태전이(START/STOP), 상태 조회, 오류 알림, 파싱 실패 대비
typedef enum {
  TYPE_PING = 0,
  TYPE_REGISTER,
  TYPE_START,
  TYPE_STOP,
  TYPE_STATUS,
  TYPE_RESET,
  TYPE_FAULT,
  TYPE_ERROR,

  TYPE_TELEMETRY,
  TYPE_ALARM,

  TYPE_UNKNOWN
} msg_type_t;

// Parsed message object (Core must never see raw bytes)
typedef struct {
  msg_role_t role;
  msg_type_t type;

  // common keys
  uint32_t dev;  int has_dev;   // device id
  uint32_t seq;  int has_seq;   // request sequence id

  int ok;        int has_ok;    // RESP only (0/1)
  int32_t code;  int has_code;  // error code

  char state[32]; int has_state; // STATUS state token (e.g., IDLE/RUNNING)
  char msg[128];  int has_msg;   // human readable (Phase1: no spaces, '_' recommended)

  double temp; int has_temp;
  double pressure; int has_pressure;
  double flow; int has_flow;
} message_t;

#ifdef __cplusplus
}
#endif
