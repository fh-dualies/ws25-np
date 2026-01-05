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
#define HEARTBEAT_WARNING_MESSAGE_TIMEOUT 30000 // in milliseconds

// global variables
char stdin_buffer[1<<16];
int socket_fd = -1;
time_t last_send_heartbeat = 0;
time_t last_received_heartbeat_time = 0;
uint32_t next_sequence = 0;
enum {TURN, TURN_ACK, WAIT} state;
uint32_t next_expected_sequence = 0;

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
    msg.length = 6;
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
    const ssize_t r = read(STDIN_FILENO, stdin_buffer, sizeof(stdin_buffer));
    if (r == -1) {
        perror("read");
        return;
    }

    int move = atoi(stdin_buffer);

    if (state != TURN) {
        printf("Not your turn yet!\n");
        return;
    }

    if (!valid_move(move)) {
        printf("Invalid Move!\n");
        return;
    }

    make_move(move, PLAYER_1);
    print_board();
    send_column_message(move);

    if (winner()) {
        printf("Wooow you won!!\n");
        exit(0);
    }

    state = TURN_ACK;
    printf("Wait for turn \n");
}

void send_heartbeat(void* arg) {
    struct MessageHeartbeat* msg = malloc(sizeof(struct MessageHeartbeat) + sizeof(struct HeartbeatContent));
    struct HeartbeatContent* content = (struct HeartbeatContent*) msg->data;
    msg->type = MESSAGE_HEARTBEAT_TYPE;
    msg->length = sizeof(struct HeartbeatContent);
    last_send_heartbeat = time(NULL);
    content->timestamp = last_send_heartbeat;
    send_message((struct MessageAny *) msg, socket_fd);
    start_timer(heartbeat_timer, HEARTBEAT_INTERVAL);

    if (last_received_heartbeat_time - time(NULL) > HEARTBEAT_WARNING_MESSAGE_TIMEOUT) {
        printf("No heartbeat recived since %d seconds. Maybe the other peer disconnected?\n", HEARTBEAT_WARNING_MESSAGE_TIMEOUT / 1000);
    }
}

void on_column_message(struct MessageColumn* msg, int fd) {
    struct MessageColumnAck ack;
    ack.type = MESSAGE_COLUMN_ACK_TYPE;
    ack.length = 4;
    if (msg->sequence > next_expected_sequence) {
        ack.sequence = next_expected_sequence - 1;
        send_message((struct MessageAny *)&ack, socket_fd);
        return;
    } else if (msg->sequence < next_expected_sequence) {
#ifdef DEBUG
        fprintf(stderr, "Received old column message. Ignoring!\n");
#endif
        return;
    }

    next_expected_sequence++;
    ack.sequence = msg->sequence;
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

void on_column_ack_message(struct MessageColumnAck* msg, int fd) {
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

void on_heartbeat_message(struct MessageHeartbeat* msg, int fd) {
    msg->type = MESSAGE_HEARTBEAT_ACK_TYPE;
    send_message((struct MessageAny *) msg, socket_fd);
#ifdef DEBUG
    fprintf(stderr, "Sent heartbeat ack\n");
#endif
}

void on_heartbeat_ack_message(struct MessageHeartbeat* msg, int fd) {
    if (msg->length < sizeof(struct HeartbeatContent)) {
        fprintf(stderr, "Received invalid heartbeat ack message\n");
        return;
    }

    last_received_heartbeat_time = time(NULL);

#ifdef DEBUG
    struct HeartbeatContent* content = (struct HeartbeatContent*) msg->data;
    if (content->timestamp == last_send_heartbeat) {
        const time_t rtt = time(NULL) - content->timestamp;
        printf("Received heartbeat ack, RTT: %ld seconds\n", rtt);
    } else {
        printf("Received heartbeat ack, RTT: N/A\n");
    }
#endif
}

void on_error_message(struct MessageError* msg, int fd) {
    const size_t err_string_size = msg->length - 4;
    printf("Received error message with code %d: %.*s\n", msg->error_code, (int)err_string_size, (char *)msg->data);
}

void start_client(const uint16_t own_port, const uint32_t peer_ip, const uint16_t peer_port, const uint16_t start) {
    socket_fd = create_udp_socket(own_port);
    if (socket_fd == -1) return;

    if (connect_socket(socket_fd, peer_ip, peer_port) == -1) {
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
    struct HandleMessageParams handle_upd_params = {
        .fd = socket_fd,
        .buffer = NULL // Use default buffer
    };
    register_fd_callback(socket_fd, handle_udp_message, &handle_upd_params);
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
        fprintf(stderr, "Usage: ./client <own_port> <peer_ip> <peer_port> <start (0|1)>\n");
    } else {
        start_client(atoi(argv[1]), inet_addr(argv[2]), atoi(argv[3]), atoi(argv[4]));
    }
}
