#pragma once
#include "protocol/message.h"

#ifdef __cplusplus
extern "C" {
#endif

// returns 1 if response generated, 0 if no response, -1 on error
int router_handle(const message_t* in, message_t* out);

#ifdef __cplusplus
}
#endif
