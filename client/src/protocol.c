//
// Created by dennis on 10/12/25.
//

#include "protocol.h"
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char udp_recv_buffer[1<<16];
char tcp_recv_buffer[1<<16];
size_t tcp_buffer_amount = 0;

static void (*g_cb_message_column)(struct MessageColumn* msg) = NULL;
static void (*g_cb_message_column_ack)(struct MessageColumnAck* msg) = NULL;
static void (*g_cb_message_heartbeat)(struct MessageHeartbeat* msg) = NULL;
static void (*g_cb_message_heartbeat_ack)(struct MessageHeartbeat* msg) = NULL;
static void (*g_cb_message_error)(struct MessageError* msg) = NULL;
static void (*g_cb_message_registration)(struct MessageRegistration* msg) = NULL;
static void (*g_cb_message_registration_ack)(struct MessageRegistrationAck* msg) = NULL;
static void (*g_cb_message_registration_error_ack)(struct MessageRegistrationErrorAck* msg) = NULL;
static void (*g_cb_message_peer_select)(struct MessagePeerSelect* msg) = NULL;
static void (*g_cb_message_peer_select_ack)(struct MessagePeerSelectAck* msg) = NULL;


void cb_message_column(void (*callback)(struct MessageColumn* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_column == NULL);
  g_cb_message_column = callback;
}

void cb_message_column_ack(void (*callback)(struct MessageColumnAck* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_column_ack == NULL);
  g_cb_message_column_ack = callback;
}

void cb_message_heartbeat(void (*callback)(struct MessageHeartbeat* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_heartbeat == NULL);
  g_cb_message_heartbeat = callback;
}

void cb_message_heartbeat_ack(void (*callback)(struct MessageHeartbeat* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_heartbeat_ack == NULL);
  g_cb_message_heartbeat_ack = callback;
}

void cb_message_error(void (*callback)(struct MessageError* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_error == NULL);
  g_cb_message_error = callback;
}

void cb_message_registration(void (*callback)(struct MessageRegistration* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_registration == NULL);
  g_cb_message_registration = callback;
}

void cb_message_registration_ack(void (*callback)(struct MessageRegistrationAck* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_registration_ack == NULL);
  g_cb_message_registration_ack = callback;
}

void cb_message_registration_error_ack(void (*callback)(struct MessageRegistrationErrorAck* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_registration_error_ack == NULL);
  g_cb_message_registration_error_ack = callback;
}

void cb_message_peer_select(void (*callback)(struct MessagePeerSelect* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_peer_select == NULL);
  g_cb_message_peer_select = callback;
}

void cb_message_peer_select_ack(void (*callback)(struct MessagePeerSelectAck* msg)) {
  assert(callback != NULL);
  assert(g_cb_message_peer_select_ack == NULL);
  g_cb_message_peer_select_ack = callback;
}

int padded_message_size(int size) {
  int remainder = size % 4;
  if (remainder == 0) {
    return size;
  } else {
    return size + (4 - remainder);
  }
}

void handle_tcp_message(void* arg) {
  assert(arg != NULL);
  int fd = *((int*)arg);

  const ssize_t recv_size = recv(fd, tcp_recv_buffer + tcp_buffer_amount, sizeof(tcp_recv_buffer) - tcp_buffer_amount, 0);
  if (recv_size == -1) {
    perror("recv");
    return;
  }

  tcp_buffer_amount += recv_size;

  while (tcp_buffer_amount >= 32) {
    struct MessageAny* message = (struct MessageAny*)tcp_recv_buffer;
    uint16_t type = ntohs(message->type);
    uint16_t length = ntohs(message->length);

    size_t expected_size = 32 + length;
    size_t expected_size_padded = padded_message_size((int) expected_size);

    if (tcp_buffer_amount < expected_size_padded) {
      break;
    }

    struct MessageAny* msg_converted = malloc(expected_size);
    memcpy(msg_converted, tcp_recv_buffer, expected_size);

    msg_converted->type = type;
    msg_converted->length = length;
    handle_incoming_message(msg_converted);

    memmove(tcp_recv_buffer, tcp_recv_buffer + expected_size_padded, tcp_buffer_amount - expected_size_padded);
    tcp_buffer_amount -= expected_size_padded;
  }
}

