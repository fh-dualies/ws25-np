//
// Created by dennis on 10/12/25.
//

#include "protocol.h"
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CB(NAME, TYPE)  static void (*g_##NAME)(TYPE* msg, int fd) = NULL; \
  void NAME(void (*callback)(TYPE* msg, int fd)) { \
    assert(callback != NULL); \
    g_##NAME = callback; \
  }

#define CALL_CB(NAME, MSG, FD) \
  if(g_##NAME != NULL) { \
    g_##NAME(MSG, FD); \
  } else if (g_cb_message_default) { \
    g_cb_message_default((struct MessageAny *) MSG, FD); \
  }

HANDLE_MESSAGE_BUFFER_DEFINITION(default_buffer, 1<<16)

CB(cb_message_column, struct MessageColumn)
CB(cb_message_column_ack, struct MessageColumnAck)
CB(cb_message_heartbeat, struct MessageHeartbeat)
CB(cb_message_heartbeat_ack, struct MessageHeartbeat)
CB(cb_message_error, struct MessageError)
CB(cb_message_registration, struct MessageRegistration)
CB(cb_message_registration_ack, struct MessageRegistrationAck)
CB(cb_message_registration_error_ack, struct MessageRegistrationErrorAck)
CB(cb_message_peer_select, struct MessagePeerSelect)
CB(cb_message_peer_select_ack, struct MessagePeerSelectAck)
CB(cb_tcp_closed, void)
CB(cb_message_default, struct MessageAny)

int padded_message_size(int size) {
  int remainder = size % 4;
  if (remainder == 0) {
    return size;
  } else {
    return size + (4 - remainder);
  }
}

void handle_incoming_message(struct MessageAny* msg, int fd) {
  switch (msg->type) {
    case MESSAGE_COLUMN_TYPE:
      if (msg->length != 6) break;
      struct MessageColumn* msg_column = (struct MessageColumn*)msg;
      msg_column->column = ntohs(msg_column->column);
      msg_column->sequence = ntohl(msg_column->sequence);
      CALL_CB(cb_message_column, msg_column, fd)
      break;
    case MESSAGE_COLUMN_ACK_TYPE:
      if (msg->length != 4) break;
      struct MessageColumnAck* msg_column_ack = (struct MessageColumnAck*)msg;
      msg_column_ack->sequence = ntohl(msg_column_ack->sequence);
      CALL_CB(cb_message_column_ack, msg_column_ack, fd)
      break;
    case MESSAGE_HEARTBEAT_TYPE:
      CALL_CB(cb_message_heartbeat, (struct MessageHeartbeat*)msg, fd)
      break;
    case MESSAGE_HEARTBEAT_ACK_TYPE:
      CALL_CB(cb_message_heartbeat_ack, (struct MessageHeartbeat*)msg, fd)
      break;
    case MESSAGE_ERROR_TYPE:
      if (msg->length < 4) break;
      struct MessageError* msg_error = (struct MessageError*)msg;
      msg_error->error_code = ntohl(msg_error->error_code);
      CALL_CB(cb_message_error, msg_error, fd)
      break;
    case MESSAGE_REGISTRATION_TYPE:
      if (msg->length < 10) break;
      struct MessageRegistration* msg_reg = (struct MessageRegistration*)msg;
      msg_reg->address = ntohl(msg_reg->address);
      msg_reg->port = ntohs(msg_reg->port);
      msg_reg->name_len = ntohs(msg_reg->name_len);
      msg_reg->pass_len = ntohs(msg_reg->pass_len);
      if (msg->length != 10 + msg_reg->name_len + msg_reg->pass_len) break;
      CALL_CB(cb_message_registration, msg_reg, fd)
      break;
    case MESSAGE_REGISTRATION_ACK_TYPE:
      if(msg->length != 0) break;
      CALL_CB(cb_message_registration_ack, (struct MessageRegistrationAck*)msg, fd)
      break;
    case MESSAGE_REGISTRATION_ERR_ACK_TYPE:
      if(msg->length != 0) break;
      CALL_CB(cb_message_registration_error_ack, (struct MessageRegistrationErrorAck*)msg, fd)
      break;
    case MESSAGE_PEER_SELECT_TYPE:
      if (msg->length < 10) break;
      struct MessagePeerSelect* msg_peer_select = (struct MessagePeerSelect*)msg;
      msg_peer_select->address = ntohl(msg_peer_select->address);
      msg_peer_select->port = ntohs(msg_peer_select->port);
      CALL_CB(cb_message_peer_select, msg_peer_select, fd)
      break;
    case MESSAGE_PEER_SELECT_ACK_TYPE:
      if(msg->length != 0) break;
      CALL_CB(cb_message_peer_select_ack, (struct MessagePeerSelectAck*)msg, fd)
      break;
    default:
      send_error_message(ERROR_CODE_UNKNOWN_TYPE, "Unkown message type", fd);
      break;
  }

  free(msg);
}


void handle_tcp_message(void* arg) {
  assert(arg != NULL);
  struct HandleMessageParams *params = arg;
  int fd = params->fd;
  struct HandleMessageBuffer *buffer = params->buffer;
  if(buffer == NULL) {
    buffer = &default_buffer;
  }

  const ssize_t recv_size = recv(fd, buffer->data + buffer->filled_size, buffer->buff_size - buffer->filled_size, 0);
  if (recv_size == -1) {
    perror("recv");
    return;
  }

  if (recv_size == 0) {
    g_cb_tcp_closed(NULL, fd);
    return;
  }

  buffer->filled_size += recv_size;

  while (buffer->filled_size >= 32) {
    struct MessageAny* message = (struct MessageAny*)buffer->data;
    uint16_t type = ntohs(message->type);
    uint16_t length = ntohs(message->length);

    size_t expected_size = 32 + length;
    size_t expected_size_padded = padded_message_size((int) expected_size);

    if (buffer->filled_size < expected_size_padded) {
      break;
    }

    struct MessageAny* msg_converted = malloc(expected_size);
    memcpy(msg_converted, buffer->data, expected_size);

    msg_converted->type = type;
    msg_converted->length = length;
    handle_incoming_message(msg_converted, fd);

    memmove(buffer->data, buffer->data + expected_size_padded, buffer->filled_size - expected_size_padded);
    buffer->filled_size -= expected_size_padded;
  }
}

void handle_udp_message(void* arg) {
  assert(arg != NULL);
  struct HandleMessageParams *params = arg;
  int fd = params->fd;
  struct HandleMessageBuffer *buffer = params->buffer;
  if(buffer == NULL) {
    buffer = &default_buffer;
  }

  assert(buffer->filled_size == 0); // There should be no content in the buffer for a UDP connection !

  const ssize_t recv_size = recv(fd, buffer->data, buffer->buff_size, 0);
  if (recv_size == -1) {
    perror("recv");
    return;
  }

  if (recv_size < 4) {
    fprintf(stderr, "Message size %ld smaller than minimum %d\n", recv_size, 4);
    return;
  }

  struct MessageAny* message = (struct MessageAny*)buffer->data;
  uint16_t type = ntohs(message->type);
  uint16_t length = ntohs(message->length);

  size_t expected_size = 4 + length;
  size_t expected_size_padded = padded_message_size((int) expected_size);
  if (recv_size != expected_size_padded) {
    fprintf(stderr, "Message size %ld does not match length %lu\n", recv_size, expected_size_padded);
    return;
  }

  struct MessageAny* msg_converted = malloc(expected_size);
  memcpy(msg_converted, buffer->data, expected_size);

  msg_converted->type = type;
  msg_converted->length = length;
  handle_incoming_message(msg_converted, fd);
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

void send_error_message(const uint32_t error_code, const char* error_string, int fd) {
    const size_t err_string_size = strlen(error_string);
    const size_t message_size = sizeof(struct MessageError) + err_string_size;
    struct MessageError *msg = malloc(message_size);
    msg->type = MESSAGE_ERROR_TYPE;
    msg->length = err_string_size + 4;
    msg->error_code = error_code;
    memcpy(msg->data, error_string, err_string_size);
    send_message((struct MessageAny*) msg, fd);
    free(msg);
}