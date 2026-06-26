/**
 * @file    sd_music_scanner.c
 * @brief   SD 卡 MP3 文件扫描层实现
 */

#include "sd_music_scanner.h"

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "sd_scan"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lsfs.h"
#include "ff.h"

/* ---- 配置 ---- */

#define SD_MUSIC_DISK_DEVICE   "SD:"
#define SD_MUSIC_MOUNT_POINT   "/" SD_MUSIC_DISK_DEVICE
#define SD_MUSIC_ROOT_DIR      SD_MUSIC_MOUNT_POINT "/"
#define SD_MUSIC_DEFAULT_SCAN_DIR SD_MUSIC_ROOT_DIR "audio"

#define SD_MUSIC_SCANNER_DIR_NAME_BYTES_MAX \
    (AUIDO_OUT_LOCAL_DIR_NAME_CHARS_MAX * AUIDO_OUT_UTF8_CN_BYTES_MAX)
#define SD_MUSIC_SCANNER_FILE_NAME_BYTES_MAX (AUIDO_OUT_NAME_LEN - 1)
#define SD_MUSIC_SCANNER_PATH_SLOT_COUNT (AUIDO_OUT_LOCAL_DIR_DEPTH_MAX + 1)

static char g_scanner_path_slots[SD_MUSIC_SCANNER_PATH_SLOT_COUNT][SD_MUSIC_SCANNER_MAX_PATH];
static struct lsfs_dirent g_scanner_entry;

static char *scanner_next_path_slot(int depth)
{
    int next_depth = depth + 1;

    if (next_depth < 0 || next_depth >= SD_MUSIC_SCANNER_PATH_SLOT_COUNT) {
        return NULL;
    }

    memset(g_scanner_path_slots[next_depth], 0,
           sizeof(g_scanner_path_slots[next_depth]));
    return g_scanner_path_slots[next_depth];
}

static bool scanner_name_fits(const char *name, size_t max_len, const char *kind)
{
    size_t len;

    if (!name) {
        return false;
    }

    len = strlen(name);
    if (len > max_len) {
        LOGW("跳过%s，名称过长: len=%u, max=%u, name=%s",
             kind, (unsigned int)len, (unsigned int)max_len, name);
        return false;
    }

    return true;
}

static bool scanner_build_path(char *out, size_t out_size,
                               const char *path, bool has_slash,
                               const char *name)
{
    int ret = snprintf(out, out_size, "%s%s%s", path, has_slash ? "" : "/", name);

    if (ret < 0 || ret >= (int)out_size) {
        LOGW("跳过路径，路径过长: max=%u, path=%s%s%s",
             (unsigned int)out_size, path, has_slash ? "" : "/", name);
        return false;
    }

    return true;
}

/* ---- MP3 检测 ---- */

static bool sd_music_scanner_is_mp3(const char *name)
{
    if (name == NULL) return false;

    size_t len = strlen(name);
    if (len < 4) return false;

    return ((name[len - 4] == '.') &&
            ((name[len - 3] == 'm') || (name[len - 3] == 'M')) &&
            ((name[len - 2] == 'p') || (name[len - 2] == 'P')) &&
            ((name[len - 1] == '3') || (name[len - 1] == '3')));
}

/* ---- MAC OS 文件系统的伴生元数据文件处理逻辑 ---- */

static bool scanner_is_macos_metadata_dir(const char *name)
{
    if (!name) {
        return false;
    }

    return (strcmp(name, ".Spotlight-V100") == 0 ||
            strcmp(name, ".Trashes") == 0 ||
            strcmp(name, ".fseventsd") == 0);
}

static bool scanner_is_macos_metadata_file_name(const char *name)
{
    if (!name) {
        return false;
    }

    return (strcmp(name, ".DS_Store") == 0 ||
            strncmp(name, "._", 2) == 0);
}

#if 0
/* TODO: 文件操作的过程拔 TF 卡（如 lsfs_open）会直接卡死。
 * 文件操作 API 具备超时能力后，再启用 AppleDouble magic 检测，
 * 用文件头确认 _xxx.mp3 是否真的是 macOS 伴生元数据文件。 */