void handle_udp_message(void* arg) {
  assert(arg != NULL);
  int fd = *((int*)arg);

  const ssize_t recv_size = recv(fd, udp_recv_buffer, sizeof(udp_recv_buffer), 0);
  if (recv_size == -1) {
    perror("recv");
    return;
  }

  if (recv_size < 32) {
    fprintf(stderr, "Message size %ld smaller than minimum %lu\n", recv_size, 32);
    return;
  }

  struct MessageAny* message = (struct MessageAny*)udp_recv_buffer;
  uint16_t type = ntohs(message->type);
  uint16_t length = ntohs(message->length);

  size_t expected_size = 32 + length;
  size_t expected_size_padded = padded_message_size((int) expected_size);
  if (recv_size != expected_size_padded) {
    fprintf(stderr, "Message size %ld does not match length %lu\n", recv_size, expected_size_padded);
    return;
  }

  struct MessageAny* msg_converted = malloc(expected_size);
  memcpy(msg_converted, udp_recv_buffer, expected_size);

  msg_converted->type = type;
  msg_converted->length = length;
  handle_incoming_message(msg_converted);
}

void handle_incoming_message(struct MessageAny* msg ) {
  switch (msg->type) {
    case MESSAGE_COLUMN_TYPE:
      assert(g_cb_message_column != NULL);
      if (msg->length != 6) {
        fprintf(stderr, "Invalid MESSAGE_COLUMN_TYPE length %u\n", msg->length);
        break;
      }
      struct MessageColumn* msg_column = (struct MessageColumn*)msg;
      msg_column->column = ntohs(msg_column->column);
      msg_column->sequence = ntohl(msg_column->sequence);
      g_cb_message_column(msg_column);
      break;
    case MESSAGE_COLUMN_ACK_TYPE:
      assert(g_cb_message_column_ack != NULL);
      if (msg->length != 4) {
        fprintf(stderr, "Invalid MESSAGE_COLUMN_ACK_TYPE length %u\n", msg->length);
        break;
      }
      struct MessageColumnAck* msg_column_ack = (struct MessageColumnAck*)msg;
      msg_column_ack->sequence = ntohl(msg_column_ack->sequence);
      g_cb_message_column_ack(msg_column_ack);
      break;
    case MESSAGE_HEARTBEAT_TYPE:
      assert(g_cb_message_heartbeat != NULL);
      g_cb_message_heartbeat((struct MessageHeartbeat*)msg);
      break;
    case MESSAGE_HEARTBEAT_ACK_TYPE:
      assert(g_cb_message_heartbeat_ack != NULL);
      g_cb_message_heartbeat_ack((struct MessageHeartbeat*)msg);
      break;
    case MESSAGE_ERROR_TYPE:
      assert(g_cb_message_error != NULL);
      if (msg->length < 4) {
        fprintf(stderr, "Invalid MESSAGE_ERROR_TYPE length %u\n", msg->length);
        break;
      }
      struct MessageError* msg_error = (struct MessageError*)msg;
      msg_error->error_code = ntohl(msg_error->error_code);
      g_cb_message_error(msg_error);
      break;
    case MESSAGE_REGISTRATION_TYPE:
      assert(g_cb_message_registration != NULL);
      if (msg->length < 10) {
        fprintf(stderr, "Invalid MESSAGE_ERROR_TYPE length %u\n", msg->length);
        break;
      }
      struct MessageRegistration* msg_reg = (struct MessageRegistration*)msg;
      msg_reg->address = ntohl(msg_reg->address);
      msg_reg->port = ntohs(msg_reg->port);
      msg_reg->name_len = ntohs(msg_reg->name_len);
      msg_reg->pass_len = ntohs(msg_reg->pass_len);
      if (msg->length != 10 + msg_reg->name_len + msg_reg->pass_len) {
        fprintf(stderr, "Invalid MESSAGE_ERROR_TYPE length %u\n", msg->length);
        break;
      }
      g_cb_message_registration(msg_reg);
      break;
    case MESSAGE_REGISTRATION_ACK_TYPE:
      assert(g_cb_message_registration_ack != NULL);
      g_cb_message_registration_ack((struct MessageRegistrationAck*)msg);
      break;
    case MESSAGE_REGISTRATION_ERR_ACK_TYPE:
      assert(g_cb_message_registration_error_ack != NULL);
      g_cb_message_registration_error_ack((struct MessageRegistrationErrorAck*)msg);
      break;
    case MESSAGE_PEER_SELECT_TYPE:
      assert(g_cb_message_peer_select != NULL);
      if (msg->length < 10) {
        fprintf(stderr, "Invalid MESSAGE_ERROR_TYPE length %u\n", msg->length);
        break;
      }
      struct MessagePeerSelect* msg_peer_select = (struct MessagePeerSelect*)msg;
      msg_peer_select->address = ntohl(msg_peer_select->address);
      msg_peer_select->port = ntohs(msg_peer_select->port);
      g_cb_message_peer_select(msg_peer_select);
      break;
    case MESSAGE_PEER_SELECT_ACK_TYPE:
      assert(g_cb_message_peer_select_ack != NULL);
      g_cb_message_peer_select_ack((struct MessagePeerSelectAck*)msg);
      break;
    default:
      fprintf(stderr, "Unknown message type %u\n", msg->type);
      break;
  }

  free(msg);
}

