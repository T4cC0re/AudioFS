
#ifndef NATIVE_UTIL_H
#define NATIVE_UTIL_H

#include "macros.h"
#include "types.h"
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

__attribute__((__warn_unused_result__)) static inline audiofs_buffer *audiofs_buffer_alloc(uint64_t size) {
    audiofs_buffer *buffer = AUDIOFS_MALLOC(sizeof(audiofs_buffer));
    if (buffer == NULL) { return NULL; }
    buffer->cookie = _AUDIOFS_CONTEXT_MAGIC_A;
    buffer->self   = buffer;
    buffer->len    = size;

    // If the caller just wanted to allocate this structure, let them.
    if (size == 0) { return buffer; }

    buffer->data = AUDIOFS_MALLOC(size);
    if (buffer->data == NULL) {
        // Alloc failed. Clear temporary memory and bail hard.
        AUDIOFS_FREE(buffer);
        return NULL;
    }
    pthread_mutex_unlock(&buffer->used_outside_audiofs);
    pthread_mutex_unlock(&buffer->lock);

    return buffer;
}

__attribute((pure)) __attribute__((__warn_unused_result__)) static inline bool audiofs_buffer_ok(audiofs_buffer *ctx) {
    if (!ctx || ctx->cookie != _AUDIOFS_CONTEXT_MAGIC_A || ctx->self != ctx) { return false; }

    return true;
}

__attribute__((__warn_unused_result__)) static inline char *generate_random_string(int length) {
    char *random_string = NULL;
    int   urandom_fd    = -1;
    int   i;

    if (length < 1) {
        errorf("Error: length should be >= 1\n");
        return NULL;
    }

    random_string = malloc(sizeof(char) * (length + 1));
    if (!random_string) {
        errorf("Error: couldn't allocate memory for the string\n");
        return NULL;
    }

    urandom_fd = open("/dev/urandom", O_RDONLY);
    if (urandom_fd == -1) {
        errorf("Error: couldn't open /dev/urandom\n");
        free(random_string);
        return NULL;
    }

    for (i = 0; i < length; ++i) {
        unsigned char random_byte = 0;
        if (read(urandom_fd, &random_byte, sizeof(random_byte)) != sizeof(random_byte)) {
            printf("Error: couldn't read from /dev/urandom\n");
            free(random_string);
            close(urandom_fd);
            return NULL;
        }
        random_byte = random_byte % 62;
        if (random_byte < 10) {
            random_string[i] = '0' + random_byte;
        } else if (random_byte < 36) {
            random_string[i] = 'A' + (random_byte - 10);
        } else {
            random_string[i] = 'a' + (random_byte - 36);
        }
    }

    random_string[length] = '\0';
    close(urandom_fd);

    return random_string;
}

/**
 * audiofs_buffer_realloc: ensures, the passed buffer is exactly the passed size.
 *
 * realloc with 0 size will free the underlying data buffer.
 *
 * The passed buffer must not have its lock held by another.
 * If the passed buffer is marked as being used outside C, it will do nothing and return false
 * @param buffer
 * @param size
 * @return whether the buffer was resized.
 */
__attribute__((__warn_unused_result__)) static inline bool
audiofs_buffer_realloc(audiofs_buffer *buffer, uint64_t size) {
    if (!audiofs_buffer_ok(buffer)) { return false; }

    pthread_mutex_lock(&buffer->lock);

    //    AUDIOFS_PRINTVAL(buffer->used_outside_audiofs, "p");
    // If used outside C, we will block every other action
    //    if (0 != pthread_mutex_trylock(&buffer->used_outside_audiofs)) {
    //        errorf("tried to resize a buffer which was passed outside of AudioFS. Denied.");
    //        AUDIOFS_PRINTVAL(buffer, "p");
    //        return false;
    //    }
    // Because modifications (including taking usage outside C) happens with a locked buffer, we can safely call
    // unlock here.
    pthread_mutex_unlock(&buffer->used_outside_audiofs);

    if (size == 0) {
        AUDIOFS_FREE_NO_TRACE(buffer->data);
        buffer->len = 0;
        goto ret;
    }

    if (size != buffer->len) {
        // reallocate with new size. New memory will be 0-initialized
        void *new_ptr = realloc(buffer->data, size);
        if (new_ptr == NULL) {
            // Could not reallocate memory (OOM, or other). Memory is unchanged
            pthread_mutex_unlock(&buffer->lock);
            return false;
        }
        // blank out new regions if realloc is larger
        if (buffer->len > size) { memset(new_ptr + buffer->len, 0, buffer->len - size); }
        buffer->len  = size;
        buffer->data = new_ptr;
       // c_frees += portable_ish_malloced_size(buffer->data);
       // c_allocs += size;
    }

    // emergency check to provide guarantee:
    if (buffer->len != 0 && buffer->data == NULL) {
        buffer->data = AUDIOFS_MALLOC_NO_TRACE(buffer->len);
        // Technically this *can* return a nullptr if OOM, but then we're screwed anyways.
    }

ret:
    pthread_mutex_unlock(&buffer->lock);
    return true;
}

__attribute((pure)) __attribute__((__warn_unused_result__)) static inline bool context_ok(struct decoder_context *ctx) {
    if (!ctx || ctx->cookieA != _AUDIOFS_CONTEXT_MAGIC_A || ctx->cookieB != _AUDIOFS_CONTEXT_MAGIC_B) { return false; }

    return true;
}

/**
 * @param sample_fmt    libav sample format (AV_SAMPLE_FMT_*)
 * @param depth         known bit-width for 32-bit formats
 * @return
 */
__attribute((always_inline)) static inline uint8_t sample_bits_by_format(int32_t sample_fmt, uint8_t depth) {
    switch (sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            return 8;
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            return 64;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return 32;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return 16;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            return (uint8_t)(depth == 24 ? 24 : 32);
        default:
            return 0;
    }
}

#endif // NATIVE_UTIL_H
