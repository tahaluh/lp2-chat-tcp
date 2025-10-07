#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_RETRIES 3
#define RETRY_DELAY 2

static int sock = -1;
static volatile int client_running = 1;
static pthread_t recv_thread;

void signal_handler(int sig)
{
    printf("\n[Cliente] Recebido sinal %d, desconectando...\n", sig);
    client_running = 0;

    if (sock >= 0)
    {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        sock = -1;
    }

    if (recv_thread != 0)
    {
        pthread_cancel(recv_thread);
    }

    printf("[Cliente] ✓ Desconectado.\n");
    exit(0);
}

void *receive_messages(void *arg)
{
    char buffer[BUFFER_SIZE];
    int bytes;

    while (client_running && sock >= 0)
    {
        bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0)
        {
            if (bytes == 0)
            {
                printf("\n[Cliente] Servidor desconectou.\n");
            }
            else if (errno != EINTR && client_running)
            {
                printf("\n[Cliente] Erro ao receber dados: %s\n", strerror(errno));
            }
            client_running = 0;
            break;
        }

        buffer[bytes] = '\0';

        printf("\r\033[K%s", buffer);

        if (client_running)
        {
            printf("> ");
            fflush(stdout);
        }
    }

    return NULL;
}

int connect_to_server(const char *server_ip)
{
    struct sockaddr_in server_addr;
    int attempts = 0;

    while (attempts < MAX_RETRIES && client_running)
    {
        attempts++;

        printf("[Cliente] Tentativa %d de %d - Conectando ao servidor %s:%d...\n",
               attempts, MAX_RETRIES, server_ip, PORT);

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            printf("[Cliente] ERRO: Falha ao criar socket: %s\n", strerror(errno));
            if (attempts < MAX_RETRIES)
            {
                printf("[Cliente] Tentando novamente em %d segundos...\n", RETRY_DELAY);
                sleep(RETRY_DELAY);
                continue;
            }
            return -1;
        }

        struct timeval timeout;
        timeout.tv_sec = 5; // 5 segundos
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);

        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
        {
            printf("[Cliente] ERRO: Endereço IP inválido: %s\n", server_ip);
            close(sock);
            sock = -1;
            return -1;
        }

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
        {
            printf("[Cliente] ✓ Conectado ao servidor com sucesso!\n");

            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            return 0;
        }
        else
        {
            printf("[Cliente] ERRO: Falha na conexão: %s\n", strerror(errno));
            close(sock);
            sock = -1;

            if (attempts < MAX_RETRIES)
            {
                printf("[Cliente] Tentando novamente em %d segundos...\n", RETRY_DELAY);
                sleep(RETRY_DELAY);
            }
        }
    }

    printf("[Cliente] ERRO: Não foi possível conectar ao servidor após %d tentativas.\n", MAX_RETRIES);
    return -1;
}

int send_message(const char *message)
{
    if (sock < 0 || !client_running)
    {
        return -1;
    }

    ssize_t bytes_sent = send(sock, message, strlen(message), MSG_NOSIGNAL);
    if (bytes_sent < 0)
    {
        if (errno == EPIPE || errno == ECONNRESET)
        {
            printf("\n[Cliente] Conexão perdida com o servidor.\n");
            client_running = 0;
        }
        else
        {
            printf("\n[Cliente] Erro ao enviar mensagem: %s\n", strerror(errno));
        }
        return -1;
    }

    return 0;
}

int send_single_message(const char *server_ip, const char *message)
{
    printf("[Cliente] Modo não-interativo - enviando mensagem única\n");

    if (connect_to_server(server_ip) != 0)
    {
        return EXIT_FAILURE;
    }

    sleep(1);

    printf("[Cliente] Autenticando automaticamente...\n");
    if (send_message("/auth chat123") != 0)
    {
        close(sock);
        return EXIT_FAILURE;
    }

    sleep(1);

    if (send_message(message) != 0)
    {
        close(sock);
        return EXIT_FAILURE;
    }

    printf("[Cliente] ✓ Mensagem enviada: %s\n", message);

    sleep(1);

    close(sock);
    return EXIT_SUCCESS;
}

