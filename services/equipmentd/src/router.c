#include "internal/router.h"
#include <string.h>

static void resp_base(const message_t* in, message_t* out){
  memset(out, 0, sizeof(*out));
  out->role = ROLE_RESP;
  out->type = in->type;

  if(in->has_dev){ out->dev = in->dev; out->has_dev = 1; }
  if(in->has_seq){ out->seq = in->seq; out->has_seq = 1; }

  out->ok = 1; out->has_ok = 1;
}

int router_handle(const message_t* in, message_t* out){
  if(!in || !out) return -1;

  // Phase1: respond only to REQ
  if(in->role != ROLE_REQ) return 0;

  // Phase1 policy: seq required
  if(!in->has_seq){
    resp_base(in, out);
    out->type = TYPE_ERROR;
    out->ok = 0;
    out->code = 400; out->has_code = 1;
    strncpy(out->msg, "MISSING_SEQ", sizeof(out->msg)-1); out->has_msg = 1;
    return 1;
  }

  switch(in->type){
    case TYPE_PING:
      resp_base(in, out);
      return 1;

    case TYPE_STATUS:
      resp_base(in, out);
      strncpy(out->state, "IDLE", sizeof(out->state)-1);
      out->has_state = 1;
      return 1;

    case TYPE_START:
    case TYPE_STOP:
      resp_base(in, out);
      return 1;

    default:
      resp_base(in, out);
      out->type = TYPE_ERROR;
      out->ok = 0;
      out->code = 404; out->has_code = 1;
      strncpy(out->msg, "UNKNOWN_TYPE", sizeof(out->msg)-1); out->has_msg = 1;
      return 1;
  }
}
