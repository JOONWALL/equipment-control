#include "io_manager.h"

#include <string.h>
#include <unistd.h>

#include "pico_device.h"
#include "protocol/line_codec.h"

typedef struct {
  int enabled;
  char serial_path[128];
  int baudrate;
} pmc_uart_aux_cfg_t;

static pmc_uart_aux_cfg_t g_uart_aux_cfg;

int pmc_io_send_message(pmc_connection_t* target, const message_t* msg){
  char outbuf[512];
  int len;

  if(!target || !msg) return -1;

  len = line_format(msg, outbuf, sizeof(outbuf));
  if(len <= 0) return -1;

  if(write(target->fd, outbuf, (size_t)len) < 0){
    return -1;
  }

  return 0;
}

int pmc_io_send_error(pmc_connection_t* target,
                      const message_t* req,
                      int code,
                      const char* text){
  message_t err;

  if(!target || !req) return -1;

  memset(&err, 0, sizeof(err));
  err.role = ROLE_RESP;
  err.type = TYPE_ERROR;

  if(req->has_dev){
    err.dev = req->dev;
    err.has_dev = 1;
  }

  if(req->has_seq){
    err.seq = req->seq;
    err.has_seq = 1;
  }

  err.ok = 0;
  err.has_ok = 1;
  err.code = code;
  err.has_code = 1;

  if(text){
    strncpy(err.msg, text, sizeof(err.msg) - 1);
    err.msg[sizeof(err.msg) - 1] = '\0';
    err.has_msg = 1;
  }

  return pmc_io_send_message(target, &err);
}

int pmc_io_send_status_cached(pmc_connection_t* eqd_conn,
                              const message_t* req,
                              const pmc_connection_t* sim_conn){
  message_t resp;

  if(!eqd_conn || !req || !sim_conn) return -1;

  memset(&resp, 0, sizeof(resp));
  resp.role = ROLE_RESP;
  resp.type = TYPE_STATUS;

  if(req->has_dev){
    resp.dev = req->dev;
    resp.has_dev = 1;
  }

  if(req->has_seq){
    resp.seq = req->seq;
    resp.has_seq = 1;
  }

  resp.ok = 1;
  resp.has_ok = 1;

  if(sim_conn->has_state){
    strncpy(resp.state, sim_conn->state, sizeof(resp.state) - 1);
    resp.state[sizeof(resp.state) - 1] = '\0';
  } else {
    strncpy(resp.state, "UNKNOWN", sizeof(resp.state) - 1);
    resp.state[sizeof(resp.state) - 1] = '\0';
  }
  resp.has_state = 1;

  return pmc_io_send_message(eqd_conn, &resp);
}

int pmc_io_uart_ping(const char* serial_path,
                     int baudrate,
                     char* reply,
                     int reply_sz){
  pico_device_t dev;
  int rc;

  rc = pico_device_open(&dev, serial_path, baudrate);
  if(rc != 0){
    return -1;
  }

  rc = pico_device_ping(&dev, reply, reply_sz);
  pico_device_close(&dev);
  return rc;
}

int pmc_io_uart_read_temp(const char* serial_path,
                          int baudrate,
                          char* reply,
                          int reply_sz){
  pico_device_t dev;
  int rc;

  rc = pico_device_open(&dev, serial_path, baudrate);
  if(rc != 0){
    return -1;
  }

  rc = pico_device_read_temp(&dev, reply, reply_sz);
  pico_device_close(&dev);
  return rc;
}

int pmc_io_uart_set_heater(const char* serial_path,
                           int baudrate,
                           int on,
                           char* reply,
                           int reply_sz){
  pico_device_t dev;
  int rc;

  rc = pico_device_open(&dev, serial_path, baudrate);
  if(rc != 0){
    return -1;
  }

  rc = pico_device_set_heater(&dev, on, reply, reply_sz);
  pico_device_close(&dev);
  return rc;
}

void pmc_io_uart_aux_clear(void){
  memset(&g_uart_aux_cfg, 0, sizeof(g_uart_aux_cfg));
}

int pmc_io_uart_aux_configure(const char* serial_path, int baudrate){
  if(!serial_path || !serial_path[0]){
    return -1;
  }

  memset(&g_uart_aux_cfg, 0, sizeof(g_uart_aux_cfg));
  g_uart_aux_cfg.enabled = 1;
  g_uart_aux_cfg.baudrate = baudrate;
  strncpy(g_uart_aux_cfg.serial_path,
          serial_path,
          sizeof(g_uart_aux_cfg.serial_path) - 1);
  g_uart_aux_cfg.serial_path[sizeof(g_uart_aux_cfg.serial_path) - 1] = '\0';

  return 0;
}

int pmc_io_apply_aux_for_command(int dev_id,
                                 const message_t* msg,
                                 char* reply,
                                 int reply_sz){
  int rc;

  if(reply && reply_sz > 0){
    reply[0] = '\0';
  }

  if(!msg){
    return -1;
  }

  if(!g_uart_aux_cfg.enabled){
    return 0;
  }

  if(dev_id != 1){
    return 0;
  }

  switch(msg->type){
    case TYPE_START:
      rc = pmc_io_uart_ping(g_uart_aux_cfg.serial_path,
                            g_uart_aux_cfg.baudrate,
                            reply,
                            reply_sz);
      if(rc != 0){
        if(reply && reply_sz > 0){
          strncpy(reply, "PICO_PING_FAILED", (size_t)reply_sz - 1);
          reply[reply_sz - 1] = '\0';
        }
        return -1;
      }

      rc = pmc_io_uart_set_heater(g_uart_aux_cfg.serial_path,
                                  g_uart_aux_cfg.baudrate,
                                  1,
                                  reply,
                                  reply_sz);
      if(rc != 0){
        if(reply && reply_sz > 0){
          strncpy(reply, "PICO_HEATER_ON_FAILED", (size_t)reply_sz - 1);
          reply[reply_sz - 1] = '\0';
        }
        return -1;
      }

      return 0;

    case TYPE_STOP:
    case TYPE_RESET:
    case TYPE_FAULT:
      rc = pmc_io_uart_set_heater(g_uart_aux_cfg.serial_path,
                                  g_uart_aux_cfg.baudrate,
                                  0,
                                  reply,
                                  reply_sz);
      if(rc != 0){
        if(reply && reply_sz > 0){
          strncpy(reply, "PICO_HEATER_OFF_FAILED", (size_t)reply_sz - 1);
          reply[reply_sz - 1] = '\0';
        }
        return -1;
      }
      return 0;

    default:
      return 0;
  }
}