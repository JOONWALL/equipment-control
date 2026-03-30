#define _POSIX_C_SOURCE 200809L
#include "util/time.h"

#include <time.h>

long long now_ms(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL;
}