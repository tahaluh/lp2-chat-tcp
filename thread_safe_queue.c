#include "thread_safe_queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int tsqueue_init(ThreadSafeQueue *queue)
{
    if (!queue)
        return -1;

    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->capacity = MAX_QUEUE_SIZE;

    if (pthread_mutex_init(&queue->mutex, NULL) != 0)
    {
        return -1;
    }

    if (pthread_cond_init(&queue->not_empty, NULL) != 0)
    {
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }

    if (pthread_cond_init(&queue->not_full, NULL) != 0)
    {
        pthread_mutex_destroy(&queue->mutex);
        pthread_cond_destroy(&queue->not_empty);
        return -1;
    }

    return 0;
}

int tsqueue_enqueue(ThreadSafeQueue *queue, const Message *msg)
{
    if (!queue || !msg)
        return -1;

    pthread_mutex_lock(&queue->mutex);

    while (queue->count >= queue->capacity)
    {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    memcpy(&queue->messages[queue->rear], msg, sizeof(Message));
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);

    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int tsqueue_dequeue(ThreadSafeQueue *queue, Message *msg)
{
    if (!queue || !msg)
        return -1;

    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0)
    {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    memcpy(msg, &queue->messages[queue->front], sizeof(Message));
    queue->front = (queue->front + 1) % queue->capacity;
    queue->count--;

    pthread_cond_signal(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int tsqueue_try_enqueue(ThreadSafeQueue *queue, const Message *msg)
{
    if (!queue || !msg)
        return -1;

    pthread_mutex_lock(&queue->mutex);

    if (queue->count >= queue->capacity)
    {
        pthread_mutex_unlock(&queue->mutex);
        return -1; // Fila cheia
    }

    memcpy(&queue->messages[queue->rear], msg, sizeof(Message));
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);

    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int tsqueue_try_dequeue(ThreadSafeQueue *queue, Message *msg)
{
    if (!queue || !msg)
        return -1;

    pthread_mutex_lock(&queue->mutex);

    if (queue->count == 0)
    {
        pthread_mutex_unlock(&queue->mutex);
        return -1; // Fila vazia
    }

    memcpy(msg, &queue->messages[queue->front], sizeof(Message));
    queue->front = (queue->front + 1) % queue->capacity;
    queue->count--;

    pthread_cond_signal(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

bool tsqueue_empty(ThreadSafeQueue *queue)
{
    if (!queue)
        return true;

    pthread_mutex_lock(&queue->mutex);
    bool empty = (queue->count == 0);
    pthread_mutex_unlock(&queue->mutex);

    return empty;
}

bool tsqueue_full(ThreadSafeQueue *queue)
{
    if (!queue)
        return true;

    pthread_mutex_lock(&queue->mutex);
    bool full = (queue->count >= queue->capacity);
    pthread_mutex_unlock(&queue->mutex);

    return full;
}

int tsqueue_size(ThreadSafeQueue *queue)
{
    if (!queue)
        return -1;

    pthread_mutex_lock(&queue->mutex);
    int size = queue->count;
    pthread_mutex_unlock(&queue->mutex);

    return size;
}

void tsqueue_destroy(ThreadSafeQueue *queue)
{
    if (!queue)
        return;

    pthread_mutex_lock(&queue->mutex);

    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);

    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_mutex_destroy(&queue->mutex);
}