int interactive_mode(const char *server_ip)
{
    char input[BUFFER_SIZE];

    printf("\n=== CLIENTE DE CHAT MULTIUSUÁRIO v3 ===\n");
    printf("Conectando ao servidor...\n");

    if (connect_to_server(server_ip) != 0)
    {
        return EXIT_FAILURE;
    }

    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0)
    {
        printf("[Cliente] ERRO: Falha ao criar thread de recepção: %s\n", strerror(errno));
        close(sock);
        return EXIT_FAILURE;
    }

    sleep(1);

    printf("\n=== CHAT ATIVO ===\n");
    printf("Digite suas mensagens (ou comandos com /):\n");
    printf("• /help - Ver comandos disponíveis\n");
    printf("• /quit - Sair do chat\n");
    printf("• Ctrl+C - Forçar saída\n\n");

    while (client_running && sock >= 0)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            if (!client_running)
            {
                printf("\n[Cliente] Finalizando por sinal...\n");
                break;
            }
            else if (feof(stdin))
            {
                printf("\n[Cliente] EOF detectado, saindo...\n");
                break;
            }
            else if (errno == EINTR)
            {
                if (!client_running)
                {
                    printf("\n[Cliente] Interrompido por sinal, saindo...\n");
                    break;
                }
                continue;
            }
            else
            {
                printf("\n[Cliente] Erro na entrada: %s\n", strerror(errno));
                break;
            }
        }

        if (!client_running)
        {
            printf("\n[Cliente] Cliente não está mais rodando, saindo...\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0)
        {
            continue;
        }

        if (strcmp(input, "/quit") == 0 || strcmp(input, "/exit") == 0)
        {
            printf("[Cliente] Saindo do chat...\n");
            send_message("/quit");
            break;
        }

        if (strcmp(input, "/help") == 0)
        {
            printf("\n=== COMANDOS LOCAIS ===\n");
            printf("• /quit, /exit - Sair do chat\n");
            printf("• /help - Esta ajuda\n");
            printf("\n=== COMANDOS DO SERVIDOR ===\n");
            printf("• /auth <senha> - Autenticar no servidor\n");
            printf("• /list - Listar usuários online\n");
            printf("• /msg <user> <mensagem> - Mensagem privada\n");
            printf("• /nick <nome> - Mudar nome de usuário\n");
            printf("\nEnvie mensagens normalmente para chat público.\n\n");
            continue;
        }

        if (send_message(input) != 0)
        {
            break;
        }
    }

    client_running = 0;

    if (sock >= 0)
    {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        sock = -1;
    }

    if (recv_thread != 0)
    {
        pthread_cancel(recv_thread);
        pthread_detach(recv_thread);
    }

    printf("[Cliente] ✓ Desconectado do servidor.\n");
    return EXIT_SUCCESS;
}

void print_usage(const char *program_name)
{
    printf("=== CLIENTE DE CHAT MULTIUSUÁRIO v3 ===\n\n");
    printf("MODO INTERATIVO:\n");
    printf("  %s [servidor]\n", program_name);
    printf("  %s 127.0.0.1\n", program_name);
    printf("  %s\n", program_name);
    printf("\nMODO NÃO-INTERATIVO (mensagem única):\n");
    printf("  %s [servidor] \"mensagem\"\n", program_name);
    printf("  %s 127.0.0.1 \"Olá pessoal!\"\n", program_name);
    printf("\nPARÂMETROS:\n");
    printf("  servidor  : IP do servidor (padrão: 127.0.0.1)\n");
    printf("  mensagem  : Mensagem única para envio (modo não-interativo)\n");
    printf("\nEXEMPLOS:\n");
    printf("  %s                           # Conecta a localhost modo interativo\n", program_name);
    printf("  %s 192.168.1.100            # Conecta a IP específico\n", program_name);
    printf("  %s \"Teste de mensagem\"       # Envia mensagem única ao localhost\n", program_name);
    printf("  %s 192.168.1.100 \"Olá!\"      # Envia mensagem única a IP específico\n", program_name);
    printf("\n");
}

int main(int argc, char *argv[])
{
    const char *server_ip = "127.0.0.1"; // Padrão: localhost
    const char *message = NULL;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    if (argc == 1)
    {
    }
    else if (argc == 2)
    {
        if (strstr(argv[1], ".") != NULL || strcmp(argv[1], "localhost") == 0)
        {
            server_ip = argv[1];
        }
        else
        {
            message = argv[1];
        }
    }
    else if (argc == 3)
    {
        server_ip = argv[1];
        message = argv[2];
    }
    else
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    struct sockaddr_in test_addr;
    if (inet_pton(AF_INET, server_ip, &test_addr.sin_addr) <= 0)
    {
        if (strcmp(server_ip, "localhost") == 0)
        {
            server_ip = "127.0.0.1";
        }
        else
        {
            printf("[Cliente] ERRO: Endereço IP inválido: %s\n", server_ip);
            printf("[Cliente] Use um IP válido (ex: 127.0.0.1) ou 'localhost'\n");
            return EXIT_FAILURE;
        }
    }

    printf("[Cliente] Servidor alvo: %s:%d\n", server_ip, PORT);

    if (message != NULL)
    {
        return send_single_message(server_ip, message);
    }
    else
    {
        return interactive_mode(server_ip);
    }
}