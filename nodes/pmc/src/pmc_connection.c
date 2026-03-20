#include "pmc_connection.h"

static pmc_connection_t* g_head = 0;

void pmc_conn_add(pmc_connection_t* conn){
  if(!conn) return;
  conn->next = g_head;
  g_head = conn;
}

void pmc_conn_remove(pmc_connection_t* conn){
  pmc_connection_t** cur = &g_head;

  while(*cur){
    if(*cur == conn){
      *cur = conn->next;
      return;
    }
    cur = &(*cur)->next;
  }
}

pmc_connection_t* pmc_find_eqd(void){
  pmc_connection_t* cur = g_head;

  while(cur){
    if(cur->registered && cur->type == PEER_EQD){
      return cur;
    }
    cur = cur->next;
  }
  return 0;
}

pmc_connection_t* pmc_find_sim(int dev_id){
  pmc_connection_t* cur = g_head;

  while(cur){
    if(cur->registered && cur->type == PEER_SIM && cur->dev_id == dev_id){
      return cur;
    }
    cur = cur->next;
  }
  return 0;
}