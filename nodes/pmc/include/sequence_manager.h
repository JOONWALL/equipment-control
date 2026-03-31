#ifndef PMC_SEQUENCE_MANAGER_H
#define PMC_SEQUENCE_MANAGER_H

#include "pmc_connection.h"
#include "protocol/message.h"

typedef enum {
  PMC_SEQ_ACTION_ROUTE_TO_SIM = 0,
  PMC_SEQ_ACTION_RESPOND_CACHED_STATUS,
  PMC_SEQ_ACTION_REJECT_MISSING_DEV,
  PMC_SEQ_ACTION_REJECT_SIM_NOT_FOUND,
  PMC_SEQ_ACTION_REJECT_INTERLOCK
} pmc_seq_action_t;

pmc_seq_action_t pmc_sequence_decide_eqd_request(const pmc_connection_t* sim,
                                                 const message_t* msg);

#endif