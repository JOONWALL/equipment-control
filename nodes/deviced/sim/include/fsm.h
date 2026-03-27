#ifndef SIM_FSM_H
#define SIM_FSM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SIM_STATE_BOOTING = 0,
  SIM_STATE_IDLE,
  SIM_STATE_READY,
  SIM_STATE_PROCESSING,
  SIM_STATE_DONE,
  SIM_STATE_FAULT,
  SIM_STATE_RECOVERING
} sim_state_t;

typedef enum {
  SIM_CMD_NONE = 0,
  SIM_CMD_START,
  SIM_CMD_STOP,
  SIM_CMD_RESET,
  SIM_CMD_FAULT
} sim_cmd_t;

typedef struct {
  sim_state_t state;
  int processing_ticks;
} fsm_t;

void fsm_init(fsm_t* fsm);

int fsm_handle_command(fsm_t* fsm, sim_cmd_t cmd);

// 주기 tick (1초 등)
void fsm_tick(fsm_t* fsm);

const char* fsm_state_to_string(sim_state_t s);

#ifdef __cplusplus
}
#endif

#endif