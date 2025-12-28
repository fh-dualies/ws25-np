//
// Created by dennis on 10/12/25.
//

#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <netinet/in.h>

int create_udp_socket(uint16_t port);

int create_tcp_socket();

int bind_listen_tcp_socket(int socket_fd, uint16_t port, uint32_t address);

int connect_socket(int socket_fd, uint32_t peer_ip, uint16_t peer_port);

#endif //SOCKET_UTIL_H
