#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "Socket.h"

#define BUFFER_SIZE (1<<16)
#define MAX_PORT 65535

struct thread_data {
    int client_fd;
    int connect_cnt;
};

void *handleThread(void *thread_data);

void handle(int client_fd, int connect_cnt);

int main(int argc, char **argv)
{
    if(argc < 3) {
        fprintf(stderr, "(ECHO_THREADED) Usage: %s <server_address> <port>\n", argv[0]);
        return 1;
    }

    int parent_fd, client_fd, opt_val;
    struct sockaddr_in server_addr, client_addr;
    struct thread_data *thread_data;
    ssize_t len;

    int count_connections = 0;

    parent_fd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    opt_val = 1;
    SetSockOpt(parent_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &opt_val, sizeof(int));

    memset((void *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;

#ifdef HAVE_SIN_LEN
    server_addr.sin_len = sizeof(struct sockaddr_in);
#endif

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

        thread_data = (struct thread_data *) malloc(sizeof(struct thread_data));
        thread_data->client_fd = client_fd;
        thread_data->connect_cnt = count_connections;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handleThread, (void *) thread_data);
        printf("Client with ID %d connected\n", count_connections);

        count_connections++;
    }

    Close(parent_fd);
    return(0);
}

void *handleThread(void *thread_data) {
    int client_fd, count_connections;

    pthread_detach(pthread_self());

    client_fd = ((struct thread_data *) thread_data)->client_fd;
    count_connections = ((struct thread_data *) thread_data)->connect_cnt;
    free(thread_data);

    handle(client_fd, count_connections);

    return (NULL);
}

void handle(int client_fd, int count_connections) {
    char buf[BUFFER_SIZE];
    int recv_bytes, sent;

    recv_bytes = Recv(client_fd, buf, BUFFER_SIZE, 0);

    while (recv_bytes > 0) {
        sent = 0;
        do {
            sent += Send(client_fd, buf + sent, recv_bytes - sent, 0);
        } while (sent < recv_bytes);

        printf("Handle Client with ID %d\n", count_connections);

        fflush(stdout);
        if (write(STDOUT_FILENO, buf, recv_bytes) < 0) {
            perror("write");
            break;
        }

        recv_bytes = Recv(client_fd, buf, BUFFER_SIZE, 0);
    }

    Close(client_fd);
    printf("Client with ID %d disconnected\n", count_connections);
}