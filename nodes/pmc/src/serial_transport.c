#define _POSIX_C_SOURCE 200809L

#include "serial_transport.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

static speed_t baud_to_speed(int baudrate){
  switch(baudrate){
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    default: return B115200;
  }
}

int serial_link_open(serial_link_t* link, const char* path, int baudrate){
  struct termios tty;
  int fd;

  if(!link || !path) return -1;

  memset(link, 0, sizeof(*link));
  fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
  if(fd < 0){
    perror("open serial");
    return -1;
  }

  memset(&tty, 0, sizeof(tty));
  if(tcgetattr(fd, &tty) != 0){
    perror("tcgetattr");
    close(fd);
    return -1;
  }

  cfsetospeed(&tty, baud_to_speed(baudrate));
  cfsetispeed(&tty, baud_to_speed(baudrate));

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
  tty.c_iflag &= ~IGNBRK;
  tty.c_lflag = 0;
  tty.c_oflag = 0;
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 1;

  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~(PARENB | PARODD);
  tty.c_cflag &= ~CSTOPB;
#ifdef CRTSCTS
  tty.c_cflag &= ~CRTSCTS;
#endif

  if(tcsetattr(fd, TCSANOW, &tty) != 0){
    perror("tcsetattr");
    close(fd);
    return -1;
  }

  link->fd = fd;
  link->baudrate = baudrate;
  link->is_open = 1;
  strncpy(link->path, path, sizeof(link->path) - 1);
  link->path[sizeof(link->path) - 1] = '\0';

  return 0;
}

void serial_link_close(serial_link_t* link){
  if(!link) return;
  if(link->is_open && link->fd >= 0){
    close(link->fd);
  }
  memset(link, 0, sizeof(*link));
  link->fd = -1;
}

ssize_t serial_link_write_line(serial_link_t* link, const char* line){
  char buf[256];
  size_t len;

  if(!link || !link->is_open || !line) return -1;

  len = strlen(line);
  if(len + 1 >= sizeof(buf)) return -1;

  memcpy(buf, line, len);
  buf[len++] = '\n';

  return write(link->fd, buf, len);
}

int serial_link_read_line(serial_link_t* link,
                          char* out,
                          size_t out_sz,
                          int timeout_ms){
  fd_set rfds;
  struct timeval tv;
  size_t used = 0;

  if(!link || !link->is_open || !out || out_sz == 0) return -1;

  memset(out, 0, out_sz);

  while(used + 1 < out_sz){
    int rc;
    char ch;

    FD_ZERO(&rfds);
    FD_SET(link->fd, &rfds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    rc = select(link->fd + 1, &rfds, NULL, NULL, &tv);
    if(rc < 0){
      if(errno == EINTR) continue;
      return -1;
    }
    if(rc == 0){
      return 0; // timeout
    }

    rc = (int)read(link->fd, &ch, 1);
    if(rc < 0){
      if(errno == EINTR) continue;
      return -1;
    }
    if(rc == 0){
      return 0;
    }

    if(ch == '\r'){
      continue;
    }
    if(ch == '\n'){
      out[used] = '\0';
      return 1;
    }

    out[used++] = ch;
  }

  out[out_sz - 1] = '\0';
  return 1;
}