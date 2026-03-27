#include "internal/scheduler.h"
#include <string.h>

static int is_state(const module_info_t* m, const char* s){
  return m && m->has_state && strcmp(m->current_state, s) == 0;
}

void scheduler_tick(module_registry_t* reg,
                    int pending_valid,
                    uint32_t* next_seq,
                    int* preclean_started,
                    int* preclean_done,
                    int* deposition_started,
                    int* deposition_done,
                    scheduler_action_t* action){
  if(!reg || !next_seq || !preclean_started || !preclean_done ||
     !deposition_started || !deposition_done || !action){
    return;
  }

  action->should_send = 0;
  action->seq = 0;
  action->dev = -1;

  if(pending_valid){
    return;
  }

  // preclean DONE 감지
  if(*preclean_started && !*preclean_done &&
     is_state(&reg->pmc_preclean, "DONE")){
    *preclean_done = 1;
  }

  // deposition DONE 감지
  if(*deposition_started && !*deposition_done &&
     is_state(&reg->pmc_deposition, "DONE")){
    *deposition_done = 1;
  }

  // 1) preclean 아직 시작 안 했고, IDLE이면 시작
  if(!*preclean_started &&
     reg->pmc_preclean.registered &&
     is_state(&reg->pmc_preclean, "IDLE")){
    action->should_send = 1;
    action->dev = 1;
    action->seq = (*next_seq)++;
    *preclean_started = 1;
    return;
  }

  // 2) preclean 끝났고 deposition 아직 시작 안 했고, deposition이 IDLE이면 시작
  if(*preclean_done &&
     !*deposition_started &&
     reg->pmc_deposition.registered &&
     is_state(&reg->pmc_deposition, "IDLE")){
    action->should_send = 1;
    action->dev = 2;
    action->seq = (*next_seq)++;
    *deposition_started = 1;
    return;
  }
}