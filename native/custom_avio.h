//
// Created by t4cc0re on 5/23/23.
//

#ifndef NATIVE_CUSTOM_AVIO_H
#define NATIVE_CUSTOM_AVIO_H

#include <inttypes.h>

int audiofs_avio_read(void *opaque, uint8_t *buf, int buf_size);

int audiofs_avio_write(void *opaque, uint8_t *buf, int buf_size);

int64_t audiofs_avio_seek(void *opaque, int64_t offset, int whence);

int audiofs_avio_open();

void audiofs_avio_close(int handle);

#endif // NATIVE_CUSTOM_AVIO_H
