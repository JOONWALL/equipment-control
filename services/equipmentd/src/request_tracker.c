#include "internal/request_tracker.h"
#include <string.h>

void request_tracker_init(request_tracker_t* t){
  if(!t) return;
  memset(t, 0, sizeof(*t));
}

pending_request_t* request_tracker_alloc(request_tracker_t* t){
  if(!t) return 0;
  for(int i = 0; i < REQUEST_TRACKER_MAX; i++){
    if(!t->items[i].valid){
      memset(&t->items[i], 0, sizeof(t->items[i]));
      t->items[i].valid = 1;
      return &t->items[i];
    }
  }
  return 0;
}

pending_request_t* request_tracker_find(request_tracker_t* t, uint32_t seq, uint32_t dev){
  if(!t) return 0;
  for(int i = 0; i < REQUEST_TRACKER_MAX; i++){
    if(t->items[i].valid &&
       t->items[i].seq == seq &&
       t->items[i].dev == dev){
      return &t->items[i];
    }
  }
  return 0;
}

void request_tracker_remove(request_tracker_t* t, pending_request_t* r){
  (void)t;
  if(!r) return;
  memset(r, 0, sizeof(*r));
}

void request_tracker_sweep_timeouts(request_tracker_t* t, long long now_ms){
  if(!t) return;
  for(int i = 0; i < REQUEST_TRACKER_MAX; i++){
    if(t->items[i].valid &&
       now_ms - t->items[i].created_ms > REQUEST_TIMEOUT_MS){
      memset(&t->items[i], 0, sizeof(t->items[i]));
    }
  }
}