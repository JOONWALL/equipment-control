#ifndef PMC_PICO_DEVICE_H
#define PMC_PICO_DEVICE_H

#include "serial_transport.h"

typedef struct {
  serial_link_t link;
  int connected;
} pico_device_t;

int pico_device_open(pico_device_t* dev, const char* path, int baudrate);
void pico_device_close(pico_device_t* dev);

int pico_device_ping(pico_device_t* dev, char* reply, int reply_sz);
int pico_device_read_temp(pico_device_t* dev, char* reply, int reply_sz);
int pico_device_set_heater(pico_device_t* dev, int on, char* reply, int reply_sz);

#endif