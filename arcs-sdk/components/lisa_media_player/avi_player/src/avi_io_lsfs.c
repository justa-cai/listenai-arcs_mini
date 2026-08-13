/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "avi_io.h"

#include <errno.h>
#include <string.h>

#include <lisa_mem.h>

#include "lsfs.h"

typedef struct {
    struct lsfs_file_t fp;
    int opened;
} avi_lsfs_ctx_t;

static int lsfs_read_cb(avi_io_t *io, void *buf, size_t size, size_t *out_read)
{
    if (out_read) {
        *out_read = 0;
    }
    if (!io || !io->ctx || !buf) {
        return -EINVAL;
    }

    avi_lsfs_ctx_t *ctx = (avi_lsfs_ctx_t *)io->ctx;
    ssize_t r = lsfs_read(&ctx->fp, buf, size);
    if (r < 0) {
        return (int)r;
    }

    if (out_read) {
        *out_read = (size_t)r;
    }
    io->pos += (int64_t)r;
    return 0;
}

static int lsfs_seek_cb(avi_io_t *io, int64_t offset, avi_io_seek_whence_t whence)
{
    if (!io || !io->ctx) {
        return -EINVAL;
    }

    avi_lsfs_ctx_t *ctx = (avi_lsfs_ctx_t *)io->ctx;
    int w = LSFS_SEEK_SET;
    if (whence == AVI_IO_SEEK_CUR) {
        w = LSFS_SEEK_CUR;
    }

    int ret = lsfs_seek(&ctx->fp, (off_t)offset, w);
    if (ret != 0) {
        return ret;
    }

    if (whence == AVI_IO_SEEK_SET) {
        io->pos = offset;
    } else {
        io->pos += offset;
    }
    return 0;
}

static int lsfs_close_cb(avi_io_t *io)
{
    if (!io || !io->ctx) {
        return 0;
    }

    avi_lsfs_ctx_t *ctx = (avi_lsfs_ctx_t *)io->ctx;
    if (ctx->opened) {
        (void)lsfs_close(&ctx->fp);
        ctx->opened = 0;
    }
    lisa_mem_free(ctx);

    io->ctx = NULL;
    io->ops = NULL;
    io->pos = 0;
    return 0;
}

static const avi_io_ops_t s_lsfs_ops = {
    .read = lsfs_read_cb,
    .seek = lsfs_seek_cb,
    .close = lsfs_close_cb,
};

int avi_io_open_lsfs(avi_io_t *io, const char *path)
{
    if (!io || !path || path[0] == '\0') {
        return -EINVAL;
    }

    memset(io, 0, sizeof(*io));

    avi_lsfs_ctx_t *ctx = (avi_lsfs_ctx_t *)lisa_mem_alloc(sizeof(*ctx));
    if (!ctx) {
        return -ENOMEM;
    }
    memset(ctx, 0, sizeof(*ctx));

    lsfs_file_t_init(&ctx->fp);
    int ret = lsfs_open(&ctx->fp, path, LSFS_O_READ);
    if (ret != 0) {
        lisa_mem_free(ctx);
        return ret;
    }

    ctx->opened = 1;
    io->ops = &s_lsfs_ops;
    io->ctx = ctx;
    io->pos = 0;
    return 0;
}
