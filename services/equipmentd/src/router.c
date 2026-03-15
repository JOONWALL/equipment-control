#include "internal/router.h"
#include <string.h>

static void resp_base(const message_t* in, message_t* out){
  memset(out, 0, sizeof(*out));
  out->role = ROLE_RESP;
  out->type = in->type;

  if(in->has_dev){ out->dev = in->dev; out->has_dev = 1; }
  if(in->has_seq){ out->seq = in->seq; out->has_seq = 1; }

  out->ok = 1;
  out->has_ok = 1;
}

static int make_error(const message_t* in, message_t* out, int code, const char* msg){
  resp_base(in, out);
  out->type = TYPE_ERROR;
  out->ok = 0;
  out->code = code;
  out->has_code = 1;

  if(msg){
    strncpy(out->msg, msg, sizeof(out->msg) - 1);
    out->has_msg = 1;
  }

  return 1;
}

int router_handle(device_manager_t* mgr, const message_t* in, message_t* out){
  device_state_t state;
  int rc;

  if(!mgr || !in || !out) return -1;

  // Phase1 policy 유지: REQ만 응답
  if(in->role != ROLE_REQ) return 0;

  // seq 필수
  if(!in->has_seq){
    return make_error(in, out, 400, "MISSING_SEQ");
  }

  switch(in->type){
    case TYPE_PING:
      resp_base(in, out);
      return 1;

    case TYPE_STATUS:
      if(!in->has_dev){
        return make_error(in, out, 400, "MISSING_DEV");
      }

      rc = device_manager_get_status(mgr, in->dev, &state);
      if(rc != 0){
        return make_error(in, out, 500, "STATUS_FAILED");
      }

      resp_base(in, out);
      strncpy(out->state, device_state_to_string(state), sizeof(out->state) - 1);
      out->has_state = 1;
      return 1;
    case TYPE_RESET:
      if(!in->has_dev){
        return make_error(in, out, 400, "MISSING_DEV");
      }

      rc = device_manager_reset(mgr, in->dev);
      if(rc == -2){
        return make_error(in, out, 409, "RESET_REQUIRES_ERROR_STATE");
      }
      if(rc != 0){
        return make_error(in, out, 500, "RESET_FAILED");
      }

      resp_base(in, out);
      return 1;

    case TYPE_FAULT:
      if(!in->has_dev){
        return make_error(in, out, 400, "MISSING_DEV");
      }

      rc = device_manager_fault(mgr, in->dev);
      if(rc == -2){
        return make_error(in, out, 409, "ALREADY_ERROR");
      }
      if(rc != 0){
        return make_error(in, out, 500, "FAULT_FAILED");
      }

      resp_base(in, out);
      return 1;

    case TYPE_START:
      if(!in->has_dev){
        return make_error(in, out, 400, "MISSING_DEV");
      }

      rc = device_manager_start(mgr, in->dev);
      if(rc == -2){
        return make_error(in, out, 409, "INVALID_STATE_TRANSITION");
      }
      if(rc != 0){
        return make_error(in, out, 500, "START_FAILED");
      }

      resp_base(in, out);
      return 1;

    case TYPE_STOP:
      if(!in->has_dev){
        return make_error(in, out, 400, "MISSING_DEV");
      }

      rc = device_manager_stop(mgr, in->dev);
      if(rc == -2){
        return make_error(in, out, 409, "INVALID_STATE_TRANSITION");
      }
      if(rc != 0){
        return make_error(in, out, 500, "STOP_FAILED");
      }

      resp_base(in, out);
      return 1;

    default:
      return make_error(in, out, 404, "UNKNOWN_TYPE");
  }
}