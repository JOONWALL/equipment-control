#include "pmc_router.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol/line_codec.h"
#include "interlock.h"

static int send_message_to(pmc_connection_t* target, const message_t* msg){
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

static int send_error_to(pmc_connection_t* target, const message_t* req, int code, const char* text){
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

  return send_message_to(target, &err);
}

static int send_status_from_cache(pmc_connection_t* eqd_conn,
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

  return send_message_to(eqd_conn, &resp);
}

int pmc_handle_register(pmc_connection_t* conn, const message_t* msg){
  if(!conn || !msg) return -1;

  if(msg->type != TYPE_REGISTER){
    return -1;
  }

  // equipmentd 등록: REQ REGISTER
  if(msg->role == ROLE_REQ){
    conn->type = PEER_EQD;
    conn->registered = 1;
    conn->dev_id = -1;
    printf("[PMC] EQD registered fd=%d\n", conn->fd);
    return 1;
  }

  // sim 등록: EVT REGISTER dev=n
  if(msg->role == ROLE_EVT){
    if(!msg->has_dev){
      return -1;
    }

    conn->type = PEER_SIM;
    conn->registered = 1;
    conn->dev_id = msg->dev;
    printf("[PMC] SIM registered dev=%d fd=%d\n", msg->dev, conn->fd);
    return 1;
  }

  return -1;
}


int pmc_route_message(pmc_connection_t* from, const message_t* msg){
  pmc_connection_t* target = 0;

  if(!from || !msg) return -1;

  if(msg->type == TYPE_REGISTER){
    return 0;  // REGISTER는 라우팅 안 함
  }
  
  if(!from->registered) return -1;

   if(from->type == PEER_EQD){
    printf("[PMC] msg dev=%d has_dev=%d type=%d\n",
           msg->dev, msg->has_dev, msg->type);

    if(!msg->has_dev){
      return send_error_to(from, msg, 400, "MISSING_DEV");
    }

    target = pmc_find_sim(msg->dev);
    if(!target){
      return send_error_to(from, msg, 404, "SIM_NOT_FOUND");
    }

    if(msg->type == TYPE_STATUS){
      printf("[PMC] EQD->PMC local STATUS dev=%d cached_state=%s has_state=%d\n",
             target->dev_id,
             target->has_state ? target->state : "UNKNOWN",
             target->has_state);

      return send_status_from_cache(from, msg, target);
    }

    printf("[PMC] EQD->SIM type=%d dev=%d\n", msg->type, msg->dev);
    printf("[PMC] before interlock dev=%d has_state=%d state=%s\n",
           target->dev_id, target->has_state, target->state);

    if(pmc_interlock_check(target, msg) != 0){
      return send_error_to(from, msg, 409, "PMC_INTERLOCK_BLOCKED");
    }

    return send_message_to(target, msg);
  }

  if(from->type == PEER_SIM){
    // SIM -> EQD
    if(msg->type == TYPE_STATUS && msg->has_state){
      printf("[PMC] status update dev=%d state=%s\n", from->dev_id, msg->state);
      strncpy(from->state, msg->state, sizeof(from->state) - 1);
      from->state[sizeof(from->state) - 1] = '\0';
      from->has_state = 1;
    }

    target = pmc_find_eqd();
    if(!target){
      return -1;
    }
    return send_message_to(target, msg);
  }

  return -1;
}