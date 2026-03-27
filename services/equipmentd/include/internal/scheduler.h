#pragma once

#include <stdint.h>
#include "internal/module_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int should_send;
  uint32_t seq;
  int dev;
} scheduler_action_t;

void scheduler_tick(module_registry_t* reg,
                    int pending_valid,
                    uint32_t* next_seq,
                    int* preclean_started,
                    int* preclean_done,
                    int* deposition_started,
                    int* deposition_done,
                    scheduler_action_t* action);

#ifdef __cplusplus
}
#endif