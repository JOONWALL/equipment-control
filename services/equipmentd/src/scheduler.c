#include "internal/scheduler.h"
#include <string.h>

static int is_state(const module_info_t* m, const char* s){
  return m && m->has_state && strcmp(m->current_state, s) == 0;
}

void scheduler_tick(module_registry_t* reg,
                    int pending_valid,
                    uint32_t* next_seq,
                    job_state_t* job_state,
                    scheduler_action_t* action){
  if(!reg || !next_seq || !job_state || !action){
    return;
  }

  action->should_send = 0;
  action->seq = 0;
  action->dev = -1;

  // pending 있으면 새 명령 안 보냄
  if(pending_valid){
    return;
  }

  // 비정상 상태 우선 감지
  if(is_state(&reg->pmc_preclean, "FAULT") ||
     is_state(&reg->pmc_deposition, "FAULT")){
    *job_state = JOB_ERROR;
    return;
  }

  switch(*job_state){
    case JOB_IDLE:
      if(reg->pmc_preclean.registered &&
         is_state(&reg->pmc_preclean, "IDLE")){
        action->should_send = 1;
        action->dev = 1;
        action->seq = (*next_seq)++;
        *job_state = JOB_PRECLEAN_SENT;
      }
      break;

    case JOB_PRECLEAN_SENT:
      if(is_state(&reg->pmc_preclean, "PROCESSING")){
        *job_state = JOB_PRECLEAN_RUNNING;
      } else if(is_state(&reg->pmc_preclean, "DONE")){
        *job_state = JOB_PRECLEAN_DONE;
      }
      break;

    case JOB_PRECLEAN_RUNNING:
      if(is_state(&reg->pmc_preclean, "DONE")){
        *job_state = JOB_PRECLEAN_DONE;
      }
      break;

    case JOB_PRECLEAN_DONE:
      if(reg->pmc_deposition.registered &&
         is_state(&reg->pmc_deposition, "IDLE")){
        action->should_send = 1;
        action->dev = 2;
        action->seq = (*next_seq)++;
        *job_state = JOB_DEPOSITION_SENT;
      }
      break;

    case JOB_DEPOSITION_SENT:
      if(is_state(&reg->pmc_deposition, "PROCESSING")){
        *job_state = JOB_DEPOSITION_RUNNING;
      } else if(is_state(&reg->pmc_deposition, "DONE")){
        *job_state = JOB_COMPLETE;
      }
      break;

    case JOB_DEPOSITION_RUNNING:
      if(is_state(&reg->pmc_deposition, "DONE")){
        *job_state = JOB_COMPLETE;
      }
      break;

    case JOB_COMPLETE:
    case JOB_ERROR:
    default:
      break;
  }
}