#include "client_manager.h"
#include "tslog.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>

#define DEFAULT_PASSWORD "chat123"

int client_manager_init(ClientManager *manager)
{
    if (!manager)
        return -1;

    manager->count = 0;
    manager->max_clients = MAX_CLIENTS;
    memset(manager->clients, 0, sizeof(manager->clients));

    if (pthread_mutex_init(&manager->mutex, NULL) != 0)
    {
        return -1;
    }

    if (pthread_cond_init(&manager->slot_available, NULL) != 0)
    {
        pthread_mutex_destroy(&manager->mutex);
        return -1;
    }

    if (pthread_cond_init(&manager->client_connected, NULL) != 0)
    {
        pthread_mutex_destroy(&manager->mutex);
        pthread_cond_destroy(&manager->slot_available);
        return -1;
    }

    return 0;
}

int client_manager_add(ClientManager *manager, int socket_fd, const char *username,
                       const char *ip_address, int port)
{
    if (!manager || socket_fd < 0)
        return -1;

    pthread_mutex_lock(&manager->mutex);

    while (manager->count >= manager->max_clients)
    {
        pthread_cond_wait(&manager->slot_available, &manager->mutex);
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd == 0)
        {
            manager->clients[i].socket_fd = socket_fd;

            if (username && strlen(username) > 0)
            {
                strncpy(manager->clients[i].username, username, MAX_USERNAME_SIZE - 1);
                manager->clients[i].username[MAX_USERNAME_SIZE - 1] = '\0';
            }
            else
            {
                snprintf(manager->clients[i].username, MAX_USERNAME_SIZE, "User_%d", socket_fd);
            }

            if (ip_address)
            {
                strncpy(manager->clients[i].ip_address, ip_address, INET_ADDRSTRLEN - 1);
                manager->clients[i].ip_address[INET_ADDRSTRLEN - 1] = '\0';
            }

            manager->clients[i].port = port;
            manager->clients[i].authenticated = false;
            manager->clients[i].active = true;
            manager->clients[i].connect_time = time(NULL);
            manager->clients[i].last_activity = time(NULL);
            manager->clients[i].thread_id = pthread_self();

            manager->count++;

            pthread_cond_signal(&manager->client_connected);

            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                     "Cliente adicionado: %s (%s:%d) socket=%d",
                     manager->clients[i].username, ip_address ? ip_address : "unknown",
                     port, socket_fd);
            tslog_write(log_msg);

            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

int client_manager_remove(ClientManager *manager, int socket_fd)
{
    if (!manager || socket_fd < 0)
        return -1;

    pthread_mutex_lock(&manager->mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd == socket_fd)
        {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                     "Cliente removido: %s (socket=%d)",
                     manager->clients[i].username, socket_fd);
            tslog_write(log_msg);

            memset(&manager->clients[i], 0, sizeof(ClientInfo));
            manager->count--;

            pthread_cond_signal(&manager->slot_available);
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

ClientInfo *client_manager_find_by_socket(ClientManager *manager, int socket_fd)
{
    if (!manager || socket_fd < 0)
        return NULL;

    pthread_mutex_lock(&manager->mutex);

    ClientInfo *result = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd == socket_fd && manager->clients[i].active)
        {
            result = &manager->clients[i];
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return result;
}

ClientInfo *client_manager_find_by_username(ClientManager *manager, const char *username)
{
    if (!manager || !username)
        return NULL;

    pthread_mutex_lock(&manager->mutex);

    ClientInfo *result = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd != 0 &&
            manager->clients[i].active &&
            strcmp(manager->clients[i].username, username) == 0)
        {
            result = &manager->clients[i];
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return result;
}

int client_manager_authenticate(ClientManager *manager, int socket_fd, const char *password)
{
    if (!manager || !password)
        return -1;

    pthread_mutex_lock(&manager->mutex);

    int result = -1;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd == socket_fd)
        {
            if (strcmp(password, DEFAULT_PASSWORD) == 0)
            {
                manager->clients[i].authenticated = true;
                result = 0;

                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg),
                         "Cliente autenticado: %s", manager->clients[i].username);
                tslog_write(log_msg);
            }
            else
            {
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg),
                         "Falha na autenticação: %s", manager->clients[i].username);
                tslog_write(log_msg);
            }
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return result;
}

