#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#ifdef HAVE_NETINET_UDPLITE_H
#include <netinet/udplite.h>
#endif

#include "Socket.h"

#define BUFFER_SIZE (1<<16)

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <checksum coverage>\n", argv[0]);
		return 1;
	}

	int fd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_len;
	ssize_t len;
	char buf[BUFFER_SIZE];

	char *endptr = NULL;
	unsigned int cscov = (unsigned int) strtoul(argv[1], &endptr, 10);
    
	fd = Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDPLITE);
	Setsockopt(fd, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV, &cscov, sizeof(cscov));
	Setsockopt(fd, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV, &cscov, sizeof(cscov));

	memset((void *) &server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
	server_addr.sin_len = sizeof(struct sockaddr_in);
#endif
	server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	server_addr.sin_port = htons(7);

	Bind(fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));


	for (;;) {
		addr_len = (socklen_t) sizeof(client_addr);
		memset((void *) &client_addr, 0, sizeof(client_addr));
		len = Recvfrom(fd, (void *) buf, sizeof(buf), 0,
                       (struct sockaddr *) &client_addr, &addr_len);
		printf("Received %zd bytes from %s:%d.\n", len, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        Sendto(fd, (const void *) buf, (size_t)len, 0, (struct sockaddr *)&client_addr, addr_len);
	}

	Close(fd);
	return(0);
}
