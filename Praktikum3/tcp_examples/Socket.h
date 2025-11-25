#include <sys/socket.h>
#include <sys/select.h>
#include <stdbool.h>

#ifndef SOCKET_H
#define SOCKET_H

#define SOCKET_ERR -1
#define COLOR_CYAN "\x1b[36m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_RESET "\x1b[0m"

int Socket(int family, int type, int protocol);
int Bind(int fd, const struct sockaddr *addr, socklen_t addrlen, bool pexit);
ssize_t Sendto(int fd, const void *buf, size_t buflen, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t Recvfrom(int fd, void *buf, size_t buflen, int flags, struct sockaddr *from, socklen_t *fromlen);
void Close(int fd);
void Connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t Send(int fd, const void *buf, size_t buflen, int flags);
ssize_t Recv(int fd, void *buf, size_t buflen, int flags);
int Shutdown(int __fd, int __how);
int Select(int __nfds, fd_set * __readfds, fd_set * __writefds, fd_set * __exceptfds, struct timeval * __timeout);
int SetSockOpt(int fd, int level, int optname, const void *optval, socklen_t optlen, bool pexit);
void Listen(int fd, int backlog);
int Accept(int fd, struct sockaddr *addr, socklen_t *addrlen);

#endif
