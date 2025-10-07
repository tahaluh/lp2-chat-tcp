#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include "tslog.h"
#include "thread_safe_queue.h"
#include "client_manager.h"

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 1024

const char *profanity_filter[] = {
    "spam", "lixo", "idiota", "burro", "estupido",
    "merda", "porra", "caralho", "fdp", "otario",
    NULL};

static ClientManager client_manager;
static ThreadSafeQueue message_queue;
static pthread_t broadcast_thread;
static volatile int server_running = 1;
static int server_socket = -1;

void signal_handler(int sig)
{
    printf("\n[Servidor] Recebido sinal %d, finalizando graciosamente...\n", sig);
    server_running = 0;

    if (server_socket >= 0)
    {
        shutdown(server_socket, SHUT_RDWR);
        close(server_socket);
        server_socket = -1;
    }

    Message shutdown_msg;
    shutdown_msg.type = MSG_BROADCAST;
    strcpy(shutdown_msg.content, "SHUTDOWN");
    shutdown_msg.timestamp = time(NULL);
    shutdown_msg.sender_fd = -1;
    tsqueue_enqueue(&message_queue, &shutdown_msg);
}

int contains_profanity(const char *message)
{
    if (!message)
        return 0;

    char lower_msg[BUFFER_SIZE];
    strncpy(lower_msg, message, BUFFER_SIZE - 1);
    lower_msg[BUFFER_SIZE - 1] = '\0';

    for (int i = 0; lower_msg[i]; i++)
    {
        if (lower_msg[i] >= 'A' && lower_msg[i] <= 'Z')
        {
            lower_msg[i] = lower_msg[i] + 32;
        }
    }

    for (int i = 0; profanity_filter[i] != NULL; i++)
    {
        if (strstr(lower_msg, profanity_filter[i]) != NULL)
        {
            return 1;
        }
    }
    return 0;
}

void *broadcast_worker(void *arg)
{
    Message msg;

    tslog_write("Thread de broadcast iniciada");

    while (server_running)
    {
        if (tsqueue_dequeue(&message_queue, &msg) == 0)
        {
            if (strcmp(msg.content, "SHUTDOWN") == 0)
            {
                tslog_write("Thread de broadcast recebeu sinal de shutdown");
                break;
            }

            switch (msg.type)
            {
            case MSG_BROADCAST:
            {
                int sent = client_manager_broadcast(&client_manager, msg.content, msg.sender_fd);

                char log_msg[1200];
                snprintf(log_msg, sizeof(log_msg),
                         "Broadcast de %s para %d clientes: %s",
                         msg.username, sent, msg.content);
                tslog_write(log_msg);
                break;
            }

            case MSG_PRIVATE:
            {
                client_manager_send_private(&client_manager, msg.username, msg.target, msg.content);
                break;
            }

            case MSG_JOIN:
            {
                char join_msg[256];
                snprintf(join_msg, sizeof(join_msg),
                         "*** %s entrou no chat ***\n", msg.username);
                client_manager_broadcast(&client_manager, join_msg, -1);

                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Cliente %s entrou no chat", msg.username);
                tslog_write(log_msg);
                break;
            }

            case MSG_LEAVE:
            {
                char leave_msg[256];
                snprintf(leave_msg, sizeof(leave_msg),
                         "*** %s saiu do chat ***\n", msg.username);
                client_manager_broadcast(&client_manager, leave_msg, -1);

                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Cliente %s saiu do chat", msg.username);
                tslog_write(log_msg);
                break;
            }

            default:
                break;
            }
        }
    }

    tslog_write("Thread de broadcast finalizada");
    return NULL;
}

