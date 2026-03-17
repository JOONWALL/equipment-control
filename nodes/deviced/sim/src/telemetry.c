#include "telemetry.h"
#include <string.h>

void telemetry_build_status_evt(message_t* msg, int dev_id, sim_state_t state){
  memset(msg, 0, sizeof(*msg));

  msg->role = ROLE_EVT;
  msg->type = TYPE_STATUS;

  msg->dev = dev_id;
  msg->has_dev = 1;

  strncpy(msg->state, fsm_state_to_string(state), sizeof(msg->state)-1);
  msg->has_state = 1;
}

void telemetry_build_telemetry_evt(message_t* msg, int dev_id, process_model_t* m){
  memset(msg, 0, sizeof(*msg));

  msg->role = ROLE_EVT;
  msg->type = TYPE_TELEMETRY;

  msg->dev = dev_id;
  msg->has_dev = 1;

  msg->temp = m->temp;
  msg->pressure = m->pressure;
  msg->flow = m->flow;

  msg->has_temp = 1;
  msg->has_pressure = 1;
  msg->has_flow = 1;
}