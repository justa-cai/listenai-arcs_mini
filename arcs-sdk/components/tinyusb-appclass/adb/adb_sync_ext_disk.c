#include "disk/disk_access.h"
#include "adb_utils.h"
#include "adb_sync_ext_disk.h"

#include "stdint.h"
#include "string.h"
#include "stdlib.h"

struct adb_sync_ext_disk_ctx *adb_sync_ext_disk_ctx_init(const char *name, uint32_t start_addr)
{
    int r;
    if (name == NULL) {
        return NULL;
    }

    struct adb_sync_ext_disk_ctx *ctx = ADB_MALLOC(sizeof(struct adb_sync_ext_disk_ctx));

    if (ctx == NULL) {
        ADB_LOGE("adb sync ext disk context malloc error");
        return NULL;
    }

    ctx->name = name;

    r = disk_access_ioctl(ctx->name, DISK_IOCTL_GET_SECTOR_SIZE, &ctx->sec_size);
    if (r) {
        ADB_LOGE("get disk sector size error, disk name:%s\n", ctx->name);
        ADB_FREE(ctx);
        return NULL;
    }

    if (start_addr % ctx->sec_size != 0) {
        ADB_LOGE("start_addr is not align with sec_size, start_addr:0x%x, sec_size:%d\n", start_addr, ctx->sec_size);
        ADB_FREE(ctx);
        return NULL;
    }

    ctx->buf = ADB_MALLOC(ctx->sec_size);
    if (ctx->buf == NULL) {
        ADB_LOGE("adb sync ext disk buffer malloc error");
        ADB_FREE(ctx);
        return NULL;
    }

    ctx->buf_idx = 0;
    ctx->curr_sec = start_addr / ctx->sec_size;

    ADB_LOGI("adb sync ext disk init, name:%s, sec_size:%d, start_sec:%d\n", name, ctx->sec_size, ctx->curr_sec);

    return ctx;
}

static int adb_sync_ext_disk_flush(struct adb_sync_ext_disk_ctx *ctx)
{
    int r;
    if (ctx->buf_idx == 0) {
        return 0;
    }

    r = disk_access_write(ctx->name, ctx->buf, ctx->curr_sec, 1);
    if (r) {
        return r;
    }

    if (ctx->buf_idx == ctx->sec_size) {
        ctx->curr_sec += 1;
    }

    ctx->buf_idx = 0;

    return 0;
}

void adb_sync_ext_disk_ctx_free(struct adb_sync_ext_disk_ctx *ctx)
{
    if (ctx) {
        adb_sync_ext_disk_flush(ctx);
        ADB_FREE(ctx->buf);
        ADB_FREE(ctx);
    }
}

static int adb_sync_ext_disk_write_align_start_sector(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len)
{
    int r;
    int n;

    n = len / ctx->sec_size;
    if (n) {
        r = disk_access_write(ctx->name, data, ctx->curr_sec, n);
        if (r) {
            return r;
        }
        ctx->curr_sec += n;
        data += n * ctx->sec_size;
    }
    n = len % ctx->sec_size;
    if (n) {
        memcpy(ctx->buf, data, n);
        ctx->buf_idx = n;
    }

    return 0;
}

int adb_sync_ext_disk_write(struct adb_sync_ext_disk_ctx *ctx, const uint8_t *data, uint32_t len)
{
    int r;
    int n;
    if (ctx == NULL || data == NULL || len == 0) {
        return -1;
    }

    if (ctx->buf_idx == 0) {
        adb_sync_ext_disk_write_align_start_sector(ctx, data, len);
    } else {
        if (ctx->buf_idx == ctx->sec_size) {
            adb_sync_ext_disk_flush(ctx);
        }

        int cpy_len = len <= ctx->sec_size - ctx->buf_idx ? len : ctx->sec_size - ctx->buf_idx;
        memcpy(ctx->buf + ctx->buf_idx, data, cpy_len);
        ctx->buf_idx += cpy_len;

        if (ctx->buf_idx == ctx->sec_size) {
            adb_sync_ext_disk_flush(ctx);
        }

        adb_sync_ext_disk_write_align_start_sector(ctx, data + cpy_len, len - cpy_len);
    }

    return 0;
}

int adb_sync_ext_disk_read(struct adb_sync_ext_disk_ctx *ctx, uint8_t *data, uint32_t len)
{
    int r;
    int cnt;
    if (ctx == NULL || data == NULL || len == 0) {
        return -1;
    }

    cnt = (len / ctx->sec_size) + ((len % ctx->sec_size) ? 1 : 0);
    r = disk_access_read(ctx->name, data, ctx->curr_sec, cnt);
    if (r) {
        return r;
    }

    ctx->curr_sec += cnt;

    return len;
}

bool adb_sync_is_ext_disk_access(const char *path)
{
    return strncmp(path, ADB_SYNC_EXT_MOUNT_POINT, strlen(ADB_SYNC_EXT_MOUNT_POINT)) == 0;
}

int adb_sync_ext_disk_get_info_by_path(char *path, char **name, uint32_t *addr, uint32_t *size)
{
    /* /RAW/name/addr/size */
    char *p = strstr(path, ADB_SYNC_EXT_MOUNT_POINT);
    char *token;
    char *saveptr;

    if (p != path) {
        return -1;
    }
    p += strlen(ADB_SYNC_EXT_MOUNT_POINT);
    *size = 0;
    *addr = 0;

    token = strtok_r(p, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }
    *name = token;

    token = strtok_r(NULL, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }
    *addr = strtol(token, NULL, 16);

    token = strtok_r(NULL, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }
    *size = strtol(token, NULL, 16);

    return 0;
}
