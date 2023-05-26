//
// Created by t4cc0re on 5/23/23.
//

#include "custom_avio.h"
#include "macros.h"
#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

struct audiofs_avio_handle {
    int  file;
    bool in_memory;
};

__attribute__((__nonnull__)) int audiofs_avio_read(void *opaque, uint8_t *buf, int buf_size) {
    // Because FFmpeg only passes an int sized buffer, that is the max amount we can read, so converting to an int is
    // fine here.
    int ret = INT32(read(((audiofs_avio_handle *)opaque)->file, buf, buf_size));

    // TODO: convert to ffmpeg error codes
    return ret;
}

__attribute__((__nonnull__)) int audiofs_avio_write(void *opaque, uint8_t *buf, int buf_size) {
    // Because FFmpeg only passes an int sized buffer, that is the max amount we can read, so converting to an int is
    // fine here.
    int ret = INT32(write(((audiofs_avio_handle *)opaque)->file, buf, buf_size));

    // TODO: convert to ffmpeg error codes
    return ret;
}

__attribute__((__nonnull__)) int64_t audiofs_avio_seek(void *opaque, int64_t offset, int whence) {
    int64_t ret = lseek(((audiofs_avio_handle *)opaque)->file, offset, whence);

    // TODO: convert to ffmpeg error codes
    return ret;
}

__attribute__((__nonnull__)) void *audiofs_avio_open(const char *filename) {
    char *tmp  = NULL;
    int   file = -1;
    int   ret  = 0;

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
        file = shm_open(tmp, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (file < 0) {
            errorf("Failed to allocate shared memory buffer");
            AUDIOFS_FREE(tmp);
            goto error;
        }
        debugf("shared memory handle: %d\n", file);
        ret = shm_unlink(tmp);
        if (ret != 0) {
            errorf("Failed to unlink shared memory buffer");
            AUDIOFS_FREE(tmp);
            goto error;
        }
        handle->in_memory = true;
    } else {
        infof("file output requested. opening '%s'", filename);
        // Because someone explicitly wants stuff written to a file, we use O_SYNC to ensure everything is perfect.
        file = open(filename, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if (file < 0) {
            errorf("Failed to open file");
            return NULL;
        }
        handle->in_memory = false;
    }
    handle->file = file;
    return handle;

error:
    AUDIOFS_FREE(handle);
    AUDIOFS_FREE(tmp);
    return NULL;
}

__attribute__((__nonnull__)) void audiofs_avio_close(audiofs_avio_handle **handle) {
    // We might change the way this works, but currently, `sync` is provided by O_SYNC in `audiofs_avio_open`.
    // TODO: Error checking
    close((*handle)->file);
    AUDIOFS_FREE(*handle);
}

__attribute__((__nonnull__)) int audiofs_avio_get_handle(audiofs_avio_handle *handle) {
    // TODO: Error checking
    return handle->file;
}

__attribute__((__nonnull__)) bool audiofs_avio_is_memory_backed(audiofs_avio_handle *handle) {
    // TODO: Error checking
    return handle->in_memory;
}