int process_command(int client_sock, const char *command)
{
    ClientInfo *client = client_manager_find_by_socket(&client_manager, client_sock);
    if (!client)
        return -1;

    char response[BUFFER_SIZE];

    if (strncmp(command, "/auth ", 6) == 0)
    {
        const char *password = command + 6;
        if (client_manager_authenticate(&client_manager, client_sock, password) == 0)
        {
            strcpy(response, "✓ Autenticado com sucesso! Bem-vindo ao chat.\n");
            send(client_sock, response, strlen(response), MSG_NOSIGNAL);

            Message join_msg;
            join_msg.type = MSG_JOIN;
            strncpy(join_msg.username, client->username, MAX_USERNAME_SIZE - 1);
            join_msg.username[MAX_USERNAME_SIZE - 1] = '\0';
            join_msg.timestamp = time(NULL);
            join_msg.sender_fd = client_sock;
            tsqueue_enqueue(&message_queue, &join_msg);
        }
        else
        {
            strcpy(response, "✗ Senha incorreta! Tente novamente.\n");
            send(client_sock, response, strlen(response), MSG_NOSIGNAL);
        }
        return 1;
    }

    if (!client->authenticated)
    {
        strcpy(response, "⚠ Você precisa se autenticar primeiro: /auth <senha>\n");
        send(client_sock, response, strlen(response), MSG_NOSIGNAL);
        return 1;
    }

    if (strcmp(command, "/list") == 0)
    {
        int client_sockets[MAX_CLIENTS];
        char usernames[MAX_CLIENTS][MAX_USERNAME_SIZE];
        int count = client_manager_get_authenticated_clients(&client_manager,
                                                             client_sockets, usernames, MAX_CLIENTS);

        strcpy(response, "=== USUÁRIOS ONLINE ===\n");
        for (int i = 0; i < count; i++)
        {
            char user_line[80];
            snprintf(user_line, sizeof(user_line), "• %s\n", usernames[i]);
            strcat(response, user_line);
        }

        char total_line[50];
        snprintf(total_line, sizeof(total_line), "\nTotal: %d usuários online\n", count);
        strcat(response, total_line);

        send(client_sock, response, strlen(response), MSG_NOSIGNAL);
        return 1;
    }

    if (strncmp(command, "/msg ", 5) == 0)
    {
        char *space = strchr(command + 5, ' ');
        if (space)
        {
            *space = '\0';
            const char *target_username = command + 5;
            const char *private_msg = space + 1;

            if (client_manager_find_by_username(&client_manager, target_username))
            {
                Message msg;
                msg.type = MSG_PRIVATE;
                strncpy(msg.username, client->username, MAX_USERNAME_SIZE - 1);
                msg.username[MAX_USERNAME_SIZE - 1] = '\0';
                strncpy(msg.target, target_username, MAX_USERNAME_SIZE - 1);
                msg.target[MAX_USERNAME_SIZE - 1] = '\0';
                strncpy(msg.content, private_msg, MAX_MESSAGE_SIZE - 1);
                msg.content[MAX_MESSAGE_SIZE - 1] = '\0';
                msg.timestamp = time(NULL);
                msg.sender_fd = client_sock;

                tsqueue_enqueue(&message_queue, &msg);

                snprintf(response, sizeof(response),
                         "✓ Mensagem privada enviada para %s\n", target_username);
            }
            else
            {
                snprintf(response, sizeof(response),
                         "✗ Usuário '%s' não encontrado ou offline\n", target_username);
            }

            send(client_sock, response, strlen(response), MSG_NOSIGNAL);
            return 1;
        }
    }

    if (strncmp(command, "/nick ", 6) == 0)
    {
        const char *new_username = command + 6;

        if (strlen(new_username) < 3 || strlen(new_username) > MAX_USERNAME_SIZE - 1)
        {
            strcpy(response, "✗ Nome deve ter entre 3 e 49 caracteres\n");
        }
        else if (client_manager_username_exists(&client_manager, new_username))
        {
            strcpy(response, "✗ Este nome já está em uso\n");
        }
        else
        {
            char old_name[MAX_USERNAME_SIZE];
            strcpy(old_name, client->username);
            strncpy(client->username, new_username, MAX_USERNAME_SIZE - 1);
            client->username[MAX_USERNAME_SIZE - 1] = '\0';

            snprintf(response, sizeof(response),
                     "✓ Nome alterado de %s para %s\n", old_name, new_username);
        }

        send(client_sock, response, strlen(response), MSG_NOSIGNAL);
        return 1;
    }

    if (strcmp(command, "/help") == 0)
    {
        strcpy(response,
               "=== COMANDOS DISPONÍVEIS ===\n"
               "/auth <senha>     - Autenticar (senha: chat123)\n"
               "/list             - Listar usuários online\n"
               "/msg <user> <msg> - Enviar mensagem privada\n"
               "/nick <nome>      - Mudar nome de usuário\n"
               "/help             - Mostrar esta ajuda\n"
               "/quit             - Sair do chat\n"
               "\nDigite mensagens normalmente para broadcast público.\n");

        send(client_sock, response, strlen(response), MSG_NOSIGNAL);
        return 1;
    }

    if (strcmp(command, "/quit") == 0)
    {
        strcpy(response, "Até logo! Desconectando...\n");
        send(client_sock, response, strlen(response), MSG_NOSIGNAL);
        return -1; // Sinaliza desconexão
    }

    return 0; // Comando não reconhecido
}

