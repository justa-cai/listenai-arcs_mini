/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avi_io avi_io_t;

typedef enum {
    AVI_IO_SEEK_SET = 0,
    AVI_IO_SEEK_CUR = 1,
} avi_io_seek_whence_t;

typedef struct {
    int (*read)(avi_io_t *io, void *buf, size_t size, size_t *out_read);
    int (*seek)(avi_io_t *io, int64_t offset, avi_io_seek_whence_t whence);
    int (*close)(avi_io_t *io);
} avi_io_ops_t;

struct avi_io {
    const avi_io_ops_t *ops;
    void *ctx;
    int64_t pos;
};

static inline int avi_io_read(avi_io_t *io, void *buf, size_t size, size_t *out_read)
{
    if (!io || !io->ops || !io->ops->read) {
        return -22; /* -EINVAL */
    }
    return io->ops->read(io, buf, size, out_read);
}

static inline int avi_io_seek(avi_io_t *io, int64_t offset, avi_io_seek_whence_t whence)
{
    if (!io || !io->ops || !io->ops->seek) {
        return -22; /* -EINVAL */
    }
    return io->ops->seek(io, offset, whence);
}

static inline int avi_io_close(avi_io_t *io)
{
    if (!io || !io->ops || !io->ops->close) {
        return 0;
    }
    return io->ops->close(io);
}

/* Convenience openers used by the sample. */
int avi_io_open_lsfs(avi_io_t *io, const char *path);

#if defined(CONFIG_AVI_PLAYER_HTTP_RANGE)
int avi_io_open_http_range(avi_io_t *io, const char *url);
#endif

#ifdef __cplusplus
}
#endif
