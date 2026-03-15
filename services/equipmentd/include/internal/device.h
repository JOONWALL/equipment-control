#ifndef EQUIPMENTD_DEVICE_H
#define EQUIPMENTD_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DEV_STATE_IDLE = 0,
  DEV_STATE_RUNNING,
  DEV_STATE_ERROR
} device_state_t;

typedef struct {
  int id;
  device_state_t state;
  int attached_fd;   // 아직 connection mapping 본격화 전이므로 최소 필드만 둠
  int in_use;
} device_t;

const char* device_state_to_string(device_state_t state);

#ifdef __cplusplus
}
#endif

#endif