void send_message(struct MessageAny* msg, int socket_fd) {
  assert(msg != NULL);
  uint16_t message_size = 4 + msg->length;
  size_t padded_size = padded_message_size(message_size);
  struct MessageAny* buffer = malloc(padded_size);
  memset(buffer, 0, padded_size);
  memcpy(buffer, msg, message_size);

  buffer->type = htons(msg->type);
  buffer->length = htons(msg->length);

  switch (msg->type) {
    case MESSAGE_COLUMN_TYPE: {
      struct MessageColumn *msg_column = (struct MessageColumn *) buffer;
      msg_column->column = htons(msg_column->column);
      msg_column->sequence = htonl(msg_column->sequence);
    } break;
    case MESSAGE_COLUMN_ACK_TYPE: {
      struct MessageColumnAck *msg_column_ack = (struct MessageColumnAck *) buffer;
      msg_column_ack->sequence = htonl(msg_column_ack->sequence);
    } break;
    case MESSAGE_ERROR_TYPE: {
      struct MessageError *msg_error = (struct MessageError *) buffer;
      msg_error->error_code = htonl(msg_error->error_code);
    } break;
    case MESSAGE_REGISTRATION_TYPE: {
      struct MessageRegistration *msg_registration = (struct MessageRegistration *) buffer;
      msg_registration->address = htonl(msg_registration->address);
      msg_registration->port = htons(msg_registration->port);
      msg_registration->name_len = htons(msg_registration->name_len);
      msg_registration->pass_len = htons(msg_registration->pass_len);
    } break;
    case MESSAGE_PEER_SELECT_TYPE: {
      struct MessagePeerSelect *msg_peer_select = (struct MessagePeerSelect *) buffer;
      msg_peer_select->address = htonl(msg_peer_select->address);
      msg_peer_select->port = htons(msg_peer_select->port);
    } break;
    default:
      break;
  }

  if (send(socket_fd, buffer, padded_size, 0) == -1) {
    perror("send");
  }
  free(buffer);
}
