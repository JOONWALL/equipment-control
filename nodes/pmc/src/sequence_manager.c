#include "sequence_manager.h"

#include "interlock.h"

pmc_seq_action_t pmc_sequence_decide_eqd_request(const pmc_connection_t* sim,
                                                 const message_t* msg){
  if(!msg) return PMC_SEQ_ACTION_REJECT_MISSING_DEV;

  if(!msg->has_dev){
    return PMC_SEQ_ACTION_REJECT_MISSING_DEV;
  }

  if(!sim){
    return PMC_SEQ_ACTION_REJECT_SIM_NOT_FOUND;
  }

  if(msg->type == TYPE_STATUS){
    return PMC_SEQ_ACTION_RESPOND_CACHED_STATUS;
  }

  if(pmc_interlock_check(sim, msg) != 0){
    return PMC_SEQ_ACTION_REJECT_INTERLOCK;
  }

  return PMC_SEQ_ACTION_ROUTE_TO_SIM;
}