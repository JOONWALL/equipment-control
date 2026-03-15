#ifndef EQUIPMENTD_DEVICE_MANAGER_H
#define EQUIPMENTD_DEVICE_MANAGER_H

#include "internal/device.h"
#include "protocol/message.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_MANAGER_MAX_DEVICES 16

typedef struct {
  device_t devices[DEVICE_MANAGER_MAX_DEVICES];
  int initialized;
} device_manager_t;

void device_manager_init(device_manager_t* mgr);

device_t* device_manager_get(device_manager_t* mgr, int dev_id);
device_t* device_manager_ensure(device_manager_t* mgr, int dev_id);

int device_manager_get_status(device_manager_t* mgr, int dev_id, device_state_t* out_state);

int device_manager_fault(device_manager_t* mgr, int dev_id);
int device_manager_reset(device_manager_t* mgr, int dev_id);

int device_manager_start(device_manager_t* mgr, int dev_id);
int device_manager_stop(device_manager_t* mgr, int dev_id);

#ifdef __cplusplus
}
#endif

#endif