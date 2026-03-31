#include "io_manager.h"

#include <string.h>
#include <unistd.h>

#include "protocol/line_codec.h"

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