#ifndef HANDLE_CLIENT_H
#define HANDLE_CLIENT_H

#include <sys/socket.h>

#define MAX_BUFFER_SIZE 65536
#define CLIENT_PORT 80

void* handle_client(void* client_socket);

int create_connection(char* host, int port);

#endif