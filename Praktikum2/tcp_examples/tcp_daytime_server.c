#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <time.h>

#include "Socket.h"

#define BUFFER_SIZE (1<<16)
#define MAX_PORT 65535

int main(int argc, char **argv)
{
    if(argc < 3) {
        fprintf(stderr, "(DAYTIME) Usage: %s <server_address> <port>\n", argv[0]);
        return 1;
    }

	int parent_fd, client_fd, opt_val, rv;
	struct sockaddr_in server_addr, client_addr;
	ssize_t len;
	char buf[BUFFER_SIZE];

	parent_fd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    opt_val = 1;
    SetSockOpt(parent_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &opt_val, sizeof(int));

	memset((void *) &server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;

#ifdef HAVE_SIN_LEN
	server_addr.sin_len = sizeof(struct sockaddr_in);
#endif

    rv = inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
    if (rv != 1) {
        if (rv < 0) {
            printf("inet_pton");
            return 1;
        }
        printf("invalid server address");
        return 1;
    }

	server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    long port = strtol(argv[2], NULL, 10);
    if (port > MAX_PORT || port < 0) {
        fprintf(stderr, "invalid port: 0-%d allowed\n", MAX_PORT);
        return 1;
    }

	server_addr.sin_port = htons(port);

	Bind(parent_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
    Listen(parent_fd, 10);

	for (;;) {
        client_fd = Accept(parent_fd, (struct sockaddr *) &client_addr, &len);
        time_t time_ = time(NULL);
        char *ascii_time = ctime(&time_);

        Send(client_fd, ascii_time, strlen(ascii_time) + 1, 0);
        Shutdown(client_fd, SHUT_WR);
        do {
            len = Recv(client_fd, (void*) buf, BUFFER_SIZE, 0);
        } while(len>0);
        Close(client_fd);
	}

	Close(parent_fd);
	return(0);
}
