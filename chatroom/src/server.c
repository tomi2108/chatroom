#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/net/connection.h>
#include <commons/net/packet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define LOGIN 0
#define CONNECTED 1
#define DISCONNECTED 2
#define USER_ALREADY_TAKEN 3
#define MESSAGE_PACKET 4
#define PORT "8081"

typedef struct {
  int socket;
  char *name;
  pthread_t thread;
} Client;

t_log *logger;
int server;
t_list *clients = NULL;

int get_client_index(char *name) {
  t_list_iterator *iterator = list_iterator_create(clients);
  while (list_iterator_has_next(iterator)) {
    Client *next = list_iterator_next(iterator);
    if (strcmp(next->name, name) == 0) {
      list_iterator_destroy(iterator);
      return list_iterator_index(iterator);
    }
  }
  list_iterator_destroy(iterator);
  return -1;
}

void broadcast(char *from, t_packet *packet) {
  t_list_iterator *iterator = list_iterator_create(clients);
  while (list_iterator_has_next(iterator)) {
    Client *next = list_iterator_next(iterator);
    if (strcmp(next->name, from) == 0)
      continue;
    packet_send(packet, next->socket);
  }
  list_iterator_destroy(iterator);
}

void handle_disconnected(Client client) {
  log_info(logger, "%s disconnected", client.name);
  connection_close(client.socket);

  int index = get_client_index(client.name);
  list_remove_and_destroy_element(clients, index, &free);

  t_packet *packet = packet_create(DISCONNECTED);
  packet_add_string(packet, client.name);
  broadcast(client.name, packet);
  packet_destroy(packet);
}

void handle_message(Client client, char *message) {
  log_info(logger, "Message recieved from %s", client.name);

  t_packet *packet = packet_create(MESSAGE_PACKET);
  packet_add_string(packet, client.name);
  packet_add_string(packet, message);
  broadcast(client.name, packet);
  packet_destroy(packet);
}

void *handle_client_connection(void *args) {
  Client client = *(Client *)args;
  while (1) {
    t_packet *packet = packet_recieve(client.socket);

    if (!packet) {
      handle_disconnected(client);
      return NULL;
    }

    if (packet->type == MESSAGE_PACKET) {
      char *message = packet_read_string(packet);
      handle_message(client, message);
    }
  }
  return NULL;
}

void register_client(int socket, char *name) {
  log_info(logger, "%s connected", name);
  Client *c = malloc(sizeof(Client));
  c->name = strdup(name);
  c->socket = socket;
  pthread_create(&c->thread, NULL, &handle_client_connection, c);
  list_add(clients, c);

  t_packet *packet = packet_create(CONNECTED);
  packet_add_string(packet, name);
  broadcast(name, packet);
  packet_destroy(packet);
}

char *handle_login(t_packet *packet) {
  char *name = packet_read_string(packet);
  packet_destroy(packet);
  return name;
}

void handle_user_taken(int client, char *name) {
  log_info(logger, "Name %s already taken", name);
  t_packet *packet = packet_create(USER_ALREADY_TAKEN);
  packet_send(packet, client);
  packet_destroy(packet);
  connection_close(client);
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

    if (packet->type == LOGIN) {
      char *name = handle_login(packet);

      if (get_client_index(name) != -1) {
        handle_user_taken(client, name);
        continue;
      };

      register_client(client, name);
    }
  }

  connection_close(server);
  list_destroy_and_destroy_elements(clients, &free);
  return 0;
}
