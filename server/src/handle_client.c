#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "../include/handle_client.h"


void* handle_client(void* client_socket_ptr) {
    int client_socket;
    char buffer[MAX_BUFFER_SIZE], method[MAX_METHOD_SIZE], url[MAX_URL_SIZE];
    char host[MAX_HOST_SIZE];
    int port = CLIENT_PORT;

    if (client_socket_ptr == NULL) {
        return NULL;
    }
    client_socket = *((int*)client_socket_ptr);
    free(client_socket_ptr);

    int flag = 1;
    if (setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0) {
        perror("setsockopt(TCP_NODELAY) failed");
        close(client_socket);
        return NULL;
    }
    
    while (1) {
        ssize_t bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("recv() failed");
            break;
        }
        if (bytes_received == 0) {
            printf("Client disconnected\n");
            break;
        }
        buffer[bytes_received] = '\0';

        printf("Received: %s\n", buffer);

        if (sscanf(buffer, "%s %s", method, url) != 2) {
            fprintf(stderr, "Invalid request format\n");
            continue;
        }

        if (process_request(client_socket, buffer, bytes_received, method, url, host, &port) < 0) {
            fprintf(stderr, "Failed to process request\n");
            break;
        }
    }

    close(client_socket);
    return NULL;
}


int create_connection(char* host, int port) {
    struct addrinfo hints, *res;
    int sock;

    if (host == NULL || strlen(host) == 0) {
        fprintf(stderr, "Host is empty or null\n");
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
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


int process_request(int client_socket, char* buffer, ssize_t bytes_received, 
                    char* method, char* url, char* host, int* port) {
    int destination_server;


    if (strcmp(method, "CONNECT") == 0) {
        if (sscanf(url, "%1023[^:]:%d", host, port) != 2) {
            fprintf(stderr, "Invalid CONNECT request format\n");
            return -1;
        }
        printf("CONNECT URL: %s\n", url);

        // Установите туннель и передайте данные
        destination_server = create_connection(host, *port);
        if (destination_server < 0) {
            perror("Failed to create connection to destination server");
            return -1;
        }
        // Отправьте ответ об установлении туннеля
        const char response[] = "HTTP/1.1 200 Connection Established\r\n\r\n";
        if (send(client_socket, response, sizeof(response) - 1, 0) < 0) {
            perror("Failed to send response to client");
            close(destination_server);
            return -1;
        }

        if (tunnel_data(client_socket, destination_server) < 0) {
            close(destination_server);
            return -1;
        }

        close(destination_server);
        printf("Tunnel closed\n");


    } else {
        printf("HTTP request\n");
        if (sscanf(url, "http://%1023[^/]", host) != 1) {
            fprintf(stderr, "Invalid HTTP request format\n");
            return -1;
        }
        *port = CLIENT_PORT;  // Default HTTP port
        destination_server = create_connection(host, *port);
        if (destination_server < 0) {
            perror("Failed to create connection to destination server");
            return -1;
        }
        if (send(destination_server, buffer, bytes_received, 0) < 0) {
            perror("Failed to send request to destination server");
            close(destination_server);
            return -1;
        }
        printf("Forwarded to destination server: %s\n", buffer);
        if (tunnel_data(client_socket, destination_server) < 0) {
            close(destination_server);
            return -1;
        }
        shutdown(destination_server, SHUT_RDWR);
        close(destination_server);
    }

    return 0;
}

int tunnel_data(int client_socket, int server_socket) {
    fd_set read_fds;
    char buffer[MAX_BUFFER_SIZE];

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        FD_SET(server_socket, &read_fds);

        int max_fd = (client_socket > server_socket) ? client_socket : server_socket;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select() failed");
            return -1;
        }

        if (FD_ISSET(client_socket, &read_fds)) {
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) {
                if (bytes_received < 0) perror("recv() from client failed");
                return 0;
            }
            if (send(server_socket, buffer, bytes_received, 0) < 0) {
                perror("send() to server failed");
                return -1;
            }
        }

        if (FD_ISSET(server_socket, &read_fds)) {
            ssize_t bytes_received = recv(server_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) {
                if (bytes_received < 0) perror("recv() from server failed");
                return 0;
            }
            if (send(client_socket, buffer, bytes_received, 0) < 0) {
                perror("send() to client failed");
                return -1;
            }
        }
    }
}
        
        // Передача данных между клиентом и сервером
    //     while (1) {
    //         bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);
    //         if (bytes_received <= 0) {
    //             if (bytes_received < 0) {
    //                 perror("recv() failed");
    //             }
    //             break;
    //         }
    //         printf("Запрос %s\n", buffer);
    //         ssize_t bytes_sent = send(destination_server, buffer, bytes_received, 0);
    //         if (bytes_sent < 0) {
    //             perror("Failed to send data to destination server");
    //             break;
    //         }
    //     }
    //     close(destination_server);
    //     printf("Tunnel closed\n");
        
    // } else {
    //     // Для HTTP-запросов
    //     printf("Обратно\n");
    //     sscanf(url, "http://%1023[^:]:%d", host, &port);
    //     printf("host: %s, port: %d", host, port);
    //     int destination_server = create_connection(host, port);
    //     if (destination_server < 0) {
    //         perror("Failed to create connection to destination server");
    //         continue;
    //     }
    //     // Перенаправление запроса к целевому серверу
    //     send(destination_server, buffer, bytes_received, 0);
    //     printf("Перенаправдение к серверу назначения: %s\n", buffer);
    //     // Передача ответа обратно клиенту
    //     while (1) {
    //         bytes_received = recv(destination_server, buffer, MAX_BUFFER_SIZE, 0);
    //         if (bytes_received <= 0) {
    //             break;
    //         }
    //         printf("Передача обратно клиенту: %s\n", buffer);
    //         send(client_socket, buffer, bytes_received, 0);
    //     }