#define SD_MUSIC_APPLEDOUBLE_PROBE_SIZE_MAX (16U * 1024U)

static bool scanner_format_path(char *out, size_t out_size,
                                const char *path, bool has_slash,
                                const char *name)
{
    int ret = snprintf(out, out_size, "%s%s%s", path, has_slash ? "" : "/", name);

    return (ret >= 0 && ret < (int)out_size);
}

static bool scanner_should_probe_appledouble(const char *name, size_t size)
{
    if (!name) {
        return false;
    }

    return (name[0] == '_' ||
            (size > 0U && size <= SD_MUSIC_APPLEDOUBLE_PROBE_SIZE_MAX));
}

static bool scanner_has_appledouble_magic(const char *path)
{
    uint8_t magic[4];
    struct lsfs_file_t file;
    ssize_t read_len;

    lsfs_file_t_init(&file);
    if (lsfs_open(&file, path, LSFS_O_READ) != 0) {
        return false;
    }

    read_len = lsfs_read(&file, magic, sizeof(magic));
    lsfs_close(&file);

    return (read_len == (ssize_t)sizeof(magic) &&
            magic[0] == 0x00 &&
            magic[1] == 0x05 &&
            magic[2] == 0x16 &&
            magic[3] == 0x07);
}
#endif

static bool scanner_should_ignore_file(const char *path, bool has_slash,
                                       const char *name, size_t size)
{
    (void)size;

    if (scanner_is_macos_metadata_file_name(name)) {
        LOGD("忽略 macOS 元数据文件: %s%s%s", path, has_slash ? "" : "/", name);
        return true;
    }

    /* 当前先按文件名过滤 Mac 移动文件时生成的伪音频，例如 _xxx.mp3。
     * 后续文件 API 支持超时后，再恢复 AppleDouble magic 校验以降低误判。
     *
     * char full_path[SD_MUSIC_SCANNER_MAX_PATH];
     * if (sd_music_scanner_is_mp3(name) &&
     *     scanner_should_probe_appledouble(name, size) &&
     *     scanner_format_path(full_path, sizeof(full_path), path, has_slash, name) &&
     *     scanner_has_appledouble_magic(full_path)) {
     *     LOGD("忽略 AppleDouble 元数据文件: %s", full_path);
     *     return true;
     * }
     */
    if (name && name[0] == '_' && sd_music_scanner_is_mp3(name)) {
        LOGD("忽略疑似 AppleDouble 元数据文件: %s%s%s",
             path, has_slash ? "" : "/", name);
        return true;
    }

    return false;
}

/* ---- 前向声明 ---- */

static void read_id3v1(const char *path,
                       char *artist, int artist_len,
                       char *album,  int album_len,
                       char *title,  int title_len);

/* ---- 扫描上下文 ---- */

typedef struct {
    sd_music_scanner_batch_cb callback;
    void *user_data;
    sd_music_scanner_stats_t stats;
    int total_scanned;        /**< 已扫描文件总数 */
    int max_total;            /**< 上限 */
    bool aborted;             /**< 回调要求终止或 I/O 错误 */
    bool callback_failed;     /**< 回调已失败一次 */
    bool read_error;          /**< 扫描过程中遇到磁盘读取错误 */
    int ret;                  /**< 最终返回码 */
} scan_ctx_t;

/*
 * 扫描批次缓冲区 — 从 PSRAM 分配（避免占用稀缺的 SRAM）。
 * music_item_t 约 969 字节/个，100 个约 95KB，SRAM 仅 164KB 放不下。
 */
static music_item_t *g_scan_batch = NULL;
static int g_scan_batch_cap = 0;
static int g_scan_batch_idx = 0;

/**
 * @brief 冲刷当前批次到回调
 *
 * @param ctx      扫描上下文
 * @param is_last  是否为最后一批
 * @return 0 成功，负值回调要求终止
 */
