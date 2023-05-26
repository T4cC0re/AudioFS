//
// Created by t4cc0re on 5/23/23.
//

#ifndef NATIVE_CUSTOM_AVIO_H
#define NATIVE_CUSTOM_AVIO_H

#include <inttypes.h>
#include <stdbool.h>

typedef struct audiofs_avio_handle audiofs_avio_handle;

/**
 * Wrapped version of `read` with FFmpeg error mappings
 *
 * attempts to read up to `buf_size` bbytes from file descriptor `opaque` into the buffer starting at `buf`. *
 *
 * @param opaque FFmpeg opaque pointer (AudioFS AVIO handle)
 * @param buf
 * @param buf_size
 * @return -1 on error, bytes read on success. errno is set to indicate the error.
 */
int audiofs_avio_read(void *opaque, uint8_t *buf, int buf_size);

/**
 * Wrapped version of `write` with FFmpeg error mappings
 *
 * writes up to `buf_size` bytes from the buffer starting at `buf` to the file referred to by the file descriptor
 * `opaque`.
 *
 * @param opaque FFmpeg opaque pointer (AudioFS AVIO handle)
 * @param buf
 * @param buf_size
 * @return -1 on error, bytes written on success. errno is set to indicate the error.
 */
int audiofs_avio_write(void *opaque, uint8_t *buf, int buf_size);

/**
 * Wrapped version of `lseek` with FFmpeg error mappings.
 *
 * Move FD's file position to `offset` bytes from the beginning of the file (if `whence` is SEEK_SET), the current
 * position (if `whence` is SEEK_CUR), or the end of the file (if `whence` is SEEK_END). Return the new file position.
 *
 * @param opaque FFmpeg opaque pointer (AudioFS AVIO handle)
 * @param offset
 * @param whence
 * @return new file position
 */
int64_t audiofs_avio_seek(void *opaque, int64_t offset, int whence);

/**
 * Create a new file handle to be used as an opaque pointer in FFmpeg.
 *
 * A memory backed file is possible. It will not be written to the filesystem and vanishes on close.
 * A filesystem backed file is opened with O_SYNC and any existing contents are truncated. No auto deletion on close.
 *
 * Currently the returned value is just a file handle (int) cast to a void*, but this might change, so do not rely on
 * this!
 * Use `audiofs_avio_get_handle` for a forward-compatible way to access the underlying file handle.
 *
 * @param filename Filename to open. 'memory' if a memory backed file is desired.
 * @return FFmpeg opaque pointer or NULL on error. Call `audiofs_avio_close` on it afterwards to prevent resource leaks.
 */
void *audiofs_avio_open(const char *filename);

/**
 * Closes an AudioFS AVIO handle.
 *
 * If 'memory' was used to open the file, no further access is possible afterwards.
 * If a filename was provided, the file will exist and by `sync`ed to the filesystem.
 *
 * Any memory allocated by `audiofs_avio_open` is freed. Any file handles are closed.
 *
 * @param opaque reference to AudioFS AVIO handle
 */
void audiofs_avio_close(audiofs_avio_handle **handle);

/**
 * Returns a file handle from a AudioFS AVIO handle.
 *
 * @param handle AudioFS AVIO handle
 * @return file handle on success, or -1 on error.
 */
int audiofs_avio_get_handle(audiofs_avio_handle *handle);

/**
 * Returns whether an AudioFS AVIO handle is in-memory, or on-disk.
 *
 * @param handle AudioFS AVIO handle
 * @return true if memory backed, false if file backed or invalid handle.
 */
bool audiofs_avio_is_memory_backed(audiofs_avio_handle *handle);

#endif // NATIVE_CUSTOM_AVIO_H
