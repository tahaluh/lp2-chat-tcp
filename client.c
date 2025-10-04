#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080

int sock;

void *receive_messages(void *arg)
{
    char buffer[1024];
    int bytes;
    while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytes] = '\0';
        printf("\n[Servidor]: %s\n> ", buffer);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr;
    pthread_t recv_thread;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Erro ao criar socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Erro na conexão");
        exit(1);
    }

    if (argc > 1)
    {
        send(sock, argv[1], strlen(argv[1]), 0);
        close(sock);
        return 0;
    }

    pthread_create(&recv_thread, NULL, receive_messages, NULL);
    pthread_detach(recv_thread);

    char msg[1024];
    while (1)
    {
        printf("> ");
        fgets(msg, sizeof(msg), stdin);
        msg[strcspn(msg, "\n")] = '\0';
        send(sock, msg, strlen(msg), 0);
    }

    close(sock);
    return 0;
}