static int scan_flush(scan_ctx_t *ctx, bool is_last)
{
    if (g_scan_batch_idx == 0 && !is_last) return 0;

    int ret = ctx->callback(g_scan_batch, g_scan_batch_idx, is_last, ctx->user_data);
    g_scan_batch_idx = 0;

    if (ret != 0) {
        ctx->aborted = true;
        ctx->ret = ret;
        return -1;
    }
    return 0;
}

static int scan_count_mp3_under_dir(const char *path)
{
    struct lsfs_dir_t dir;
    struct lsfs_dirent entry;
    int count = 0;

    if (!path || path[0] == '\0') {
        return 0;
    }

    lsfs_dir_t_init(&dir);
    if (lsfs_opendir(&dir, path) != 0) {
        return 0;
    }

    size_t path_len = strlen(path);
    bool has_slash = (path_len > 0 && path[path_len - 1] == '/');

    while (lsfs_readdir(&dir, &entry) == 0 && entry.name[0] != '\0') {
        if (entry.type == LSFS_DIR_ENTRY_DIR) {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            if (scanner_is_macos_metadata_dir(entry.name)) {
                continue;
            }

            char sub_path[SD_MUSIC_SCANNER_MAX_PATH];
            if (!scanner_build_path(sub_path, sizeof(sub_path), path, has_slash, entry.name)) {
                continue;
            }
            count += scan_count_mp3_under_dir(sub_path);
        } else if (!scanner_should_ignore_file(path, has_slash, entry.name, entry.size) &&
                   sd_music_scanner_is_mp3(entry.name)) {
            count++;
        }
    }

    lsfs_closedir(&dir);
    return count;
}

static void scan_add_skipped_dir(scan_ctx_t *ctx,
                                 const char *path,
                                 bool has_slash,
                                 const char *name)
{
    char sub_path[SD_MUSIC_SCANNER_MAX_PATH];

    if (!scanner_build_path(sub_path, sizeof(sub_path), path, has_slash, name)) {
        ctx->stats.unknown_skip_count++;
        return;
    }

    int skipped = scan_count_mp3_under_dir(sub_path);
    if (skipped > 0) {
        ctx->stats.skipped_count += (uint32_t)skipped;
    }
}

/**
 * @brief 向批次缓冲区添加一个文件
 *
 * @param ctx   扫描上下文
 * @param path  文件所在目录路径
 * @param name  文件名
 * @return 0 成功，-1 达到上限或已终止
 */
static int scan_add_file(scan_ctx_t *ctx, const char *path, const char *name)
{
    bool has_slash = (path[0] != '\0' && path[strlen(path) - 1] == '/');

    if (!scanner_name_fits(name, SD_MUSIC_SCANNER_FILE_NAME_BYTES_MAX, "文件")) {
        ctx->stats.skipped_count++;
        return 0;
    }

    music_item_t *item = &g_scan_batch[g_scan_batch_idx];
    memset(item, 0, sizeof(music_item_t));
    if (!scanner_build_path(item->m_url, sizeof(item->m_url), path, has_slash, name)) {
        ctx->stats.skipped_count++;
        return 0;
    }
    snprintf(item->m_name, sizeof(item->m_name), "%s", name);
    // TODO: 文件操作的过程拔TF卡（如：lsfs_open）会直接卡死，文件操作API具备超时功能后再继续开发
    // DEBUG: 跳过 ID3 读取，验证拔卡恢复能力
    // read_id3v1(item->m_url,
    //            item->m_artist, sizeof(item->m_artist),
    //            item->m_album,  sizeof(item->m_album),
    //            item->m_title,  sizeof(item->m_title));
    // 用文件名兜底 title（去掉 .mp3 后缀）
    {
        size_t nlen = strlen(name);
        const char *title_src = name;
        size_t title_len = nlen;
        if (nlen > 4 &&
            (name[nlen - 4] == '.') &&
            (name[nlen - 3] == 'm' || name[nlen - 3] == 'M') &&
            (name[nlen - 2] == 'p' || name[nlen - 2] == 'P') &&
            (name[nlen - 1] == '3')) {
            title_len = nlen - 4;
        }
        size_t copy_len = (title_len < sizeof(item->m_title) - 1) ? title_len : (sizeof(item->m_title) - 1);
        memcpy(item->m_title, title_src, copy_len);
        item->m_title[copy_len] = '\0';
    }

    LOGD("[%d] file=\"%s\" title=\"%s\" artist=\"%s\" album=\"%s\"",
         ctx->total_scanned, item->m_name,
         item->m_title, item->m_artist, item->m_album);

    g_scan_batch_idx++;
    ctx->total_scanned++;
    ctx->stats.reported_count++;

    bool at_cap = (ctx->total_scanned >= ctx->max_total);

    /* 批次满 或 达上限 → 冲刷 */
    if (g_scan_batch_idx >= SD_MUSIC_SCANNER_BATCH_MAX || at_cap) {
        /*
         * at_cap 时标记 is_last=true（无论批次是否满），
         * 避免精确命中上限时重复发送空批次。
         */
        if (scan_flush(ctx, at_cap) != 0) return -1;
    }

    /* 达上限 → 终止扫描 */
    if (at_cap) {
        ctx->aborted = true;
        return -1;
    }

    return 0;
}

