#ifndef PMC_ALARM_MANAGER_H
#define PMC_ALARM_MANAGER_H

typedef enum {
  PMC_ALARM_NONE = 0,
  PMC_ALARM_MISSING_DEV,
  PMC_ALARM_SIM_NOT_FOUND,
  PMC_ALARM_INTERLOCK_BLOCKED,
  PMC_ALARM_IO_SEND_FAILED
} pmc_alarm_code_t;

void pmc_alarm_init(void);
void pmc_alarm_clear(void);
void pmc_alarm_raise(int dev_id, pmc_alarm_code_t code, const char* detail);

pmc_alarm_code_t pmc_alarm_last_code(void);
int pmc_alarm_last_dev(void);
const char* pmc_alarm_last_detail(void);

#endif