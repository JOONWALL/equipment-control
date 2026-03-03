#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char   buf[4096];
  size_t len;
} session_t;

// Initialize session buffer
void session_init(session_t* s);

// Append raw bytes into internal buffer
// returns 0 on success, -1 on overflow/invalid args
int session_feed(session_t* s, const char* data, size_t n);

// Pop one '\n'-terminated line into out (out is null-terminated).
// returns 1 if a line was popped,
//         0 if no full line available yet,
//        -1 on error (e.g., output cap too small)
int session_pop_line(session_t* s, char* out, size_t cap);

#ifdef __cplusplus
}
#endif
