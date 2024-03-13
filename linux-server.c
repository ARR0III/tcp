#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define USER_MSG_QUIT "quit"
#define USER_MSG_ECHO "echo"

typedef struct USER {
  int  uds;

  char msg[BUFFER_SIZE];
  int  msg_len;

  struct USER * prev;
} user_t;

int users_list_empty(stack_t ** users) {
 return *users == NULL;
}

void create_user(user_t ** users, int uds) {
  user_t * tmp = (user_t *)malloc(sizeof(user_t));

  if (!tmp) exit(CANNOT_ALLOCATE_MEMORY);

  tmp->uds     = uds;
  tmp->msg_len = 0;
  tmp->prev    = *users;
  *users       = tmp;
}

user_t * user_search(user_t ** users,  int uds) {
  user_t * tmp = *users;

  while (tmp) {
    if (tmp->uds == uds)
      return tmp;

    tmp = tmp->prev;
  }

  return NULL;
}

/* I don't understand HOW this SHIT works! */
void delete_user(user_t ** users, int uds) {
  while (*users) {
    if ((*users)->uds == uds) {
      users_t * tmp = *users;
      *users = (*users)->prev;
      free(tmp);
    }
    else {
      users = &((*users)->prev);
    }
  }
}

void nonblock(int sd) {
  int flags = fcntl(sd, F_GETFL);
  fcntl(sd, flags | O_NONBLOCK);
}

int main(int argc, char * argv[]) {

  int     opt = 1;

  int     server_socket;
  int     client_socket;

  int     users_counter;
  int     max_uds;
  int     result;

  short   ip_port = IP_PORT_NORMAL;

  user_t * users = NULL;

  struct user_data ud;
  size_t ud_size = sizeof(ud);

  fd_set read_uds, write_uds; /* read and write "user descriptor socket" */

  struct sockaddr_in server_addr;
  struct sockaddr_in user_addr;

  //struct timeval tmp_tv; /* struct for save timeout time */

  //struct timeval tv;     /* timeout 2.5 second */
  //  tv.tv_sec  = 2;
  //  tv.tv_usec = 500000; /* max value = 999999 */

/*****************************************************************************/

  if (argc == 2) {
    ip_port = (short)atoi(argv[1]);

    if (ip_port < 1001 || ip_port > 0xFFF0) {
      printf("[#] Invalid server port number:\t%d\n[#] Set server port:\t%d\n", ip_port, IP_PORT_NORMAL);
      ip_port = IP_PORT_NORMAL;
    }
  }

/*****************************************************************************/

  memset(&ud,          0x00, sizeof(ud));
  memset(&server_addr, 0x00, sizeof(server_addr));
  memset(&user_addr,   0x00, sizeof(user_addr));

  memset(&users_list, 0x00, LISTEN_USERS_MAX * sizeof(int));

/*****************************************************************************/

  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(ip_port);    /* port number from user or normal */
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /* using ip address server host */

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

/*****************************************************************************/
  
  FD_CLR(ud, &read_uds);   /*delete discr*/
  FD_CLR(ud, &write_uds);

  FD_ISSET(ud, &write_uds);

  while (1) {
    //memcpy(&tmp_tv, &tv, sizeof(tv));

    max_uds = server_socket;

    FD_ZERO(&read_uds);           /*clear lists*/
    FD_ZERO(&write_uds);   

    FD_SET(max_uds, &read_uds);   /*set discr*/

    for (users_counter = 0 ;; users_counter++) {
      if (users_counter >= LISTEN_USERS_MAX || users_list[users_counter] == 0) {
        break;
      }

      client_socket = users_list[users_counter];

      FD_SET(client_socket, &read_uds);   /*set discr*/

      if (user_write_ready) {
        FD_SET(client_socket, &write_uds);
      }

      if (max_uds < client_socket) {
        max_uds = client_socket;
      }
    }

    result = select(max_uds + 1, &read_uds, &write_uds, NULL, NULL);

    if (result == SYS_CALL_SELECT_ERROR) {
      if (errno == EINTR) {
        /* signal */ 
      }
      else {
        /* error select */
      }
    }

    if (!result) {
      continue;
    }

    if (FD_ISSET(server_socket, &read_uds)) {
      client_socket = accept(server_socket, (struct sockaddr *)(&user_addr), sizeof(user_addr));

      /*if server are first writer to user, then unblock socket user*/

      if (client_socket == SOCKET_ACCEPT_ERROR) {
        /*error accept user*/
      }

      if (users_counter >= LISTEN_USERS_MAX-1) {
        break;
      }
      else {
        create_user(&users, client_socket);

        if (!users) {
          break;
        }
        /*nonblock(result);*/
      }
    }

    for (users_counter = 0 ;; users_counter++) {
      if (users_counter >= LISTEN_USERS_MAX || users_list[users_counter] == 0) {
        break;
      }

      client_socket = users_list[users_counter];

      if (FD_ISSET(client_socket, &read_uds)) {
        result = read_user_message(client_socket);

        if (read == EOF) {
          /*read from user 0 byte*/
        }
      }

      if (FD_ISSET(client_socket, &write_uds)) {
        result = send_user_message(client_socket);
      }
    }
  }

/*****************************************************************************/

  for (users_counter = 0 ;; users_counter++) {

    if (users_counter >= LISTEN_USERS_MAX) {
      break;
    }

    if (users_list[users_counter]) {
      shutdown(users_list[users_counter], SHUT_RDWR);
      close(users_list[users_counter]);
    }
  }

  close(server_socket);

  memset(&server_addr, 0x00, sizeof(server_addr));
  memset(&user_addr,   0x00, sizeof(user_addr));

  return 0;
}

