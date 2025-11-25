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

struct thread_data {
    int client_fd;
};

void *handleThread(void *thread_data);

void handle(int client_fd);

int main(int argc, char **argv)
{
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <server_address> <port>\n", argv[0]);
        return 1;
    }

    int parent_fd, client_fd, opt_val;
    struct sockaddr_in server_addr, client_addr;
    struct thread_data *thread_data;
    ssize_t len;

    parent_fd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    opt_val = 1;
    SetSockOpt(parent_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &opt_val, sizeof(int), true);

    memset((void *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;

#ifdef HAVE_SIN_LEN
    server_addr.sin_len = sizeof(struct sockaddr_in);
#endif

    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    long port = strtol(argv[2], NULL, 10);
    server_addr.sin_port = htons(port);

    Bind(parent_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr), true);
    Listen(parent_fd, 10);

    for (;;) {
        client_fd = Accept(parent_fd, (struct sockaddr *) &client_addr, (socklen_t *)&len);

        thread_data = (struct thread_data *) malloc(sizeof(struct thread_data));
        thread_data->client_fd = client_fd;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handleThread, (void *) thread_data);
        printf("%sClient with FD %d connected%s\n", COLOR_CYAN, client_fd, COLOR_RESET);
    }

    Close(parent_fd);
    return(0);
}

void *handleThread(void *thread_data) {
    int client_fd;

    pthread_detach(pthread_self());

    client_fd = ((struct thread_data *) thread_data)->client_fd;
    free(thread_data);

    handle(client_fd);

    return (NULL);
}

void handle(int client_fd) {
    char buf[BUFFER_SIZE];
    int recv_bytes, sent;

    recv_bytes = Recv(client_fd, buf, BUFFER_SIZE, 0);

    while (recv_bytes > 0) {
        sent = 0;
        do {
            sent += Send(client_fd, buf + sent, recv_bytes - sent, 0);
        } while (sent < recv_bytes);

        printf("%sFD %d >%s ", COLOR_GREEN, client_fd, COLOR_RESET);
        fwrite(buf, 1, (size_t)recv_bytes, stdout);

        recv_bytes = Recv(client_fd, buf, BUFFER_SIZE, 0);
    }

    Close(client_fd);

    printf("%sClient with FD %d disconnected%s\n", COLOR_CYAN, client_fd, COLOR_RESET);
}