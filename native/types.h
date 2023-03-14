
#ifndef NATIVE_TYPES_H
#define NATIVE_TYPES_H

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#define _AUDIOFS_CONTEXT_MAGIC_A 0xd0d0cafe
#define _AUDIOFS_CONTEXT_MAGIC_B 0xdeadbeef

typedef struct audiofs_buffer {
    int             cookie;
    void *          data;
    uint32_t        len;
    pthread_mutex_t lock;
    void *          self;
} audiofs_buffer;

#endif // NATIVE_TYPES_H
