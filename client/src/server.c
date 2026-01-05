#include <stdio.h>

#include "../src/socket_util.h"
#include "../src/protocol.h"
#include "../lib/cblib.h"

#define MAX_NAME_LEN 1 << 16

int socket_fd = -1;

struct ConnectionInfo {
  int registered;
  uint32_t address;
  uint16_t port;
  uint16_t name_len;
  char name[MAX_NAME_LEN];
  int conn_send;
  struct HandleMessageParams handle_message_params;
};

HANDLE_MESSAGE_BUFFER_DEFINITION(conn_1_buffer, 1<<16)
HANDLE_MESSAGE_BUFFER_DEFINITION(conn_2_buffer, 1<<16)

struct ConnectionInfo connections[2] = {
  {
    .registered = 0,
    .address = 0,
    .port = 0,
    .name_len = 0,
    .name = NULL,
    .conn_send = 0,
    .handle_message_params.fd = -1,
    .handle_message_params.buffer = &conn_1_buffer,
  }, {
    .address = 0,
    .registered = 0,
    .port = 0,
    .name_len = 0,
    .name = NULL,
    .conn_send = 0,
    .handle_message_params.fd = -1,
    .handle_message_params.buffer = &conn_2_buffer,
  }
};

struct ConnectionInfo* get_conn(int fd) {
  for (size_t i = 0; i < sizeof(connections); i++) {
    if(connections[i].handle_message_params.fd == fd) return &connections[i];
  }
  return NULL;
}

struct MessagePeerSelect* peer_select_msg(struct ConnectionInfo* conn_info) {
  size_t msg_size = 10 + conn_info->name_len;
  struct MessagePeerSelect* msg = malloc(msg_size);
  msg->type = MESSAGE_PEER_SELECT_TYPE;
  msg->length = msg_size;
  msg->address = conn_info->address;
  msg->port = conn_info->port;
  memcpy(msg->name, conn_info->name, conn_info->name_len);
  return msg;
}

void connect_clients(struct ConnectionInfo* conn_a, struct ConnectionInfo* conn_b) {
  if (conn_a->registered != 1 || conn_b->registered != 1) return;
  if (conn_a->conn_send == 1 || conn_b->conn_send == 1) return;

  struct MessagePeerSelect* conn_a_msg = peer_select_msg(conn_a);
  struct MessagePeerSelect* conn_b_msg = peer_select_msg(conn_b);

  send_message(conn_a_msg, conn_b->handle_message_params.fd);
  send_message(conn_b_msg, conn_a->handle_message_params.fd);

  // We are done. Just shutdown connection
  shutdown(conn_a->handle_message_params.fd, SHUT_WR);
  shutdown(conn_b->handle_message_params.fd, SHUT_WR);

  conn_a->conn_send = 1;
  conn_b->conn_send = 1;

  free(conn_a_msg);
  free(conn_b_msg);
}

int on_message_registration(struct MessageRegistration* msg, int fd) {
  struct ConnectionInfo* conn = get_conn(fd);
  if (conn == NULL) return;
  if (conn->registered == 1) return; // already registered
  
  // Basic PW check
  if (
    msg->name_len > MAX_NAME_LEN ||
    (msg->name_len != msg->pass_len) ||
    memcmp(msg->data, msg->data + msg->name_len, msg->name_len)
  ) {
    struct MessageRegistrationErrorAck response = {
      .type = MESSAGE_REGISTRATION_ERR_ACK_TYPE,
      .length = 0
    };
    send_message(&response, fd);
    return;
  }
 
  conn->registered = 1;
  conn->address = msg->address;
  conn->port = msg->port;
  memcpy(conn->name, msg->data, msg->name_len);

  struct MessageRegistrationAck response = {
    .type = MESSAGE_REGISTRATION_ACK_TYPE,
    .length = 0
  };
  send_message(&response, fd);

  // TODO: more than 2 fixed connections
  connect_clients(&connections[0], &connections[1]);
}

void close_connection(int fd) {
  struct ConnectionInfo* conn = get_conn(fd);
  if (conn == NULL) return;
  conn->handle_message_params.fd = -1;
  conn->registered = 0;
  conn->conn_send = 0;

  deregister_fd_callback(fd);
  close(fd);
}

int on_tcp_closed(void* args, int fd) {
  close_connection(fd);
}

int on_peer_select_ack(struct MessagePeerSelectAck* msg, int fd) {
  close_connection(fd);
}

void on_heartbeat_message(struct MessageHeartbeat* msg, int fd) {
    msg->type = MESSAGE_HEARTBEAT_ACK_TYPE;
    send_message((struct MessageAny *) msg, fd);
}

void on_default(struct MessageAny* msg, int fd) {
  send_error_message(ERROR_CODE_PROTOCOL_ERROR, "Unsupported message type", fd);
}

int on_acceptable(void *arg) {
  struct ConnectionInfo* conn = get_conn(-1); // Get any unused connection slot if availabe
  int fd = accept(socket_fd, NULL, NULL);
  if (fd == -1) return;

  conn->handle_message_params.fd = fd,
  conn->registered = 0;
  conn->conn_send = 0;

  register_fd_callback(fd, handle_tcp_message, &(conn->handle_message_params));
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    return 1;
  }

  cb_tcp_closed(on_tcp_closed);
  cb_message_peer_select_ack(on_peer_select_ack);
  cb_message_registration(on_message_registration);
  cb_message_heartbeat(on_heartbeat_message);
  cb_message_default(on_default);

  socket_fd = create_tcp_socket();
  if (socket_fd == -1) return 1;

  if (bind_listen_tcp_socket(socket_fd, atoi(argv[1]), INADDR_ANY) == -1) {
    close(socket_fd);
    return 1;
  }

  init_cblib();

  register_fd_callback(socket_fd, on_acceptable, NULL);

  handle_events();
}