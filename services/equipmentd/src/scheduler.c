#include "internal/scheduler.h"
#include <string.h>

const char* job_state_to_string(job_state_t s){
  switch(s){
    case JOB_IDLE: return "JOB_IDLE";
    case JOB_PRECLEAN_SENT: return "JOB_PRECLEAN_SENT";
    case JOB_PRECLEAN_RUNNING: return "JOB_PRECLEAN_RUNNING";
    case JOB_PRECLEAN_DONE: return "JOB_PRECLEAN_DONE";
    case JOB_DEPOSITION_SENT: return "JOB_DEPOSITION_SENT";
    case JOB_DEPOSITION_RUNNING: return "JOB_DEPOSITION_RUNNING";
    case JOB_STOPPED: return "JOB_STOPPED";
    case JOB_COMPLETE: return "JOB_COMPLETE";
    case JOB_ERROR: return "JOB_ERROR";
    default: return "JOB_UNKNOWN";
  }
}

static int is_state(const module_info_t* m, const char* s){
  return m && m->has_state && strcmp(m->current_state, s) == 0;
}

static int is_usable(const module_info_t* m, const char* s){
  return m &&
         m->registered &&
         m->healthy &&
         m->has_state &&
         strcmp(m->current_state, s) == 0;
}

static int is_done(const module_info_t* m){
  return m && m->registered && m->healthy && m->has_state &&
         strcmp(m->current_state, "DONE") == 0;
}

static int is_idle_usable(const module_info_t* m){
  return m && m->registered && m->healthy && m->has_state &&
         strcmp(m->current_state, "IDLE") == 0;
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

    if((*job_state == JOB_PRECLEAN_SENT || *job_state == JOB_PRECLEAN_RUNNING) &&
     reg->pmc_preclean.registered &&
     !reg->pmc_preclean.healthy){
    *job_state = JOB_ERROR;
    return;
  }

  if((*job_state == JOB_DEPOSITION_SENT || *job_state == JOB_DEPOSITION_RUNNING) &&
     reg->pmc_deposition.registered &&
     !reg->pmc_deposition.healthy){
    *job_state = JOB_ERROR;
    return;
  }

  // 비정상 상태 우선 감지
  if(is_state(&reg->pmc_preclean, "FAULT") ||
     is_state(&reg->pmc_deposition, "FAULT")){
    *job_state = JOB_ERROR;
    return;
  }

  if(reg->pmc_preclean.registered && !reg->pmc_preclean.healthy &&
    (*job_state == JOB_PRECLEAN_SENT || *job_state == JOB_PRECLEAN_RUNNING)){
    *job_state = JOB_ERROR;
    return;
  }

  if(reg->pmc_deposition.registered && !reg->pmc_deposition.healthy &&
    (*job_state == JOB_DEPOSITION_SENT || *job_state == JOB_DEPOSITION_RUNNING)){
    *job_state = JOB_ERROR;
    return;
  }

  switch(*job_state){
    case JOB_IDLE:
      // 이미 둘 다 끝났으면 complete
      if(is_done(&reg->pmc_preclean) && is_done(&reg->pmc_deposition)){
        *job_state = JOB_COMPLETE;
        break;
      }

      // preclean이 끝났고 deposition이 idle이면 deposition 재시작/시작
      if(is_done(&reg->pmc_preclean) &&
        is_idle_usable(&reg->pmc_deposition)){
        action->should_send = 1;
        action->dev = 2;
        action->seq = (*next_seq)++;
        *job_state = JOB_DEPOSITION_SENT;
        break;
      }

      // 그 외에는 preclean부터 시작
      if(is_idle_usable(&reg->pmc_preclean)){
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
      if(is_usable(&reg->pmc_deposition, "IDLE")){
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

    case JOB_STOPPED:
    case JOB_COMPLETE:
    case JOB_ERROR:
    default:
      break;
  }
}