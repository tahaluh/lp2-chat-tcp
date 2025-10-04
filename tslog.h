#ifndef TSLOG_H
#define TSLOG_H

#include <pthread.h>

void tslog_init(const char *filename);
void tslog_write(const char *message);
void tslog_close();

#endif
