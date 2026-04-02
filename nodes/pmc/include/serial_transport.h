#ifndef PMC_SERIAL_TRANSPORT_H
#define PMC_SERIAL_TRANSPORT_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
  int fd;
  char path[128];
  int baudrate;
  int is_open;
} serial_link_t;

int serial_link_open(serial_link_t* link, const char* path, int baudrate);
void serial_link_close(serial_link_t* link);

ssize_t serial_link_write_line(serial_link_t* link, const char* line);
int serial_link_read_line(serial_link_t* link,
                          char* out,
                          size_t out_sz,
                          int timeout_ms);

#endif