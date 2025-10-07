#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define MAX_QUEUE_SIZE 1000
#define MAX_MESSAGE_SIZE 1024
#define MAX_USERNAME_SIZE 50

typedef enum
{
    MSG_BROADCAST,
    MSG_PRIVATE,
    MSG_JOIN,
    MSG_LEAVE,
    MSG_AUTH,
    MSG_ERROR
} MessageType;

typedef struct
{
    MessageType type;
    char username[MAX_USERNAME_SIZE];
    char content[MAX_MESSAGE_SIZE];
    char target[MAX_USERNAME_SIZE]; // Para mensagens privadas
    time_t timestamp;
    int sender_fd;
} Message;

typedef struct
{
    Message messages[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    int capacity;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ThreadSafeQueue;

int tsqueue_init(ThreadSafeQueue *queue);

int tsqueue_enqueue(ThreadSafeQueue *queue, const Message *msg);

int tsqueue_dequeue(ThreadSafeQueue *queue, Message *msg);

int tsqueue_try_enqueue(ThreadSafeQueue *queue, const Message *msg);

int tsqueue_try_dequeue(ThreadSafeQueue *queue, Message *msg);

bool tsqueue_empty(ThreadSafeQueue *queue);

bool tsqueue_full(ThreadSafeQueue *queue);

int tsqueue_size(ThreadSafeQueue *queue);

void tsqueue_destroy(ThreadSafeQueue *queue);

#endif