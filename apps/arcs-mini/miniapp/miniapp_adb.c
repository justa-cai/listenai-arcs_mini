#include "miniapp.h"
#include "adb.h"
#include "adb_services.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "lisa_log.h"
#include "mbedtls/md5.h"
#include "sys_init.h"

#include <stdio.h>
#include <string.h>

#define TAG "miniapp.adb"
#define SYNC_ID(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define SYNC_STAT SYNC_ID('S', 'T', 'A', 'T')
#define SYNC_RECV           SYNC_ID('R', 'E', 'C', 'V')
#define SYNC_SEND SYNC_ID('S', 'E', 'N', 'D')
#define SYNC_DATA SYNC_ID('D', 'A', 'T', 'A')
#define SYNC_DONE SYNC_ID('D', 'O', 'N', 'E')
#define SYNC_QUIT SYNC_ID('Q', 'U', 'I', 'T')
#define SYNC_OKAY SYNC_ID('O', 'K', 'A', 'Y')
#define SYNC_FAIL SYNC_ID('F', 'A', 'I', 'L')
#define UPLOAD_PATH_MAX 160u
#define INSTALL_STACK_BYTES 4096u

/* Only the ADB receive task parses packets. The installer holds a separate
 * reference after DONE, so disconnect never frees a source still used by Lua.
 * There is no packet queue, filesystem staging buffer or idle worker task. */
typedef struct {
    uint32_t local_id, remote_id;
    unsigned refs;
    bool closed, installing, failed, locked, receiving;
    uint8_t header[8];
    uint32_t header_size, command, remaining, path_size;
    char path[UPLOAD_PATH_MAX + 1];
    miniapp_package_t package;
    char *source;
    miniapp_source_t *snapshot;
    size_t read_offset;
    bool downloading, waiting_ack;
    uint8_t *download_packet;
} miniapp_adb_t;

static bool s_connection_open;

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void write_u32(uint8_t *p, uint32_t n)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (8 * i));
}

static void upload_release(miniapp_adb_t *ctx)
{
    lisa_mem_free(ctx->source);
    ctx->source = NULL;
    if (ctx->locked) {
        miniapp_install_end();
        ctx->locked = false;
    }
}

static void context_unref(miniapp_adb_t *ctx)
{
    taskENTER_CRITICAL();
    bool last = --ctx->refs == 0;
    taskEXIT_CRITICAL();
    if (last) {
        miniapp_source_release(ctx->snapshot);
        lisa_mem_free(ctx->download_packet);
        lisa_mem_free(ctx);
    }
}

static void reply(miniapp_adb_t *ctx, uint32_t id, const char *error)
{
    uint8_t response[8 + 128];
    size_t size = error ? strlen(error) : 0;
    if (size > sizeof(response) - 8) size = sizeof(response) - 8;
    write_u32(response, id);
    write_u32(response + 4, size);
    if (size) memcpy(response + 8, error, size);
    taskENTER_CRITICAL();
    bool closed = ctx->closed;
    taskEXIT_CRITICAL();
    /* IDs are immutable and never taken from a potentially reused service
     * slot. A disconnect racing this send can only drop the old reply. */
    if (!closed) adb_write(ctx->local_id, ctx->remote_id, response, size + 8);
}

static void fail(miniapp_adb_t *ctx, const char *error)
{
    ctx->failed = true;
    upload_release(ctx);
    reply(ctx, SYNC_FAIL, error);
}

