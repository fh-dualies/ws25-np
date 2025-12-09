#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "cblib.h"

#define MESSAGE_COLUMN_TYPE 1
#define MESSAGE_COLUMN_ACK_TYPE 2
#define MESSAGE_HEARTBEAT_TYPE 3
#define MESSAGE_HEARTBEAT_ACK_TYPE 4
#define MESSAGE_ERROR_TYPE 5

#define ERROR_CODE_UNKNOWN_TYPE 1
#define ERROR_CODE_PROTOCOL_ERROR 2

struct State {
    int socket_fd;
} state = {-1};

char buffer[1<<16];

struct MessageUnknown {
    uint16_t type;
    uint16_t length;
    uint8_t data[];
};

struct MessageColumn {
    uint16_t type;
    uint16_t length;
    uint32_t sequence;
    uint16_t column;
};

struct MessageColumnAck {
    uint16_t type;
    uint16_t length;
    uint32_t ack;
};

struct MessageHeartbeat {
    uint16_t type;
    uint16_t length;
    uint8_t data[];
};

struct MessageError {
    uint16_t type;
    uint16_t length;
    uint32_t error_code;
    uint8_t data[];
};

int create_udp_socket(const uint16_t port) {
    const int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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

int connect_peer(const uint32_t peer_ip, const uint16_t peer_port) {
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = peer_ip;
    address.sin_port = htons(peer_port);
#ifdef HAVE_SIN_LEN
    server_addr.sin_len = sizeof(struct sockaddr_in);
#endif
    if (connect(state.socket_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("connect");
        return -1;
    }
    return 0;
}

void on_stdin(void* arg) {
    const ssize_t r = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (r == -1) {
        perror("read");
    } else {
        const ssize_t s_count = send(state.socket_fd, buffer, r, 0);
        printf("SEND %ld BYTES\n", s_count);
    }
}

void send_to_peer(const struct MessageUnknown* data, const size_t total_size) {
    // Ensures padding at the end. Padding contents will be random data from the buffer.
    const size_t padded_size = total_size + (4 - (total_size % 4));
    memcpy(buffer, data, total_size);
    if (send(state.socket_fd, buffer, padded_size, 0) == -1) {
        perror("send");
    }
}

void send_error_message(const uint32_t error_code, const char* error_string) {
    const size_t err_string_size = strlen(error_string);
    const size_t message_size = sizeof(struct MessageError) + err_string_size;
    struct MessageError *msg = malloc(message_size);
    msg->type = htons(MESSAGE_ERROR_TYPE);
    msg->length = htons(err_string_size + 4); // +4 to include the error code;
    msg->error_code = htonl(error_code);
    memcpy(msg->data, error_string, err_string_size);
    send_to_peer((struct MessageUnknown*) msg, message_size);
    free(msg);

#ifdef DEBUG
    fprintf(stderr, "Send error message with code %d to Peer: %s", error_code, error_string);
#endif
}

void recv_error_message(struct MessageUnknown* msg) {
    if (msg->length < 4) return; // Error code is 4 byte
    struct MessageError *error = (struct MessageError*)msg;
    error->error_code = ntohs(error->error_code);
    const size_t err_string_size = msg->length - 4;
    printf("Recieved error message with code %d: %.*s\n", error->error_code, (int)err_string_size, (char *)error->data);
}

void recv_heartbeat_message(struct MessageUnknown* msg, ssize_t msg_size) {
    struct MessageHeartbeat *heartbeat = (struct MessageHeartbeat*)msg;

    heartbeat->type = htons(MESSAGE_HEARTBEAT_ACK_TYPE);
    heartbeat->length = ntohs(heartbeat->length);
    send_to_peer((const struct MessageUnknown *) heartbeat, msg_size);
#ifdef DEBUG
    fprintf(stderr, "Sent heartbeat ack\n");
#endif
}

void on_message(void* arg) {
    const ssize_t msg_size = recv(state.socket_fd, buffer, sizeof(buffer), 0);
    if (msg_size == -1) {
        perror("recv");
        return;
    }

    if (msg_size < sizeof(struct MessageUnknown)) {
#ifdef DEBUG
        fprintf(stderr, "Message too short\n");
#endif
        return;
    }

    struct MessageUnknown *message = (struct MessageUnknown*)buffer;
    message->length = ntohs(message->length);
    message->type = ntohs(message->type);

    if (msg_size < (sizeof(struct MessageUnknown) + message->length)) {
#ifdef DEBUG
        fprintf(stderr, "Message length does not match size\n");
#endif
        return;
    }

    switch (message->type) {
        case MESSAGE_COLUMN_TYPE:
            puts("Column type");
            break;
        case MESSAGE_COLUMN_ACK_TYPE:
            puts("Column ack");
            break;
        case MESSAGE_HEARTBEAT_TYPE:
            recv_heartbeat_message(message, msg_size);
            break;
        case MESSAGE_HEARTBEAT_ACK_TYPE:
            puts("Heartbeat ack");
            break;
        case MESSAGE_ERROR_TYPE:
            recv_error_message(message);
            break;
        default:
            send_error_message(ERROR_CODE_UNKNOWN_TYPE, "Message type unknown");
            break;
    }
}

void start_client(const uint16_t own_port, const uint32_t peer_ip, const uint16_t peer_port) {
    state.socket_fd = create_udp_socket(own_port);
    if (state.socket_fd == -1) return;

    if (connect_peer(peer_ip, peer_port) == -1) {
        close(state.socket_fd);
        return;
    }

    init_cblib();

    register_stdin_callback(on_stdin, NULL);
    register_fd_callback(state.socket_fd, on_message, NULL);

    handle_events();
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./client <own_port> <peer_ip> <peer_port>");
    }

    start_client(atoi(argv[1]), inet_addr(argv[2]), atoi(argv[3]));
}