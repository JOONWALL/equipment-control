#include "internal/module_registry.h"
#include <string.h>
#include <stdio.h>

void module_registry_init(module_registry_t* reg){
  if(!reg) return;
  memset(reg, 0, sizeof(*reg));
}

void module_registry_touch(module_info_t* m, long long now_ms){
  if(!m) return;
  m->last_update_ms = now_ms;
  m->healthy = 1;
}

void module_registry_touch_status(module_info_t* m, long long now_ms, const char* new_state){
  if(!m) return;

  if(new_state && m->has_state){
    if(strcmp(m->current_state, new_state) != 0){
      m->last_state_change_ms = now_ms;
    }
  } else if(new_state){
    m->last_state_change_ms = now_ms;
  }

  m->last_update_ms = now_ms;
  m->last_status_ms = now_ms;
  m->healthy = 1;
}

void module_registry_touch_telemetry(module_info_t* m, long long now_ms){
  if(!m) return;
  m->last_update_ms = now_ms;
  m->last_telemetry_ms = now_ms;
  m->healthy = 1;
}

int module_registry_is_stale(const module_info_t* m, long long now_ms, long long stale_ms){
  if(!m) return 1;
  if(!m->registered) return 1;
  if(m->last_update_ms <= 0) return 1;
  return (now_ms - m->last_update_ms > stale_ms) ? 1 : 0;
}

static void dump_one(const char* label, const module_info_t* m){
  if(!m) return;

  printf("[REG] %s: registered=%d dev=%d busy=%d healthy=%d last_update_ms=%lld",
         label, m->registered, m->dev, m->busy, m->healthy, m->last_update_ms);

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