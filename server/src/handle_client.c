#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include "../include/handle_client.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* handle_client(void* client_socket_ptr) {
    if (client_socket_ptr == NULL) {
        return NULL;
    }
    int client_socket = *((int*)client_socket_ptr);
    free(client_socket_ptr);

    int flag = 1;
    if (setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0) {
        perror("setsockopt(TCP_NODELAY) failed");
        close(client_socket);
        return NULL;
    }

    char buffer[MAX_BUFFER_SIZE], method[8], url[2048];
    char host[1024];
    int port = CLIENT_PORT;

    
    while (1) {
        // pthread_mutex_lock(&mutex);
        ssize_t bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("recv() failed");
            break;
        }
        if (bytes_received == 0) {
            break;
        }
        buffer[bytes_received] = '\0';

        printf("Received: %s\n", buffer);

        if (sscanf(buffer, "%s %s", method, url) != 2) {
            fprintf(stderr, "Invalid request format\n");
            continue;
        }

        if (strcmp(method, "CONNECT") == 0) {
            sscanf(url, "%1023[^:]:%d", host, &port);
            printf("URL: %s\n", url);
            // Установите туннель и передайте данные
            int destination_server = create_connection(host, port);
            if (destination_server < 0) {
                perror("Failed to create connection to destination server");
                continue;
            }
            // Отправьте ответ об установлении туннеля
            char response[] = "HTTP/1.1 200 Connection Established\r\n\r\n";
            if (send(client_socket, response, sizeof(response) - 1, 0) < 0) {
                perror("Failed to send response to client");
                close(destination_server);
                continue;
            }

            // Передача данных между клиентом и сервером
            while (1) {
                bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    if (bytes_received < 0) {
                        perror("recv() failed");
                    }
                    break;
                }
                printf("Запрос %s\n", buffer);
                ssize_t bytes_sent = send(destination_server, buffer, bytes_received, 0);
                if (bytes_sent < 0) {
                    perror("Failed to send data to destination server");
                    break;
                }
            }

            close(destination_server);
            printf("Tunnel closed\n");
        } else {
            // Для HTTP-запросов
            printf("Обратно\n");
            sscanf(url, "http://%1023[^:]:%d", host, &port);
            printf("host: %s, port: %d", host, port);
            int destination_server = create_connection(host, port);
            if (destination_server < 0) {
                perror("Failed to create connection to destination server");
                continue;
            }

            // Перенаправление запроса к целевому серверу
            send(destination_server, buffer, bytes_received, 0);
            printf("Перенаправдение к серверу назначения: %s\n", buffer);

            // Передача ответа обратно клиенту
            while (1) {
                bytes_received = recv(destination_server, buffer, MAX_BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    break;
                }
                printf("Передача обратно клиенту: %s\n", buffer);
                send(client_socket, buffer, bytes_received, 0);
            }

        shutdown(destination_server, SHUT_RDWR);
        close(destination_server);
        // pthread_mutex_unlock(&mutex);
        }   
    }
    

    close(client_socket);
    return NULL;
    
}


int create_connection(char* host, int port) {
    if (host == NULL || strlen(host) == 0) {
        fprintf(stderr, "Host is empty or null\n");
        return -1;
    }

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
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    int flag = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0) {
        perror("setsockopt(TCP_NODELAY) failed");
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect() failed");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    printf("Connected to %s:%d\n", host, port);
    return sock;
}