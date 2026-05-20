#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/net/connection.h>
#include <commons/net/packet.h>
#include <commons/net/status.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define PORT "8080"
#define AMOUNT_OF_ROOMS 3

typedef enum {
  LOGIN,
  USER_ALREADY_TAKEN,

  JOINED,
  LEFT,

  ROOM_LIST,
  JOIN_ROOM,
  LEAVE_ROOM,

  MESSAGE,
} Packet_type;

typedef struct {
  int socket;
  char *name;
  pthread_t thread;
  u_int32_t room;
} Client;

t_log *logger;
int server;
t_list *clients = NULL;

int get_client_index(const char *name) {
  for (int i = 0; i < clients->elements_count; i++) {
    const Client *next = list_get(clients, i);
    if (strcmp(next->name, name) == 0)
      return i;
  }
  return -1;
}

t_list *get_clients_in_room(u_int32_t room) {
  t_list *res = list_create();
  for (int i = 0; i < clients->elements_count; i++) {
    Client *next = list_get(clients, i);
    if (next->room == room)
      list_add(res, next);
  }
  return res;
}

void broadcast(Client from, t_packet *packet) {
  t_list_iterator *iterator = list_iterator_create(clients);
  while (list_iterator_has_next(iterator)) {
    Client *next = list_iterator_next(iterator);

    if (next->room != from.room)
      continue;

    if (strcmp(next->name, from.name) == 0)
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

  t_packet *packet = packet_create(LEFT);
  packet_add_string(packet, client.name);
  broadcast(client, packet);
  packet_destroy(packet);
}

void handle_join(Client *client, u_int32_t room) {
  log_info(logger, "%s joined room %u", client->name, client->room);

  t_packet *res = status_pack(OK);

  // TODO: get
  t_list *clients_in_same_room = get_clients_in_room(room);
  int count = clients_in_same_room->elements_count;
  packet_add_uint32(res, count);
  for (int i = 0; i < count; i++) {
    const Client *next = list_get(clients_in_same_room, i);
    packet_add_string(res, next->name);
  }

  list_destroy(clients_in_same_room);
  packet_send(res, client->socket);
  packet_destroy(res);

  client->room = room;

  t_packet *packet = packet_create(JOINED);
  packet_add_string(packet, client->name);
  broadcast(*client, packet);
  packet_destroy(packet);
}

void handle_left(Client *client) {
  log_info(logger, "%s left room %u", client->name, client->room);
  t_packet *packet = packet_create(LEFT);
  packet_add_string(packet, client->name);
  broadcast(*client, packet);
  packet_destroy(packet);

  client->room = 0;
}

void handle_message(Client client, char *message) {
  log_info(logger, "Message recieved from %s", client.name);

  t_packet *packet = packet_create(MESSAGE);
  packet_add_string(packet, client.name);
  packet_add_string(packet, message);
  broadcast(client, packet);
  packet_destroy(packet);
}

void handle_user_taken(Client client) {
  log_info(logger, "Name %s already taken", client.name);
  t_packet *packet = packet_create(USER_ALREADY_TAKEN);
  packet_send(packet, client.socket);
  packet_destroy(packet);
  connection_close(client.socket);
}

void send_rooms(Client client) {
  t_packet *packet = packet_create(AMOUNT_OF_ROOMS);
  packet_add_uint32(packet, AMOUNT_OF_ROOMS);
  packet_send(packet, client.socket);
  packet_destroy(packet);
}

void handle_login(Client *client) {
  t_packet *login_packet = packet_recieve(client->socket);
  if (login_packet->type == LOGIN) {
    char *name = packet_read_string(login_packet);

    if (get_client_index(name) != -1)
      handle_user_taken(*client);

    client->name = name;
    list_add(clients, client);
    log_info(logger, "%s connected", client->name);

    t_packet *res = status_pack(OK);
    packet_send(res, client->socket);
    packet_destroy(res);

    send_rooms(*client);
  } else
    unregister_client(*client);

  packet_destroy(login_packet);
}

void *handle_client_connection(void *args) {
  Client *client = (Client *)args;
  handle_login(client);

  while (1) {
    t_packet *packet = packet_recieve(client->socket);
    if (!packet) {
      handle_disconnected(*client);
      return NULL;
    }

    switch (packet->type) {
    case LEAVE_ROOM: {
      handle_left(client);
      break;
    }

    case JOIN_ROOM: {
      u_int32_t room = packet_read_uint32(packet);
      if (room > AMOUNT_OF_ROOMS) {
        // TODO: probably send an error
        handle_disconnected(*client);
        return NULL;
      }
      handle_join(client, room);
      break;
    }

    case MESSAGE: {
      char *message = packet_read_string(packet);
      handle_message(*client, message);
      break;
    }
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
