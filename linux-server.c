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

#define ERROR                 -1
#define TRUE                   1
#define FALSE                  0

#define BUFFER_SIZE         1024
#define IP_PORT_NORMAL      1888
#define LISTEN_USERS_MAX      16

#define ECHO_LEN              12

/* error codes for stderr stream */
#define RETURN_MEMORY_ERROR    1
#define RETURN_LISTEN_ERROR    2
#define RETURN_SOCKET_ERROR    3
#define RETURN_BIND_ERROR      4
#define RETURN_SELECT_ERROR    5

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

/*
  if result = EOF then return -1;
  if result in [0..BUFFER_SIZE-1] then clear user struct and return result;
  if result >= BUFFER_SIZE then delete user data and return -1;
*/
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

int welcome_message(int socket) {
  char data[] = "Welcome to test return data server!\n";
  return write(socket, data, sizeof(data));
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

  if (result > 0 && result < user->message_len) {
    user->rewrite     = user->message_len - (user->message_len - result);
    user->message_len = user->message_len - result;
  }

  return result;
}

int user_add(int socket, user_t ** user) {
  int pos = 0;
  user_t * user_struct;

  while(pos < BUFFER_SIZE) {
    user_struct = user[pos];

    if (0 == user_struct->uds) {
      user_struct->uds = socket;
      return pos;
    }

    pos++;
  }

  return ERROR;
}

int user_del(int socket, user_t ** user) {
  int pos = 0;
  user_t * user_struct;

  while(pos < BUFFER_SIZE) {
    user_struct = user[pos];

    if (socket == user_struct->uds) {
      shutdown(socket, SHUT_RDWR);
      close(socket);

/* !!! DO NOT CLEAR AND NOT CHANGE user_struct->id !!! */

      user_struct->uds = 0;
      user_struct->message_len = 0;
      user_struct->rewrite = 0;

      memset(user_struct->message, 0x00, BUFFER_SIZE);

      return TRUE;
    }

    pos++;
  }

  return FALSE;
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
      fprintf(stderr, "[#] Invalid server port number:\t%s\n[#] Set server port:\t%d\n", argv[1], IP_PORT_NORMAL);
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

  if (server_socket == ERROR) {
    fprintf(stderr, "[X] SOCKET CREATE ERROR.\n");
    exit(RETURN_SOCKET_ERROR);
  }

  result = bind(server_socket, (struct sockaddr *)(&server_addr), sizeof(server_addr));

  if (result == ERROR) {
    fprintf(stderr, "[X] SOCKET BIND ERROR.\n");
    exit(RETURN_BIND_ERROR);
  }

  setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO,  &tv, sizeof(tv));
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt/*int*/));

  fprintf(stderr, "[#] Server starting port:%d\n", ip_port);

/*****************************************************************************/

  result = listen(server_socket, LISTEN_USERS_MAX);

  switch(result) {
    case 0         : fprintf(stderr, "[#] Listening clients...\n[#] Maximal clients:%d\n", LISTEN_USERS_MAX); 
                     break;
    case EADDRINUSE: fprintf(stderr, "[X] Socket active on port:%d\n", ip_port);
                     break; 
    case EBADF:      fprintf(stderr, "[X] Descriptor socket:%d -> not correct!\n", server_socket);
                     break;
    case ENOTSOCK:   fprintf(stderr, "[X] Descriptor socket:%d -> not socket!\n", server_socket);
                     break;
    case EOPNOTSUPP: fprintf(stderr, "[X] Descriptor socket:%d -> not for listen!\n", server_socket);
                     break;
    default: break;
  }

  if (result) exit(RETURN_LISTEN_ERROR);

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

    if (result == ERROR) {
      if (errno == EINTR) {
        /* signal */
        continue;
      }
      else {
        /* error select */
        fprintf(stderr, "[X] System call \"select\" return error:%d\n", errno);
        perror("select");
        exit(RETURN_SELECT_ERROR);
      }
    }

    if (!result) { /* not data for check or reaction timer */
      continue;
    }

    if (FD_ISSET(server_socket, &read_uds)) {
      user_addr_len = sizeof(user_addr);
      client_socket = accept(server_socket, (struct sockaddr *)(&user_addr), &user_addr_len);

      /*if server are first writer to user, then unblock socket user*/

      if (client_socket == ERROR) {
        fprintf(stderr, "[!] User don\'t connect code:%d", errno);
        continue;
        /*error accept user*/
      }

      result = user_add(client_socket, users_list);

      if (result != ERROR) { /* position for user found! */
        users_counter++;
        nonblock(client_socket);
        welcome_message(client_socket);

        fprintf(stderr, "[@] User \"%d\" connect to server!\n", (*users_list[result]).id);  
      }
      else { /* position for user NOT FOUND! */
        close(client_socket);
        fprintf(stderr, "[!] Maximal users on server:\t%d\n", LISTEN_USERS_MAX);
      }
    }

    for (i = 0; i < LISTEN_USERS_MAX; i++) {
      if (users_counter == 0) {
       break;
      }

      client_socket = (*users_list[i]).uds;

      if (FD_ISSET(client_socket, &read_uds)) {
        result = read_user_message(client_socket, users_list[i]);

        if (result == EOF || result == 0) { /*read from user 0 byte or none*/
          if (user_del(client_socket, users_list) == TRUE) {
            users_counter--;
            fprintf(stderr, "[!] User \"%d\" disconnect!\n", (*users_list[i]).id);
          }
          else {
            fprintf(stderr, "[!] SERVER LOGICAL ERROR: Not found user for delete!\n");
          }

          if (users_counter == 0) {
            fprintf(stderr, "[!] Not active connections!\n");
            break;
          }

          continue;
        }

        if (result > 0) {
          if (correct_message(users_list[i]) == TRUE) {
            fprintf(stderr, "[User #%d]>>>%s\n", (*users_list[i]).id, (*users_list[i]).message);
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
          fprintf(stderr, "[!] User \"%d\" error send message!\n", (*users_list[i]).id);

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

