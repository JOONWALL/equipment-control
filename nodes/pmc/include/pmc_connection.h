#ifndef PMC_CONNECTION_H
#define PMC_CONNECTION_H

#include "protocol/session.h"

typedef enum {
  PEER_UNKNOWN = 0,
  PEER_EQD,
  PEER_SIM
} peer_type_t;

typedef struct pmc_connection {
  int fd;
  peer_type_t type;

  int registered;
  int dev_id;   // SIM일 때만 유효, EQD는 -1

  char state[32];//상태 문자열 저장해서 인터락 대비
  int has_state;

  session_t session;
  struct pmc_connection* next;
} pmc_connection_t;

void pmc_conn_add(pmc_connection_t* conn);
void pmc_conn_remove(pmc_connection_t* conn);

pmc_connection_t* pmc_find_eqd(void);
pmc_connection_t* pmc_find_sim(int dev_id);

#endif