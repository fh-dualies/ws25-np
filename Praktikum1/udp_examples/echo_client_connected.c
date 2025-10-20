#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "Socket.h"

#define BUFFER_SIZE  (1<<16)
#define MESSAGE_SIZE (9216)

int main(int argc, char **argv)
{
    if(argc != 2) {
        fprintf(stderr, "Adress must be given!\n");
        return 1;
    }

    int fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    ssize_t len;
    char buf[BUFFER_SIZE];

    fd = Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
    server_addr.sin_len = sizeof(struct sockaddr_in);
#endif
    server_addr.sin_port = htons(7);
    if ((server_addr.sin_addr.s_addr = (in_addr_t)inet_addr(argv[1])) == INADDR_NONE) {
        fprintf(stderr, "Invalid address\n");
    }

    // Connect
    //Connect(fd, (const struct sockaddr*)&server_addr, (socklen_t)sizeof(struct sockaddr_in));
    Connect(fd, (const struct sockaddr*)&server_addr, sizeof(server_addr));

    memset((void *) buf, 'A', sizeof(buf));

    // Send() statt SendTo()
    Send(fd, buf, MESSAGE_SIZE, 0);

    memset((void *) buf, 0, sizeof(buf));

    // Recv statt RecvFrom
    len = Recv(fd, (void *)buf, sizeof(buf), 0);
    printf("Received %zd bytes from %s.\n", len, argv[1]);
    buf[len] = 0;
    Close(fd);
    return(0);
}
