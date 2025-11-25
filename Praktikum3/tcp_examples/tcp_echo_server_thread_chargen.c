#include "Socket.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>


#define BUFFER_SIZE (1 << 16)
#define LINE_LENGTH 72

void *handleThread(void *fd_ptr);
void chargen_main(int client_fd);

int main(int argc, char **argv)
{
    int sfd, client_fd;
    char *service = "19";
    char *bind_addr = NULL;
    struct addrinfo *result, hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

    int opt;
    while ((opt = getopt(argc, argv, "b:p:")) != -1) {
        switch (opt) {
            case 'b':
                bind_addr = optarg;
                hints.ai_family = AF_UNSPEC;
                break;
            case 'p':
                service = optarg;
                break;
            case '?':
            default:
                printf("Usage: %s [-b bind_address ] [-p port ]", argv[0]);
                return 0;
        }
    }

    int ret = getaddrinfo(bind_addr, service, &hints, &result);
    if (ret != 0) {
        printf("getaddrinfo: %s", gai_strerror(ret));
        return 0;
    }

    struct addrinfo *entry;
    for (entry = result; entry != NULL; entry = entry->ai_next) {
        sfd = Socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);

        if (sfd < 0) continue;

        if (entry->ai_family == AF_INET6) {
            /**
             * The default value of the V6ONLY option varies per platform. So
             * the option is set explicitly to ensure consistent behavior.
             * If no bind address was specified on the cmd line then a wildcard
             * socket with ipv6 and ipv4 support is used. Else if an ipv6
             * address was specified only ipv6 is used. This is needed for Linux.
             * Otherwise if 0.0.0.0 was specified as the bind address it is not
             * possible to bind a second socket with :: as address (and vice versa).
             */
            int optval = (bind_addr == NULL) ? 0 : 1;
            if (SetSockOpt(sfd, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(int), false) < 0) {
                Close(sfd);
                continue;
            }
        }

        if (Bind(sfd, entry->ai_addr, entry->ai_addrlen, false) == 0)
            break;

        Close(sfd);
    }

    if (entry == NULL) {
        printf( "binding failed: no working socket found");
        return 0;
    }

    freeaddrinfo(result);
    result = entry = NULL;

    Listen(sfd, 10);
    printf("listening...\n");

    while (1) {
        client_fd = Accept(sfd, NULL, NULL);

        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handleThread, fd_ptr);
        printf("%sClient with ID %d connected%s\n",COLOR_CYAN, client_fd, COLOR_RESET);

    }

    return 0;
}

void *handleThread(void *fd_ptr)
{
    int client_fd;

    pthread_detach(pthread_self());
    client_fd = *((int*)fd_ptr);
    free(fd_ptr);

    chargen_main(client_fd);

    return NULL;
}

void chargen_main(int client_fd)
{
    char recv_buf[BUFFER_SIZE];
    char send_buf[LINE_LENGTH + 2];
    int sent, line_number = 0;
    fd_set rset, wset;

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    int reset = 0;
    while (!reset) {
        FD_SET(client_fd, &rset);
        FD_SET(client_fd, &wset);

        Select(client_fd + 1, &rset, &wset, NULL, NULL);

        if (FD_ISSET(client_fd, &rset)) {
            int n = Recv(client_fd, &recv_buf, BUFFER_SIZE, 0);
            if (n <= 0) {
                if (n < 0) {
                    printf("%sClient %d recv: %s%s\n", COLOR_GREEN, client_fd, strerror(errno), COLOR_RESET);
                }
                break;
            }
        }

        if (FD_ISSET(client_fd, &wset)) {
            int i;
            for (i = 0; i < LINE_LENGTH; i++) {
                send_buf[i] = ((i + line_number) % 95) + 32;
            }

            send_buf[i] = '\r';
            send_buf[i + 1] = '\n';
            i += 2;

            sent = 0;
            do {
                int n = Send(client_fd, send_buf + sent, i - sent, 0);
                if (n < 0) {
                    printf("%sClient %d send: %s%s\n", COLOR_GREEN, client_fd, strerror(errno), COLOR_RESET);
                    reset = 1;
                    break;
                }
                sent += n;
            } while (sent < i);

            line_number++;
        }
    }

    Close(client_fd);
    printf("%sClient %d closed%s\n", COLOR_CYAN, client_fd, COLOR_RESET);
}
