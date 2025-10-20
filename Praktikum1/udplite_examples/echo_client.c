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
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <address> <checksum coverage>\n", argv[0]);
		return 1;
	}

	int fd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_len;
	ssize_t len;
	char buf[BUFFER_SIZE];

	char *endptr = NULL;
	unsigned int cscov = (unsigned int) strtoul(argv[2], &endptr, 10);
    
	fd = Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDPLITE);
	Setsockopt(fd, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV, &cscov, sizeof(cscov));
	
	// Check how UDPLITE_SEND_CSCOV was set
	socklen_t sockt_opt_len = sizeof(cscov);
	Getsockopt(fd, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV, &cscov, &sockt_opt_len);
	printf("UDPLITE_SEND_CSCOV: %u\n", cscov);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
	server_addr.sin_len = sizeof(struct sockaddr_in);
#endif
	server_addr.sin_port = htons(7);
	if ((server_addr.sin_addr.s_addr = (in_addr_t)inet_addr(argv[1])) == INADDR_NONE) {
		fprintf(stderr, "Invalid address\n");
	}

	memset((void *) buf, 'A', sizeof(buf));
	Sendto(fd, (const void *) buf, (size_t) MESSAGE_SIZE, 0, (const struct sockaddr *) &server_addr, sizeof(server_addr));

	addr_len = (socklen_t) sizeof(client_addr);
	memset((void *) buf, 0, sizeof(buf));
	len = Recvfrom(fd, (void *) buf, sizeof(buf), 0, (struct sockaddr *) &client_addr, &addr_len);
    printf("Received %zd bytes from %s.\n", len, inet_ntoa(client_addr.sin_addr));
	Close(fd);
	return(0);
}
