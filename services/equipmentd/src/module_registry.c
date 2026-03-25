#include "internal/module_registry.h"
#include <string.h>

void module_registry_init(module_registry_t* reg){
  if(!reg) return;
  memset(reg, 0, sizeof(*reg));
}