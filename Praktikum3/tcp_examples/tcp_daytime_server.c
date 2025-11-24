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
#define MAX_FDS 20

int main(int argc, char **argv)
{
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <server_address> <port>\n", argv[0]);
        return 1;
    }

	int parent_fd, opt_val, rv;
	struct sockaddr_in server_addr;
	char buf[BUFFER_SIZE];

	int count_connections = 0;
	int max_fd;
	int connection_fds[MAX_FDS];
	fd_set rset;

	memset(connection_fds, -1, MAX_FDS * sizeof(int));

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

	server_addr.sin_port = htons(port);

	Bind(parent_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
    Listen(parent_fd, 10);

	for (;;) {
		FD_ZERO(&rset);
		FD_SET(parent_fd, &rset);
		max_fd = parent_fd;

		for (int i = 0; i < MAX_FDS; ++i) {
			if (connection_fds[i] < 0) continue;
			FD_SET(connection_fds[i], &rset);
			if (connection_fds[i] > max_fd) {
				max_fd = connection_fds[i];
			}
		}

		Select(max_fd + 1, &rset, NULL, NULL, (struct timeval *) 0);

		// New Connection?
		if (FD_ISSET(parent_fd, &rset)) {
			if (count_connections < MAX_FDS) {
				for (int i = 0; i < MAX_FDS; ++i) {
					if (connection_fds[i] == -1) {
						connection_fds[i] = Accept(parent_fd, NULL, NULL);
						count_connections++;
						time_t time_ = time(NULL);
						char *ascii_time = ctime(&time_);
						Send(connection_fds[i], ascii_time, strlen(ascii_time) + 1, 0);
						Shutdown(connection_fds[i], SHUT_WR);
						break;
					}
				}
			} else {
				int fd = Accept(parent_fd, NULL, NULL);
				Close(fd);
			}
		}

		// New information from the client?
		for (int i = 0; i < MAX_FDS; ++i) {
			if (connection_fds[i] != -1 && FD_ISSET(connection_fds[i], &rset)) {
				if (!Recv(connection_fds[i], buf, BUFFER_SIZE, 0)) {
					printf("%sClosing FD %d%s\n", COLOR_CYAN, connection_fds[i], COLOR_RESET);
					Close(connection_fds[i]);
					connection_fds[i] = -1;
					count_connections--;
				} else {
					printf("Data discarded from FD %d\n", connection_fds[i]);
				}
			}
		}
	}

	Close(parent_fd);
	return(0);
}
