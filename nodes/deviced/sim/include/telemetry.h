#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "process_model.h"
#include "fsm.h"
#include "protocol/message.h"

#ifdef __cplusplus
extern "C" {
#endif

void telemetry_build_status_evt(message_t* msg, int dev_id, sim_state_t state);

void telemetry_build_telemetry_evt(message_t* msg, int dev_id, process_model_t* m);

#ifdef __cplusplus
}
#endif

#endif