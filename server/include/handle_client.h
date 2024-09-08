#ifndef HANDLE_CLIENT_H
#define HANDLE_CLIENT_H

#include <sys/socket.h>

#define MAX_BUFFER_SIZE 65536
#define MAX_METHOD_SIZE 16
#define MAX_URL_SIZE 2048
#define MAX_HOST_SIZE 1024
#define CLIENT_PORT 80

void* handle_client(void* client_socket);
int process_request(int client_socket, char* buffer, ssize_t bytes_received, 
                    char* method, char* url, char* host, int* port);
int tunnel_data(int client_socket, int server_socket);

int create_connection(char* host, int port);

#endif