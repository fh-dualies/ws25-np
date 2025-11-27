#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include "Socket.h"

#define BUFFER_SIZE  (1<<16)
#define MESSAGE_SIZE (9216)

/**
 * Gets all adresses from a string
 * When a Ipv4 address is entered, it can be handled too.
 * 
 * @param name Input string
 * @param result Reference to return the linked list of results
 * @returns the number of entrys avaliable in result 
 */
int get_addresses(char* name, struct addrinfo** result) {
	struct addrinfo hints;

	in_addr_t addr = inet_addr(name);
	if (addr != INADDR_NONE) {
		struct sockaddr_in *sock_addr = calloc(1, sizeof(struct sockaddr_in));
		if (sock_addr == NULL) {
			return 0;
		}

		sock_addr->sin_addr.s_addr = addr;
		sock_addr->sin_family = AF_INET;
		#ifdef HAVE_SIN_LEN
			addr->sin_len = sizeof(struct sockaddr_in);
		#endif

		struct addrinfo *r = calloc(1, sizeof(struct addrinfo));
		if (r == NULL) {
			return 0;
		}
		r->ai_family = AF_INET;
		r->ai_socktype = SOCK_STREAM;
		r->ai_protocol = IPPROTO_TCP;
		r->ai_addr = (struct sockaddr *)sock_addr;
		r->ai_addrlen = sizeof(struct sockaddr_in);
		r->ai_canonname = NULL;
		r->ai_next = NULL;

		*result = r;
		return 1; // Always 1 address
	}

	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

	int ret = getaddrinfo(name, NULL, &hints, result);
    if (ret != 0) {
        printf("getaddrinfo: %s", gai_strerror(ret));
        return 0; // No address
    }

	int count = 0;
	struct addrinfo *entry;
	for (entry = *result; entry != NULL; entry = entry->ai_next) {
		count++;
	}
	return count;
}

void set_address_ports(struct addrinfo *addresses, int port) {
	struct addrinfo *entry;
	int port_net = htons(port);
	for (entry = addresses; entry != NULL; entry = entry->ai_next) {
		if (entry->ai_family == AF_INET) {
			((struct sockaddr_in*)(entry->ai_addr))->sin_port = port_net;
		} else if (entry->ai_family == AF_INET6) {
			((struct sockaddr_in6*)(entry->ai_addr))->sin6_port = port_net;
		}
	}
}

void create_sockets(struct addrinfo *addresses, int* fds, char** ips ) {
	struct addrinfo *entry;
	int i = 0;
	for (entry = addresses; entry != NULL; entry = entry->ai_next) {
		int fd = Socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
		Connect(fd, entry->ai_addr, entry->ai_addrlen);
		fds[i] = fd;
		ips[i] = malloc(NI_MAXHOST);
		getnameinfo(
          entry->ai_addr,
          entry->ai_addrlen,
          ips[i],
          NI_MAXHOST,
          NULL,
          0,
          NI_NUMERICHOST
        );
		i++;
	}
}

int main(int argc, char **argv)
{
	if(argc < 3) {
		fprintf(stderr, "Usage: %s <server_address | domain> <port>\n", argv[0]);
		return 1;
	}

	int port = atoi(argv[2]);
	if (port == 0) {
		fprintf(stderr, "Unvalid port entered!");
		return 1;
	}

	struct addrinfo *addresses;
	int addresses_count = get_addresses(argv[1], &addresses);
	if (addresses_count == 0) {
		return 1;
	}

	set_address_ports(addresses, port);

	int *sockets = calloc(addresses_count, sizeof(int));
	char **socket_ips = calloc(addresses_count, sizeof(char*));
	create_sockets(addresses, sockets, socket_ips);
	freeaddrinfo(addresses);

	int max_fd = STDIN_FILENO;
	for (int i = 0; i < addresses_count; i++) {
		if (sockets[i] > max_fd) {
			max_fd = sockets[i];
		}
	}
	max_fd++;

	fd_set fdset;
	ssize_t len;
	char buf[BUFFER_SIZE];
	int stdin_open = 1;
	int open_connections;

	for(;;) {
		open_connections = 0;
		FD_ZERO(&fdset);
		if (stdin_open) {
			FD_SET(STDIN_FILENO, &fdset);
		}
		for (int i = 0; i < addresses_count; i++) {
			if (sockets[i] <= 0) continue;
			FD_SET(sockets[i], &fdset);
			open_connections++;
		}

		if (open_connections <= 0) {
			break;
		}

		Select(max_fd, &fdset, NULL, NULL, (struct timeval *) 0);

		if (FD_ISSET(STDIN_FILENO, &fdset)) {
			len = read(STDIN_FILENO, buf, MESSAGE_SIZE);
            if (len <= 0) {
                stdin_open = 0;
				puts(COLOR_CYAN "Stdin closed - sending FIN to server." COLOR_RESET);
				for(int i = 0; i < addresses_count; i++) {
					if (sockets[i] <= 0) continue;
					Shutdown(sockets[i], SHUT_WR);
				}
            } else {
				for(int i = 0; i < addresses_count; i++) {
					if (sockets[i] <= 0) continue;
					Send(sockets[i], (const void *) buf, (size_t)len, 0);
				}
            }
		}

		for(int i = 0; i < addresses_count; i++) {
			if (sockets[i] <= 0) continue;
			if (!FD_ISSET(sockets[i], &fdset)) continue;
			len = Recv(sockets[i], (void *) buf, sizeof(buf), 0);
			if (len <= 0) {
				Close(sockets[i]);
				sockets[i] = 0;
			} else {
				if (socket_ips[i] == NULL) continue;
				printf("%s%s >%s ", COLOR_GREEN, socket_ips[i], COLOR_RESET);
				fflush(stdout);
				write(STDOUT_FILENO, buf, (size_t)len);
			}
		}
	}

	free(sockets);
	for (int i = 0; i < addresses_count; i++) {
		if (socket_ips[i] == NULL) continue;
		free(socket_ips[i]);
	}
	free(socket_ips);

	/*
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

	server_addr.sin_port = htons(port);
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
	*/
}
