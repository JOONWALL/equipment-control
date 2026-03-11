#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>

#include "internal/connection_table.h"

#define MAX_CONN 1024
#define CONN_IDLE_TIMEOUT_MS 60000

static connection_t* conn_table[MAX_CONN];
static int conn_count = 0;

void register_connection(connection_t* conn){
    if(conn_count < MAX_CONN)
        conn_table[conn_count++] = conn;
}

void unregister_connection(connection_t* conn){
    for(int i=0;i<conn_count;i++){
        if(conn_table[i] == conn){
            conn_table[i] = conn_table[conn_count-1];
            conn_count--;
            return;
        }
    }
}

void scan_connection_timeout(int epfd, long long now){
    for(int i=0;i<conn_count;i++){
        connection_t* conn = conn_table[i];

        if(now - conn->last_active_ms > CONN_IDLE_TIMEOUT_MS){
            int fd = conn->fd;

            printf("[timeout] fd=%d\n", fd);

            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);

            unregister_connection(conn);
            free(conn);

            i--;
        }
    }
}