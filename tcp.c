#include <time.h>
#include <stdio.h>
#include <string.h>
#include <winapi/winsock2.h> /* for Tiny C Compiler */
#include <windows.h>

#define _OK     0
#define _ERROR -1

#define _ERROR_BIND 1
#define _ERROR_LIST 2
#define _ERROR_RECV 3
#define _ERROR_CONN 4
#define _ERROR_ADDR 5

#define _MIN_IP_LENGTH 4   /* a.io */
#define _MAX_IP_LENGTH 32  /* *.io */

#define _MIN_BUFFER_LENGTH 16 * 1024
#define _MAX_PORT_NUMBER   64000

#define _MSG_HELLO "$$PlexusTCL tcp/ip server\r\n"
#define _MSG_SERV  "[server]>>"
#define _MSG_CLNT  "MSG-$:"

SOCKET global_socket; /* GLOGAL SOCKET */

time_t timer;
struct tm * real_time;
char strtime[9];

void timeprint(void) {
  memset(strtime, 0x00, 9);
  memset((void*)&real_time, 0x00, sizeof(real_time));

  timer = time(NULL);
  real_time = localtime(&timer);
  strftime(strtime, 9, "%H:%M:%S", real_time);
  printf("[%s] | ", strtime);
}

void printclients_count (int count) {
  timeprint();
  if (count > 0)
    printf("Users online: %d\n", count);
  else
    printf("Not on-lile users!\n");
}

int clients_count = 0;

DWORD WINAPI linktoclient(LPVOID client_socket) {
  SOCKET link_socket = ((SOCKET *)client_socket)[0];
  unsigned char local_buffer[_MIN_BUFFER_LENGTH] = {0};
  // отправляем клиенту приветствие
  send(link_socket, _MSG_HELLO, sizeof(_MSG_HELLO), 0);
  // цикл эхо-сервера: прием строки от клиента и возвращение ее клиенту
  int bytes_recv;
  // MSG_PEEK - просмотр пришедших ip пакетов
  // MSG_OOB  - прием и передача СРОЧНЫХ сообщений / recv читает такие данные только если send такой же
  while ((bytes_recv = recv(link_socket, local_buffer, _MIN_BUFFER_LENGTH - 1, 0)) && bytes_recv != SOCKET_ERROR) {
    printf("%s\n", local_buffer);
    send(link_socket, local_buffer, bytes_recv, 0);
    memset(local_buffer, 0x00, bytes_recv);
  }
  // если мы здесь, то произошел выход из цикла по причине
  // возращения функцией recv ошибки - соединение с клиентом разорвано
  clients_count--; // уменьшаем счетчик активных клиентов
  timeprint();
  printf("Client disconnect!\n");
  printclients_count(clients_count);
  // закрываем сокет
  closesocket(link_socket);
  return _OK;
}

int server (int port) {
  // Шаг 3 - связывание сокета с локальным адресом
  struct sockaddr_in local_address;
  memset((void*)&local_address, 0x00, sizeof(local_address));

  local_address.sin_family = AF_INET;
  local_address.sin_port = htons(port); // не забываем о сетевом порядке!!!
  local_address.sin_addr.s_addr = 0;    // сервер принимает подключения
                                        // на все свои IP-адреса
  // вызываем bind для связывания
  if (bind(global_socket, &local_address, sizeof(local_address)) != _OK)
    return _ERROR_BIND; // WSAGetLastError();
  // Шаг 4 - ожидание подключений
  // размер очереди - 0x100
  if (listen(global_socket, 16) != _OK)
    // Ошибка
    return _ERROR_LIST;

  timeprint();
  printf("Connecting users progress...\n");

  // Шаг 5 - извлекаем сообщение из очереди
  SOCKET client_socket; // сокет для клиента
  struct sockaddr_in client_address; // адрес клиента (заполняется системой)
  
  memset((void*)&client_address, 0x00, sizeof(client_address));

  // функции accept необходимо передать размер структуры
  int client_address_size = sizeof(client_address);
  // цикл извлечения запросов на подключение из очереди
  while (client_socket = accept(global_socket, &client_address, &client_address_size)) {
    clients_count++; // увеличиваем счетчик подключившихся клиентов
    // пытаемся получить имя хоста
    HOSTENT * host;
    memset((void*)&host, 0x00, sizeof(host));

    host = gethostbyaddr((char *)&client_address.sin_addr.s_addr, 4, AF_INET);
    // вывод сведений о клиенте
    timeprint();
    printf("User: [%s] [%s] connect!\n", host ? host->h_name : "Unknown", inet_ntoa(client_address.sin_addr));
    printclients_count(clients_count);
    // Вызов нового потока для обслужвания клиента
    // Да, для этого рекомендуется использовать _beginthreadex
    // но, поскольку никаких вызовов функций стандартной Си библиотеки
    // поток не делает, можно обойтись и CreateThread
    DWORD new_thread;
    CreateThread(NULL, NULL, &linktoclient, &client_socket, NULL, &new_thread);
  }
 return _OK;
}

