#include "Socket.h"

#include <unistd.h>

int Socket(int family, int type, int protocol)
{
    socket(family, type, protocol);
}

void Bind(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    bind(fd, addr, addrlen);
}

ssize_t SendTo(int fd, const void *buf, size_t buflen, int flags, const struct sockaddr *to, socklen_t tolen)
{
    sendto(fd, buf, buflen, flags, to, tolen);
}

ssize_t Recvform(int fd, void *buf, size_t buflen, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    recvfrom(fd, buf, buflen, flags, from, fromlen);
}

void Close(int fd)
{
    close(fd);
}
