#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE         1024
#define IP_PORT_NORMAL      1888
#define LISTEN_USERS_MAX      16

#define ECHO_LEN              12

#define SYS_CALL_SELECT_ERROR -1

#define SOCKET_CREATE_ERROR   -1
#define SOCKET_BIND_ERROR     -1
#define SOCKET_ACCEPT_ERROR   -1

#define CANNOT_ALLOCATE_MEMORY 1
#define SELECT_RETURN_ERROR    2

#define USER_MSG_QUIT "quit"
#define USER_MSG_ECHO "echo"

typedef struct USER {
  int  id;
  int  uds;

  char message[BUFFER_SIZE];
  int  message_len;
} user_t;

user_t * create_user(int uds) {
  user_t * tmp = (user_t *)malloc(sizeof(user_t));

  if (!tmp) exit(CANNOT_ALLOCATE_MEMORY);

  memset(tmp, 0x00, sizeof(user_t));

  tmp->uds     = uds;
  tmp->message_len = 0;

  return tmp;
}

void nonblock(int sd) {
  int flags = fcntl(sd, F_GETFL);
  fcntl(sd, flags | O_NONBLOCK);
}

int read_user_message(user_t * user, int client_socket) {
  return read(client_socket, user->message, user->message_len);
}

int write_user_message(user_t * user, int client_socket) {
  return write(client_socket, user->message, user->message_len);
}

int main(int argc, char * argv[]) {
  int     i;
  int     opt = 1;
  //int     user_write_ready = 0;
  //int     user_read_ready = 0;

  int     server_socket;
  int     client_socket;

  int     users_counter;
  int     max_uds;
  int     result;

  short   ip_port = IP_PORT_NORMAL;

  user_t * users_list[LISTEN_USERS_MAX];

  fd_set read_uds, write_uds; /* read and write "user descriptor socket" */

  struct sockaddr_in server_addr;
  struct sockaddr_in user_addr;
  socklen_t user_addr_len = sizeof(user_addr);

  //struct timeval tmp_tv; /* struct for save timeout time */

  struct timeval tv;     /* timeout 2.5 second */
    tv.tv_sec  = 2;
    tv.tv_usec = 500000; /* max value = 999999 */

/*****************************************************************************/

  if (argc == 2) {
    ip_port = (short)atoi(argv[1]);

    if (ip_port < 1001 || ip_port > 0xFFFF) {
      printf("[#] Invalid server port number:\t%s\n[#] Set server port:\t%d\n", argv[1], IP_PORT_NORMAL);
      ip_port = IP_PORT_NORMAL;
    }
  }

/*****************************************************************************/

  memset(&server_addr, 0x00, sizeof(server_addr));
  memset(&user_addr,   0x00, sizeof(user_addr));

/*****************************************************************************/

  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(ip_port);    /* port number from user or normal */
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* using ip address server host */

  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (server_socket == SOCKET_CREATE_ERROR) {
    printf("[X] SOCKET CREATE ERROR.\n");
    exit(1);
  }

  result = bind(server_socket, (struct sockaddr *)(&server_addr), sizeof(server_addr));

  if (result == SOCKET_BIND_ERROR) {
    printf("[X] SOCKET BIND ERROR.\n");
    exit(2);
  }

  setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO,  &tv, sizeof(tv));
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt/*int*/));

  printf("[#] Server starting port:\t%d\n", ip_port);

/*****************************************************************************/

  result = listen(server_socket, LISTEN_USERS_MAX);

  printf("[#] Listing clients...\n[#] Maximal clients:\t%d\n", LISTEN_USERS_MAX); 

/*****************************************************************************/

  for (i = 0; i < LISTEN_USERS_MAX; i++) {
    users_list[i] = create_user(0);
    users_list[i]->id = i+1;
  }

  while (1) {
    max_uds = server_socket;

    FD_ZERO(&read_uds);           /*clear lists*/
    FD_ZERO(&write_uds);

    FD_SET(max_uds, &read_uds);   /*set discr*/

    for (users_counter = 0; users_counter < LISTEN_USERS_MAX; users_counter++) {
      client_socket = (*users_list[users_counter]).uds;

      if (client_socket) {
        FD_SET(client_socket, &read_uds);   /*set discr*/
      }

      //if (user_write_ready) {
      //  FD_SET(client_socket, &write_uds);
      //}

      if (client_socket > max_uds) {
        max_uds = client_socket;
      }
    }

    result = select(max_uds + 1, &read_uds, &write_uds, NULL, NULL);

    if (result == SYS_CALL_SELECT_ERROR) {
      if (errno == EINTR) {
        /* signal */
        continue;
      }
      else {
        /* error select */
        printf("[X] System call \"select\" return error:\t%d\n", errno);
        perror("select");
        exit(SELECT_RETURN_ERROR);
      }
    }

    if (!result) { /* not data for check or reaction */
      continue;
    }

    if (FD_ISSET(server_socket, &read_uds)) {
      client_socket = accept(server_socket, (struct sockaddr *)(&user_addr), &user_addr_len);

      /*if server are first writer to user, then unblock socket user*/

      if (client_socket == SOCKET_ACCEPT_ERROR) {
        printf("[!] User don\'t connect code:%d", errno);
        continue;
        /*error accept user*/
      }

      if (users_counter >= LISTEN_USERS_MAX) {
        printf("[!] Users connect to server MAX");
        continue;
      }

      nonblock(client_socket);
      write(client_socket, "Welcome!\n", 9);

      (*users_list[users_counter]).uds = client_socket;

      //if (!users) {
      //  break;
      //}
    }

    for (users_counter = 0; users_counter < LISTEN_USERS_MAX; users_counter++) {
      client_socket = (*users_list[users_counter]).uds;

      if (FD_ISSET(client_socket, &read_uds)) {
        result = read_user_message(users_list[users_counter], client_socket);

        if (result == EOF) { /*read from user 0 byte*/
          shutdown(client_socket, SHUT_RDWR);
          close(client_socket);
        }
      }

      if (FD_ISSET(client_socket, &write_uds)) {
        result = write_user_message(users_list[users_counter], client_socket);

        if (result == EOF) {

        }
      }
    }
  }

/*****************************************************************************/

  for (users_counter = 0; users_counter < LISTEN_USERS_MAX; users_counter++) {
    client_socket = (*users_list[users_counter]).uds;

    if (client_socket) {
      shutdown(client_socket, SHUT_RDWR);
      close(client_socket);
    }
  }

  close(server_socket);

  for (i = 0; i < LISTEN_USERS_MAX; i++) {
    free(users_list[i]);
  }

  memset(&server_addr, 0x00, sizeof(server_addr));
  memset(&user_addr,   0x00, sizeof(user_addr));

  return 0;
}

