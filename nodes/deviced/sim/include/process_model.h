#ifndef PROCESS_MODEL_H
#define PROCESS_MODEL_H

#include "fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double temp;
  double pressure;
  double flow;

  int heater_on;
  int pump_on;
} process_model_t;

void process_model_init(process_model_t* m);

// FSM 상태 기반으로 내부 상태 설정
void process_model_apply_state(process_model_t* m, sim_state_t s);

// 1 tick 업데이트
void process_model_update(process_model_t* m);

#ifdef __cplusplus
}
#endif

#endif