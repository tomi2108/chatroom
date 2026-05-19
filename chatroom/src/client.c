#include <commons/net/connection.h>
#include <commons/net/packet.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define PORT "8081"

typedef enum {
  LOGIN,
  CONNECTED,
  DISCONNECTED,
  USER_ALREADY_TAKEN,
  MESSAGE_PACKET,
} Packet_type;

int client = -1;
char name[64] = {0};

int read_line(char *buffer, size_t size) {
  if (fgets(buffer, size, stdin) == NULL)
    return 0;
  buffer[strcspn(buffer, "\n")] = '\0';
  return 1;
}

void handle_disconnected(t_packet *packet) {
  const char *other = packet_read_string(packet);
  printf("%s left the room\n", other);
}

void handle_connected(t_packet *packet) {
  const char *other = packet_read_string(packet);
  printf("%s joined the room\n", other);
}

void handle_message(t_packet *packet) {
  const char *other = packet_read_string(packet);
  const char *message = packet_read_string(packet);
  printf("[%s]: %s\n", other, message);
}

void send_message(char *message) {
  t_packet *packet = packet_create(MESSAGE_PACKET);
  packet_add_string(packet, message);
  packet_send(packet, client);
  packet_destroy(packet);
}

void *handle_input(void *args) {
  (void)args;

  char message[500];

  while (1) {
    read_line(message, sizeof(message));
    send_message(message);
  }
  return NULL;
};

void login() {
  if (client != -1)
    connection_close(client);

  client = connection_create_client("localhost", PORT);
  printf("Enter name to login: ");

  read_line(name, sizeof(name));
  t_packet *packet = packet_create(LOGIN);
  packet_add_string(packet, name);
  packet_send(packet, client);
  packet_destroy(packet);
}

int main(void) {
  login();

  pthread_t input;
  pthread_create(&input, NULL, &handle_input, NULL);

  while (1) {
    t_packet *packet = packet_recieve(client);
    switch (packet->type) {

    case USER_ALREADY_TAKEN: {
      printf("Username already taken\n");
      login();
    } break;

    case CONNECTED:
      handle_connected(packet);
      break;

    case DISCONNECTED:
      handle_disconnected(packet);
      break;

    case MESSAGE_PACKET:
      handle_message(packet);
      break;
    }
    packet_destroy(packet);
  }

  pthread_join(input, NULL);
  connection_close(client);
  return 0;
}
