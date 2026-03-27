#include "fsm.h"

void fsm_init(fsm_t* fsm){
  if(!fsm) return;
  fsm->state = SIM_STATE_BOOTING;
  fsm->processing_ticks = 0;
}

int fsm_handle_command(fsm_t* fsm, sim_cmd_t cmd){
  if(!fsm) return -1;

  switch(cmd){
    case SIM_CMD_START:
      if(fsm->state == SIM_STATE_IDLE || fsm->state == SIM_STATE_READY){
        fsm->state = SIM_STATE_READY;
        fsm->processing_ticks = 0;
        return 0;
      }
      return -2;

    case SIM_CMD_STOP:
      if(fsm->state == SIM_STATE_PROCESSING){
        fsm->state = SIM_STATE_IDLE;
        fsm->processing_ticks = 0;
        return 0;
      }
      return -2;

    case SIM_CMD_RESET:
      if(fsm->state == SIM_STATE_FAULT){
        fsm->state = SIM_STATE_RECOVERING;
        fsm->processing_ticks = 0;
        return 0;
      }
      return -2;

    case SIM_CMD_FAULT:
      fsm->state = SIM_STATE_FAULT;
      fsm->processing_ticks = 0;
      return 0;

    default:
      return -1;
  }
}

void fsm_tick(fsm_t* fsm){
  if(!fsm) return;

  switch(fsm->state){
    case SIM_STATE_BOOTING:
      fsm->state = SIM_STATE_IDLE;
      break;

    case SIM_STATE_READY:
      fsm->state = SIM_STATE_PROCESSING;
      fsm->processing_ticks = 0;
      break;

    case SIM_STATE_PROCESSING:
      fsm->processing_ticks++;
      if(fsm->processing_ticks >= 5){
        fsm->state = SIM_STATE_DONE;
        fsm->processing_ticks = 0;
      }  
      break;

    case SIM_STATE_RECOVERING:
      fsm->state = SIM_STATE_IDLE;
      break;

    default:
      break;
  }
}

const char* fsm_state_to_string(sim_state_t s){
  switch(s){
    case SIM_STATE_BOOTING: return "BOOTING";
    case SIM_STATE_IDLE: return "IDLE";
    case SIM_STATE_READY: return "READY";
    case SIM_STATE_PROCESSING: return "PROCESSING";
    case SIM_STATE_FAULT: return "FAULT";
    case SIM_STATE_RECOVERING: return "RECOVERING";
    case SIM_STATE_DONE: return "DONE";
    default: return "UNKNOWN";
  }
}