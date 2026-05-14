#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/net/connection.h>
#include <commons/net/packet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define CONNECTION_PACKET 0
#define MESSAGE_PACKET 1
#define USER_ALREADY_TAKEN 2
#define PORT "8082"

typedef struct {
  int socket;
  char *name;
  pthread_t thread;
} Client;

t_log *logger;
int server;
t_list *clients = NULL;

void notify_message(char *client, char *message) {
  t_list_iterator *iterator = list_iterator_create(clients);
  while (list_iterator_has_next(iterator)) {
    Client *next = list_iterator_next(iterator);
    if (strcmp(next->name, client) == 0)
      continue;

    t_packet *packet = packet_create(MESSAGE_PACKET);
    packet_add_string(packet, client);
    packet_add_string(packet, message);
    packet_send(packet, next->socket);
    packet_destroy(packet);
  }
  list_iterator_destroy(iterator);
}

void *handle_client_connection(void *args) {
  Client client = *(Client *)args;
  t_packet *packet = packet_recieve(client.socket);

  if (packet->type == MESSAGE_PACKET) {
    char *message = packet_read_string(packet);
    log_info(logger, "Message recieved from %s", client.name);
    notify_message(client.name, message);
  }

  return NULL;
}

void register_client(int socket, char *name) {
  Client *c = malloc(sizeof(Client));
  c->name = strdup(name);
  c->socket = socket;
  pthread_create(&c->thread, NULL, &handle_client_connection, c);
  list_add(clients, c);
}

void notify_connection(char *client) {
  t_list_iterator *iterator = list_iterator_create(clients);
  while (list_iterator_has_next(iterator)) {
    Client *next = list_iterator_next(iterator);

    if (strcmp(next->name, client) == 0)
      continue;

    t_packet *connected = packet_create(CONNECTION_PACKET);
    packet_add_string(connected, client);
    packet_send(connected, next->socket);
    packet_destroy(connected);
  }
  list_iterator_destroy(iterator);
}

char *handle_connection_packet(t_packet *packet) {
  char *name = packet_read_string(packet);
  packet_destroy(packet);
  return name;
}

int is_name_taken(char *name) {
  t_list_iterator *iterator = list_iterator_create(clients);
  while (list_iterator_has_next(iterator)) {
    Client *next = list_iterator_next(iterator);
    if (strcmp(next->name, name) == 0)
      return 1;
  }
  list_iterator_destroy(iterator);
  return 0;
}

int main(void) {
  logger = log_create(NULL, "SERVER", true, LOG_LEVEL_INFO);
  clients = list_create();
  server = connection_create_server(PORT);
  log_info(logger, "Server listening in port %s", PORT);

  while (1) {
    log_info(logger, "Waiting for clients...");
    int client = connection_accept_client(server);
    t_packet *packet = packet_recieve(client);

    if (packet->type == CONNECTION_PACKET) {
      char *name = handle_connection_packet(packet);

      if (is_name_taken(name)) {
        t_packet *packet = packet_create(USER_ALREADY_TAKEN);
        packet_send(packet, client);
        packet_destroy(packet);
        continue;
      };

      log_info(logger, "%s connected", name);
      register_client(client, name);
      notify_connection(name);
    }
  }

  connection_close(server);
  list_destroy_and_destroy_elements(clients, &free);
  return 0;
}
