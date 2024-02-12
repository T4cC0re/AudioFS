//
// Created by t4cc0re on 5/23/23.
//

#include "custom_avio.h"
#include "macros.h"
#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

__attribute__((__nonnull__)) int audiofs_avio_read(void *opaque, uint8_t *buf, int buf_size) {
    // Because FFmpeg only passes an int sized buffer, that is the max amount we can read, so converting to an int is
    // fine here.
    audiofs_avio_handle *handle = (audiofs_avio_handle *)opaque;
    if (buf_size < 0 || buf_size > INT32_MAX) {
        errorf("input wraparound\n");
        return -1;
    }
    uint64_t bs64 = buf_size;
    if (handle->in_memory) {
        uint64_t end_position = MIN(handle->position + bs64, handle->apparent_size);
        uint64_t read_count   = end_position - handle->position;
        memcpy(buf, handle->buffer->data, read_count);
        handle->position = end_position;
        return INT32(read_count);
    } else {
        int ret = INT32(read(handle->file, buf, bs64));
        // TODO: convert to ffmpeg error codes
        return ret;
    }
}

__attribute__((__nonnull__)) int64_t audiofs_avio_resize_to(audiofs_avio_handle *handle, uint64_t buf_size) {
    while (handle->buffer->len < MAX(buf_size, handle->buffer->len)) {
        uint64_t new_size = 0;
        // We need to resize the buffer. By how much?
        if (handle->buffer->len >= 16 * 1024 * 1024) { new_size = handle->buffer->len + 16 * 1024 * 1024; }
        if (handle->buffer->len < 16 * 1024 * 1024) { new_size = 16 * 1024 * 1024; }
        if (handle->buffer->len < 1024 * 1024) { new_size = 1024 * 1024; }
        if (handle->buffer->len < 1024 * 64) { new_size = 1024 * 64; }

        debugf("Resizing from %" PRIu64 " to %" PRIu64 " bytes\n", handle->buffer->len, new_size);
        if (new_size <= 0) {
            errorf("int wraparound\n");
            return -1;
        }
        if (!audiofs_buffer_realloc(handle->buffer, new_size)) {
            errorf("failed to resize\n");
            return -1;
        }
        debugf("Resizing to %" PRIu64 " bytes.\n", new_size);
    }
    return 0;
}

__attribute__((__nonnull__)) int audiofs_avio_write(void *opaque, uint8_t *buf, int buf_size) {
    // Because FFmpeg only passes an int sized buffer, that is the max amount we can read, so converting to an int is
    // fine here.
    audiofs_avio_handle *handle = (audiofs_avio_handle *)opaque;
    if (buf_size < 0 || buf_size > INT32_MAX) {
        errorf("input wraparound\n");
        return -1;
    }
    uint64_t bs64 = buf_size;
    if (handle->in_memory) {
        debugf(
            "Position %" PRIu64 ", request %" PRIu64 " bytes, size %" PRIu64 " bytes\n",
            handle->position,
            bs64,
            handle->buffer->len);
        if (0 != audiofs_avio_resize_to(handle, handle->position + bs64)) {
            errorf("failed to resize\n");
            return -1;
        }
        debugf("Writing %" PRIu64 " bytes to %p\n", bs64, handle);
        memcpy(handle->buffer->data, buf, bs64);
        handle->position += bs64;
        handle->apparent_size += bs64;
        return INT32(bs64);
    } else {
        int ret = INT32(write(handle->file, buf, buf_size));

        // TODO: convert to ffmpeg error codes
        return ret;
    }
}

