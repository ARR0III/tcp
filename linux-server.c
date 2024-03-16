#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TRUE                   1
#define FALSE                  0

#define BUFFER_SIZE         1024
#define IP_PORT_NORMAL      1888
#define LISTEN_USERS_MAX      16

#define ECHO_LEN              12

#define SYS_CALL_SELECT_ERROR -1

#define SOCKET_CREATE_ERROR   -1
#define SOCKET_BIND_ERROR     -1
#define SOCKET_ACCEPT_ERROR   -1

/* error codes for stderr stream */
#define RETURN_MEMORY_ERROR    1
#define RETURN_SOCKET_ERROR    2
#define RETURN_BIND_ERROR      3
#define RETURN_SELECT_ERROR    4

#define USER_MSG_QUIT "quit"
#define USER_MSG_ECHO "echo"

typedef struct USER {
  int  id;
  int  uds;

  char message[BUFFER_SIZE];
  int  message_len;
  int  rewrite;
} user_t;

user_t * create_user(int uds) {
  user_t * tmp = (user_t *)malloc(sizeof(user_t));

  if (!tmp) exit(RETURN_MEMORY_ERROR);

  memset(tmp, 0x00, sizeof(user_t));

  tmp->uds         = uds;
  tmp->message_len = 0;
  tmp->rewrite     = 0;

  return tmp;
}

void nonblock(int sd) {
  int flags = fcntl(sd, F_GETFL);
  fcntl(sd, flags | O_NONBLOCK);
}

int correct_message(user_t * user) {
  int i;
  char ch;

  for (i = 0; i < (BUFFER_SIZE-1) && i < user->message_len; i++) {
    ch = user->message[i];

    if (!isprint((int)(ch))) {
      if (ch != 0x0A && ch != 0x0D && ch != '\t' && ch != '\0') {
        return FALSE;
      }
    }
  }

  return TRUE;
}

int read_user_message(int client_socket, user_t * user) {
  int result = read(client_socket, user->message, BUFFER_SIZE-1);

  if (result != EOF) {
    if (result < BUFFER_SIZE) {
      user->message[result] = '\0';
      user->message_len = result;
      user->rewrite = 0;
    }
    else {
      /* if read > buffer length (buffer owerflow) then return EOF */
      memset(user->message, 0x00, BUFFER_SIZE);
      user->message_len = 0;
      user->rewrite = 0;
      result = EOF;
    }
  }

  return result;
}

int write_user_message(int client_socket, user_t * user) {
  int result;

  if (user->message_len > 0) {
    result = write(client_socket, user->message + user->rewrite, user->message_len);
  }
  else {
    return EOF;
  }

  if (result == user->message_len) {
    user->rewrite = 0;
    user->message_len = 0;
    return result;
  }

  if (result && result < user->message_len) {
    user->rewrite     = user->message_len - (user->message_len - result);
    user->message_len = user->message_len - result;
  }

  return result;
}

int main(int argc, char * argv[]) {
  int     i;
  int     opt = 1;

  int     server_socket;
  int     client_socket;

  int     result;

  int     users_counter = 0;
  int     max_uds;

  short   ip_port = IP_PORT_NORMAL;

  user_t * users_list[LISTEN_USERS_MAX];

  fd_set read_uds, write_uds; /* read and write "user descriptor socket" */

  struct sockaddr_in server_addr;

  struct sockaddr_in user_addr;
  socklen_t user_addr_len = sizeof(user_addr);

  //struct timeval tmp_tv; /* struct for save timeout time */

  struct timeval tv;     /* timeout 1.5 second */
    tv.tv_sec  = 1;
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

  for (i = 0; i < LISTEN_USERS_MAX; i++) {
    users_list[i] = create_user(0);
    users_list[i]->id = i+1;
  }

/*****************************************************************************/

  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(ip_port);    /* port number from user or normal */
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* using ip address server host */

  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (server_socket == SOCKET_CREATE_ERROR) {
    printf("[X] SOCKET CREATE ERROR.\n");
    exit(RETURN_SOCKET_ERROR);
  }

  result = bind(server_socket, (struct sockaddr *)(&server_addr), sizeof(server_addr));

  if (result == SOCKET_BIND_ERROR) {
    printf("[X] SOCKET BIND ERROR.\n");
    exit(RETURN_BIND_ERROR);
  }

  setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO,  &tv, sizeof(tv));
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt/*int*/));

  printf("[#] Server starting port:\t%d\n", ip_port);

