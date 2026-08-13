/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "avi_io.h"

#if defined(CONFIG_AVI_PLAYER_HTTP_RANGE)

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_thread.h>

#include "HTTPCUsr_api.h"

#ifndef LOG_TAG
#define LOG_TAG "avi_http"
#endif

typedef struct {
    HTTPParameters params;
    char url[HTTP_CLIENT_MAX_URL_LENGTH];
    char hdr_buf[192];
    int opened;
    int64_t base; /* 请求的 range 起始偏移 */
    int eof;
    int64_t eof_pos;

    /* 预读缓存：避免很小的 read（例如 8 字节的 chunk header） */
    uint8_t *cache;
    uint32_t cache_cap;
    uint32_t cache_len;
    uint32_t cache_off;
} avi_http_range_ctx_t;

/* httpclient header callback 不带参数：用一个全局指针指向“当前活跃实例”。 */
static avi_http_range_ctx_t *s_active_ctx;

static void *http_range_get_header(void)
{
    if (!s_active_ctx) {
        return NULL;
    }
    return s_active_ctx->hdr_buf;
}

static int http_open_at(avi_http_range_ctx_t *ctx, int64_t offset)
{
    if (!ctx) {
        return -EINVAL;
    }

    if (ctx->opened) {
        (void)HTTPC_close(&ctx->params);
        ctx->opened = 0;
    }

    if (ctx->eof && offset >= ctx->eof_pos) {
        return 0;
    }
    ctx->eof = 0;

    /* 每次 (re)open 时重置缓存 */
    ctx->cache_len = 0;
    ctx->cache_off = 0;

    memset(&ctx->params, 0, sizeof(ctx->params));
    strncpy(ctx->params.Uri, ctx->url, sizeof(ctx->params.Uri) - 1);
    ctx->params.Uri[sizeof(ctx->params.Uri) - 1] = '\0';

    ctx->params.HttpVerb = VerbGet;
    ctx->params.Verbose = 0;
    ctx->params.AuthType = AuthSchemaNone;
    ctx->params.Flags = 0;
    ctx->params.pData = NULL;
    ctx->params.pLength = 0;
    ctx->params.nTimeout = 15;

    int ret = HTTPC_open(&ctx->params);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "HTTPC_open failed ret=%d url=%s", ret, ctx->params.Uri);
        return -EIO;
    }

    /* 构造本次请求的自定义 header */
    ctx->hdr_buf[0] = '\0';
    if (offset > 0) {
        /* 禁用 gzip/deflate，确保拿到原始字节流。 */
        (void)snprintf(ctx->hdr_buf,
                       sizeof(ctx->hdr_buf),
                       "Range: bytes=%lld-\r\nAccept-Encoding: identity\r\n",
                       (long long)offset);
    } else {
        (void)snprintf(ctx->hdr_buf, sizeof(ctx->hdr_buf), "Accept-Encoding: identity\r\n");
    }

    s_active_ctx = ctx;
    ret = HTTPC_request(&ctx->params, http_range_get_header);
    s_active_ctx = NULL;
    if (ret != 0) {
        LISA_LOGE(LOG_TAG,
                  "HTTPC_request failed ret=%d url=%s hdr=\"%s\"",
                  ret,
                  ctx->params.Uri,
                  ctx->hdr_buf);
        (void)HTTPC_close(&ctx->params);
        ctx->opened = 0;
        return -EIO;
    }

    HTTP_CLIENT httpClient;
    memset(&httpClient, 0, sizeof(httpClient));
    (void)HTTPC_get_request_info(&ctx->params, &httpClient);

#if 0
    LISA_LOGI(LOG_TAG,
              "HTTP status=%lu pos=%lld total_len=%lu rx=%lu state=0x%lx url=%s",
              (unsigned long)httpClient.HTTPStatusCode,
              (long long)offset,
              (unsigned long)httpClient.TotalResponseBodyLength,
              (unsigned long)httpClient.ResponseBodyLengthReceived,
              (unsigned long)httpClient.HttpState,
              ctx->params.Uri);
