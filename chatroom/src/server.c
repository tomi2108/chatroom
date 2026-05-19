#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/net/connection.h>
#include <commons/net/packet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define PORT "8081"

typedef enum {
  LOGIN,
  CONNECTED,
  DISCONNECTED,
  USER_ALREADY_TAKEN,
  MESSAGE_PACKET,
} Packet_type;

typedef struct {
  int socket;
  char *name;
  pthread_t thread;
} Client;

t_log *logger;
int server;
t_list *clients = NULL;

int get_client_index(const char *name) {
  t_list_iterator *iterator = list_iterator_create(clients);
  while (list_iterator_has_next(iterator)) {
    const Client *next = list_iterator_next(iterator);
    if (strcmp(next->name, name) == 0) {
      list_iterator_destroy(iterator);
      return list_iterator_index(iterator);
    }
  }
  list_iterator_destroy(iterator);
  return -1;
}

void broadcast(const char *from, t_packet *packet) {
  t_list_iterator *iterator = list_iterator_create(clients);
  while (list_iterator_has_next(iterator)) {
    Client *next = list_iterator_next(iterator);
    if (strcmp(next->name, from) == 0)
      continue;
    packet_send(packet, next->socket);
  }
  list_iterator_destroy(iterator);
}

void unregister_client(Client client) {
  connection_close(client.socket);
  int index = get_client_index(client.name);
  list_remove_and_destroy_element(clients, index, &free);
}

void handle_disconnected(Client client) {
  log_info(logger, "%s disconnected", client.name);
  unregister_client(client);

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

char *handle_login(t_packet *packet) {
  char *name = packet_read_string(packet);
  packet_destroy(packet);
  return name;
}

void handle_user_taken(Client client) {
  log_info(logger, "Name %s already taken", client.name);
  t_packet *packet = packet_create(USER_ALREADY_TAKEN);
  packet_send(packet, client.socket);
  packet_destroy(packet);
  connection_close(client.socket);
}

void handle_connected(Client client) {
  t_packet *packet = packet_create(CONNECTED);
  packet_add_string(packet, client.name);
  broadcast(client.name, packet);
  packet_destroy(packet);
}

void *handle_client_connection(void *args) {
  Client client = *(Client *)args;
  t_packet *login_packet = packet_recieve(client.socket);

  if (login_packet->type == LOGIN) {
    client.name = handle_login(login_packet);
    if (get_client_index(client.name) != -1) {
      handle_user_taken(client);
      return NULL;
    };
  } else {
    unregister_client(client);
    return NULL;
  }

  list_add(clients, &client);
  log_info(logger, "%s connected", client.name);
  handle_connected(client);

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

void register_client(int socket) {
  Client *c = malloc(sizeof(Client));
  if (!c)
    return;
  c->socket = socket;
  pthread_create(&c->thread, NULL, &handle_client_connection, c);
}

int main(void) {
  logger = log_create(NULL, "SERVER", true, LOG_LEVEL_INFO);
  clients = list_create();
  server = connection_create_server(PORT);
  log_info(logger, "Server listening in port %s", PORT);

  while (1) {
    log_info(logger, "Waiting for clients...");
    int client = connection_accept_client(server);
    register_client(client);
  }

  connection_close(server);
  list_destroy_and_destroy_elements(clients, &free);
  return 0;
}