/*****************************************************************************/

  result = listen(server_socket, LISTEN_USERS_MAX);

  printf("[#] Listening clients...\n[#] Maximal clients:\t%d\n", LISTEN_USERS_MAX); 

/*****************************************************************************/

  while (1) {
    max_uds = server_socket;

    FD_ZERO(&read_uds);                 /*clear lists for read*/
    FD_ZERO(&write_uds);                /*clear lists for read*/
    FD_SET(server_socket, &read_uds);   /*set server for select*/

    for (i = 0; i < LISTEN_USERS_MAX; i++) {
      client_socket = (*users_list[i]).uds;  /*uds set if user connect*/

      if (client_socket > 0) {
        FD_SET(client_socket, &read_uds);    /*set discr*/

        if ((*users_list[i]).message_len > 0) {
          FD_SET(client_socket, &write_uds);
        }
      }

      if (client_socket > max_uds) {
        max_uds = client_socket;
      }
    }

    tv.tv_sec  = 1;
    tv.tv_usec = 500000; /* max value = 999999 */

    result = select(max_uds + 1, &read_uds, &write_uds, NULL, &tv);

    if (result == SYS_CALL_SELECT_ERROR) {
      if (errno == EINTR) {
        /* signal */
        continue;
      }
      else {
        /* error select */
        printf("[X] System call \"select\" return error:\t%d\n", errno);
        perror("select");
        exit(RETURN_SELECT_ERROR);
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

      if (users_counter >= (LISTEN_USERS_MAX-1)) {
        printf("[!] Maximal users on server:\t%d\n", LISTEN_USERS_MAX);
        close(client_socket);
        continue;
      }

      nonblock(client_socket);
      write(client_socket, "Welcome!\n", 9);

      (*users_list[users_counter]).uds = client_socket;

      printf("[@] User \"%d\" connect to server!\n", (*users_list[users_counter]).id);  

      users_counter++;
    }

    for (i = 0; i < LISTEN_USERS_MAX; i++) {
      if (users_counter == 0) {
       break;
      }

      client_socket = (*users_list[i]).uds;

      if (FD_ISSET(client_socket, &read_uds)) {
        result = read_user_message(client_socket, users_list[i]);

        if (result == EOF || result == 0) { /*read from user 0 byte*/

          FD_CLR(client_socket, &read_uds);
          FD_CLR(client_socket, &write_uds);

          shutdown(client_socket, SHUT_RDWR);
          close(client_socket);

          memset((*users_list[i]).message, 0x00, BUFFER_SIZE);

          (*users_list[i]).uds = 0;
          (*users_list[i]).message_len = 0;
          (*users_list[i]).rewrite = 0;

          users_counter--;

          printf("[!] User \"%d\" disconnect!\n", (*users_list[i]).id);

          if (users_counter == 0) {
            printf("[!] Not active connections!\n");
            break;
          }

          continue;
        }

        if (result > 0) {
          if (correct_message(users_list[i]) == TRUE) {
            printf("[User #%d]>>>%s\n", (*users_list[i]).id, (*users_list[i]).message);
          }
          else {
            write(client_socket, "EOF.\n", 5);

            (*users_list[i]).message_len = 0;
            (*users_list[i]).rewrite = 0;
          }
        }
      }

      if (FD_ISSET(client_socket, &write_uds)) {
        if (write_user_message(client_socket, users_list[i]) == EOF) {
          printf("[!] User \"%d\" error send message!\n", (*users_list[i]).id);

          (*users_list[i]).message_len = 0;
          (*users_list[i]).rewrite = 0;
        }
      }
    }
  }

/*****************************************************************************/

  for (i = 0; i < LISTEN_USERS_MAX; i++) {
    client_socket = (*users_list[i]).uds;

    if (client_socket) {
      shutdown(client_socket, SHUT_RDWR);
      close(client_socket);

      memset(users_list[i], 0x00, sizeof(user_t));
      free(users_list[i]);
      users_list[i] = NULL;
    }
  }

  close(server_socket);

  memset(&server_addr, 0x00, sizeof(server_addr));
  memset(&user_addr,   0x00, sizeof(user_addr));

  return 0;
}

