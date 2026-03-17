#include "process_model.h"

#define TEMP_AMBIENT 25.0
#define PRESSURE_AMBIENT 1.0

void process_model_init(process_model_t* m){
  if(!m) return;

  m->temp = TEMP_AMBIENT;
  m->pressure = PRESSURE_AMBIENT;
  m->flow = 0.0;

  m->heater_on = 0;
  m->pump_on = 0;
}

void process_model_apply_state(process_model_t* m, sim_state_t s){
  if(!m) return;

  switch(s){
    case SIM_STATE_PROCESSING:
      m->heater_on = 1;
      m->pump_on = 1;
      break;

    case SIM_STATE_IDLE:
    case SIM_STATE_READY:
      m->heater_on = 0;
      m->pump_on = 0;
      break;

    case SIM_STATE_FAULT:
      m->heater_on = 0;
      m->pump_on = 0;
      break;

    default:
      break;
  }
}

void process_model_update(process_model_t* m){
  if(!m) return;

  // temperature
  if(m->heater_on){
    m->temp += 1.5;
  } else {
    m->temp -= 0.5;
    if(m->temp < TEMP_AMBIENT) m->temp = TEMP_AMBIENT;
  }

  // pressure
  if(m->pump_on){
    m->pressure -= 0.05;
    if(m->pressure < 0.1) m->pressure = 0.1;
  } else {
    m->pressure += 0.05;
    if(m->pressure > PRESSURE_AMBIENT) m->pressure = PRESSURE_AMBIENT;
  }

  // flow (간단 모델)
  if(m->heater_on){
    m->flow += 2.0;
    if(m->flow > 100.0) m->flow = 100.0;
  } else {
    m->flow -= 2.0;
    if(m->flow < 0.0) m->flow = 0.0;
  }
}