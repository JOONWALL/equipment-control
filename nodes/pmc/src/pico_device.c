#include "pico_device.h"

#include <stdio.h>
#include <string.h>

static int pico_send_and_wait(pico_device_t* dev,
                              const char* cmd,
                              char* reply,
                              int reply_sz){
  int rc;
  int tries;

  if(!dev || !dev->connected || !cmd || !reply || reply_sz <= 0){
    return -1;
  }

  rc = (int)serial_link_write_line(&dev->link, cmd);
  if(rc < 0){
    return -1;
  }

  /* 빈 줄/잡응답이 끼는 경우가 있어서
     non-empty line이 나올 때까지 몇 번 읽는다. */
  for(tries = 0; tries < 5; tries++){
    rc = serial_link_read_line(&dev->link, reply, (size_t)reply_sz, 500);
    if(rc <= 0){
      return -1;
    }

    if(reply[0] == '\0'){
      continue;
    }

    return 0;
  }

  return -1;
}

int pico_device_open(pico_device_t* dev, const char* path, int baudrate){
  if(!dev) return -1;

  memset(dev, 0, sizeof(*dev));
  dev->link.fd = -1;

  if(serial_link_open(&dev->link, path, baudrate) != 0){
    return -1;
  }

  dev->connected = 1;
  return 0;
}

void pico_device_close(pico_device_t* dev){
  if(!dev) return;
  serial_link_close(&dev->link);
  dev->connected = 0;
}

int pico_device_ping(pico_device_t* dev, char* reply, int reply_sz){
  return pico_send_and_wait(dev, "PING", reply, reply_sz);
}

int pico_device_read_temp(pico_device_t* dev, char* reply, int reply_sz){
  return pico_send_and_wait(dev, "READ TEMP", reply, reply_sz);
}

int pico_device_set_heater(pico_device_t* dev, int on, char* reply, int reply_sz){
  return pico_send_and_wait(dev,
                            on ? "SET HEATER ON" : "SET HEATER OFF",
                            reply,
                            reply_sz);
}