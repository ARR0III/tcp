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

/* CHANGE THIS CONST FOR INITIALIZED MAXIMUM LISTING CLIENTS */
#define LISTEN_USERS_MAX      32

/* CHANGE THIS CONST FOR INITIALIZED MAXIMUM USERS ON SERVER */
#define CONNECT_USERS_MAX     32

/*****************************************************************************/

#define ERROR                 -1
#define TRUE                   1
#define FALSE                  0

/*****************************************************************************/

#define BUFFER_SIZE         1024
#define IP_PORT_STANDART    1888

/*****************************************************************************/

/* error codes for stderr stream */
#define RETURN_MEMORY_ERROR    1
#define RETURN_LISTEN_ERROR    2
#define RETURN_SOCKET_ERROR    3
#define RETURN_BIND_ERROR      4
#define RETURN_SELECT_ERROR    5

/*****************************************************************************/

typedef struct USER {
  int  id;                    /* identification code of user */
  int  uds;                   /* user data socket */

  char message[BUFFER_SIZE];  /* buffer for user read data */
  int  message_len;           /* size of data for send to user or read from user */
  int  message_pos;               /* if > 0 then resend */
} user_t;

void * memset2(void * ptr, size_t data, size_t size) {
  register unsigned char * p = (unsigned char *)ptr;
  register size_t block, bt, dword = data;

  if (data < 256) {
    dword |= data  <<  8;
    dword |= dword << 16;
  }

  block = size >> 2; /* blocks of 4 bytes */
  bt    = size  & 3; /* bytes not in 1 block */

  while (block) {
    *((size_t *)p) = dword;
    p += sizeof(size_t);
    block--;
    if (!block) break;

    *((size_t *)p) = dword;
    p += sizeof(size_t);
    block--;
    if (!block) break;

    *((size_t *)p) = dword;
    p += sizeof(size_t);
    block--;
    if (!block) break;

    *((size_t *)p) = dword;
    p += sizeof(size_t);
    block--;
    if (!block) break;
  }

  if (p > (unsigned char *)ptr) {
    p -= (sizeof(size_t) - bt);
    *((size_t *)p) = dword;
    
    return ptr;
  }
  
  while (bt) {
    *p = (unsigned char)dword;

    p++;
    bt--;
  }

  return ptr;
}

user_t * create_user(int uds) {
  user_t * tmp = (user_t *)malloc(sizeof(user_t));

  if (!tmp) exit(RETURN_MEMORY_ERROR);

  memset2(tmp, 0x00, sizeof(user_t));

  tmp->uds         = uds;
  tmp->message_len = 0;
  tmp->message_pos = 0;

  return tmp;
}

void users_create(user_t ** user, int users_list) {
  int i;

  for (i = 0; i < users_list; i++) {
    user[i]      = create_user(0);
    user[i]->id  = i+1;
  }
}

/* set socket unblock mode */
void nonblock(int sd) {
  int flags = fcntl(sd, F_GETFL);
  fcntl(sd, flags | O_NONBLOCK);
}


void eof_message(int client) {
  char EOF_MESSAGE[] = "EOF.\n";
  write(client, EOF_MESSAGE, sizeof(EOF_MESSAGE));
}

int correct_message(user_t * user) {
  int i;
  char ch;

  for (i = 0; i < (BUFFER_SIZE-1) && i < (user->message_len); i++) {
    ch = user->message[i];

    if (!isprint((int)(ch))) {
      if (ch != 0x0A && ch != 0x0D && ch != '\t' && ch != '\0') {
        return FALSE;
      }
    }
  }

  return TRUE;
}

int welcome_message(int socket) {
  char data[] = "Welcome!\n";
  return write(socket, data, sizeof(data));
}

int read_user_message(int client_socket, user_t * user) {
  int result;

  if (user->message_len < BUFFER_SIZE-1) {
    result = read(client_socket, user->message + user->message_len,
                                   BUFFER_SIZE - user->message_len - 1);
  }
  else {
    user->message[user->message_len] = '\0';
    user->message_pos = 0;
    return TRUE;
  }

  if (result == EOF) {
    user->message[user->message_len] = '\0';
    user->message_pos = 0;
    return TRUE;
  }

  if (result > 0 && result < BUFFER_SIZE-1) {
    user->message_len += result;
    user->message_pos = 0;
    return TRUE;
  }

  return result;
}

