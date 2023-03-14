#ifndef NATIVE_MACROS_H
#define NATIVE_MACROS_H

#include <stdio.h>
#include <stdlib.h>

// Make sure all printf is redirected to stderr.
#undef printf
#define printf(...) \
    _Pragma("GCC warning \"printf is discouraged, use errorf or fprintf explicitly\"") fprintf(stderr, __VA_ARGS__)
#define errorf(...) fprintf(stderr, __VA_ARGS__)

#ifdef AUDIOFS_NO_TRACE
#    define AUDIOFS_PRINTVAL_PREFIX(val, print_type, prefix) \
        {}
#    define AUDIOFS_PRINTVAL_PREFIX2(val, print_type, prefix) \
        {}
#    define AUDIOFS_PRINTVAL(val, print_type) \
        {}
#else
#    define AUDIOFS_PRINTVAL_PREFIX(val, print_type, prefix) \
        errorf(__FILE__ ":%d\t(%s) >>\t" #val ":\t%" print_type "\n", __LINE__, prefix, val)
#    define AUDIOFS_PRINTVAL_PREFIX2(val, print_type, prefix) \
        errorf(__FILE__ ":%d\t(%s::%s) >>\t" #val ":\t%" print_type "\n", __LINE__, __FUNCTION__, prefix, val)
#    define AUDIOFS_PRINTVAL(val, print_type) AUDIOFS_PRINTVAL_PREFIX(val, print_type, __FUNCTION__)
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
#    define AUDIOFS_MALLOC(size)                                                 \
        ({                                                                       \
            void *pointer;                                                       \
            AUDIOFS_PRINTVAL_PREFIX2((size_t)(size), PRIuPTR, "AUDIOFS_MALLOC"); \
            pointer = AUDIOFS_MALLOC_NO_TRACE(size);                             \
            AUDIOFS_PRINTVAL_PREFIX2(pointer, "p", "AUDIOFS_MALLOC");            \
            pointer;                                                             \
        })
#    define AUDIOFS_CALLOC(count, size)                                          \
        ({                                                                       \
            void *pointer;                                                       \
            AUDIOFS_PRINTVAL_PREFIX2(count, "d", "AUDIOFS_CALLOC");              \
            AUDIOFS_PRINTVAL_PREFIX2((size_t)(size), PRIuPTR, "AUDIOFS_CALLOC"); \
            pointer = AUDIOFS_CALLOC_NO_TRACE(count, size);                      \
            AUDIOFS_PRINTVAL_PREFIX2(pointer, "p", "AUDIOFS_CALLOC");            \
            pointer;                                                             \
        })
#    define AUDIOFS_FREE(ptr)                                         \
        AUDIOFS_PRINTVAL_PREFIX2((void *)(ptr), "p", "AUDIOFS_FREE"); \
        AUDIOFS_FREE_NO_TRACE(ptr)
#endif

#define LOCK_ACTIVE_CONTEXT()   pthread_mutex_lock(&active_context_lock)
#define UNLOCK_ACTIVE_CONTEXT() pthread_mutex_unlock(&active_context_lock)

#endif // NATIVE_MACROS_H
