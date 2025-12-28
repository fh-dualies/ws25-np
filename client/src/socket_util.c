#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "socket_util.h"

struct sockaddr_in server_addr;

int create_udp_socket(const uint16_t port) {
  const int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_fd == -1) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
#ifdef HAVE_SIN_LEN
  server_addr.sin_len = sizeof(struct sockaddr_in);
#endif

  if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
    perror("bind");
    close(socket_fd);
    return -1;
  }
  return socket_fd;
}

int create_tcp_socket() {
  const int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_fd == -1) {
    perror("socket");
    return -1;
  }

  return socket_fd;
}

int bind_listen_tcp_socket(const int socket_fd, const uint16_t port, const uint32_t address) {
  int reuse_sock_opt_val = 1;
  if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse_sock_opt_val, sizeof(reuse_sock_opt_val)) == -1) {
    perror("setsockopt");
    return -1;
  }

  struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(address),
    .sin_port = htons(port),
    #ifdef HAVE_SIN_LEN
    .sin_len = sizeof(struct sockaddr_in),
    #endif
  };
  
  if(bind(socket_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
    perror("bind");
    return -1;
  }

  if (listen(socket_fd, 10) == -1) {
    perror("listen");
    return -1;
  }
  
  return 0;
}

int connect_socket(int socket_fd, const uint32_t peer_ip, const uint16_t peer_port) {
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = peer_ip;
  address.sin_port = htons(peer_port);
#ifdef HAVE_SIN_LEN
  server_addr.sin_len = sizeof(struct sockaddr_in);
#endif
  if (connect(socket_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
    perror("connect");
    return -1;
  }
  return 0;
}