__attribute__((__nonnull__)) int64_t audiofs_avio_seek(void *opaque, int64_t offset, int whence) {
    audiofs_avio_handle *handle = (audiofs_avio_handle *)opaque;
    if (offset < 0 || offset > INT64_MAX) {
        errorf("input wraparound\n");
        return -1;
    }
    int64_t target_offset = 0;
    if (handle->in_memory) {
        switch (whence) {
            case SEEK_CUR:
                target_offset = handle->position + offset;
                debugf("SEEK_CUR %" PRId64 ". Result: %" PRId64 "\n", offset, target_offset);
                break;
            case SEEK_SET:
                target_offset = offset;
                debugf("SEEK_SET %" PRId64 ". Result: %" PRId64 "\n", offset, target_offset);

                break;
            case SEEK_END:
                target_offset = handle->apparent_size + offset;
                debugf("SEEK_END %" PRId64 ". Result: %" PRId64 "\n", offset, target_offset);
                if (target_offset < 0) {
                    debugf("result negative. Invalid\n");
                    return -EINVAL;
                }

                break;
            default:
                errorf("unsupported whence %d\n", whence);
                return -EINVAL;
        }

        if (!WITHIN_BOUNDS(0, target_offset, handle->apparent_size)) {
            if (0 != audiofs_avio_resize_to(handle, target_offset)) {
                errorf("failed to resize\n");
                return -ENOMEM;
            }
        }
        handle->position = target_offset;
        return target_offset;
    } else {
        int64_t ret = lseek(((audiofs_avio_handle *)opaque)->file, offset, whence);
        // TODO: convert to ffmpeg error codes
        return ret;
    }
}

__attribute__((__nonnull__)) void *audiofs_avio_open(const char *filename) {
    char *tmp  = NULL;
    int   file = -1;

    AUDIOFS_PRINTVAL(tmp, "p");

    audiofs_avio_handle *handle = AUDIOFS_MALLOC(sizeof(audiofs_avio_handle));
    if (handle == NULL) {
        errorf("Failed to allocate handle");
        goto error;
    }

    // Because both `audiofs_avio_open` and `open` return an int file handle, the rest of the code (including the other
    // `audiofs_avio_*`-functions) works symantically identical.
    if (0 == memcmp(filename, "memory", strlen(filename))) {
        infof("using memory buffer");
        tmp = generate_random_string(16);
        debugf("shared memory name: %s\n", tmp);
        /*
         * First we allocate, then unlink a shared buffer with a random name.
         * We immediately unlink it, so it automatically gets destroyed when the file handle is closed.
         * This is done to prevent resource leaks.
         */
        file           = 0;
        handle->buffer = audiofs_buffer_alloc(1024);
        if (handle->buffer < 0) {
            errorf("Failed to allocate shared memory buffer");
            AUDIOFS_FREE(tmp);
            goto error;
        }
        debugf("shared memory handle: %p\n", handle);
        debugf("Initialized buffer to %" PRIu64 " bytes.\n", handle->buffer->len);
        handle->in_memory     = true;
        handle->apparent_size = 0;
        handle->position      = 0;
    } else {
        infof("file output requested. opening '%s'", filename);
        // Because someone explicitly wants stuff written to a file, we use O_SYNC to ensure everything is perfect.
        file = open(filename, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if (file < 0) {
            errorf("Failed to open file");
            return NULL;
        }
        handle->file          = file;
        handle->in_memory     = false;
        handle->apparent_size = 0;
        handle->position      = 0;
    }
    return handle;

error:
    AUDIOFS_FREE(handle);
    AUDIOFS_FREE(tmp);
    return NULL;
}

__attribute__((__nonnull__)) void audiofs_avio_close(audiofs_avio_handle **handle) {
    if ((*handle)->in_memory) {
        AUDIOFS_FREE((*handle)->buffer);
    } else {
        // We might change the way this works, but currently, `sync` is provided by O_SYNC in `audiofs_avio_open`.
        // TODO: Error checking
        close((*handle)->file);
    }
    AUDIOFS_FREE(*handle);
}

__attribute__((__nonnull__)) off_t audiofs_avio_get_size(audiofs_avio_handle *handle) {
    if (handle->in_memory) {
        return handle->apparent_size;
    } else {
        struct stat s;
        int         status = fstat(handle->file, &s); // TODO: Check return
        if (status != 0) {
            errorf("returned file handle is not okay.");
            return -1;
        }

        if (s.st_size == 0) {
            errorf("returned file handle is has 0 bytes.");
            return -1;
        }
        return s.st_size;
    }
}

__attribute__((__nonnull__)) bool audiofs_avio_is_memory_backed(audiofs_avio_handle *handle) {
    // TODO: Error checking
    return handle->in_memory;
}
