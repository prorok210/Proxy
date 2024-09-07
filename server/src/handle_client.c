#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include "../include/handle_client.h"

void* handle_client(void* client_socket_ptr) {
    if (client_socket_ptr == NULL) {
        return NULL;
    }
    int client_socket = *((int*)client_socket_ptr);
    free(client_socket_ptr);

    char buffer[MAX_BUFFER_SIZE], method[8], url[2048], protocol[10];
    char host[1024];
    int port = CLIENT_PORT;

    while (1) {
        ssize_t bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("recv() failed");
            break;
        }
        buffer[bytes_received] = '\0';

        printf("Received: %s\n", buffer);

        sscanf(buffer, "%s %s %s", method, url, protocol);
        printf("Method: %s\nURL: %s\nProtocol: %s\n", method, url, protocol);

        char* host_start = strstr(url, "://");
        if (host_start) {
            host_start += 3;
        } else {
            host_start = url;
        }
        char* host_end = strchr(host_start, '/');
        if (host_end) {
            *host_end = '\0';
            strcpy(host, host_start);
            *host_end = '/';
        } else {
            strcpy(host, host_start);
        }

        int destination_server = create_conection(host, port);
        if (destination_server < 0) {
            perror("Failed to create connection to destination server");
            return NULL;
        }

        send(destination_server, buffer, bytes_received, 0);

        while (1) {
            bytes_received = recv(destination_server, buffer, MAX_BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                break;
            }
            send(client_socket, buffer, bytes_received, 0);
        }

        close(destination_server);
        memset(buffer, 0, MAX_BUFFER_SIZE);
    }

    close(client_socket);
    return NULL;
    
}


int create_conection(char* host, int port) {
    struct addrinfo hints, *res;
    int sock;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        perror("socket() failed");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect() failed");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    
    return sock;
}