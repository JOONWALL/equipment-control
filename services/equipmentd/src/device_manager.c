#include "internal/device_manager.h"
#include <string.h>

const char* device_state_to_string(device_state_t state){
  switch(state){
    case DEV_STATE_IDLE:    return "IDLE";
    case DEV_STATE_RUNNING: return "RUNNING";
    case DEV_STATE_ERROR:   return "ERROR";
    default:                return "UNKNOWN";
  }
}

void device_manager_init(device_manager_t* mgr){
  if(!mgr) return;
  memset(mgr, 0, sizeof(*mgr));
  mgr->initialized = 1;
}

static int valid_dev_id(int dev_id){
  return dev_id >= 0 && dev_id < DEVICE_MANAGER_MAX_DEVICES;
}

device_t* device_manager_get(device_manager_t* mgr, int dev_id){
  if(!mgr || !valid_dev_id(dev_id)) return 0;
  if(!mgr->devices[dev_id].in_use) return 0;
  return &mgr->devices[dev_id];
}

device_t* device_manager_ensure(device_manager_t* mgr, int dev_id){
  device_t* dev;

  if(!mgr || !valid_dev_id(dev_id)) return 0;

  dev = &mgr->devices[dev_id];
  if(!dev->in_use){
    memset(dev, 0, sizeof(*dev));
    dev->id = dev_id;
    dev->state = DEV_STATE_IDLE;
    dev->attached_fd = -1;
    dev->in_use = 1;
  }

  return dev;
}

int device_manager_get_status(device_manager_t* mgr, int dev_id, device_state_t* out_state){
  device_t* dev = device_manager_ensure(mgr, dev_id);
  if(!dev || !out_state) return -1;

  *out_state = dev->state;
  return 0;
}

int device_manager_fault(device_manager_t* mgr, int dev_id){
  device_t* dev = device_manager_ensure(mgr, dev_id);
  if(!dev) return -1;

  if(dev->state == DEV_STATE_ERROR){
    return -2;
  }

  dev->state = DEV_STATE_ERROR;
  return 0;
}

int device_manager_reset(device_manager_t* mgr, int dev_id){
  device_t* dev = device_manager_ensure(mgr, dev_id);
  if(!dev) return -1;

  if(dev->state != DEV_STATE_ERROR){
    return -2;
  }

  dev->state = DEV_STATE_IDLE;
  return 0;
}

int device_manager_start(device_manager_t* mgr, int dev_id){
  device_t* dev = device_manager_ensure(mgr, dev_id);
  if(!dev) return -1;

  if(dev->state != DEV_STATE_IDLE){
    return -2;
  }

  dev->state = DEV_STATE_RUNNING;
  return 0;
}

int device_manager_stop(device_manager_t* mgr, int dev_id){
  device_t* dev = device_manager_ensure(mgr, dev_id);
  if(!dev) return -1;

  if(dev->state != DEV_STATE_RUNNING){
    return -2;
  }

  dev->state = DEV_STATE_IDLE;
  return 0;
}