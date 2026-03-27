#include "interlock.h"
#include <string.h>
#include <stdio.h>

int pmc_interlock_check(const pmc_connection_t* sim, const message_t* msg){
  if(!sim || !msg) return -1;
  if(!sim->has_state) return -1;
  printf("[INTERLOCK] has_state=%d state=%s type=%d\n",
    sim->has_state, sim->state, msg->type);
  switch(msg->type){
    case TYPE_START:
      return (strcmp(sim->state, "IDLE") == 0) ? 0 : -1;

    case TYPE_STOP:
      return (strcmp(sim->state, "PROCESSING") == 0) ? 0 : -1;

    case TYPE_RESET:
      return (strcmp(sim->state, "FAULT") == 0) ? 0 : -1;

    case TYPE_FAULT:
      return 0;

    default:
      return 0;
  }
}