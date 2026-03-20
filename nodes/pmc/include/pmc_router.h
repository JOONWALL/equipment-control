#ifndef PMC_ROUTER_H
#define PMC_ROUTER_H

#include "pmc_connection.h"
#include "protocol/message.h"

int pmc_handle_register(pmc_connection_t* conn, const message_t* msg);
int pmc_route_message(pmc_connection_t* from, const message_t* msg);

#endif