#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "src/socket_util.h"
#include "src/protocol.h"
#include "lib/cblib.h"

char buffer[1<<16];
int socket_fd = -1;

void on_stdin(void* arg) {
    const ssize_t r = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (r == -1) {
        perror("read");
    } else {
        /* implement sending of data */
    }
}

void send_error_message(const uint32_t error_code, const char* error_string) {
    const size_t err_string_size = strlen(error_string);
    const size_t message_size = sizeof(struct MessageError) + err_string_size;
    struct MessageError *msg = malloc(message_size);
    msg->type = MESSAGE_ERROR_TYPE;
    msg->length = message_size - sizeof(struct MessageAny);
    msg->error_code = error_code;
    memcpy(msg->data, error_string, err_string_size);
    send_message((struct MessageAny*) msg, socket_fd);
    free(msg);

#ifdef DEBUG
    fprintf(stderr, "Send error message with code %d to Peer: %s", error_code, error_string);
#endif
}

void on_column_message(struct MessageColumn* msg) {
    printf("Received column message with sequence %d and column %d\n", ntohl(msg->sequence), ntohs(msg->column));
    // TODO: implement game logic here
}

void on_column_ack_message(struct MessageColumnAck* msg) {
    printf("Received column ack message with sequence %d\n", ntohl(msg->sequence));
    // TODO: implement ack logic here
}

void on_heartbeat_message(struct MessageHeartbeat* msg) {
    msg->type = MESSAGE_HEARTBEAT_ACK_TYPE;
    send_message((struct MessageAny *) msg, socket_fd);
#ifdef DEBUG
    fprintf(stderr, "Sent heartbeat ack\n");
#endif
}

void on_heartbeat_ack_message(struct MessageHeartbeat* msg) {
    printf("Received heartbeat ack\n");
    // TODO: implement heartbeat ack logic here
}

void on_error_message(struct MessageError* msg) {
    const size_t err_string_size = msg->length - 4;
    printf("Recieved error message with code %d: %.*s\n", msg->error_code, (int)err_string_size, (char *)msg->data);
}

void start_client(const uint16_t own_port, const uint32_t peer_ip, const uint16_t peer_port) {
    socket_fd = create_udp_socket(own_port);
    if (socket_fd == -1) return;

    if (connect_peer(socket_fd, peer_ip, peer_port) == -1) {
        close(socket_fd);
        return;
    }

    cb_message_column(on_column_message);
    cb_message_column_ack(on_column_ack_message);
    cb_message_heartbeat(on_heartbeat_message);
    cb_message_heartbeat_ack(on_heartbeat_ack_message);
    cb_message_error(on_error_message);

    init_cblib();

    register_stdin_callback(on_stdin, NULL);
    register_fd_callback(socket_fd, handle_incoming_message, &(socket_fd));

    handle_events();
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./client <own_port> <peer_ip> <peer_port>");
    }

    start_client(atoi(argv[1]), inet_addr(argv[2]), atoi(argv[3]));
}