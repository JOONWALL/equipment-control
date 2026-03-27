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

    printf("[PMC] EQD->SIM type=%d dev=%d\n", msg->type, msg->dev);
    // EQD -> SIM
    if(!msg->has_dev){
      return -1;
    }

    target = pmc_find_sim(msg->dev);
    if(!target){
      return -1;
    }
    printf("[PMC] before interlock dev=%d has_state=%d state=%s\n",
       target->dev_id, target->has_state, target->state);
    if(pmc_interlock_check(target, msg) != 0){
      message_t err;
      memset(&err, 0, sizeof(err));

      err.role = ROLE_RESP;
      err.type = TYPE_ERROR;

      if(msg->has_dev){
        err.dev = msg->dev;
        err.has_dev = 1;
      }

      if(msg->has_seq){
        err.seq = msg->seq;
        err.has_seq = 1;
      }

      err.ok = 0;
      err.has_ok = 1;
      err.code = 409;
      err.has_code = 1;
      strcpy(err.msg, "PMC_INTERLOCK_BLOCKED");
      err.has_msg = 1;

      return send_message_to(from, &err);
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