#endif

    /*
     * 最小检查：
     * - offset==0：接受 200 或 206
     * - offset>0：必须是 206；若返回 200 通常表示服务端忽略 Range（则不支持 seek）。
     */
    if (offset == 0) {
        if (httpClient.HTTPStatusCode != HTTP_STATUS_OK && httpClient.HTTPStatusCode != 206) {
            LISA_LOGE(LOG_TAG, "unexpected status=%lu (need 200/206)", (unsigned long)httpClient.HTTPStatusCode);
            (void)HTTPC_close(&ctx->params);
            ctx->opened = 0;
            return -EIO;
        }
    } else {
        if (httpClient.HTTPStatusCode == 416) {
            ctx->eof = 1;
            ctx->eof_pos = offset;
            (void)HTTPC_close(&ctx->params);
            ctx->opened = 0;
            return 0;
        }
        if (httpClient.HTTPStatusCode != 206) {
            LISA_LOGE(LOG_TAG, "HTTP Range not supported? status=%lu", (unsigned long)httpClient.HTTPStatusCode);
            (void)HTTPC_close(&ctx->params);
            ctx->opened = 0;
            return -ENOTSUP;
        }
    }

    ctx->opened = 1;
    ctx->base = offset;
    return 0;
}

static int http_read_cb(avi_io_t *io, void *buf, size_t size, size_t *out_read)
{
    if (out_read) {
        *out_read = 0;
    }
    if (!io || !io->ctx || !buf) {
        return -EINVAL;
    }

    avi_http_range_ctx_t *ctx = (avi_http_range_ctx_t *)io->ctx;
    if (ctx->eof && io->pos >= ctx->eof_pos) {
        return 0;
    }

    if (!ctx->opened) {
        int ret = http_open_at(ctx, io->pos);
        if (ret != 0) {
            return ret;
        }
        if (ctx->eof && io->pos >= ctx->eof_pos) {
            return 0;
        }
    }

    if (size == 0) {
        return 0;
    }

    /* 延迟分配缓存 */
    if (!ctx->cache) {
        ctx->cache_cap = HTTP_CLIENT_BUFFER_SIZE; /* 4096 */
        ctx->cache = (uint8_t *)lisa_mem_alloc(ctx->cache_cap);
        if (!ctx->cache) {
            return -ENOMEM;
        }
        ctx->cache_len = 0;
        ctx->cache_off = 0;
    }

    uint8_t *out = (uint8_t *)buf;
    size_t remain = size;

    while (remain > 0) {
        /* Serve from cache first */
        if (ctx->cache_off < ctx->cache_len) {
            uint32_t avail = ctx->cache_len - ctx->cache_off;
            uint32_t to_copy = (remain < (size_t)avail) ? (uint32_t)remain : avail;
            memcpy(out, &ctx->cache[ctx->cache_off], to_copy);
            ctx->cache_off += to_copy;
            out += to_copy;
            remain -= (size_t)to_copy;
            continue;
        }

        /* Cache empty: refill from network with retry+reconnect */
        ctx->cache_len = 0;
        ctx->cache_off = 0;

        const int max_retry = 3;
        bool got_any = false;
        for (int attempt = 0; attempt < max_retry; attempt++) {
            if (!ctx->opened) {
                int oret = http_open_at(ctx, io->pos);
                if (oret != 0) {
                    return oret;
                }
                if (ctx->eof && io->pos >= ctx->eof_pos) {
                    break;
                }
            }

            UINT32 recvd = 0;
            UINT32 req = ctx->cache_cap;
            int ret = HTTPC_read(&ctx->params, ctx->cache, req, &recvd);
            if (ret == 0 && recvd > 0) {
                ctx->cache_len = recvd;
                ctx->cache_off = 0;
                got_any = true;
                break;
            }

            /*
             * Some HTTP stacks may return a non-zero status together with valid payload.
             * Dropping these bytes creates holes in MJPEG bitstream (broken SOI/segment),
             * which can later crash decoder paths. Keep payload first, then reconnect.
             */
            if (ret != 0 && recvd > 0) {
                ctx->cache_len = recvd;
                ctx->cache_off = 0;
                got_any = true;

                LISA_LOGW(LOG_TAG,
                          "HTTPC_read partial: ret=%d recvd=%lu req=%lu pos=%lld (keep bytes)",
                          ret,
                          (unsigned long)recvd,
                          (unsigned long)req,
                          (long long)io->pos);

                (void)HTTPC_close(&ctx->params);
                ctx->opened = 0;
                break;
            }

            if (ret == 0 && recvd == 0) {
                ctx->eof = 1;
                ctx->eof_pos = io->pos;
                (void)HTTPC_close(&ctx->params);
                ctx->opened = 0;
                break;
            }

            LISA_LOGW(LOG_TAG,
                      "HTTPC_read retry: attempt=%d ret=%d recvd=%lu req=%lu pos=%lld",
                      attempt + 1,
                      ret,
                      (unsigned long)recvd,
                      (unsigned long)req,
                      (long long)io->pos);

            /* Close and retry from the same position */
            (void)HTTPC_close(&ctx->params);
            ctx->opened = 0;

            if (attempt < (max_retry - 1)) {
                lisa_thread_mdelay(20);
            }
        }

        if (!got_any) {
            /* If we already copied some bytes this call, return partial read */
            size_t done = size - remain;
            if (done > 0) {
                if (out_read) {
                    *out_read = done;
                }
                io->pos += (int64_t)done;
                return 0;
            }
            if (ctx->eof) {
                return 0;
            }
            return -EIO;
        }
    }

    if (out_read) {
        *out_read = size;
    }
    io->pos += (int64_t)size;
    return 0;
}

