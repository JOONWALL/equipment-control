#include "io_manager.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "pico_device.h"
#include "protocol/line_codec.h"

typedef struct {
  int enabled;
  char serial_path[128];
  int baudrate;
  pico_device_t dev;
  int session_ready;
} pmc_uart_aux_cfg_t;

static pmc_uart_aux_cfg_t g_uart_aux_cfg;

static void pmc_aux_sleep_ms(int ms){
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000L * 1000L;
  nanosleep(&ts, NULL);
}

static int pmc_aux_reply_is(const char* got, const char* expect_prefix){
  if(!got || !expect_prefix) return 0;
  return strncmp(got, expect_prefix, strlen(expect_prefix)) == 0;
}

static void pmc_aux_drain_boot_lines(void){
  char line[128];
  int i;

  if(!g_uart_aux_cfg.session_ready){
    return;
  }

  for(i = 0; i < 4; i++){
    int rc = serial_link_read_line(&g_uart_aux_cfg.dev.link,
                                   line,
                                   sizeof(line),
                                   200);
    if(rc <= 0){
      break;
    }

    printf("[PMC][AUX] boot line=%s\n", line);
  }
}

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
    snprintf(resp.state, sizeof(resp.state), "%s", sim_conn->state);
  } else {
    snprintf(resp.state, sizeof(resp.state), "%s", "UNKNOWN");
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
  if(g_uart_aux_cfg.dev.connected){
    pico_device_close(&g_uart_aux_cfg.dev);
  }

  memset(&g_uart_aux_cfg, 0, sizeof(g_uart_aux_cfg));
  g_uart_aux_cfg.dev.link.fd = -1;
}

int pmc_io_uart_aux_configure(const char* serial_path, int baudrate){
  if(!serial_path || !serial_path[0]){
    return -1;
  }

  pmc_io_uart_aux_clear();

  strncpy(g_uart_aux_cfg.serial_path,
          serial_path,
          sizeof(g_uart_aux_cfg.serial_path) - 1);
  g_uart_aux_cfg.serial_path[sizeof(g_uart_aux_cfg.serial_path) - 1] = '\0';
  g_uart_aux_cfg.baudrate = baudrate;

  if(pico_device_open(&g_uart_aux_cfg.dev, serial_path, baudrate) != 0){
    pmc_io_uart_aux_clear();
    return -1;
  }

  g_uart_aux_cfg.enabled = 1;
  g_uart_aux_cfg.session_ready = 1;

  /* Arduino UNO 계열은 serial open 직후 리셋될 수 있으므로
     부팅 배너가 흘러나올 시간을 주고 한 번 비워준다. */
  pmc_aux_sleep_ms(1800);
  pmc_aux_drain_boot_lines();

  return 0;
}

void pmc_io_uart_aux_shutdown(void){
  pmc_io_uart_aux_clear();
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

  if(!g_uart_aux_cfg.enabled || !g_uart_aux_cfg.session_ready){
    return 0;
  }

  /* 우선 dev=1(preclean)에만 aux device 적용 */
  if(dev_id != 1){
    return 0;
  }

  switch(msg->type){
    case TYPE_START:
      rc = pico_device_ping(&g_uart_aux_cfg.dev, reply, reply_sz);
      if(rc != 0){
        if(reply && reply_sz > 0){
          snprintf(reply, (size_t)reply_sz, "%s", "AUX_PING_FAILED");
        }
        return -1;
      }
      if(!pmc_aux_reply_is(reply, "OK PONG")){
        if(reply && reply_sz > 0){
          snprintf(reply, (size_t)reply_sz, "%s", "AUX_PING_BAD_REPLY");
        }
        return -1;
      }

      rc = pico_device_set_heater(&g_uart_aux_cfg.dev, 1, reply, reply_sz);
      if(rc != 0){
        if(reply && reply_sz > 0){
          snprintf(reply, (size_t)reply_sz, "%s", "AUX_HEATER_ON_FAILED");
        }
        return -1;
      }
      if(!pmc_aux_reply_is(reply, "OK HEATER ON")){
        if(reply && reply_sz > 0){
          snprintf(reply, (size_t)reply_sz, "%s", "AUX_HEATER_ON_BAD_REPLY");
        }
        return -1;
      }

      return 0;

    case TYPE_STOP:
    case TYPE_RESET:
    case TYPE_FAULT:
      rc = pico_device_set_heater(&g_uart_aux_cfg.dev, 0, reply, reply_sz);
      if(rc != 0){
        if(reply && reply_sz > 0){
          snprintf(reply, (size_t)reply_sz, "%s", "AUX_HEATER_OFF_FAILED");
        }
        return -1;
      }
      if(!pmc_aux_reply_is(reply, "OK HEATER OFF")){
        if(reply && reply_sz > 0){
          snprintf(reply, (size_t)reply_sz, "%s", "AUX_HEATER_OFF_BAD_REPLY");
        }
        return -1;
      }

      return 0;

    default:
      return 0;
  }
}