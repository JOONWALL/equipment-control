#ifndef PMC_INTERLOCK_H
#define PMC_INTERLOCK_H

#include "pmc_connection.h"
#include "protocol/message.h"

#ifdef __cplusplus
extern "C" {
#endif

int pmc_interlock_check(const pmc_connection_t* sim, const message_t* msg);

#ifdef __cplusplus
}
#endif

#endif