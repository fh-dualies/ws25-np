#include "Socket.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int Socket(int family, int type, int protocol)
{
    int fd = socket(family, type, protocol);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    return fd;
}

void Bind(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (bind(fd, addr, addrlen) < 0) {
        perror("bind");
        exit(SOCKET_ERR);
    }
}

ssize_t Sendto(int fd, const void *buf, size_t buflen, int flags, const struct sockaddr *to, socklen_t tolen)
{
    ssize_t sent = sendto(fd, buf, buflen, flags, to, tolen);
    if (sent < 0) {
        perror("sendto");
        exit(SOCKET_ERR);
    }
    return sent;
}

ssize_t Recvfrom(int fd, void *buf, size_t buflen, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    ssize_t received = recvfrom(fd, buf, buflen, flags, from, fromlen);
    if (received < 0) {
        perror("recvfrom");
        exit(SOCKET_ERR);
    }

    return received;
}

void Close(int fd)
{
    if (close(fd) < 0) {
        perror("close");
        exit(SOCKET_ERR);
    }
}

void Connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (connect(fd, addr, addrlen) < 0) {
        perror("connect");
        exit(SOCKET_ERR);
    }
}

ssize_t Send(int fd, const void *buf, size_t buflen, int flags)
{
    ssize_t sent = send(fd, buf, buflen, flags);
    if (sent < 0) {
        perror("send");
        exit(SOCKET_ERR);
    }

    return sent;
}

ssize_t Recv(int fd, void *buf, size_t buflen, int flags)
{
    ssize_t received = recv(fd, buf, buflen, flags);
    if (received < 0) {
        perror("recv");
        exit(SOCKET_ERR);
    }

    return received;
}

int Shutdown(int __fd, int __how) {
    int result = shutdown(__fd, __how);
    if (result < 0) {
        perror("shutdown");
        exit(SOCKET_ERR);
    }
    return result;
}

int Select(int __nfds, fd_set * __readfds, fd_set * __writefds, fd_set * __exceptfds, struct timeval * __timeout) {
    int result = select(__nfds, __readfds, __writefds, __exceptfds, __timeout);
    if (result < 0) {
        perror("select");
        exit(SOCKET_ERR);
    }
    return result;
}