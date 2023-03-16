
#ifndef NATIVE_UTIL_H
#define NATIVE_UTIL_H

#include "macros.h"
#include "types.h"
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

static inline audiofs_buffer *audiofs_buffer_alloc(uint32_t size) {
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

    return buffer;
}

static inline bool audiofs_buffer_ok(audiofs_buffer *ctx) {
    if (!ctx || ctx->cookie != _AUDIOFS_CONTEXT_MAGIC_A || ctx->self != ctx) { return false; }

    return true;
}

/**
 * audiofs_buffer_realloc: ensures, the passed buffer is exactly the passed size.
 *
 * realloc with 0 size will free the underlying data buffer.
 *
 * The passed buffer must not have its lock held by another.
 * @param buffer
 * @param size
 * @return
 */
static inline bool audiofs_buffer_realloc(audiofs_buffer *buffer, uint32_t size) {
    if (!audiofs_buffer_ok(buffer)) { return false; }

    pthread_mutex_lock(&buffer->lock);

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

#endif // NATIVE_UTIL_H