int write_user_message(int client_socket, user_t * user) {
  int result;

  if (user->message_len > 0) {
    result = write(client_socket, user->message + user->message_pos, user->message_len);
  }
  else {
    return EOF;
  }

  if (result == user->message_len) {
    user->message[0]  = '\0';
    user->message_len = 0;
    user->message_pos = 0;
    return TRUE;
  }

  if (result > 0 && result < user->message_len) {
    user->message_pos  = user->message_len - (user->message_len - result);
    user->message_len -= result;
    return FALSE;
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
      user_struct->message_pos = 0;

      memset2(user_struct->message, 0x00, BUFFER_SIZE);

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

  short   ip_port = IP_PORT_STANDART;

  user_t * users_list[CONNECT_USERS_MAX];

  fd_set read_uds, write_uds; /* reader and writer "user descriptor socket" */

  struct sockaddr_in server_addr;

  struct sockaddr_in user_addr;
  socklen_t user_addr_len = sizeof(user_addr);

  struct timeval tv;     /* timeout 1.5 second */
    tv.tv_sec  = 1;
    tv.tv_usec = 500000; /* max value = 999999 */

/*****************************************************************************/

  if (argc == 2) {
    ip_port = (short)atoi(argv[1]);

    if (ip_port < 1001) {
      fprintf(stderr, "[#] Invalid server port number:%s\n[#] Set server port:%d\n", argv[1], IP_PORT_STANDART);
      ip_port = IP_PORT_STANDART;
    }
  }

/*****************************************************************************/

  memset2(&server_addr, 0x00, sizeof(server_addr));
  memset2(&user_addr,   0x00, sizeof(user_addr));

/*****************************************************************************/

  users_create(users_list, CONNECT_USERS_MAX);

/*****************************************************************************/

  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = htons(ip_port);    /* port number from user or normal */
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* using ip address server host */

  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (server_socket == ERROR) {
    fprintf(stderr, "[X] SOCKET CREATE ERROR.\n");
    exit(RETURN_SOCKET_ERROR);
  }

  /* What??? Why??? */
  nonblock(server_socket);

  result = bind(server_socket, (struct sockaddr *)(&server_addr), sizeof(server_addr));

  if (result == ERROR) {
    close(server_socket);
    fprintf(stderr, "[X] SOCKET BIND ERROR.\n");
    exit(RETURN_BIND_ERROR);
  }

  setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO,  &tv, sizeof(tv));
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt/*int*/));

  fprintf(stderr, "[#] Server starting port:%d\n", ip_port);

/*****************************************************************************/

  result = listen(server_socket, LISTEN_USERS_MAX);

  switch(result) {
    case 0         : fprintf(stderr, "[#] Maximal wait OS listening:%d\n[#] Listening...\n", LISTEN_USERS_MAX);
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

  if (result) {
    close(server_socket);
    exit(RETURN_LISTEN_ERROR);
  }

/*****************************************************************************/

  while (1) {
    max_uds = server_socket;

    FD_ZERO(&read_uds);                      /*clear lists for read*/
    FD_ZERO(&write_uds);                     /*clear lists for read*/
    FD_SET(server_socket, &read_uds);        /*set server for select*/

    for (i = 0; i < CONNECT_USERS_MAX; i++) {
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
        /* if process <-- signal */
        continue;
      }
      else {
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
      }

      if (users_counter < 0 || users_counter >= CONNECT_USERS_MAX) {
        close(client_socket);
        fprintf(stderr, "[!] Maximal users on server:%d\n", CONNECT_USERS_MAX);
        continue;
      }

      result = user_add(client_socket, users_list);

      if (result != ERROR) { /* position for user in array found! */
        users_counter++;
        nonblock(client_socket);
        welcome_message(client_socket);

        fprintf(stderr, "[@] User \"%d\" connect to server!\n", (*users_list[result]).id);
      }
      else { /* position for user NOT FOUND! */
        close(client_socket);
        fprintf(stderr, "[!] Position for user on server not found!\n");
        continue;
      }
    }

    for (i = 0; i < CONNECT_USERS_MAX; i++) {
      if (0 == users_counter) {
       break;
      }

      client_socket = (*users_list[i]).uds;

      if (FD_ISSET(client_socket, &read_uds)) {
        result = read_user_message(client_socket, users_list[i]);

        /* if result != TRUE then continue read from socket */

        if (result == TRUE) {
          if (correct_message(users_list[i]) == TRUE) {
            fprintf(stderr, "[User #%d]>>>%s\n", (*users_list[i]).id, (*users_list[i]).message);
          }
          else {
            eof_message(client_socket);
          }
        }

        if (result == 0) {
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
      }

      if (FD_ISSET(client_socket, &write_uds)) {
        if (write_user_message(client_socket, users_list[i]) == EOF) {
          fprintf(stderr, "[!] User \"%d\" error send message!\n", (*users_list[i]).id);
        }
      }
    }
  }

/*****************************************************************************/

  for (i = 0; i < CONNECT_USERS_MAX; i++) {
    client_socket = (*users_list[i]).uds;

    if (client_socket) {
      shutdown(client_socket, SHUT_RDWR);
      close(client_socket);

      memset2(users_list[i], 0x00, sizeof(user_t));
      free(users_list[i]);
      users_list[i] = NULL;
    }
  }

  close(server_socket);

  memset2(&server_addr, 0x00, sizeof(server_addr));
  memset2(&user_addr,   0x00, sizeof(user_addr));

  return 0;
}

