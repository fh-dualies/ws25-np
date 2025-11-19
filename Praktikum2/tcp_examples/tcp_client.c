#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "Socket.h"

#define BUFFER_SIZE  (1<<16)
#define MESSAGE_SIZE (9216)

int main(int argc, char **argv)
{
	if(argc < 3) {
		fprintf(stderr, "Usage: %s <server_address> <port>\n", argv[0]);
		return 1;
	}

	int fd;
	struct sockaddr_in server_addr;
	ssize_t len;
	char buf[BUFFER_SIZE];

    fd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
	server_addr.sin_len = sizeof(struct sockaddr_in);
#endif

	server_addr.sin_port = htons(atoi(argv[2]));
	if ((server_addr.sin_addr.s_addr = (in_addr_t)inet_addr(argv[1])) == INADDR_NONE) {
		fprintf(stderr, "Invalid address\n");
		return 1;
	}

    printf("%sConnecting to %s:%s...%s\n", COLOR_CYAN, argv[1], argv[2], COLOR_RESET);
	Connect(fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
    printf("%sConnected to %s:%s%s\n", COLOR_CYAN, argv[1], argv[2], COLOR_RESET);

	fd_set read_fds;
	int server_closed = 0;
    int stdin_closed = 0;
    FD_ZERO(&read_fds);
    while (!server_closed && !stdin_closed) {
		FD_SET(fd, &read_fds);
		FD_SET(STDIN_FILENO, &read_fds);
		Select(fd + 1, &read_fds, NULL, NULL, NULL);

		if (FD_ISSET(fd, &read_fds)) {
			len = Recv(fd, (void *) buf, sizeof(buf), 0);
			if (len <= 0) {
				server_closed = 1;
			} else {
				printf("%s>%s ", COLOR_GREEN, COLOR_RESET);
                write(STDOUT_FILENO, buf, (size_t)len);
			}
		}
		if (FD_ISSET(STDIN_FILENO, &read_fds)) {
			len = read(STDIN_FILENO, buf, MESSAGE_SIZE);
            if (len <= 0) {
                stdin_closed = 1;
            } else {
                Send(fd, (const void *) buf, (size_t)len, 0);
            }
		}
	}

    if (server_closed) {
        // If the server closed the connection, we are done
        puts(COLOR_CYAN "Server closed the connection - Stdin was still open." COLOR_RESET);
        Close(fd);
        return 0;
    }

    puts(COLOR_CYAN "Stdin closed - sending FIN to server." COLOR_RESET);

    Shutdown(fd, SHUT_WR);

    // Read any remaining data from the server
    while (!server_closed) {
        len = Recv(fd, (void *) buf, sizeof(buf), 0);
        if (len <= 0) {
            puts(COLOR_CYAN "Server closed the connection." COLOR_RESET);
            server_closed = 1;
        } else {
            write(STDOUT_FILENO, buf, (size_t)len);
        }
    }

    Close(fd);
	return(0);
}
