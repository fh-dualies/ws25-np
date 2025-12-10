#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "socket_util.h"

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

int connect_peer(int socket_fd, const uint32_t peer_ip, const uint16_t peer_port) {
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