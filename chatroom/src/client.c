#include <commons/net/connection.h>
#include <commons/net/packet.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define CONNECTION_PACKET 0
#define MESSAGE_PACKET 1
#define USER_ALREADY_TAKEN 2
#define PORT "8082"

int client;
char *name = NULL;

void handle_connection_packet(t_packet *packet) {
  char *other = packet_read_string(packet);
  printf("%s connected!\n", other);
  fflush(stdout);
}

void handle_message_packet(t_packet *packet) {
  char *other = packet_read_string(packet);
  char *message = packet_read_string(packet);
  printf("[%s]: %s\n", other, message);
  fflush(stdout);
}

void send_connection_packet() {
  t_packet *packet = packet_create(CONNECTION_PACKET);
  packet_add_string(packet, name);
  packet_send(packet, client);
  packet_destroy(packet);
}

void send_message_packet(char *message) {
  t_packet *packet = packet_create(MESSAGE_PACKET);
  packet_add_string(packet, message);
  packet_send(packet, client);
  packet_destroy(packet);
}

void *handle_input(void *args) {
  // sleep(4);
  // t_packet *packet = packet_create(MESSAGE_PACKET);
  // packet_add_string(packet, "Some message");
  // packet_send(packet, client);
  // packet_destroy(packet);
  // sleep(4);
  // packet = packet_create(MESSAGE_PACKET);
  // packet_add_string(packet, "Some other message");
  // packet_send(packet, client);
  // packet_destroy(packet);
  // sleep(4);
  // packet = packet_create(MESSAGE_PACKET);
  // packet_add_string(packet, "Another message");
  // packet_send(packet, client);
  // packet_destroy(packet);
  return NULL;
};

void login() {
  printf("Enter name to login:\n");
  size_t size;
  getline(&name, &size, stdin);
  send_connection_packet();
}

int main(void) {
  client = connection_create_client("localhost", PORT);

  login();

  // pthread_t input;
  // pthread_create(&input, NULL, &handle_input, NULL);

  while (1) {
    t_packet *packet = packet_recieve(client);
    switch (packet->type) {
    case USER_ALREADY_TAKEN:
      login();
      break;

    case CONNECTION_PACKET:
      handle_connection_packet(packet);
      break;

    case MESSAGE_PACKET:
      handle_message_packet(packet);
      break;

    default:
      break;
    }
    packet_destroy(packet);
  }

  // pthread_join(input, NULL);
  connection_close(client);
  return 0;
}
