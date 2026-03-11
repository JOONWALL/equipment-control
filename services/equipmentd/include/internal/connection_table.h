#pragma once

#include "internal/connection.h"

void register_connection(connection_t* conn);
void unregister_connection(connection_t* conn);
void scan_connection_timeout(int epfd, long long now);