//
// Created by t4cc0re on 5/23/23.
//

#include "custom_avio.h"
#include "macros.h"
#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int audiofs_avio_read(void *opaque, uint8_t *buf, int buf_size) {
    int ret = read((int)opaque, buf, buf_size);

    // TODO: convert to ffmpeg error codes
    return ret;
}

int audiofs_avio_write(void *opaque, uint8_t *buf, int buf_size) {
    int ret = write((int)opaque, buf, buf_size);

    // TODO: convert to ffmpeg error codes
    return ret;
}

int64_t audiofs_avio_seek(void *opaque, int64_t offset, int whence) {
    int ret = lseek((int)opaque, offset, whence);
    // TODO: convert to ffmpeg error codes
    return ret;
}

int audiofs_avio_open() {
    char *tmp  = generate_random_string(16);
    int   file = -1;
    int   ret  = 0;

    AUDIOFS_PRINTVAL(tmp, "p");

    infof("shared memory name: %s\n", tmp);

    /*
     * First we allocate, then unlink a shared buffer with a random name.
     * We immediately unlink it, so it automatically gets destroyed when
     * the file handle is closed.
     * This is done to prevent resource leaks.
     */
    file = shm_open(tmp, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (file == -1) {
        errorf("Failed to allocate shared memory buffer");
        return -1;
    }
    infof("shared memory handle: %d\n", file);
    ret = shm_unlink(tmp);
    if (ret != 0) {
        errorf("Failed to unlink shared memory buffer");
        return -1;
    }

    return file;
}

void audiofs_avio_close(int handle) { close(handle); }
