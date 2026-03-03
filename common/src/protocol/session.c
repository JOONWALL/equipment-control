#include "protocol/session.h"
#include <string.h>

void session_init(session_t* s) {
  if (!s) return;
  s->len = 0;
}

int session_feed(session_t* s, const char* data, size_t n) {
  if (!s) return -1;
  if (n == 0) return 0;
  if (!data) return -1;

  if (s->len + n > sizeof(s->buf)) {
    // Phase1: simple fail on overflow (later you can grow buffer or drop policy)
    return -1;
  }

  memcpy(s->buf + s->len, data, n);
  s->len += n;
  return 0;
}

int session_pop_line(session_t* s, char* out, size_t cap) {
  if (!s || !out || cap < 2) return -1;

  // Find newline
  for (size_t i = 0; i < s->len; i++) {
    if (s->buf[i] == '\n') {
      size_t line_len = i + 1;      // include '\n'
      if (line_len >= cap) return -1;

      memcpy(out, s->buf, line_len);
      out[line_len] = '\0';

      // Remove consumed bytes
      memmove(s->buf, s->buf + line_len, s->len - line_len);
      s->len -= line_len;

      return 1;
    }
  }
  return 0; // no full line yet
}