/**
 * @brief 递归扫描目录
 */
static void scan_dir(scan_ctx_t *ctx, const char *path, int depth)
{
    if (ctx->aborted) return;

    struct lsfs_dir_t dir;
    lsfs_dir_t_init(&dir);
    if (lsfs_opendir(&dir, path) != 0) return;

    size_t path_len = strlen(path);
    bool has_slash = (path_len > 0 && path[path_len - 1] == '/');

    int rd_ret;
    while (!ctx->aborted
           && (rd_ret = lsfs_readdir(&dir, &g_scanner_entry)) == 0
           && g_scanner_entry.name[0] != '\0') {

        if (g_scanner_entry.type == LSFS_DIR_ENTRY_DIR) {
            if (strcmp(g_scanner_entry.name, ".") == 0 || strcmp(g_scanner_entry.name, "..") == 0) continue;
            if (scanner_is_macos_metadata_dir(g_scanner_entry.name)) {
                LOGD("忽略 macOS 元数据目录: %s%s%s",
                     path, has_slash ? "" : "/", g_scanner_entry.name);
                continue;
            }
            if (depth >= AUIDO_OUT_LOCAL_DIR_DEPTH_MAX) {
                LOGW("跳过目录，超过最大层级: max=%d, path=%s%s%s",
                     AUIDO_OUT_LOCAL_DIR_DEPTH_MAX, path, has_slash ? "" : "/", g_scanner_entry.name);
                scan_add_skipped_dir(ctx, path, has_slash, g_scanner_entry.name);
                continue;
            }
            if (!scanner_name_fits(g_scanner_entry.name, SD_MUSIC_SCANNER_DIR_NAME_BYTES_MAX, "目录")) {
                scan_add_skipped_dir(ctx, path, has_slash, g_scanner_entry.name);
                continue;
            }
            char *sub_path = scanner_next_path_slot(depth);
            if (!sub_path) {
                ctx->stats.unknown_skip_count++;
                continue;
            }
            if (!scanner_build_path(sub_path, SD_MUSIC_SCANNER_MAX_PATH, path, has_slash, g_scanner_entry.name)) {
                ctx->stats.unknown_skip_count++;
                continue;
            }
            scan_dir(ctx, sub_path, depth + 1);
        } else if (scanner_should_ignore_file(path, has_slash, g_scanner_entry.name, g_scanner_entry.size)) {
            continue;
        } else if (sd_music_scanner_is_mp3(g_scanner_entry.name)) {
            if (scan_add_file(ctx, path, g_scanner_entry.name) != 0) {
                /* 达到上限或回调终止，停止当前层遍历 */
                break;
            }
        }
    }

    /* lsfs_readdir 返回非 0 表示磁盘读取错误，不是正常的目录末尾。
     * 此时剩余批次不应标记 is_last=true，避免把不完整的文件列表当作完成上报。 */
    if (rd_ret != 0) {
        LOGW("目录读取失败: ret=%d, path=%s", rd_ret, path);
        ctx->read_error = true;
        ctx->aborted = true;
        ctx->ret = -1;
    }

    lsfs_closedir(&dir);
}

