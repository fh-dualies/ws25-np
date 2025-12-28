//
// Created by dennis on 10/12/25.
//

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <netinet/in.h>

#define MESSAGE_COLUMN_TYPE 1
#define MESSAGE_COLUMN_ACK_TYPE 2
#define MESSAGE_HEARTBEAT_TYPE 3
#define MESSAGE_HEARTBEAT_ACK_TYPE 4
#define MESSAGE_ERROR_TYPE 5

#define ERROR_CODE_UNKNOWN_TYPE 1
#define ERROR_CODE_PROTOCOL_ERROR 2

struct MessageAny {
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
    uint32_t sequence;
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

void cb_message_column(void (*callback)(struct MessageColumn* msg));

void cb_message_column_ack(void (*callback)(struct MessageColumnAck* msg));

void cb_message_heartbeat(void (*callback)(struct MessageHeartbeat* msg));

void cb_message_heartbeat_ack(void (*callback)(struct MessageHeartbeat* msg));

void cb_message_error(void (*callback)(struct MessageError* msg));

void handle_incoming_message(void* arg);

void send_message(struct MessageAny* msg, int socket_fd);

#endif //PROTOCOL_H