void *handle_client(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    char welcome_msg[512];
    int bytes;

    ClientInfo *client = client_manager_find_by_socket(&client_manager, client_sock);
    if (!client)
    {
        close(client_sock);
        return NULL;
    }

    snprintf(welcome_msg, sizeof(welcome_msg),
             "=== BEM-VINDO AO CHAT MULTIUSUÁRIO ===\n"
             "Seu nome temporário: %s\n"
             "IMPORTANTE: Você precisa se autenticar primeiro!\n"
             "\nComandos disponíveis:\n"
             "• /auth <senha>     - Autenticar (senha: chat123)\n"
             "• /help             - Ver todos os comandos\n"
             "• /quit             - Sair do chat\n"
             "\nDigite /auth chat123 para começar!\n"
             "=====================================\n\n",
             client->username);

    if (send(client_sock, welcome_msg, strlen(welcome_msg), MSG_NOSIGNAL) < 0)
    {
        tslog_write("Erro ao enviar boas-vindas");
        close(client_sock);
        client_manager_remove(&client_manager, client_sock);
        return NULL;
    }

    while (server_running && (bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[bytes] = '\0';

        char *newline = strchr(buffer, '\n');
        if (newline)
            *newline = '\0';
        newline = strchr(buffer, '\r');
        if (newline)
            *newline = '\0';

        if (strlen(buffer) == 0)
            continue;

        client_manager_update_activity(&client_manager, client_sock);

        if (buffer[0] == '/')
        {
            int cmd_result = process_command(client_sock, buffer);
            if (cmd_result == -1)
            {
                break; // Cliente solicitou desconexão
            }
            continue;
        }

        client = client_manager_find_by_socket(&client_manager, client_sock);
        if (!client || !client->authenticated)
        {
            const char *auth_required = "⚠ Você precisa se autenticar antes de enviar mensagens: /auth <senha>\n";
            send(client_sock, auth_required, strlen(auth_required), MSG_NOSIGNAL);

            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg),
                     "Mensagem rejeitada (não autenticado) - %s: %s",
                     client ? client->username : "unknown", buffer);
            tslog_write(log_msg);
            continue;
        }

        if (contains_profanity(buffer))
        {
            const char *warning = "⚠ AVISO: Sua mensagem contém conteúdo proibido e foi bloqueada.\n";
            send(client_sock, warning, strlen(warning), MSG_NOSIGNAL);

            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg),
                     "Mensagem bloqueada por filtro - %s: %s",
                     client->username, buffer);
            tslog_write(log_msg);
            continue;
        }

        char formatted_msg[BUFFER_SIZE + 100];
        snprintf(formatted_msg, sizeof(formatted_msg),
                 "[%s]: %s\n", client->username, buffer);

        Message msg;
        msg.type = MSG_BROADCAST;
        strncpy(msg.username, client->username, MAX_USERNAME_SIZE - 1);
        msg.username[MAX_USERNAME_SIZE - 1] = '\0';
        strncpy(msg.content, formatted_msg, MAX_MESSAGE_SIZE - 1);
        msg.content[MAX_MESSAGE_SIZE - 1] = '\0';
        msg.timestamp = time(NULL);
        msg.sender_fd = client_sock;

        if (tsqueue_enqueue(&message_queue, &msg) != 0)
        {
            const char *error = "⚠ Servidor ocupado, tente novamente.\n";
            send(client_sock, error, strlen(error), MSG_NOSIGNAL);

            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                     "Fila de mensagens cheia - mensagem descartada");
            tslog_write(log_msg);
        }

        printf("[Chat] %s", formatted_msg);
    }

    client = client_manager_find_by_socket(&client_manager, client_sock);
    if (client && client->authenticated)
    {
        Message leave_msg;
        leave_msg.type = MSG_LEAVE;
        strncpy(leave_msg.username, client->username, MAX_USERNAME_SIZE - 1);
        leave_msg.username[MAX_USERNAME_SIZE - 1] = '\0';
        leave_msg.timestamp = time(NULL);
        leave_msg.sender_fd = client_sock;
        tsqueue_enqueue(&message_queue, &leave_msg);
    }

    close(client_sock);
    client_manager_remove(&client_manager, client_sock);

    return NULL;
}

