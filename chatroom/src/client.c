#include <commons/collections/list.h>
#include <commons/net/connection.h>
#include <commons/net/packet.h>
#include <commons/net/status.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "8080"

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

int client = -1;
bool quit = false;
char name[64] = {0};
u_int32_t rooms = 0;

int read_line(char *buffer, size_t size) {
  if (fgets(buffer, size, stdin) == NULL)
    return 0;
  buffer[strcspn(buffer, "\n")] = '\0';
  return 1;
}

void handle_left(t_packet *packet) {
  const char *other = packet_read_string(packet);
  printf("%s left the room\n", other);
}

void handle_joined(t_packet *packet) {
  const char *other = packet_read_string(packet);
  printf("%s joined the room\n", other);
}

void handle_message(t_packet *packet) {
  const char *other = packet_read_string(packet);
  const char *message = packet_read_string(packet);
  printf("[%s]: %s\n", other, message);
}

void send_message(char *message) {
  t_packet *packet = packet_create(MESSAGE);
  packet_add_string(packet, message);
  packet_send(packet, client);
  packet_destroy(packet);
}

void quit_program() {
  quit = 1;
  connection_close(client);
}

bool login() {
  if (client != -1)
    connection_close(client);

  printf("Enter name to login: ");
  read_line(name, sizeof(name));

  client = connection_create_client("localhost", PORT);

  t_packet *packet = packet_create(LOGIN);
  packet_add_string(packet, name);
  packet_send(packet, client);
  packet_destroy(packet);

  t_packet *res = packet_recieve(client);
  if (status_unpack(res) == OK) {
    packet_destroy(res);
    return true;
  }

  if (packet->type == USER_ALREADY_TAKEN) {
    printf("Username already taken\n");
  }

  packet_destroy(res);
  return false;
}

u_int32_t get_rooms(t_packet *rooms_packet) {
  u_int32_t length = packet_read_uint32(rooms_packet);
  return length;
}

t_list *join_room(u_int32_t room) {
  t_packet *packet = packet_create(JOIN_ROOM);
  packet_add_uint32(packet, room);
  packet_send(packet, client);
  packet_destroy(packet);
  t_packet *res = packet_recieve(client);

  if (status_unpack(res) != OK) {
    // TODO: do something, prompt again ?
  }

  t_list *people_in_room = list_create();
  u_int32_t count = packet_read_uint32(res);
  for (u_int32_t i = 0; i < count; i++) {
    list_add(people_in_room, packet_read_string(res));
  }
  return people_in_room;
}

void leave_room() {
  t_packet *packet = packet_create(LEAVE_ROOM);
  packet_send(packet, client);
  packet_destroy(packet);
}

void prompt_rooms() {
  char input[64];

  printf("Available rooms:\n");
  for (u_int32_t i = 1; i <= rooms; i++) {
    printf("%d.\n", i);
  }
  printf("Join: ");

  read_line(input, sizeof(input));
  u_int32_t room = strtoul(input, NULL, 10);
  t_list *people_in_room = join_room(room);
  int count = people_in_room->elements_count;

  printf("--------------- ROOM %u ---------------\n", room);
  if (count > 0) {
    printf("People in room: ");
    for (u_int32_t i = 0; i < count; i++) {
      const char *other = list_get(people_in_room, i);
      printf("%s, ", other);
    }
    printf("\n");
  }
  list_destroy(people_in_room);
};

void execute_command(const char *cmd) {
  if (strcmp(cmd, "/quit") == 0) {
    quit_program();
  };

  if (strcmp(cmd, "/leave") == 0) {
    leave_room();
    prompt_rooms();
  }
}

void *handle_input(void *args) {
  (void)args;

  char message[500];

  while (!quit) {
    read_line(message, sizeof(message));
    if (message[0] == '/') {
      execute_command(message);
    } else
      send_message(message);
  }

  return NULL;
};

int main(void) {
  bool logged_in = false;
  while (!logged_in) {
    logged_in = login();
  }

  t_packet *rooms_packet = packet_recieve(client);
  rooms = get_rooms(rooms_packet);
  prompt_rooms();

  pthread_t input;
  pthread_create(&input, NULL, &handle_input, NULL);

  while (!quit) {
    t_packet *packet = packet_recieve(client);
    if (!packet)
      break;

    switch (packet->type) {
    case JOINED:
      handle_joined(packet);
      break;

    case LEFT:
      handle_left(packet);
      break;

    case MESSAGE:
      handle_message(packet);
      break;
    }
    packet_destroy(packet);
  }

  pthread_join(input, NULL);
  return 0;
}
