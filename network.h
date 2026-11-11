#pragma once

#include <arpa/inet.h>

#define MY_PORT 5000

sockaddr_in serv_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(MY_PORT),
    .sin_addr = {.s_addr = inet_addr("127.0.0.1")}
};