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
} module_info_t;

typedef struct {
  module_info_t tmc;
  module_info_t pmc_preclean;
  module_info_t pmc_deposition;
} module_registry_t;

void module_registry_init(module_registry_t* reg);

#endif