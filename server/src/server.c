#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include "../include/handle_client.h"

#define SERVER_PORT 8081
#define MAX_CLIENTS 100


int start_server() {
    printf("Starting server...\n");
    int server_socket;
    struct sockaddr_in server_addr;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket() failed");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(server_socket);
        return -1;
    }

    printf("Server started on port %d\n", SERVER_PORT);

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("listen() failed");
        close(server_socket);
        return -1;
    }

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("accept() failed");
            continue;
        }
        
        printf("Client connected\n");

        int *client_socket_ptr = (int*)malloc(sizeof(int));
        if (client_socket_ptr == NULL) {
            perror("malloc() failed");
            close(client_socket);
            continue;
        }
        *client_socket_ptr = client_socket;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void*)client_socket_ptr) != 0) {
            perror("pthread_create() failed");
            close(client_socket);
            free(client_socket_ptr);

            continue;
        }

        pthread_detach(thread);
    }

    close(server_socket);
    return 0;
}

int main() {
    if (start_server() < 0) {
        return -1;
    }

    return 0;
}
