#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>

#include "../src/socket_util.h"
#include "../src/protocol.h"
#include "../lib/cblib.h"
#include "../lib/4clib.h"

// defines
#define DEFAULT_RETRANSMISSION_TIME 1000
#define HEARTBEAT_INTERVAL 5000 // in milliseconds

// global variables
char buffer[1<<16];
int socket_fd = -1;
time_t last_send_heartbeat = 0;
uint32_t next_sequence = 0;
enum {TURN, TURN_ACK, WAIT} state;

// structs
static struct timer *heartbeat_timer;
struct HeartbeatContent {
    time_t timestamp;
};

struct un_ack_column_message {
    uint32_t sequence;
    uint16_t column;
    struct timer *timer;
    unsigned int next_timeout;
    struct un_ack_column_message *next;
} un_ack_column_messages = {0, 0, NULL, 0, NULL};

void transmit_column_message(void* arg) {
    struct un_ack_column_message* un_ack = (struct un_ack_column_message *)arg;
    struct MessageColumn msg;
    msg.type = MESSAGE_COLUMN_TYPE;
    msg.length = sizeof(struct MessageColumn) - sizeof(struct MessageAny);
    msg.sequence = un_ack->sequence;
    msg.column = un_ack->column;

    send_message((struct MessageAny *)&msg, socket_fd);

    start_timer(un_ack->timer, un_ack->next_timeout);
    un_ack->next_timeout *= 2;
}

void send_column_message(const uint16_t column) {
    struct un_ack_column_message *queue_item = malloc(sizeof(struct un_ack_column_message));
    queue_item->sequence = next_sequence;
    queue_item->column = column;
    queue_item->timer = create_timer(transmit_column_message, queue_item, "");
    queue_item->next_timeout = DEFAULT_RETRANSMISSION_TIME;
    queue_item->next = NULL;

    // Enqueue unacknowledged message
    struct un_ack_column_message *prev = &un_ack_column_messages;
    for (; prev->next != NULL; prev = prev->next) {}
    prev->next = queue_item;

    transmit_column_message(queue_item);
    next_sequence++;
}

void on_stdin(void* arg) {
    const ssize_t r = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (r == -1) {
        perror("read");
        return;
    }

    int move = atoi(buffer);

    if (state != TURN) {
        printf("Not your turn yet!\n");
        fflush(stdout);
        return;
    }

    if (!valid_move(move)) {
        printf("Invalid Move!\n");
        fflush(stdout);
        return;
    }

    make_move(move, PLAYER_1);
    print_board();
    send_column_message(move);

    if (winner()) {
        printf("Wooow you won!!\n");
        fflush(stdout);
        exit(0);
    }

    state = TURN_ACK;
    printf("Wait for turn \n");
    fflush(stdout);
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

void send_heartbeat(void* arg) {
    struct MessageHeartbeat* msg = malloc(sizeof(struct MessageHeartbeat) + sizeof(struct HeartbeatContent));
    struct HeartbeatContent* content = (struct HeartbeatContent*) msg->data;
    msg->type = MESSAGE_HEARTBEAT_TYPE;
    msg->length = (sizeof(struct MessageHeartbeat) - sizeof(struct MessageAny)) + sizeof(struct HeartbeatContent);
    last_send_heartbeat = time(NULL);
    content->timestamp = last_send_heartbeat;
    send_message((struct MessageAny *) msg, socket_fd);
    start_timer(heartbeat_timer, HEARTBEAT_INTERVAL);
}

void on_column_message(struct MessageColumn* msg) {
    struct MessageColumnAck ack;
    ack.type = MESSAGE_COLUMN_ACK_TYPE;
    ack.sequence = msg->sequence;
    ack.length = sizeof(struct MessageColumnAck) - sizeof(struct MessageAny);

    send_message((struct MessageAny *)&ack, socket_fd);

    uint16_t move = msg->column;

    if (!valid_move(move)){
        printf("Invalid Move!\n");
        return;
    }

    make_move(move, PLAYER_2);
    print_board();

    if(winner()){
        printf("You Lost :(\n");
        exit(0);
    }

    state = TURN;
    printf("Your turn: \n");
}

void on_column_ack_message(struct MessageColumnAck* msg) {
    for (struct un_ack_column_message *prev = &un_ack_column_messages; prev->next != NULL; prev = prev->next) {
        struct un_ack_column_message *curr = prev->next;
        if (curr->sequence == msg->sequence) {
            state = WAIT;
            prev->next = curr->next;
            stop_timer(curr->timer);
            delete_timer(curr->timer);
            free(curr);
            return;
        }
    }
#ifdef DEBUG
    fprintf(stderr, "Received column ack message with sequence %d, but no packet is outstanding\n", msg->sequence);
#endif
}

void on_heartbeat_message(struct MessageHeartbeat* msg) {
    msg->type = MESSAGE_HEARTBEAT_ACK_TYPE;
    send_message((struct MessageAny *) msg, socket_fd);
#ifdef DEBUG
    fprintf(stderr, "Sent heartbeat ack\n");
#endif
}

void on_heartbeat_ack_message(struct MessageHeartbeat* msg) {
    if (msg->length < sizeof(struct HeartbeatContent)) {
        fprintf(stderr, "Received invalid heartbeat ack message\n");
        return;
    }
    struct HeartbeatContent* content = (struct HeartbeatContent*) msg->data;
    if (content->timestamp != last_send_heartbeat) {
        fprintf(stderr, "Received heartbeat ack with invalid timestamp\n");
        return;
    }
#ifdef DEBUG
    const time_t rtt = time(NULL) - content->timestamp;
    printf("Received heartbeat ack, RTT: %ld seconds\n", rtt);
#endif
}

void on_error_message(struct MessageError* msg) {
    const size_t err_string_size = msg->length - 4;
    printf("Recieved error message with code %d: %.*s\n", msg->error_code, (int)err_string_size, (char *)msg->data);
}

void start_client(const uint16_t own_port, const uint32_t peer_ip, const uint16_t peer_port, const uint16_t start) {
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
    init_4clib();
    print_board();

    heartbeat_timer = create_timer(send_heartbeat, NULL, "Heartbeat Timer");

    register_stdin_callback(on_stdin, NULL);
    register_fd_callback(socket_fd, handle_incoming_message, &(socket_fd));
    start_timer(heartbeat_timer, HEARTBEAT_INTERVAL);

    if (start) {
        state = TURN;
        printf("Your turn: \n");
    } else {
        state = WAIT;
        printf("Wait for turn \n");
    }

    handle_events();
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: ./client <own_port> <peer_ip> <peer_port> <start?>");
    }

    start_client(atoi(argv[1]), inet_addr(argv[2]), atoi(argv[3]), atoi(argv[4]));
}