/* ---- ID3v1 解析 ---- */

static void trim_id3_field(char *str, int len)
{
    int end = len - 1;
    while (end >= 0 && (str[end] == '\0' || str[end] == ' ')) end--;
    str[end + 1] = '\0';
}

static bool is_valid_utf8(const uint8_t *s, int len)
{
    int i = 0;
    while (i < len && s[i]) {
        if (s[i] < 0x80) {
            i++;
        } else if ((s[i] & 0xE0) == 0xC0) {
            if (i + 1 >= len || (s[i + 1] & 0xC0) != 0x80) return false;
            if (((s[i] & 0x1E) == 0)) return false;
            i += 2;
        } else if ((s[i] & 0xF0) == 0xE0) {
            if (i + 2 >= len || (s[i + 1] & 0xC0) != 0x80
                || (s[i + 2] & 0xC0) != 0x80) return false;
            if (s[i] == 0xE0 && (s[i + 1] & 0x20) == 0) return false;
            i += 3;
        } else {
            return false;
        }
    }
    return true;
}

/**
 * @brief 确保 ID3 字段为 UTF-8 编码
 *
 * 若已是合法 UTF-8 则直接返回原指针；
 * 否则尝试按 GBK 解码为 Unicode，再编码为 UTF-8 写入静态缓冲区。
 */
static const char *ensure_utf8(const char *raw, int len)
{
#define CONV_BUF_SIZE 96
    static char conv_buf[CONV_BUF_SIZE];

    if (len <= 0 || raw[0] == '\0') return raw;
    if (is_valid_utf8((const uint8_t *)raw, len)) return raw;

    int out = 0, in = 0;
    conv_buf[0] = '\0';

    while (in < len && raw[in] && out < CONV_BUF_SIZE - 4) {
        uint8_t c = (uint8_t)raw[in];
        if (c < 0x80) {
            conv_buf[out++] = (char)c;
            in++;
        } else if (in + 1 < len && raw[in + 1]) {
            uint16_t gbk = ((uint16_t)c << 8) | (uint8_t)raw[in + 1];
            uint16_t uni = ff_oem2uni(gbk, 936);
            if (uni != gbk && uni > 0x7F) {
                conv_buf[out++] = (char)(0xE0 | (uni >> 12));
                conv_buf[out++] = (char)(0x80 | ((uni >> 6) & 0x3F));
                conv_buf[out++] = (char)(0x80 | (uni & 0x3F));
            }
            in += 2;
        } else {
            in++;
        }
    }
    conv_buf[out] = '\0';
    return conv_buf;
#undef CONV_BUF_SIZE
}

static void read_id3v1(const char *path,
                       char *artist, int artist_len,
                       char *album,  int album_len,
                       char *title,  int title_len)
{
    artist[0] = '\0'; album[0] = '\0'; title[0] = '\0';

    struct lsfs_file_t file; lsfs_file_t_init(&file);
    if (lsfs_open(&file, path, LSFS_O_READ) != 0) return;
    off_t fsize = lsfs_lsize(&file);
    if (fsize < 128 || lsfs_seek(&file, -128, LSFS_SEEK_END) != 0) { lsfs_close(&file); return; }
    uint8_t buf[128];
    if (lsfs_read(&file, buf, 128) != 128) { lsfs_close(&file); return; }
    lsfs_close(&file);
    if (buf[0] != 'T' || buf[1] != 'A' || buf[2] != 'G') return;

    char raw[31];
    strncpy(raw, (char *)&buf[3], 30); raw[30] = '\0';
    trim_id3_field(raw, 30);
    strncpy(title, ensure_utf8(raw, strlen(raw)), title_len - 1);
    title[title_len - 1] = '\0';

    strncpy(raw, (char *)&buf[33], 30); raw[30] = '\0';
    trim_id3_field(raw, 30);
    strncpy(artist, ensure_utf8(raw, strlen(raw)), artist_len - 1);
    artist[artist_len - 1] = '\0';

    strncpy(raw, (char *)&buf[63], 30); raw[30] = '\0';
    trim_id3_field(raw, 30);
    strncpy(album, ensure_utf8(raw, strlen(raw)), album_len - 1);
    album[album_len - 1] = '\0';
}

