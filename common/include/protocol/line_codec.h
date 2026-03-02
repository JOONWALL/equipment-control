#pragma once
#include <stddef.h>
#include "protocol/message.h"

#ifdef __cplusplus
extern "C" {
#endif

// Parse one line message (may include trailing '\n' and/or '\r').
// Returns 0 on success, -1 on parse error.
int line_parse(const char* line, message_t* out);

// Format message into one line ending with '\n' (null-terminated too).
// Returns number of bytes written (including '\n', excluding final '\0') or -1 on error.
int line_format(const message_t* msg, char* out, size_t cap);

#ifdef __cplusplus
}
#endif
