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
#define MESSAGE_REGISTRATION_TYPE 6
#define MESSAGE_REGISTRATION_ACK_TYPE 7
#define MESSAGE_REGISTRATION_ERR_ACK_TYPE 8
#define MESSAGE_PEER_SELECT_TYPE 9
#define MESSAGE_PEER_SELECT_ACK_TYPE 10

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

struct MessageRegistration {
    uint16_t type;
    uint16_t length;
    uint32_t address;
    uint16_t port;
    uint16_t name_len;
    uint16_t pass_len;
    uint8_t data[];
};

struct MessageRegistrationAck {
    uint16_t type;
    uint16_t length;
};

struct MessageRegistrationErrorAck {
    uint16_t type;
    uint16_t length;
};

struct MessagePeerSelect {
    uint16_t type;
    uint16_t length;
    uint32_t address;
    uint16_t port;
    uint16_t start;
    uint8_t name[];
};

struct MessagePeerSelectAck {
    uint16_t type;
    uint16_t length;
};

#define HANDLE_MESSAGE_BUFFER_DEFINITION(NAME, SIZE) \
  uint8_t NAME##data[SIZE]; \
  struct HandleMessageBuffer NAME = { \
    .buff_size = SIZE, \
    .filled_size = 0, \
    .data = NAME##data \
  };

struct HandleMessageBuffer {
    size_t buff_size;
    size_t filled_size;
    uint8_t *data;
};

struct HandleMessageParams {
    int fd;
    struct HandleMessageBuffer *buffer;
};

void cb_message_column(void (*callback)(struct MessageColumn* msg, int fd));

void cb_message_column_ack(void (*callback)(struct MessageColumnAck* msg, int fd));

void cb_message_heartbeat(void (*callback)(struct MessageHeartbeat* msg, int fd));

void cb_message_heartbeat_ack(void (*callback)(struct MessageHeartbeat* msg, int fd));

void cb_message_error(void (*callback)(struct MessageError* msg, int fd));

void cb_message_registration(void (*callback)(struct MessageRegistration* msg, int fd));

void cb_message_registration_ack(void (*callback)(struct MessageRegistrationAck* msg, int fd));

void cb_message_registration_error_ack(void (*callback)(struct MessageRegistrationErrorAck* msg, int fd));

void cb_message_peer_select(void (*callback)(struct MessagePeerSelect* msg, int fd));

void cb_message_peer_select_ack(void (*callback)(struct MessagePeerSelectAck* msg, int fd));

void cb_message_default(void (*callback)(struct MessageAny* msg, int fd));

void cb_tcp_closed(void (*callback)(void* args, int fd));

void handle_tcp_message(void* arg);

void handle_udp_message(void* arg);

void send_message(struct MessageAny* msg, int socket_fd);

void send_error_message(const uint32_t error_code, const char* error_string, int fd);

#endif //PROTOCOL_H
