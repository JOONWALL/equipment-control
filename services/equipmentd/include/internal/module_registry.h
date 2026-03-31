#ifndef INTERNAL_MODULE_REGISTRY_H
#define INTERNAL_MODULE_REGISTRY_H

typedef enum {
  MODULE_UNKNOWN = 0,
  MODULE_TMC,
  MODULE_PMC
} module_type_t;

typedef struct {
  int registered;
  module_type_t type;
  int dev;
  int busy;
  char name[32];

  char current_state[32];
  int has_state;

  double temp;
  double pressure;
  double flow;
  int has_telemetry;

  long long last_update_ms;
  long long last_status_ms;
  long long last_telemetry_ms;
  long long last_state_change_ms;
  int healthy;
  int fault_latched;

} module_info_t;

typedef struct {
  module_info_t tmc;
  module_info_t pmc_preclean;
  module_info_t pmc_deposition;
} module_registry_t;

void module_registry_init(module_registry_t* reg);

void module_registry_dump(const module_registry_t* reg);

void module_registry_touch(module_info_t* m, long long now_ms);

int module_registry_is_stale(const module_info_t* m, long long now_ms, long long stale_ms);

void module_registry_touch_status(module_info_t* m, long long now_ms, const char* new_state);

void module_registry_touch_telemetry(module_info_t* m, long long now_ms);

#endif