static bool parse_path(miniapp_adb_t *ctx)
{
    const char prefix[] = "/miniapp/";
    size_t len = strlen(ctx->path);
    if (len <= sizeof(prefix) - 1 + 4 || strncmp(ctx->path, prefix, sizeof(prefix) - 1) ||
        strcmp(ctx->path + len - 4, ".lua")) return false;
    size_t id_len = len - (sizeof(prefix) - 1) - 4;
    if (id_len > MINIAPP_ID_MAX - 6) return false;
    const char *id = ctx->path + sizeof(prefix) - 1;
    for (size_t i = 0; i < id_len; ++i) {
        unsigned char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    snprintf(ctx->package.id, sizeof(ctx->package.id), "local:%.*s", (int)id_len, id);
    return true;
}

static void install_task(void *arg)
{
    miniapp_adb_t *ctx = arg;
    uint8_t digest[16];
    const char hex[] = "0123456789abcdef";
    char error[128] = {0};
    mbedtls_md5((const uint8_t *)ctx->source, ctx->package.size, digest);
    for (unsigned i = 0; i < sizeof(digest); ++i) {
        ctx->package.hash[2 * i] = hex[digest[i] >> 4];
        ctx->package.hash[2 * i + 1] = hex[digest[i] & 15];
    }
    ctx->package.hash[32] = '\0';
    snprintf(ctx->package.version, sizeof(ctx->package.version), "%s", ctx->package.hash);
    /* Always run again, even for identical source: push is a local restart. */
    int rc = miniapp_install(&ctx->package, ctx->source, error, sizeof(error));
    upload_release(ctx);
    LISA_LOGI(TAG, "local install %s, source=%u, context=%u, stack_free=%u bytes",
              rc == 0 ? "started" : "failed", (unsigned)ctx->package.size,
              (unsigned)sizeof(*ctx), (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    reply(ctx, rc == 0 ? SYNC_OKAY : SYNC_FAIL, rc == 0 ? NULL : error);
    /* installing stays set: one upload per sync connection, released by CLSE. */
    context_unref(ctx);
}

static void finish_upload(miniapp_adb_t *ctx)
{
    if (!ctx->package.size) {
        fail(ctx, "empty Lua source");
        return;
    }
    ctx->source[ctx->package.size] = '\0';
    ctx->installing = true;
    taskENTER_CRITICAL();
    ++ctx->refs;
    taskEXIT_CRITICAL();
    const lisa_thread_attr_t attr = {
        .name = "miniapp.adb", .stack_size = INSTALL_STACK_BYTES, .priority = 4,
    };
    if (!lisa_thread_create(&attr, install_task, ctx)) {
        ctx->installing = false;
        context_unref(ctx);
        fail(ctx, "not enough memory for installer");
    }
}

/* The ADB ready callback paces one DATA packet per host acknowledgement.
 * All download state belongs to the receive task; no worker or full-file
 * copy is needed. Keep a snapshot until CLSE, including during replacement. */
static void download_next(miniapp_adb_t *ctx)
{
    uint8_t *packet = ctx->download_packet;
    size_t size;
    const char *data = miniapp_source_data(ctx->snapshot, &size);
    size_t count = size - ctx->read_offset;
    if (count > 1024 - 8) {
        count = 1024 - 8;
    }
    if (count > MAX_PAYLOAD - 8) {
        count = MAX_PAYLOAD - 8;
    }
    write_u32(packet, count ? SYNC_DATA : SYNC_DONE);
    write_u32(packet + 4, count);
    if (count) {
        memcpy(packet + 8, data + ctx->read_offset, count);
    }
    ctx->read_offset += count;
    ctx->waiting_ack = count != 0;
    adb_write(ctx->local_id, ctx->remote_id, packet, count + 8);
}

static void local_sync_ready(struct adb_service *service)
{
    miniapp_adb_t *ctx = service->data;
    if (ctx->downloading && ctx->waiting_ack) {
        download_next(ctx);
    }
}

static void path_received(miniapp_adb_t *ctx)
{
    ctx->path[ctx->path_size] = '\0';
    if (memchr(ctx->path, '\0', ctx->path_size)) {
        fail(ctx, "invalid path");
        return;
    }
    if (ctx->command == SYNC_STAT) {
        uint8_t stat[16] = {0};
        write_u32(stat, SYNC_STAT);
        if (!strcmp(ctx->path, "/miniapp") || !strcmp(ctx->path, "/miniapp/")) {
            write_u32(stat + 4, 0040755); /* virtual directory */
        }
        if (!strcmp(ctx->path, "/miniapp/miniapp.lua")) {
            if (!ctx->snapshot) {
                ctx->snapshot = miniapp_source_acquire();
            }
            if (ctx->snapshot) {
                size_t size;
                (void)miniapp_source_data(ctx->snapshot, &size);
                write_u32(stat + 4, 0100444);
                write_u32(stat + 8, size);
            }
        }
        /* Other paths are upload entrances, not stored files. */
        adb_write(ctx->local_id, ctx->remote_id, stat, sizeof(stat));
        return;
    }
    if (ctx->command == SYNC_RECV) {
        if (strcmp(ctx->path, "/miniapp/miniapp.lua")) {
            fail(ctx, "use adb pull /miniapp/miniapp.lua <local path>");
            return;
        }
        if (!ctx->snapshot) {
            ctx->snapshot = miniapp_source_acquire();
        }
        if (!ctx->snapshot) {
            fail(ctx, "no miniapp is running");
            return;
        }
        ctx->download_packet = lisa_mem_alloc(1024);
        if (!ctx->download_packet) {
            fail(ctx, "not enough memory for download");
            return;
        }
        ctx->downloading = true;
        download_next(ctx);
        return;
    }
    char *mode = strrchr(ctx->path, ',');
    if (!mode) {
        fail(ctx, "invalid SEND path");
        return;
    }
    *mode = '\0';
    if (!parse_path(ctx)) {
        fail(ctx, "use /miniapp/<id>.lua (letters, digits, hyphen, underscore)");
        return;
    }
    if (!miniapp_install_begin()) {
        fail(ctx, "miniapp unavailable or install in progress");
        return;
    }
    ctx->locked = true;
    ctx->receiving = true;
}

static void header_received(miniapp_adb_t *ctx)
{
    ctx->command = read_u32(ctx->header);
    uint32_t size = read_u32(ctx->header + 4);
    ctx->header_size = 0;
    if (!ctx->receiving && (ctx->command == SYNC_STAT || ctx->command == SYNC_SEND || ctx->command == SYNC_RECV)) {
        if (!size || size > UPLOAD_PATH_MAX) fail(ctx, "path too long or empty");
        else { ctx->remaining = size; ctx->path_size = 0; }
    } else if (ctx->receiving && ctx->command == SYNC_DATA) {
        if (!size || size > MINIAPP_SOURCE_MAX - ctx->package.size) {
            fail(ctx, "Lua source exceeds device limit or DATA is empty");
            return;
        }
        /* The standard client declares up to 64 KiB per DATA chunk. Typical
         * miniapps need one exact allocation; fragmented ADB packets copy
         * directly into it, without retaining their transport buffers. */
        char *source = lisa_mem_realloc(ctx->source, ctx->package.size + size + 1);
        if (!source) { fail(ctx, "not enough memory for Lua source"); return; }
        ctx->source = source;
        ctx->remaining = size;
    } else if (ctx->receiving && ctx->command == SYNC_DONE) {
        finish_upload(ctx); /* size contains mtime, not a payload length */
    } else if (ctx->command == SYNC_QUIT) {
        ctx->failed = true;
        upload_release(ctx);
        adb_close(ctx->local_id, ctx->remote_id);
    } else {
        fail(ctx, "unsupported sync request; use adb push or pull under /miniapp");
    }
}

static int local_sync_write(struct adb_service *service, adb_packet_t *packet)
{
    miniapp_adb_t *ctx = service->data;
    const uint8_t *p = packet->data;
    uint32_t size = packet->msg.data_length;
    /* One file per connection. The normal client sends QUIT before CLSE.
     * Accept even a fragmented QUIT; reject a second upload, which otherwise
     * would wait forever for a result that will never be produced. */
    if (ctx->installing || ctx->downloading) {
        uint32_t n = sizeof(ctx->header) - ctx->header_size;
        if (size > n) {
            adb_packet_free(packet);
            return -1;
        }
        memcpy(ctx->header + ctx->header_size, p, size);
        ctx->header_size += size;
        bool valid = ctx->header_size < sizeof(ctx->header) ||
            (read_u32(ctx->header) == SYNC_QUIT && read_u32(ctx->header + 4) == 0);
        adb_packet_free(packet);
        if (valid && ctx->header_size == sizeof(ctx->header)) {
            adb_close(ctx->local_id, ctx->remote_id);
        }
        return valid ? 0 : -1;
    }
    if (ctx->failed) {
        adb_packet_free(packet);
        return -1;
    }
    while (size && !ctx->failed && !ctx->installing && !ctx->downloading) {
        uint32_t n;
        if (!ctx->remaining) {
            n = sizeof(ctx->header) - ctx->header_size;
            if (n > size) n = size;
            memcpy(ctx->header + ctx->header_size, p, n);
            ctx->header_size += n;
            if (ctx->header_size == sizeof(ctx->header)) header_received(ctx);
        } else {
            n = ctx->remaining < size ? ctx->remaining : size;
            if (ctx->command == SYNC_DATA) {
                memcpy(ctx->source + ctx->package.size, p, n);
                ctx->package.size += n;
            } else {
                memcpy(ctx->path + ctx->path_size, p, n);
                ctx->path_size += n;
            }
            ctx->remaining -= n;
            if (!ctx->remaining && ctx->command != SYNC_DATA) path_received(ctx);
        }
        p += n;
        size -= n;
    }
    adb_packet_free(packet);
    return 0;
}

static int local_sync_open(struct adb_service *service, const uint8_t *args)
{
    if (s_connection_open || (args && args[0])) return -1;
    miniapp_adb_t *ctx = lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) return -1;
    ctx->local_id = service->local_id;
    ctx->remote_id = service->remote_id;
    ctx->refs = 1;
    service->data = ctx;
    s_connection_open = true;
    return 0;
}

static int local_sync_close(struct adb_service *service)
{
    miniapp_adb_t *ctx = service->data;
    taskENTER_CRITICAL();
    ctx->closed = true;
    taskEXIT_CRITICAL();
    if (!ctx->installing) upload_release(ctx);
    service->data = NULL;
    s_connection_open = false;
    context_unref(ctx);
    return 0;
}

static const struct adb_service_handle s_local_sync = {
    .name = (uint8_t *)"sync",
    .open = local_sync_open,
    .close = local_sync_close,
    .write = local_sync_write,
    .ready = local_sync_ready,
};

static int miniapp_adb_init(void)
{
    return adb_service_hd_register(&s_local_sync);
}
SYS_INIT(miniapp_adb_init, SYS_INIT_LEVEL_PRE_APPLICATION, 60);
