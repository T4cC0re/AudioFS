#ifndef NATIVE_MACROS_H
#define NATIVE_MACROS_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define AUDIOFS_LOG_PANIC 0
#define AUDIOFS_LOG_FATAL 1
#define AUDIOFS_LOG_ERROR 2
#define AUDIOFS_LOG_WARN  3
#define AUDIOFS_LOG_INFO  4
#define AUDIOFS_LOG_DEBUG 5
#define AUDIOFS_LOG_TRACE 6

// Make sure all printf is redirected to stderr.
#undef printf
#ifndef AUDIOFS_CGO
#    define printf(...)             _Pragma("GCC warning \"printf is discouraged, use errorf explicitly\"") errorf(__VA_ARGS__)
#    define audiofs_log(level, ...) fprintf(stderr, __VA_ARGS__)
#else

// Defined in glue.go
extern void go_print(int level, const char *function, const char *file, int line, const char *log_msg, int len);

// Defined in logbuffer.c
extern pthread_mutex_t go_log_mutex;
extern char *          go_log_buffer;
extern int             go_log_buffer_len;
extern int             go_log_level;

#    define audiofs_log(level, ...)                                                                                 \
        ({                                                                                                          \
            if (level <= go_log_level) {                                                                            \
                pthread_mutex_lock(&go_log_mutex);                                                                  \
                int len = snprintf(NULL, 0, __VA_ARGS__);                                                           \
                if (go_log_buffer == NULL) {                                                                        \
                    go_log_buffer = (char *)malloc(len + 1);                                                        \
                } else {                                                                                            \
                    if (go_log_buffer_len < (len + 1)) { go_log_buffer = (char *)realloc(go_log_buffer, len + 1); } \
                }                                                                                                   \
                if (go_log_buffer == NULL) {                                                                        \
                    fprintf(stderr, "[AUDIOFS_C EXCEPTION] Could not allocate log buffer!\n");                      \
                } else {                                                                                            \
                    snprintf(go_log_buffer, len + 1, __VA_ARGS__);                                                  \
                    go_print(level, __FUNCTION__, __FILE__, __LINE__, go_log_buffer, len);                          \
                }                                                                                                   \
                pthread_mutex_unlock(&go_log_mutex);                                                                \
            }                                                                                                       \
        })

#    define printf(...) audiofs_log(AUDIOFS_LOG_INFO, __VA_ARGS__)
#endif
#define errorf(...) audiofs_log(AUDIOFS_LOG_ERROR, __VA_ARGS__)
#define warnf(...)  audiofs_log(AUDIOFS_LOG_WARN, __VA_ARGS__)
#define infof(...)  audiofs_log(AUDIOFS_LOG_INFO, __VA_ARGS__)
#define debugf(...) audiofs_log(AUDIOFS_LOG_DEBUG, __VA_ARGS__)
#define tracef(...) audiofs_log(AUDIOFS_LOG_TRACE, __VA_ARGS__)

#ifdef AUDIOFS_NO_TRACE
#    define AUDIOFS_PRINTVAL_PREFIX(val, print_type, prefix) \
        {}
#    define AUDIOFS_PRINTVAL_PREFIX2(val, print_type, prefix) \
        {}
#    define AUDIOFS_PRINTVAL(val, print_type) \
        {}
#else
#    define AUDIOFS_PRINTVAL_PREFIX(val, print_type, prefix) tracef("%s:" #    val "=%" print_type, prefix, val)
#    define AUDIOFS_PRINTVAL(val, print_type)                tracef(#    val "=%" print_type, val)
#endif

#define MIN(X, Y)                      (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)                      (((X) > (Y)) ? (X) : (Y))
#define WITHIN_BOUNDS(min, check, max) (check > min ? (check <= max ? check : max) : min)

// Use zero-initialized calloc rather than malloc
#define AUDIOFS_MALLOC_NO_TRACE(size)        AUDIOFS_CALLOC_NO_TRACE(1, size)
#define AUDIOFS_CALLOC_NO_TRACE(count, size) calloc(count, (size_t)(size))
// Free and set to NULL, but only if the incoming pointer is not NULL already
#define AUDIOFS_FREE_NO_TRACE(ptr) \
    {                              \
        if ((ptr) != NULL) {       \
            free(ptr);             \
            (ptr) = NULL;          \
        }                          \
    }

#ifdef AUDIOFS_NO_TRACE
#    define AUDIOFS_MALLOC AUDIOFS_MALLOC_NO_TRACE
#    define AUDIOFS_CALLOC AUDIOFS_CALLOC_NO_TRACE
#    define AUDIOFS_FREE   AUDIOFS_FREE_NO_TRACE
#else
#    define AUDIOFS_MALLOC(size)                                                \
        ({                                                                      \
            void *pointer;                                                      \
            AUDIOFS_PRINTVAL_PREFIX((size_t)(size), PRIuPTR, "AUDIOFS_MALLOC"); \
            pointer = AUDIOFS_MALLOC_NO_TRACE(size);                            \
            AUDIOFS_PRINTVAL_PREFIX(pointer, "p", "AUDIOFS_MALLOC");            \
            pointer;                                                            \
        })
#    define AUDIOFS_CALLOC(count, size)                                         \
        ({                                                                      \
            void *pointer;                                                      \
            AUDIOFS_PRINTVAL_PREFIX(count, "d", "AUDIOFS_CALLOC");              \
            AUDIOFS_PRINTVAL_PREFIX((size_t)(size), PRIuPTR, "AUDIOFS_CALLOC"); \
            pointer = AUDIOFS_CALLOC_NO_TRACE(count, size);                     \
            AUDIOFS_PRINTVAL_PREFIX(pointer, "p", "AUDIOFS_CALLOC");            \
            pointer;                                                            \
        })
#    define AUDIOFS_FREE(ptr)                                        \
        AUDIOFS_PRINTVAL_PREFIX((void *)(ptr), "p", "AUDIOFS_FREE"); \
        AUDIOFS_FREE_NO_TRACE(ptr)
#endif

#define LOCK_ACTIVE_CONTEXT()   pthread_mutex_lock(&active_context_lock)
#define UNLOCK_ACTIVE_CONTEXT() pthread_mutex_unlock(&active_context_lock)

#endif // NATIVE_MACROS_H