static int http_seek_cb(avi_io_t *io, int64_t offset, avi_io_seek_whence_t whence)
{
    if (!io || !io->ctx) {
        return -EINVAL;
    }

    avi_http_range_ctx_t *ctx = (avi_http_range_ctx_t *)io->ctx;

    int64_t target = 0;
    if (whence == AVI_IO_SEEK_SET) {
        target = offset;
    } else {
        target = io->pos + offset;
    }

    if (target < 0) {
        return -ENOTSUP;
    }

    if (ctx->eof) {
        if (target >= ctx->eof_pos) {
            io->pos = target;
            return 0;
        }
        ctx->eof = 0;
    }

    if (whence == AVI_IO_SEEK_CUR && offset >= 0 && ctx->opened) {
        /*
         * Sequential forward skip (common in drop-video path):
         * prefer read&discard over reconnect to avoid TCP alloc/free churn.
         */
        const int64_t max_stream_skip = 512 * 1024;
        if (offset > max_stream_skip) {
            /* Very large jump: keep previous behavior (reopen with new Range). */
            goto do_reopen_seek;
        }

        uint8_t tmp[1024];
        int64_t left = offset;
        while (left > 0) {
            size_t chunk = (left > (int64_t)sizeof(tmp)) ? sizeof(tmp) : (size_t)left;
            size_t got = 0;
            int r = http_read_cb(io, tmp, chunk, &got);
            if (r != 0) {
                return r;
            }
            if (got == 0) {
                return -EIO;
            }
            left -= (int64_t)got;
        }
        return 0;
    }

do_reopen_seek:
    /* Reconnect with a new Range */
    io->pos = target;
    if (ctx) {
        ctx->cache_len = 0;
        ctx->cache_off = 0;
    }
    return http_open_at(ctx, target);
}

static int http_close_cb(avi_io_t *io)
{
    if (!io || !io->ctx) {
        return 0;
    }

    avi_http_range_ctx_t *ctx = (avi_http_range_ctx_t *)io->ctx;
    if (ctx->opened) {
        (void)HTTPC_close(&ctx->params);
        ctx->opened = 0;
    }

    if (ctx->cache) {
        lisa_mem_free(ctx->cache);
        ctx->cache = NULL;
        ctx->cache_cap = 0;
        ctx->cache_len = 0;
        ctx->cache_off = 0;
    }

    lisa_mem_free(ctx);
    io->ctx = NULL;
    io->ops = NULL;
    io->pos = 0;
    return 0;
}

static const avi_io_ops_t s_http_ops = {
    .read = http_read_cb,
    .seek = http_seek_cb,
    .close = http_close_cb,
};

int avi_io_open_http_range(avi_io_t *io, const char *url)
{
    if (!io || !url || url[0] == '\0') {
        return -EINVAL;
    }

    memset(io, 0, sizeof(*io));

    avi_http_range_ctx_t *ctx = (avi_http_range_ctx_t *)lisa_mem_alloc(sizeof(*ctx));
    if (!ctx) {
        return -ENOMEM;
    }
    memset(ctx, 0, sizeof(*ctx));

    strncpy(ctx->url, url, sizeof(ctx->url) - 1);
    ctx->url[sizeof(ctx->url) - 1] = '\0';

    io->ops = &s_http_ops;
    io->ctx = ctx;
    io->pos = 0;

    /* Do not open immediately; open on first read/seek. */
    return 0;
}

#endif /* CONFIG_AVI_PLAYER_HTTP_RANGE */
