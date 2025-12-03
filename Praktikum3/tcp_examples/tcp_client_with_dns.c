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
 * Gets all addresses from a string
 * When a Ipv4 address is entered, it can be handled too.
 * 
 * @param name Input string
 * @param result Reference to return the linked list of results
 * @returns the number of entries available in result
 */
int get_addresses_port(char* name, char* port, struct addrinfo** result) {
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

	int ret = getaddrinfo(name, port, &hints, result);
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

int set_address_ports(struct addrinfo *addresses, char* input) {
	int port = atoi(input);
	if (port == 0) {
		struct servent* servname = getservbyname(input, "tcp");
		if (servname == NULL) {
			fprintf(stderr, "Service name \"%s\" not found\n", input);
			return 1;
		}
		port = servname->s_port;
	} else {
		port = htons(port);
	}


	struct addrinfo *entry;
	for (entry = addresses; entry != NULL; entry = entry->ai_next) {
		if (entry->ai_family == AF_INET) {
			((struct sockaddr_in*)(entry->ai_addr))->sin_port = port;
		} else if (entry->ai_family == AF_INET6) {
			((struct sockaddr_in6*)(entry->ai_addr))->sin6_port = port;
		}
	}
	return 0;
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

	struct addrinfo *addresses;
	int addresses_count = get_addresses_port(argv[1], argv[2], &addresses);
	if (addresses_count == 0) {
		return 1;
	}

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
}
