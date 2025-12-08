#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "cblib.h"

struct State {
    int socket_fd;
};

int create_udp_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
#ifdef HAVE_SIN_LEN
    server_addr.sin_len = sizeof(struct sockaddr_in);
#endif

    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("bind");
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

int connect_peer(int socket_fd, uint32_t peer_ip, uint16_t peer_port) {
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = peer_ip;
    address.sin_port = htons(peer_port);
#ifdef HAVE_SIN_LEN
    server_addr.sin_len = sizeof(struct sockaddr_in);
#endif
    if (connect(socket_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("connect");
        return -1;
    }
    return 0;
}

void on_stdin(void* arg) {
    struct State *state = arg;
    char buffer[1<<16];
    ssize_t r = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (r == -1) {
        perror("read");
    } else {
        ssize_t s_count = send(state->socket_fd, buffer, r, 0);
        printf("SEND %ld BYTES\n", s_count);
    }
}

void on_message(void* arg) {
    struct State *state = arg;
    char buffer[1<<16];
    ssize_t msg_size = read(state->socket_fd, buffer, sizeof(buffer));
    if (msg_size == -1) {
        perror("recv");
    }
    write(STDOUT_FILENO, buffer, msg_size);
}

void start_client(uint16_t own_port, uint32_t peer_ip, uint16_t peer_port) {
    printf("Start client on Port %d. With Peer %u:%d\n", own_port, peer_ip, peer_port);
    int socket_fd = create_udp_socket(own_port);
    if (socket_fd == -1) return;

    if (connect_peer(socket_fd, peer_ip, peer_port) == -1) {
        close(socket_fd);
        return;
    }

    init_cblib();

    struct State state = {
        socket_fd,
    };

    register_stdin_callback(on_stdin, &state);
    register_fd_callback(socket_fd, on_message, &state);

    handle_events();
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./client <own_port> <peer_ip> <peer_port>");
    }

    start_client(atoi(argv[1]), inet_addr(argv[2]), atoi(argv[3]));
}