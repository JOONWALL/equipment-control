#pragma once
#include <stdint.h>
#include "protocol/message.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REQUEST_TRACKER_MAX 16
#define REQUEST_TIMEOUT_MS 10000

typedef struct {
  int valid;
  int client_fd;
  uint32_t seq;
  uint32_t dev;
  msg_type_t type;
  int from_scheduler;
  long long created_ms;
} pending_request_t;

typedef struct {
  pending_request_t items[REQUEST_TRACKER_MAX];
} request_tracker_t;

void request_tracker_init(request_tracker_t* t);
pending_request_t* request_tracker_alloc(request_tracker_t* t);
pending_request_t* request_tracker_find(request_tracker_t* t, uint32_t seq, uint32_t dev);
void request_tracker_remove(request_tracker_t* t, pending_request_t* r);
void request_tracker_sweep_timeouts(request_tracker_t* t, long long now_ms);

#ifdef __cplusplus
}
#endif