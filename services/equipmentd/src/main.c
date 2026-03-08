#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol/session.h"
#include "protocol/line_codec.h"
#include "internal/router.h"

static int make_server(int port){
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) return -1;

  int yes = 1;
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons((uint16_t)port);

  if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
  if(listen(fd, 16) < 0) return -1;
  return fd;
}

int main(int argc, char** argv){
  int port = (argc >= 2) ? atoi(argv[1]) : 12345;

  int sfd = make_server(port);
  if(sfd < 0){ perror("server"); return 1; }

  printf("[equipmentd] listening on %d\n", port);

  int cfd = accept(sfd, NULL, NULL);
  if(cfd < 0){ perror("accept"); return 1; }
  printf("[equipmentd] client connected\n");

  session_t sess; session_init(&sess);

  char rbuf[1024];
  char line[512];
  char wbuf[512];

  while(1){
    ssize_t n = read(cfd, rbuf, sizeof(rbuf));
    if(n == 0){ printf("[equipmentd] client closed\n"); break; }
    if(n < 0){
      if(errno == EINTR) continue;
      perror("read"); break;
    }

    if(session_feed(&sess, rbuf, (size_t)n) != 0){
      fprintf(stderr, "[equipmentd] session overflow\n");
      break;
    }

    while(1){
      int got = session_pop_line(&sess, line, sizeof(line));
      if(got == 0) break;
      if(got < 0){ fprintf(stderr, "[equipmentd] line too long\n"); goto done; }
      printf("[RX line] %s\n", line);

      message_t in, out;
      int pr = line_parse(line, &in);
      if(pr != 0){
        printf("[PARSE ERROR]\n");
        memset(&out, 0, sizeof(out));
        out.role = ROLE_RESP;
        out.type = TYPE_ERROR;
        out.ok = 0; out.has_ok = 1;
        out.code = 400; out.has_code = 1;
        strncpy(out.msg, "PARSE_ERROR", sizeof(out.msg)-1); out.has_msg = 1;
      } else {
        printf("[PARSED] role=%d type=%d\n", in.role, in.type);
        int rr = router_handle(&in, &out);
        if(rr <= 0) continue; // no response
        /*router가 응답 생성 책임만 가지고
        I/O는 main이 담당하는 구조*/
      }

      int wn = line_format(&out, wbuf, sizeof(wbuf));
      if(wn < 0){ fprintf(stderr, "[equipmentd] format fail\n"); goto done; }
      printf("[TX line] %s", wbuf);
      (void)write(cfd, wbuf, (size_t)wn);
    }
  }

done:
  close(cfd);
  close(sfd);
  return 0;
}
