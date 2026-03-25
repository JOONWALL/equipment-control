#include "pmc_router.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol/line_codec.h"

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
    return 0;
  }

  // equipmentd 등록: REQ REGISTER
  if(msg->role == ROLE_EVT){
    conn->type = PEER_EQD;
    conn->registered = 1;
    conn->dev_id = -1;
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
    return 1;
  }

  return -1;
}

int pmc_route_message(pmc_connection_t* from, const message_t* msg){
  pmc_connection_t* target = 0;

  if(!from || !msg) return -1;
  if(!from->registered) return -1;

  if(from->type == PEER_EQD){
    // EQD -> SIM
    if(!msg->has_dev){
      return -1;
    }

    target = pmc_find_sim(msg->dev);
    if(!target){
      return -1;
    }

    return send_message_to(target, msg);
  }

  if(from->type == PEER_SIM){
    // SIM -> EQD
    target = pmc_find_eqd();
    if(!target){
      return -1;
    }

    return send_message_to(target, msg);
  }

  return -1;
}