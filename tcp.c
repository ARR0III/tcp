#include <stdio.h>
#include <string.h>
#include <winapi/winsock2.h> /* for Tiny C Compiler */
#include <windows.h>

#define _OK     0
#define _ERROR -1

#define _ERROR_BIND -2
#define _ERROR_LIST -3
#define _ERROR_RECV -4
#define _ERROR_CONN -5
#define _ERROR_ADDR -6

#define _MIN_IP_LENGTH 7   /* 1.1.1.1 */
#define _MAX_IP_LENGTH 15  /* 127.216.284.666 */

#define _MIN_BUFFER_LENGTH  4096
#define _MAX_PORT_NUMBER   64000

#define _MSG_HELLO "PlexusTCL tcp/ip server\r\n"

SOCKET global_socket; /* GLOGAL SOCKET */

void printclients_count (int count) {
  if (count)
    printf("Users online: %d\n", count);
  else
    printf("Not on-lile users!\n");
}

int clients_count;

DWORD WINAPI linktoclient(LPVOID client_socket) {
  SOCKET link_socket = ((SOCKET *)client_socket)[0];
  char local_buffer[_MIN_BUFFER_LENGTH] = {0};

  send(link_socket, _MSG_HELLO, sizeof(_MSG_HELLO), 0);

  int bytes_recv;

  while ((bytes_recv = recv(link_socket, local_buffer, sizeof(local_buffer), 0)) && bytes_recv != SOCKET_ERROR) {
    printf("%s\n", bytes_recv > 0 ? local_buffer : "NULL");
    send(link_socket, local_buffer, bytes_recv, 0);
  }

  clients_count--;
  
  printf("Client disconnect!\n");
  printclients_count(clients_count);

  closesocket(link_socket);
  return _OK;
}

int server (int port) {
  struct sockaddr_in local_address;
  local_address.sin_family = AF_INET;
  local_address.sin_port = htons(port);
  local_address.sin_addr.s_addr = 0;

  if (bind(global_socket, &local_address, sizeof(local_address)) != _OK)
    return _ERROR_BIND;

  if (listen(global_socket, 0x100) != _OK)
    return _ERROR_LIST;

  printf("Connecting users progress...\n");

  SOCKET client_socket;
  struct sockaddr_in client_address;

  int client_address_size = sizeof(client_address);

  while (client_socket = accept(global_socket, &client_address, &client_address_size)) {
    clients_count++;

    HOSTENT * host = gethostbyaddr((char *)& client_address.sin_addr.s_addr, 4, AF_INET);

    printf("[+] User: [%s] [%s] connect!\n", host ? host->h_name : "Unknown", inet_ntoa( client_address.sin_addr));
    printclients_count(clients_count);

    DWORD new_thread;
    CreateThread(NULL, NULL, &linktoclient, &client_socket, NULL, &new_thread);
  }
 return _OK;
}

char buffer[_MIN_BUFFER_LENGTH] = {0};

int client (unsigned char * ip_adress, int port) {
  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  HOSTENT * server_host;

  if (inet_addr(ip_adress) != INADDR_NONE)
    server_address.sin_addr.s_addr = inet_addr(ip_adress);
  else {
    if (server_host = gethostbyname(ip_adress))
      ((unsigned long *)&server_address.sin_addr)[0] = ((unsigned long **)server_host->h_addr_list)[0][0];
    else
      return _ERROR_ADDR;
  }

  if (connect(global_socket, &server_address, sizeof(server_address)) != 0)
    return _ERROR_CONN;

  printf("Connect form \"%s:%d\" complete! >)\n", ip_adress, port);
  printf("Enter \"~quit\" for quit\n");
  
  int nsize;
  while ((nsize = recv(global_socket, buffer, sizeof(buffer) - 1, 0)) != SOCKET_ERROR) {
    buffer[nsize] = 0;

    printf("[server]:%s", buffer);

    printf("[client]:");
    fgets(buffer, sizeof(buffer) - 1, stdin);

    if (strcmp(buffer, "~quit\n") == 0)
      return _OK;
    
    send(global_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0x00, _MIN_BUFFER_LENGTH);
  }
  printf("Recv error nimber: %d\n", WSAGetLastError());
  return _OK;
}

