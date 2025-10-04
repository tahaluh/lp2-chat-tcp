#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "tslog.h"

#define N_THREADS 4

void *escreve_log(void *arg)
{
    int id = *(int *)arg;
    char msg[100];

    for (int i = 0; i < 5; i++)
    {
        snprintf(msg, sizeof(msg), "Thread %d - linha %d", id, i + 1);
        tslog_write(msg);
        usleep(200000); // pausa 0.2s
    }

    return NULL;
}

int main()
{
    pthread_t th[N_THREADS];
    int ids[N_THREADS];

    tslog_init("saida.log");

    for (int i = 0; i < N_THREADS; i++)
    {
        ids[i] = i + 1;
        pthread_create(&th[i], NULL, escreve_log, &ids[i]);
    }

    for (int i = 0; i < N_THREADS; i++)
    {
        pthread_join(th[i], NULL);
    }

    tslog_close();
    printf("Concluído. Veja o arquivo saida.log\n");

    return 0;
}
