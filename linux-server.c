#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define LISTEN_USERS_MAX 64
#define BUFFER_SIZE 256

#define ECHO_LEN 12

#define SOCKET_CREATE_ERROR -1
#define SOCKET_BIND_ERROR   -1

#define USER_MSG_QUIT "quit"
#define USER_MSG_ECHO "echo"

int main(int argc, char * argv[]) {
  int    server_socket = 0;
  int    client_socket = 0;

  ssize_t count = 0;

  int    result   = 0;
  short  ip_port  = 1888;       /* port number */
  int    message_from_user_len = 0;

  struct sockaddr_in addr;
  socklen_t addr_size = sizeof(addr);

  struct sockaddr_in addr_user;
  socklen_t addr_user_size = sizeof(addr_user);

  struct timeval tv;  /* timeout 10 second */
    tv.tv_sec  = 10;
    tv.tv_usec = 0;

  char buffer[BUFFER_SIZE];

/*****************************************************************************/

  if (argc == 2) {
    ip_port = (short)atoi(argv[1]);

    if (ip_port < 1001 || ip_port > 32500) {
      printf("[#] Invalid ip port:%d\n", ip_port);
      ip_port = 1888;
    }
  }

/*****************************************************************************/

  memset(&addr,      0x00, addr_size);
  memset(&addr_user, 0x00, addr_user_size);
  memset(buffer,     0x00, BUFFER_SIZE);

/*****************************************************************************/

  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(ip_port);    /* host to number short  */
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /* using ip address host */

  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (server_socket == SOCKET_CREATE_ERROR) {
    printf("[X] SOCKET CREATE ERROR.\n");
    exit(1);
  }

  result = bind(server_socket, (struct sockaddr *)&addr, addr_size);

  if (result == SOCKET_BIND_ERROR) {
    printf("[X] SOCKET BIND ERROR.\n");
    exit(2);
  }

  setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO,  &tv, sizeof(tv));

  int opt = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

  printf("[#] Server starting port:\t%d\n", ip_port);

/*****************************************************************************/

  message_from_user_len = 0;

  result = listen(server_socket, LISTEN_USERS_MAX);

  addr_user_size = sizeof(addr_user);
  client_socket = accept(server_socket, (struct sockaddr *)&addr_user, &addr_user_size);
  
  if (client_socket < 0) {
    printf("[X] Error connect to user\n");
    exit(3);
  }

  result = write(client_socket, "[welcome]", 9);

  if (result <= 0) {
    printf("[X] Error welcome new user\n");
    exit(4);
  }

/*****************************************************************************/

  while (1) {
    result = read(client_socket, buffer, BUFFER_SIZE-1);

    if (result <= 0) {
      printf("[X] Error get message from user\n");
      break;
    }

    buffer[result] = '\0';

    if (strcmp(buffer, USER_MSG_QUIT) == 0) {
      result = write(client_socket, "Goodbye user...", 15);

      if (result <= 0) {
        printf("[X] Error send message to user\n");
      }

      shutdown(client_socket, SHUT_RDWR);
      close(client_socket);
      printf("[$$$]client close connect\n");
      break;
    }

    if (strcmp(buffer, USER_MSG_ECHO) == 0) {
      printf("[$$$]client echo check server\n");
      memcpy(buffer, "echo..echo..", ECHO_LEN);
      buffer[ECHO_LEN] = '\0';
      message_from_user_len = ECHO_LEN;
    }

    if (message_from_user_len) {
      result = write(client_socket, buffer, message_from_user_len);

      if (result <= 0) {
        printf("[X] Error send message to user\n");
        break;
      }

      message_from_user_len = 0;
      continue;
    }

    printf("client_msg:%s\n", buffer);

    result = write(client_socket, "[ok]", 4);

    if (result <= 0) {
      printf("[X] Error send message to user\n");
      break;
    }
  }

/*****************************************************************************/

  memset(&addr,      0x00, addr_size);
  memset(&addr_user, 0x00, addr_user_size);
  memset(buffer,     0x00, BUFFER_SIZE);

  close(server_socket);
  close(client_socket);

  return 0;
}

