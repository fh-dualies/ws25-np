//
// Created by dennis on 10/12/25.
//

#include "protocol.h"
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char recv_buffer[1<<16];

static void (*g_cb_message_column)(struct MessageColumn* msg) = NULL;
static void (*g_cb_message_column_ack)(struct MessageColumnAck* msg) = NULL;
static void (*g_cb_message_heartbeat)(struct MessageHeartbeat* msg) = NULL;
static void (*g_cb_message_heartbeat_ack)(struct MessageHeartbeat* msg) = NULL;
static void (*g_cb_message_error)(struct MessageError* msg) = NULL;

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

int padded_message_size(int size) {
  int remainder = size % 4;
  if (remainder == 0) {
    return size;
  } else {
    return size + (4 - remainder);
  }
}

void handle_incoming_message(void* arg) {
  assert(arg != NULL);
  int fd = *((int*)arg);
  const ssize_t recv_size = recv(fd, recv_buffer, sizeof(recv_buffer), 0);
  if (recv_size == -1) {
    perror("recv");
    return;
  }
  
  if (recv_size < sizeof (struct MessageAny)) {
    fprintf(stderr, "Message size %ld smaller than minimum %lu\n", recv_size, sizeof(struct MessageAny));
    return;
  }

  struct MessageAny* message = (struct MessageAny*)recv_buffer;
  uint16_t type = ntohs(message->type);
  uint16_t length = ntohs(message->length);

  size_t expected_size = sizeof(struct MessageAny) + length;
  size_t expected_size_padded = padded_message_size(expected_size);
  if (recv_size != expected_size_padded) {
    fprintf(stderr, "Message size %ld does not match length %lu\n", recv_size, expected_size_padded);
    return;
  }

  struct MessageAny* msg_converted = malloc(expected_size);
  memcpy(msg_converted, recv_buffer, expected_size);
  msg_converted->type = type;
  msg_converted->length = length;

  switch (type) {
    case MESSAGE_COLUMN_TYPE:
      assert(g_cb_message_column != NULL);
      struct MessageColumn* msg_column = (struct MessageColumn*)msg_converted;
      msg_column->column = ntohs(msg_column->column);
      msg_column->sequence = ntohl(msg_column->sequence);
      g_cb_message_column(msg_column);
      break;
    case MESSAGE_COLUMN_ACK_TYPE:
      assert(g_cb_message_column_ack != NULL);
      struct MessageColumnAck* msg_column_ack = (struct MessageColumnAck*)msg_converted;
      msg_column_ack->sequence = ntohs(msg_column_ack->sequence);
      g_cb_message_column_ack(msg_column_ack);
      break;
    case MESSAGE_HEARTBEAT_TYPE:
      assert(g_cb_message_heartbeat != NULL);
      g_cb_message_heartbeat((struct MessageHeartbeat*)msg_converted);
      break;
    case MESSAGE_HEARTBEAT_ACK_TYPE:
      assert(g_cb_message_heartbeat_ack != NULL);
      g_cb_message_heartbeat_ack((struct MessageHeartbeat*)msg_converted);
      break;
    case MESSAGE_ERROR_TYPE:
      assert(g_cb_message_error != NULL);
      struct MessageError* msg_error = (struct MessageError*)msg_converted;
      msg_error->error_code = ntohl(msg_error->error_code);
      g_cb_message_error(msg_error);
      break;
    default:
      fprintf(stderr, "Unknown message type %u\n", type);
      break;
  }

  free(msg_converted);
}

void send_message(struct MessageAny* msg, int socket_fd) {
  assert(msg != NULL);
  size_t message_size = sizeof(struct MessageAny) + msg->length;
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
      msg_column_ack->sequence = htons(msg_column_ack->sequence);
    } break;
    case MESSAGE_ERROR_TYPE: {
      struct MessageError *msg_error = (struct MessageError *) buffer;
      msg_error->error_code = htonl(msg_error->error_code);
    } break;
    default:
      break;
  }

  if (send(socket_fd, buffer, padded_size, 0) == -1) {
    perror("send");
  }
  free(buffer);
}