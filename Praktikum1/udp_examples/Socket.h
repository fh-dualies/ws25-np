#include <sys/socket.h>

#ifndef SOCKET_H
#define SOCKET_H

#define SOCKET_ERR -1

int Socket(int family, int type, int protocol);
void Bind(int fd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t Sendto(int fd, const void *buf, size_t buflen, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t Recvfrom(int fd, void *buf, size_t buflen, int flags, struct sockaddr *from, socklen_t *fromlen);
void Close(int fd);

#endif