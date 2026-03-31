#include "alarm_manager.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  pmc_alarm_code_t code;
  int dev_id;
  char detail[128];
} pmc_alarm_state_t;

static pmc_alarm_state_t g_alarm_state;

void pmc_alarm_init(void){
  memset(&g_alarm_state, 0, sizeof(g_alarm_state));
  g_alarm_state.code = PMC_ALARM_NONE;
  g_alarm_state.dev_id = -1;
}

void pmc_alarm_clear(void){
  g_alarm_state.code = PMC_ALARM_NONE;
  g_alarm_state.dev_id = -1;
  g_alarm_state.detail[0] = '\0';
}

void pmc_alarm_raise(int dev_id, pmc_alarm_code_t code, const char* detail){
  g_alarm_state.code = code;
  g_alarm_state.dev_id = dev_id;

  if(detail){
    strncpy(g_alarm_state.detail, detail, sizeof(g_alarm_state.detail) - 1);
    g_alarm_state.detail[sizeof(g_alarm_state.detail) - 1] = '\0';
  } else {
    g_alarm_state.detail[0] = '\0';
  }

  printf("[PMC][ALARM] dev=%d code=%d detail=%s\n",
         dev_id,
         code,
         g_alarm_state.detail[0] ? g_alarm_state.detail : "NONE");
}

pmc_alarm_code_t pmc_alarm_last_code(void){
  return g_alarm_state.code;
}

int pmc_alarm_last_dev(void){
  return g_alarm_state.dev_id;
}

const char* pmc_alarm_last_detail(void){
  return g_alarm_state.detail;
}