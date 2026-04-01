#include "pmc_router.h"

#include <stdio.h>
#include <string.h>

#include "alarm_manager.h"
#include "io_manager.h"
#include "sequence_manager.h"

int pmc_handle_register(pmc_connection_t* conn, const message_t* msg){
  if(!conn || !msg) return -1;

  if(msg->type != TYPE_REGISTER){
    return -1;
  }

  if(msg->role == ROLE_REQ){
    conn->type = PEER_EQD;
    conn->registered = 1;
    conn->dev_id = -1;
    printf("[PMC] EQD registered fd=%d\n", conn->fd);
    return 1;
  }

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
  pmc_seq_action_t action;

  if(!from || !msg) return -1;

  if(msg->type == TYPE_REGISTER){
    return 0;
  }

  if(!from->registered) return -1;

  if(from->type == PEER_EQD){
    printf("[PMC] msg dev=%d has_dev=%d type=%d\n",
           msg->dev, msg->has_dev, msg->type);

    target = msg->has_dev ? pmc_find_sim(msg->dev) : 0;
    action = pmc_sequence_decide_eqd_request(target, msg);

    switch(action){
      case PMC_SEQ_ACTION_REJECT_MISSING_DEV:
        pmc_alarm_raise(-1, PMC_ALARM_MISSING_DEV, "MISSING_DEV");
        return pmc_io_send_error(from, msg, 400, "MISSING_DEV");

      case PMC_SEQ_ACTION_REJECT_SIM_NOT_FOUND:
        pmc_alarm_raise(msg->has_dev ? msg->dev : -1,
                        PMC_ALARM_SIM_NOT_FOUND,
                        "SIM_NOT_FOUND");
        return pmc_io_send_error(from, msg, 404, "SIM_NOT_FOUND");

      case PMC_SEQ_ACTION_RESPOND_CACHED_STATUS:
        printf("[PMC] EQD->PMC local STATUS dev=%d cached_state=%s has_state=%d\n",
               target->dev_id,
               target->has_state ? target->state : "UNKNOWN",
               target->has_state);
        return pmc_io_send_status_cached(from, msg, target);

      case PMC_SEQ_ACTION_REJECT_INTERLOCK:
        printf("[PMC] before interlock dev=%d has_state=%d state=%s\n",
               target->dev_id, target->has_state, target->state);
        pmc_alarm_raise(msg->dev, PMC_ALARM_INTERLOCK_BLOCKED, "PMC_INTERLOCK_BLOCKED");
        return pmc_io_send_error(from, msg, 409, "PMC_INTERLOCK_BLOCKED");

      case PMC_SEQ_ACTION_ROUTE_TO_SIM:
      default: {
        char aux_reply[128];

        printf("[PMC] EQD->SIM type=%d dev=%d\n", msg->type, msg->dev);
        printf("[PMC] before interlock dev=%d has_state=%d state=%s\n",
               target->dev_id, target->has_state, target->state);

        if(pmc_io_apply_aux_for_command(msg->dev,
                                        msg,
                                        aux_reply,
                                        sizeof(aux_reply)) != 0){
          pmc_alarm_raise(msg->dev,
                          PMC_ALARM_AUX_IO_FAILED,
                          aux_reply[0] ? aux_reply : "PICO_IO_FAILED");
          return pmc_io_send_error(from,
                                   msg,
                                   502,
                                   aux_reply[0] ? aux_reply : "PICO_IO_FAILED");
        }

        if(pmc_io_send_message(target, msg) != 0){
          pmc_alarm_raise(msg->dev, PMC_ALARM_IO_SEND_FAILED, "SIM_CMD_FAILED");
          return pmc_io_send_error(from, msg, 500, "SIM_CMD_FAILED");
        }

        return 0;
      }
    }
  }

  if(from->type == PEER_SIM){
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

    return pmc_io_send_message(target, msg);
  }

  return -1;
}