int main()
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);
    pthread_t tid;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignora SIGPIPE

    printf("=== SERVIDOR DE CHAT MULTIUSUÁRIO v3 ===\n");
    printf("Inicializando componentes...\n");

    tslog_init("server.log");

    tslog_write("=== SERVIDOR DE CHAT INICIANDO ===");

    if (client_manager_init(&client_manager) != 0)
    {
        fprintf(stderr, "ERRO: Falha ao inicializar gerenciador de clientes\n");
        tslog_write("ERRO: Falha ao inicializar gerenciador de clientes");
        tslog_close();
        exit(EXIT_FAILURE);
    }

    if (tsqueue_init(&message_queue) != 0)
    {
        fprintf(stderr, "ERRO: Falha ao inicializar fila de mensagens\n");
        tslog_write("ERRO: Falha ao inicializar fila de mensagens");
        client_manager_destroy(&client_manager);
        tslog_close();
        exit(EXIT_FAILURE);
    }

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERRO: Falha ao criar socket");
        tslog_write("ERRO: Falha ao criar socket do servidor");
        tsqueue_destroy(&message_queue);
        client_manager_destroy(&client_manager);
        tslog_close();
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("ERRO: Falha no setsockopt");
        tslog_write("ERRO: Falha ao configurar socket (setsockopt)");
        close(server_socket);
        tsqueue_destroy(&message_queue);
        client_manager_destroy(&client_manager);
        tslog_close();
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("ERRO: Falha no bind");
        tslog_write("ERRO: Falha ao fazer bind do socket");
        close(server_socket);
        tsqueue_destroy(&message_queue);
        client_manager_destroy(&client_manager);
        tslog_close();
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, BACKLOG) < 0)
    {
        perror("ERRO: Falha no listen");
        tslog_write("ERRO: Falha ao colocar socket em modo listen");
        close(server_socket);
        tsqueue_destroy(&message_queue);
        client_manager_destroy(&client_manager);
        tslog_close();
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&broadcast_thread, NULL, broadcast_worker, NULL) != 0)
    {
        perror("ERRO: Falha ao criar thread de broadcast");
        tslog_write("ERRO: Falha ao criar thread de broadcast");
        close(server_socket);
        tsqueue_destroy(&message_queue);
        client_manager_destroy(&client_manager);
        tslog_close();
        exit(EXIT_FAILURE);
    }

    printf("✓ Componentes inicializados com sucesso\n");
    printf("✓ Servidor rodando na porta %d\n", PORT);
    printf("✓ Thread de broadcast ativa\n");
    printf("✓ Aguardando conexões...\n");
    printf("✓ Pressione Ctrl+C para finalizar graciosamente\n\n");

    tslog_write("Servidor de chat iniciado com sucesso na porta 8080");

    while (server_running)
    {
        int client_sock = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);

        if (client_sock < 0)
        {
            if (server_running)
            {
                if (errno != EINTR)
                {
                    perror("ERRO: Falha no accept");
                    tslog_write("ERRO: Falha ao aceitar conexão de cliente");
                }
                continue;
            }
            else
            {
                break; // Servidor finalizando
            }
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        if (!client_manager_has_available_slots(&client_manager))
        {
            const char *full_msg = "Servidor lotado! Tente novamente mais tarde.\n";
            send(client_sock, full_msg, strlen(full_msg), MSG_NOSIGNAL);
            close(client_sock);

            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                     "Conexão rejeitada (servidor lotado): %s:%d", client_ip, client_port);
            tslog_write(log_msg);
            continue;
        }

        char temp_username[MAX_USERNAME_SIZE];
        snprintf(temp_username, sizeof(temp_username), "User_%d", client_port);

        if (client_manager_add(&client_manager, client_sock, temp_username,
                               client_ip, client_port) != 0)
        {
            fprintf(stderr, "ERRO: Não foi possível adicionar cliente\n");
            close(client_sock);
            continue;
        }

        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg),
                 "Nova conexão aceita: %s:%d (socket %d, username: %s)",
                 client_ip, client_port, client_sock, temp_username);
        tslog_write(log_msg);
        printf("[Servidor] %s\n", log_msg);

        int *new_sock = malloc(sizeof(int));
        if (!new_sock)
        {
            perror("ERRO: Falha ao alocar memória");
            close(client_sock);
            client_manager_remove(&client_manager, client_sock);
            continue;
        }

        *new_sock = client_sock;

        if (pthread_create(&tid, NULL, handle_client, new_sock) != 0)
        {
            perror("ERRO: Falha ao criar thread do cliente");
            free(new_sock);
            close(client_sock);
            client_manager_remove(&client_manager, client_sock);
            continue;
        }

        pthread_detach(tid);
    }

    printf("\n[Servidor] Iniciando processo de finalização...\n");
    tslog_write("Iniciando finalização gracioso do servidor");

    Message shutdown_msg;
    shutdown_msg.type = MSG_BROADCAST;
    strcpy(shutdown_msg.content, "SHUTDOWN");
    shutdown_msg.timestamp = time(NULL);
    shutdown_msg.sender_fd = -1;
    tsqueue_enqueue(&message_queue, &shutdown_msg);

    printf("[Servidor] Aguardando thread de broadcast finalizar...\n");
    pthread_join(broadcast_thread, NULL);
    printf("[Servidor] Thread de broadcast finalizada.\n");

    if (server_socket >= 0)
    {
        close(server_socket);
    }

    printf("[Servidor] Finalizando componentes...\n");
    tsqueue_destroy(&message_queue);
    client_manager_destroy(&client_manager);

    tslog_write("=== SERVIDOR DE CHAT FINALIZADO ===");
    tslog_close();

    printf("[Servidor] ✓ Servidor finalizado com sucesso.\n");
    return 0;
}