int client (unsigned char * hostname, int port) {
  // Шаг 3 - установка соединения
  // заполнение структуры sockaddr_in - указание адреса и порта сервера

  struct sockaddr_in server_address;
  memset((void*)&server_address, 0x00, sizeof(server_address));
  
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port); /* host to number short (16 bits) */
  HOSTENT * server_host;
  // преобразование IP адреса из символьного в сетевой формат
  if (inet_addr(hostname) != INADDR_NONE) /* если это IP адресс */
    server_address.sin_addr.s_addr = inet_addr(hostname);
  else { // если это не IP адресс а доменное имя
    // попытка получить IP адрес по доменному имени сервера
    if (server_host = gethostbyname(hostname))
      // hst->h_addr_list содержит не массив адресов,
      // а массив указателей на адреса
      ((unsigned long *)&server_address.sin_addr)[0] = ((unsigned long **)server_host->h_addr_list)[0][0];
    else
      return _ERROR_ADDR;
  }

  // адрес сервера получен - пытаемся установить соединение
  if (connect(global_socket, &server_address, sizeof(server_address)) != 0)
    return _ERROR_CONN;

  timeprint();
  printf("Connect form \"%s:%d\" complete!", hostname, port);
  printf("Enter \"--quit\" for quit.\n");

  unsigned char * buffer = NULL;
  buffer = (unsigned char *)calloc(_MIN_BUFFER_LENGTH, 1);

  if (buffer == NULL) {
    printf("Cannot allocate memory!\n");
    return _OK;
  }

  //char * GET_MSG = "GET / HTTP/1.1\r\nHost: hostname\r\n\r\n\0";
  //send(global_socket, GET_MSG, strlen(GET_MSG), 0);

  // Шаг 4 - чтение и передача сообщений
  int nsize;
  while ((nsize = recv(global_socket, buffer, _MIN_BUFFER_LENGTH - 1, 0)) != SOCKET_ERROR) {
    // ставим завершающий ноль в конце
    buffer[nsize] = 0x00;
    // выводим на экран
    //printf("%s%s", _MSG_SERV, buffer);
    printf("\n%s\n", buffer);
    memset(buffer, 0x00, _MIN_BUFFER_LENGTH);
    // читаем пользовательский ввод с клавиатуры
    printf("%s", _MSG_CLNT);

    fgets(buffer, _MIN_BUFFER_LENGTH - 1, stdin);

    // проверка на "quit"
    if (strcmp(buffer, "--quit\n") == 0) {
      // Корректный выход
      printf("You connection close!\n");
      free(buffer);
      return _OK;
    }
    
    // передаем строку клиента серверу
    send(global_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0x00, _MIN_BUFFER_LENGTH);
  }
  free(buffer);
  timeprint();
  printf("Recv error number: %d\n", WSAGetLastError());
  return _OK;
}

void printsocketmsg(int socket_result) {
  char * socket_msg[] = {"Connect close!","Bind","List","Recv",
                        "Connect","Address","Unknown"};
  if (socket_result == 0)
    printf("%s\n", socket_msg[socket_result]);
  else
    printf("%s error code: %d\n", socket_msg[socket_result], WSAGetLastError());
}

int main (int argc, char * argv[]) { /* tcp --server --null 6218 */

  int tumbler;
  unsigned char buffer[_MIN_BUFFER_LENGTH] = {0};
  unsigned char hostname[_MAX_IP_LENGTH + 1] = {0};

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

    strncpy(hostname, argv[2], _MAX_IP_LENGTH);
  }

  int port = atoi(argv[3]);

  if ((port <= 0) || (port > _MAX_PORT_NUMBER)) {
    printf("Error: incorrect port number!\n");
    printf("Enter porn number: 1..%d.\n", _MAX_PORT_NUMBER);
    return _ERROR;
  }

  timeprint();
  printf("Starting tcp/ip %s\n", tumbler ? "client" : "server");

  if (WSAStartup(0x0202, (WSADATA *)buffer) != 0) {
    timeprint();
    printf("WSAStartup error number: %d\n", WSAGetLastError());
    return _ERROR;
  }

  timeprint();
  printf("Socket ver: %s\n", buffer);
  memset(buffer, 0x00, _MIN_BUFFER_LENGTH);

  global_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (global_socket == INVALID_SOCKET) {
    timeprint();
    printf("Socket error number: %d\n", WSAGetLastError());
    return _ERROR;
  }

  timeprint();
  printf("Socket started!\n");

  printsocketmsg(tumbler ? client(hostname, port) : server(port));

  closesocket(global_socket);
  WSACleanup();

  return _OK;
}