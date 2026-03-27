#include "internal/module_registry.h"
#include <string.h>
#include <stdio.h>

void module_registry_init(module_registry_t* reg){
  if(!reg) return;
  memset(reg, 0, sizeof(*reg));
}

static void dump_one(const char* label, const module_info_t* m){
  if(!m) return;

  printf("[REG] %s: registered=%d dev=%d busy=%d",
         label, m->registered, m->dev, m->busy);
  if(m->has_state){
    printf(" state=%s", m->current_state);
  }
  if(m->has_telemetry){
    printf(" temp=%.2f pressure=%.2f flow=%.2f",
           m->temp, m->pressure, m->flow);
  }

  printf("\n");
}

void module_registry_dump(const module_registry_t* reg){
  if(!reg) return;
  dump_one("tmc", &reg->tmc);
  dump_one("pmc_preclean", &reg->pmc_preclean);
  dump_one("pmc_deposition", &reg->pmc_deposition);
}