int main (int argc, char * argv[]) { /* tcp --server --null 6218 */

  int tumbler;
  unsigned char ip_adress[_MAX_IP_LENGTH + 1] = {0};

  if (argc != 4) {
    printf("Error: Incorrect count arguments!\n");
    printf("Enter: %s [-s/c] [ip address] [port number]\n", argv[0]);
    return _ERROR;
  }

  if ((strcmp(argv[1], "-s") == 0) || (strcmp(argv[1], "--server") == 0))
    tumbler = 0;
  else    
  if ((strcmp(argv[1], "-c") == 0) || (strcmp(argv[1], "--client") == 0))
    tumbler = 1;
  else {
    printf("Error: argument \"%s\" incorrent!\n", argv[1]);
    printf("Enter argument \"-s/c\" or \"--server/client\"\n");
    return _ERROR;
  }

  if ((tumbler == 0) &&
  	 ((strcmp(argv[2], "-n") != 0) && (strcmp(argv[2], "--null") != 0))) {
       
       printf("Error: argument \"%s\" incorrent!\n", argv[2]);
       printf("Enter argument \"-n\" or \"--null\"\n");
       return _ERROR;
  }
  else
  if (tumbler == 1) {
    int length = strlen(argv[2]);

    if (length < _MIN_IP_LENGTH || length > _MAX_IP_LENGTH) {
      printf("Error: ip adress length incorrect!\n");
      return _ERROR;
    }

    strncpy(ip_adress, argv[2], _MAX_IP_LENGTH);
  }

  int port = atoi(argv[3]);

  if ((port <= 0) || (port > _MAX_PORT_NUMBER)) {
    printf("Error: incorrect port number!\n");
    printf("Enter porn number: 1..%d.\n", _MAX_PORT_NUMBER);
    return _ERROR;
  }

  printf("Starting tcp/ip %s\n", tumbler ? "client" : "server");

  if (WSAStartup(0x0202, (WSADATA *)buffer) != 0) {
    printf("WSAStartup error number: %d\n", WSAGetLastError());
    return _ERROR;
  }

  printf("Socket: %s\n", buffer);
  memset(buffer, 0x00, _MIN_BUFFER_LENGTH);

  global_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (global_socket == INVALID_SOCKET) {
    printf("Socket error number: %d\n", WSAGetLastError());
    return _ERROR;
  }

  printf("Socket started!\n");

  int result;

  if (tumbler == 0) {
    result = server(port);
    switch (result) {
      case _OK:         printf("Connect close!\n");
                        break;
      case _ERROR_BIND: printf("Bind error code: %d\n", WSAGetLastError());
                        break;
      case _ERROR_LIST: printf("List error code: %d\n", WSAGetLastError());
                        break;
      case _ERROR_RECV: printf("Recv error code: %d\n", WSAGetLastError());
                        break;
      case _ERROR_CONN: printf("Connect error code: %d\n", WSAGetLastError());
                        break;
      case _ERROR_ADDR: printf("Address error code: %d\n", WSAGetLastError());
                        break;
      default:          printf("Error code unknown!\n");
                        break;
    }
  }
  else
  if (tumbler == 1) {
    result = client(ip_adress, port);
    switch (result) {
      case _OK:         printf("Connect close!\n");
                        break;
      case _ERROR_CONN: printf("Connect error code: %d\n", WSAGetLastError());
                        break;
      case _ERROR_ADDR: printf("Address error code: %d\n", WSAGetLastError());
                        break;
      default:          printf("Error code unknown!\n");
                        break;
    }
  }

  closesocket(global_socket);
  WSACleanup();

  return _OK;
}
