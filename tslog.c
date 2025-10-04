#include "tslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

static FILE *arquivo_log = NULL;
static pthread_mutex_t mutex_log;

void tslog_init(const char *nome_arquivo)
{
    if (pthread_mutex_init(&mutex_log, NULL) != 0)
    {
        fprintf(stderr, "Falha ao inicializar mutex do log\n");
        exit(1);
    }

    arquivo_log = fopen(nome_arquivo, "a");
    if (arquivo_log == NULL)
    {
        perror("Não foi possível abrir o arquivo de log");
        exit(1);
    }
}

void tslog_write(const char *mensagem)
{
    pthread_mutex_lock(&mutex_log);

    time_t agora;
    time(&agora);

    struct tm *info_tempo = localtime(&agora);
    char buffer_tempo[32];
    strftime(buffer_tempo, sizeof(buffer_tempo), "%Y-%m-%d %H:%M:%S", info_tempo);

    fprintf(arquivo_log, "[%s] %s\n", buffer_tempo, mensagem);
    fflush(arquivo_log);

    pthread_mutex_unlock(&mutex_log);
}

void tslog_close()
{
    if (arquivo_log != NULL)
    {
        fclose(arquivo_log);
        arquivo_log = NULL;
    }
    pthread_mutex_destroy(&mutex_log);
}
