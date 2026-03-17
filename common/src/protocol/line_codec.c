#include "protocol/line_codec.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void msg_init(message_t* m) {
  memset(m, 0, sizeof(*m));
  m->role = ROLE_UNKNOWN;
  m->type = TYPE_UNKNOWN;
  m->ok = -1; // meaning: not present
}

static msg_role_t role_from(const char* s) {
  if (strcmp(s, "REQ") == 0) return ROLE_REQ;
  if (strcmp(s, "RESP") == 0) return ROLE_RESP;
  if (strcmp(s, "EVT") == 0) return ROLE_EVT;
  return ROLE_UNKNOWN;
}

static msg_type_t type_from(const char* s) {
  if (strcmp(s, "PING") == 0) return TYPE_PING;
  if (strcmp(s, "START") == 0) return TYPE_START;
  if (strcmp(s, "STOP") == 0) return TYPE_STOP;
  if (strcmp(s, "STATUS") == 0) return TYPE_STATUS;
  if (strcmp(s, "RESET") == 0) return TYPE_RESET;
  if (strcmp(s, "FAULT") == 0) return TYPE_FAULT;
  if (strcmp(s, "TELEMETRY") == 0) return TYPE_TELEMETRY;
  if (strcmp(s, "ALARM") == 0) return TYPE_ALARM;
  if (strcmp(s, "ERROR") == 0) return TYPE_ERROR;
  return TYPE_UNKNOWN;
}

static const char* role_to(msg_role_t r) {
  switch (r) {
    case ROLE_REQ: return "REQ";
    case ROLE_RESP: return "RESP";
    case ROLE_EVT: return "EVT";
    default: return "UNK";
  }
}

static const char* type_to(msg_type_t t) {
  switch (t) {
    case TYPE_PING: return "PING";
    case TYPE_START: return "START";
    case TYPE_STOP: return "STOP";
    case TYPE_STATUS: return "STATUS";
    case TYPE_RESET: return "RESET";
    case TYPE_FAULT: return "FAULT";
    case TYPE_TELEMETRY: return "TELEMETRY";
    case TYPE_ALARM: return "ALARM";
    case TYPE_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

// Strip trailing \r/\n in-place
static void rstrip_crlf(char* s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
    s[n - 1] = '\0';
    n--;
  }
}

int line_parse(const char* line, message_t* out) {
  if (!line || !out) return -1;
  msg_init(out);

  // Copy into scratch because tokenization modifies the string
  char buf[512];
  size_t n = strnlen(line, sizeof(buf) - 1);
  if (n == 0 || n >= sizeof(buf) - 1) return -1;
  memcpy(buf, line, n);
  buf[n] = '\0';

  rstrip_crlf(buf);

  // Tokenize by space/tab (multiple spaces ok)
  char* save = NULL;
  char* tok_role = strtok_r(buf, " \t", &save);
  char* tok_type = strtok_r(NULL, " \t", &save);
  if (!tok_role || !tok_type) return -1;

  out->role = role_from(tok_role);
  out->type = type_from(tok_type);
  if (out->role == ROLE_UNKNOWN || out->type == TYPE_UNKNOWN) return -1;

  for (char* tok = strtok_r(NULL, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
    char* eq = strchr(tok, '=');
    if (!eq) continue; // ignore malformed token
    *eq = '\0';
    const char* key = tok;
    const char* val = eq + 1;

    // Known keys
    if (strcmp(key, "dev") == 0) {
      out->dev = (uint32_t)strtoul(val, NULL, 10);
      out->has_dev = 1;
    } else if (strcmp(key, "seq") == 0) {
      out->seq = (uint32_t)strtoul(val, NULL, 10);
      out->has_seq = 1;
    } else if (strcmp(key, "ok") == 0) {
      out->ok = (int)strtol(val, NULL, 10);
      out->has_ok = 1;
    } else if (strcmp(key, "code") == 0) {
      out->code = (int32_t)strtol(val, NULL, 10);
      out->has_code = 1;
    } else if (strcmp(key, "state") == 0) {
      strncpy(out->state, val, sizeof(out->state) - 1);
      out->state[sizeof(out->state) - 1] = '\0';
      out->has_state = 1;
    } else if (strcmp(key, "msg") == 0) {
      strncpy(out->msg, val, sizeof(out->msg) - 1);
      out->msg[sizeof(out->msg) - 1] = '\0';
      out->has_msg = 1;
    } else if (strcmp(key, "temp") == 0) {
      out->temp = strtod(val, NULL);
      out->has_temp = 1;
    } else if (strcmp(key, "pressure") == 0) {
      out->pressure = strtod(val, NULL);
      out->has_pressure = 1;
    } else if (strcmp(key, "flow") == 0) {
      out->flow = strtod(val, NULL);
      out->has_flow = 1;
    } else {
      // Unknown keys must be ignored (forward compatibility)
    }
  }

  return 0;
}

int line_format(const message_t* m, char* out, size_t cap) {
  if (!m || !out || cap < 8) return -1;

  // Canonical-ish output order to keep logs consistent
  int w = snprintf(out, cap, "%s %s", role_to(m->role), type_to(m->type));
  if (w < 0 || (size_t)w >= cap) return -1;

  if (m->has_dev) {
    int r = snprintf(out + w, cap - (size_t)w, " dev=%u", m->dev);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }
  if (m->has_seq) {
    int r = snprintf(out + w, cap - (size_t)w, " seq=%u", m->seq);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }
  if (m->has_ok) {
    int r = snprintf(out + w, cap - (size_t)w, " ok=%d", m->ok);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }
  if (m->has_state) {
    int r = snprintf(out + w, cap - (size_t)w, " state=%s", m->state);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }
  if (m->has_code) {
    int r = snprintf(out + w, cap - (size_t)w, " code=%d", (int)m->code);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }
  if (m->has_msg) {
    int r = snprintf(out + w, cap - (size_t)w, " msg=%s", m->msg);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }
  if (m->has_temp) {
    int r = snprintf(out + w, cap - (size_t)w, " temp=%.2f", m->temp);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }
  if (m->has_pressure) {
    int r = snprintf(out + w, cap - (size_t)w, " pressure=%.2f", m->pressure);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }
  if (m->has_flow) {
    int r = snprintf(out + w, cap - (size_t)w, " flow=%.2f", m->flow);
    if (r < 0 || (size_t)(w + r) >= cap) return -1;
    w += r;
  }

  // Ensure we can append '\n' and '\0'
  if ((size_t)w + 2 > cap) return -1;
  out[w++] = '\n';
  out[w] = '\0';
  return w;
}
