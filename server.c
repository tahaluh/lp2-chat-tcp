#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "tslog.h"

#define PORT 8080
#define MAX_CLIENTS 100

int clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(char *message, int sender)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i] != sender)
        {
            send(clients[i], message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg); // libera memória alocada no main
    char buffer[1024];
    int bytes;

    while ((bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytes] = '\0';

        char msg[1200];
        snprintf(msg, sizeof(msg), "Cliente %d: %s", client_sock, buffer);

        tslog_write(msg);
        broadcast_message(msg, client_sock);

        printf("[Servidor] %s\n", msg);
    }

    close(client_sock);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i] == client_sock)
        {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}

int main()
{
    int server_fd, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);
    pthread_t tid;

    tslog_init("server.log");

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Erro ao criar socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Erro no setsockopt");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Erro no bind");
        exit(1);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Erro no listen");
        exit(1);
    }

    printf("Servidor rodando na porta %d...\n", PORT);
    tslog_write("Servidor iniciado.");

    while (1)
    {
        client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0)
        {
            perror("Erro no accept");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        clients[client_count++] = client_sock;
        pthread_mutex_unlock(&clients_mutex);

        tslog_write("Novo cliente conectado.");
        printf("Novo cliente conectado (socket %d)\n", client_sock);

        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_create(&tid, NULL, handle_client, new_sock);
        pthread_detach(tid);
    }

    tslog_close();
    return 0;
}
