#include <sys/socket.h>
#include <sys/select.h>

#ifndef SOCKET_H
#define SOCKET_H

#define SOCKET_ERR -1

int Socket(int family, int type, int protocol);
void Bind(int fd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t Sendto(int fd, const void *buf, size_t buflen, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t Recvfrom(int fd, void *buf, size_t buflen, int flags, struct sockaddr *from, socklen_t *fromlen);
void Close(int fd);
void Connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t Send(int fd, const void *buf, size_t buflen, int flags);
ssize_t Recv(int fd, void *buf, size_t buflen, int flags);
int Shutdown(int __fd, int __how);
int Select(int __nfds, fd_set * __readfds, fd_set * __writefds, fd_set * __exceptfds, struct timeval * __timeout);
void SetSockOpt(int fd, int level, int optname, const void *optval, socklen_t optlen);
void Listen(int fd, int backlog);
int Accept(int fd, struct sockaddr *addr, socklen_t *addrlen);

#endif
