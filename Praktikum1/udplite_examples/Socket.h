#include <sys/socket.h>

#ifndef IPPROTO_UDPLITE
#define IPPROTO_UDPLITE 136
#endif

#ifndef UDPLITE_SEND_CSCOV
#define UDPLITE_SEND_CSCOV 10
#endif

#ifndef UDPLITE_RECV_CSCOV
#define UDPLITE_RECV_CSCOV 11
#endif

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
int Setsockopt(int __fd, int __level, int __optname, const void *__optval, socklen_t __optlen);
int Getsockopt(int __fd, int __level, int __optname, void *__optval, socklen_t *__optlen);

#endif
