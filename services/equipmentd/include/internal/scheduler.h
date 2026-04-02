#pragma once

#include <stdint.h>
#include "internal/module_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  JOB_IDLE = 0,
  JOB_PRECLEAN_SENT,
  JOB_PRECLEAN_RUNNING,
  JOB_PRECLEAN_DONE,
  JOB_DEPOSITION_SENT,
  JOB_DEPOSITION_RUNNING,
  JOB_STOPPED,
  JOB_COMPLETE,
  JOB_ERROR
} job_state_t;

const char* job_state_to_string(job_state_t s);

typedef struct {
  int should_send;
  uint32_t seq;
  int dev;
} scheduler_action_t;

void scheduler_tick(module_registry_t* reg,
                    int pending_valid,
                    uint32_t* next_seq,
                    job_state_t* job_state,
                    scheduler_action_t* action);

#ifdef __cplusplus
}
#endif