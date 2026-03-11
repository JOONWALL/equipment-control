#pragma once

#include "protocol/session.h"

typedef enum {
  CONN_OPEN = 0,
  CONN_CLOSED = 1
} conn_state_t;

typedef struct {
  int fd;
  session_t session;
  long long last_active_ms;
  conn_state_t state;
} connection_t;