/* ---- 公开 API ---- */

/**
 * @brief 扫描入口
 *
 * 路径解析：传入路径 > 默认 /SD:/audio/ > 降级 /SD:/
 * 单次遍历，通过回调逐批输出。
 */
int sd_music_scanner_scan(const char *path, int max_total,
                           sd_music_scanner_batch_cb callback,
                           void *user_data,
                           sd_music_scanner_stats_t *stats)
{
    if (!callback) return -1;
    if (max_total <= 0) {
        max_total = INT_MAX;
    }

    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }

    /* 从 PSRAM 分配批次缓冲区（避免 SRAM 溢出） */
    int batch_cap = SD_MUSIC_SCANNER_BATCH_MAX;
    g_scan_batch = (music_item_t *)lisa_mem_calloc((uint32_t)batch_cap,
                                                    sizeof(music_item_t));
    if (!g_scan_batch) {
        LOGE("分配扫描批次缓冲区失败, cap=%d", batch_cap);
        return -1;
    }
    g_scan_batch_cap = batch_cap;
    g_scan_batch_idx = 0;

    char actual_path[SD_MUSIC_SCANNER_MAX_PATH];
    bool use_fallback = false;

    if (path != NULL && path[0] != '\0') {
        snprintf(actual_path, sizeof(actual_path), "%s", path);
    } else {
        snprintf(actual_path, sizeof(actual_path), "%s", SD_MUSIC_DEFAULT_SCAN_DIR);
        use_fallback = true;
    }

    scan_ctx_t ctx = {
        .callback   = callback,
        .user_data  = user_data,
        .total_scanned = 0,
        .max_total  = max_total,
        .aborted    = false,
        .callback_failed = false,
        .ret        = 0,
    };

    /* 扫描主目录 */
    scan_dir(&ctx, actual_path, 0);

    /* 默认路径不存在时降级到根目录 */
    if (ctx.total_scanned == 0 && use_fallback) {
        snprintf(actual_path, sizeof(actual_path), "%s", SD_MUSIC_ROOT_DIR);
        scan_dir(&ctx, actual_path, 0);
    }

    /* 冲刷最后一批 */
    if (!ctx.aborted) {
        /*
         * g_scan_batch_idx > 0: 还有未满批次的剩余文件 → 作为最后一批上报。
         * g_scan_batch_idx == 0 && total_scanned > 0: 全部文件恰好填满整批
         *   （每批都标记了 is_last=false）→ 发送空批次 is_last=true 通知服务端扫描完成。
         */
        if (g_scan_batch_idx > 0 || ctx.total_scanned > 0) {
            scan_flush(&ctx, true);
        }
    }

    if (ctx.read_error) {
        LOGW("扫描被磁盘读取错误中断: 共上报 %d 个文件, skipped=%u, unknown_skip=%u, 残留 %d 个文件未上报",
             ctx.total_scanned - g_scan_batch_idx,
             (unsigned int)ctx.stats.skipped_count,
             (unsigned int)ctx.stats.unknown_skip_count,
             g_scan_batch_idx);
    } else {
        LOGI("扫描完成: 共 %d 个文件, skipped=%u, unknown_skip=%u",
             ctx.total_scanned,
             (unsigned int)ctx.stats.skipped_count,
             (unsigned int)ctx.stats.unknown_skip_count);
    }

    if (stats) {
        *stats = ctx.stats;
    }

    /* 释放批次缓冲区 */
    lisa_mem_free(g_scan_batch);
    g_scan_batch = NULL;
    g_scan_batch_cap = 0;

    return ctx.ret;
}
