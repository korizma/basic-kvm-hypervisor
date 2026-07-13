#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>


#define BUFFER_SIZE 100

struct buffer
{
    uint32_t buffer[BUFFER_SIZE];
    uint32_t size;

    uint32_t reader_num;
    uint32_t readers_finished;

    bool reading, writing;

    pthread_mutex_t mutex;
};

void reset_buffer(struct buffer* buffer);

#endif // BUFFER_H