#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>

#define MAX_CLIENTS 100
#define MAX_USERNAME_SIZE 50
#define MAX_PASSWORD_SIZE 64

typedef struct
{
    int socket_fd;
    char username[MAX_USERNAME_SIZE];
    char ip_address[INET_ADDRSTRLEN];
    int port;
    bool authenticated;
    bool active;
    time_t connect_time;
    time_t last_activity;
    pthread_t thread_id;
} ClientInfo;

typedef struct
{
    ClientInfo clients[MAX_CLIENTS];
    int count;
    int max_clients;
    pthread_mutex_t mutex;
    pthread_cond_t slot_available;
    pthread_cond_t client_connected;
} ClientManager;

int client_manager_init(ClientManager *manager);

int client_manager_add(ClientManager *manager, int socket_fd, const char *username,
                       const char *ip_address, int port);

int client_manager_remove(ClientManager *manager, int socket_fd);

ClientInfo *client_manager_find_by_socket(ClientManager *manager, int socket_fd);

ClientInfo *client_manager_find_by_username(ClientManager *manager, const char *username);

int client_manager_authenticate(ClientManager *manager, int socket_fd, const char *password);

int client_manager_get_clients(ClientManager *manager, int *sockets,
                               char usernames[][MAX_USERNAME_SIZE], int max_count);

int client_manager_get_authenticated_clients(ClientManager *manager, int *sockets,
                                             char usernames[][MAX_USERNAME_SIZE], int max_count);

bool client_manager_username_exists(ClientManager *manager, const char *username);

void client_manager_update_activity(ClientManager *manager, int socket_fd);

bool client_manager_has_available_slots(ClientManager *manager);

int client_manager_get_total_count(ClientManager *manager);
int client_manager_get_authenticated_count(ClientManager *manager);

int client_manager_broadcast(ClientManager *manager, const char *message, int sender_fd);

int client_manager_send_private(ClientManager *manager, const char *from_user,
                                const char *to_user, const char *message);

void client_manager_destroy(ClientManager *manager);

#endif