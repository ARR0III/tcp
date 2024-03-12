#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 256

#define SOCKET_CREATE_ERROR -1
#define SOCKET_BIND_ERROR   -1

int main(void) {
  int counter, server_socket  = 0;
  int result = 0;

  short ip_port = 1888;       /* port number */

  struct sockaddr_in addr;
  socklen_t addr_size = sizeof(addr);

  char buffer[BUFFER_SIZE];

  struct timeval tv;  /* timeout 10 second */
    tv.tv_sec  = 15;
    tv.tv_usec = 0;

/*****************************************************************************/

  memset(&addr,   0x00, addr_size);
  memset(buffer, 0x00, BUFFER_SIZE);

/*****************************************************************************/

  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(ip_port);         /* host to number short  */
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* using ip address host */

  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (server_socket == SOCKET_CREATE_ERROR) {
    printf("[X] SOCKET CREATE ERROR.\n");
    exit(1);
  }

/*****************************************************************************/

  setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  result = connect(server_socket, (struct sockaddr *)&addr, addr_size);

  if (result < 0) {
    printf("[X] Error connect to server\n");
    exit(2);
  }

  while (1) {
    printf(">>>");

    if (fgets(buffer, BUFFER_SIZE-1, stdin) == NULL) {
      break;
    }

    for (counter = 0; counter < BUFFER_SIZE; counter++) {
      if (buffer[counter] == '\n') {
        buffer[counter] = '\0';
        break;
      }
    }

    if (strcmp(buffer, "--quit") == 0) {
      break;
    }

    printf("\n");

    result = write(server_socket, buffer, counter);

    if (result <= 0) {
      printf("[X] Error write message to server\n");
      break;
    }

    result = read(server_socket, buffer, BUFFER_SIZE-1);

    if (result <= 0) {
      printf("[X] Error get message from server\n");
      break;
    }

    buffer[result] = '\0';

    printf("server_msg:%s\n", buffer);
  }

/*****************************************************************************/

  memset(buffer, 0x00, BUFFER_SIZE);
  memset(&addr,   0x00, addr_size);

  close(server_socket);
  return 0;
}

