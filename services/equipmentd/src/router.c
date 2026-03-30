#include "internal/router.h"
#include "util/time.h"
#include <string.h>
#include <stdio.h>

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

static int handle_req(device_manager_t* mgr, int pmc_fd, const message_t* in, message_t* out){
  device_state_t state;
  int rc;

  if(!mgr || !in || !out) return -1;

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
    case TYPE_FAULT:
    case TYPE_START:
    case TYPE_STOP:
      if(!in->has_dev){
        return make_error(in, out, 400, "MISSING_DEV");
      }

      if(pmc_fd < 0){
        return make_error(in, out, 503, "PMC_NOT_CONNECTED");
      }

      return 2;  
    default:
      return make_error(in, out, 404, "UNKNOWN_TYPE");
  }
}

static int handle_evt(device_manager_t* mgr, module_registry_t* reg, const message_t* in, message_t* out){
  (void)mgr;
  (void)out;

  switch(in->type){
    case TYPE_REGISTER:
      if(!in->has_dev){
        return 0;
      }

      if(in->dev == 100){
        reg->tmc.registered = 1;
        reg->tmc.type = MODULE_TMC;
        reg->tmc.dev = in->dev;
        strncpy(reg->tmc.name, "tmc", sizeof(reg->tmc.name)-1);

        printf("[CTC] TMC registered (dev=%d)\n", in->dev);
      } else if(in->dev == 1){
        reg->pmc_preclean.registered = 1;
        reg->pmc_preclean.type = MODULE_PMC;
        reg->pmc_preclean.dev = in->dev;
        strncpy(reg->pmc_preclean.name, "pmc_preclean", sizeof(reg->pmc_preclean.name)-1);

        printf("[CTC] PMC preclean registered (dev=%d)\n", in->dev);
      } else if(in->dev == 2){
        reg->pmc_deposition.registered = 1;
        reg->pmc_deposition.type = MODULE_PMC;
        reg->pmc_deposition.dev = in->dev;
        strncpy(reg->pmc_deposition.name, "pmc_deposition", sizeof(reg->pmc_deposition.name)-1);

        printf("[CTC] PMC deposition registered (dev=%d)\n", in->dev);
      }

      return 0;
    case TYPE_STATUS:
      if(!in->has_dev){
        return 0;
      }

      module_info_t* target = NULL;

      if(in->dev == 1){
        target = &reg->pmc_preclean;
      } else if(in->dev == 2){
        target = &reg->pmc_deposition;
      }

      if(target){
        target->registered = 1;
        target->type = MODULE_PMC;
        target->dev = in->dev;

        if(in->dev == 1){
          strncpy(target->name, "pmc_preclean", sizeof(target->name) - 1);
        } else if(in->dev == 2){
          strncpy(target->name, "pmc_deposition", sizeof(target->name) - 1);
        }
      }


      if(target && in->has_state){
        strncpy(target->current_state, in->state, sizeof(target->current_state)-1);
        target->current_state[sizeof(target->current_state)-1] = '\0';
        target->has_state = 1;

        if(strcmp(in->state, "PROCESSING") == 0 ||
          strcmp(in->state, "READY") == 0){
          target->busy = 1;
        } else {
          target->busy = 0;
        }

        if(strcmp(in->state, "FAULT") == 0){
          target->fault_latched = 1;
        }

        module_registry_touch_status(target, now_ms(), in->state);
      }

      return 0;
    case TYPE_TELEMETRY:
      if(!in->has_dev){
        return 0;
      }

      target = NULL;

      if(in->dev == 1){
        target = &reg->pmc_preclean;
      } else if(in->dev == 2){
        target = &reg->pmc_deposition;
      }

      if(target){
        target->registered = 1;
        target->type = MODULE_PMC;
        target->dev = in->dev;

        if(in->dev == 1){
          strncpy(target->name, "pmc_preclean", sizeof(target->name) - 1);
        } else if(in->dev == 2){
          strncpy(target->name, "pmc_deposition", sizeof(target->name) - 1);
        }
      }

      if(target){
        target->temp = in->temp;
        target->pressure = in->pressure;
        target->flow = in->flow;
        target->has_telemetry = 1;
        module_registry_touch_telemetry(target, now_ms());
      }

      if(in->dev == 1){
        printf("[CTC] PMC preclean telemetry: temp=%.2f pressure=%.2f flow=%.2f\n",
              in->temp, in->pressure, in->flow);
      }
      else if(in->dev == 2){
        printf("[CTC] PMC deposition telemetry: temp=%.2f pressure=%.2f flow=%.2f\n",
              in->temp, in->pressure, in->flow);
      }

      return 0;

    default:
      return 0;
  }
}

int router_handle(device_manager_t* mgr,
                  module_registry_t* reg,
                  int pmc_fd,
                  const message_t* in,
                  message_t* out){
  if(!mgr || !in || !out) return -1;

  switch(in->role){
    case ROLE_REQ:
      return handle_req(mgr, pmc_fd, in, out);
   case ROLE_EVT:
      return handle_evt(mgr, reg, in, out);
    default:
      return 0;
  }
}