int client_manager_get_clients(ClientManager *manager, int *sockets,
                               char usernames[][MAX_USERNAME_SIZE], int max_count)
{
    if (!manager || !sockets)
        return -1;

    pthread_mutex_lock(&manager->mutex);

    int copied = 0;
    for (int i = 0; i < MAX_CLIENTS && copied < max_count; i++)
    {
        if (manager->clients[i].socket_fd != 0 && manager->clients[i].active)
        {
            sockets[copied] = manager->clients[i].socket_fd;
            if (usernames)
            {
                strcpy(usernames[copied], manager->clients[i].username);
            }
            copied++;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return copied;
}

int client_manager_get_authenticated_clients(ClientManager *manager, int *sockets,
                                             char usernames[][MAX_USERNAME_SIZE], int max_count)
{
    if (!manager || !sockets)
        return -1;

    pthread_mutex_lock(&manager->mutex);

    int copied = 0;
    for (int i = 0; i < MAX_CLIENTS && copied < max_count; i++)
    {
        if (manager->clients[i].socket_fd != 0 &&
            manager->clients[i].active &&
            manager->clients[i].authenticated)
        {
            sockets[copied] = manager->clients[i].socket_fd;
            if (usernames)
            {
                strcpy(usernames[copied], manager->clients[i].username);
            }
            copied++;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return copied;
}

bool client_manager_username_exists(ClientManager *manager, const char *username)
{
    if (!manager || !username)
        return false;

    pthread_mutex_lock(&manager->mutex);

    bool exists = false;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd != 0 &&
            manager->clients[i].active &&
            strcmp(manager->clients[i].username, username) == 0)
        {
            exists = true;
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return exists;
}

void client_manager_update_activity(ClientManager *manager, int socket_fd)
{
    if (!manager || socket_fd < 0)
        return;

    pthread_mutex_lock(&manager->mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd == socket_fd)
        {
            manager->clients[i].last_activity = time(NULL);
            break;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
}

bool client_manager_has_available_slots(ClientManager *manager)
{
    if (!manager)
        return false;

    pthread_mutex_lock(&manager->mutex);
    bool available = (manager->count < manager->max_clients);
    pthread_mutex_unlock(&manager->mutex);

    return available;
}

int client_manager_get_total_count(ClientManager *manager)
{
    if (!manager)
        return -1;

    pthread_mutex_lock(&manager->mutex);
    int count = manager->count;
    pthread_mutex_unlock(&manager->mutex);

    return count;
}

int client_manager_get_authenticated_count(ClientManager *manager)
{
    if (!manager)
        return -1;

    pthread_mutex_lock(&manager->mutex);

    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd != 0 &&
            manager->clients[i].active &&
            manager->clients[i].authenticated)
        {
            count++;
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return count;
}

int client_manager_broadcast(ClientManager *manager, const char *message, int sender_fd)
{
    if (!manager || !message)
        return -1;

    pthread_mutex_lock(&manager->mutex);

    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd != 0 &&
            manager->clients[i].active &&
            manager->clients[i].authenticated &&
            manager->clients[i].socket_fd != sender_fd)
        {

            if (send(manager->clients[i].socket_fd, message, strlen(message), MSG_NOSIGNAL) > 0)
            {
                sent_count++;
            }
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return sent_count;
}

int client_manager_send_private(ClientManager *manager, const char *from_user,
                                const char *to_user, const char *message)
{
    if (!manager || !from_user || !to_user || !message)
        return -1;

    pthread_mutex_lock(&manager->mutex);

    int result = -1;
    ClientInfo *target = NULL;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd != 0 &&
            manager->clients[i].active &&
            manager->clients[i].authenticated &&
            strcmp(manager->clients[i].username, to_user) == 0)
        {
            target = &manager->clients[i];
            break;
        }
    }

    if (target)
    {
        char private_msg[1024];
        snprintf(private_msg, sizeof(private_msg),
                 "[PRIVADA de %s]: %s\n", from_user, message);

        if (send(target->socket_fd, private_msg, strlen(private_msg), MSG_NOSIGNAL) > 0)
        {
            result = 0;

            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg),
                     "Mensagem privada: %s -> %s", from_user, to_user);
            tslog_write(log_msg);
        }
    }

    pthread_mutex_unlock(&manager->mutex);
    return result;
}

void client_manager_destroy(ClientManager *manager)
{
    if (!manager)
        return;

    pthread_mutex_lock(&manager->mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (manager->clients[i].socket_fd != 0)
        {
            close(manager->clients[i].socket_fd);
        }
    }

    pthread_cond_broadcast(&manager->slot_available);
    pthread_cond_broadcast(&manager->client_connected);

    pthread_mutex_unlock(&manager->mutex);

    pthread_cond_destroy(&manager->slot_available);
    pthread_cond_destroy(&manager->client_connected);
    pthread_mutex_destroy(&manager->mutex);

    tslog_write("Client manager destruído");
}