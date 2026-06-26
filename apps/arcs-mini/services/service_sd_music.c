/**
 * @file    service_sd_music.c
 * @brief   SD 卡离线音乐服务（编排层）
 *
 * 职责：编排 sd_music_card / sd_music_scanner / sd_music_http 三层中间件，
 *       实现 SD 卡热插拔检测、MP3 扫描、云端上报的完整业务流程。
 *
 * 不负责：
 *   - 文件扫描细节    → sd_music_scanner
 *   - HTTP 请求细节   → sd_music_http
 *   - 卡硬件操作细节  → sd_music_card
 *   - 播放控制        → voice_intent / MCP tool / player_event_callback
 *
 * 架构：
 *   service_sd_music_init()          — 模块入口，创建互斥锁 + 启动 sd_init
 *     └── sd_music_initial_scan_thread() — 上电有卡/无卡状态收敛
 *     └── sd_music_poll_thread()         — 运行时插卡/拔卡检测与恢复
 *
 * 线程安全：所有公开 API 内部使用 g_sd_music.lock 互斥锁保护本地缓存。
 *           voice_music_list 自身也有独立的锁，无需在此额外处理。
 *
 * 扫描策略：
 *   1. sd_init 独立收敛上电状态：无卡无 KV 无事发生；无卡有 KV 上报 removed；
 *      有卡无 KV 或 KV 变化才上报 inserted + 文件列表；有卡 KV 未变无事发生。
 *   2. sd_poll 只处理运行时插拔：插卡必报 inserted，再扫描本地列表；
 *      文件列表仅在快速指纹变更时全量流式分批上传并持久化。
 *   3. 快速指纹只使用文件系统剩余空间，是近似判断，允许同容量替换音频时漏报，
 *      以换取插卡后的快速响应。
 *
 * 扫描路径优先级：
 *   1. 调用者传入的 path（非 NULL 非空字符串）
 *   2. 默认路径 /SD:/audio/
 *   3. 降级到根目录 /SD:/
 *
 * 代码分区：
 *   - 配置和全局上下文
 *   - UI 同步状态与交互屏蔽
 *   - 本地运行态缓存
 *   - KV 卡状态和快速指纹
 *   - 扫描/上传回调
 *   - 卡快照与快速指纹读取
 *   - 公共同步动作
 *   - 上电状态收敛
 *   - 运行时插拔处理
 *   - 后台轮询和线程生命周期
 *   - 公开 API
 */

#include "service_sd_music.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#define TAG "sd_music"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"
#include "lisa_thread.h"
#include "lisa_time.h"
#include "lisa_device.h"
#include "lisa_kv.h"
#include "lsfs.h"
#include "lisa_sdmmc.h"
#include "drv_sdc.h"
#include "listen_system.h"

#include "kv_user.h"
#include "voice_msg.h"
#include "voice_music_list.h"
#include "voice_player_comm.h"
#include "voice_intent/voice_intent_mgr.h"
#include "sd_music_card.h"
#include "sd_music_scanner.h"
#include "sd_music_http.h"

/* platform.c 暴露的 SD 卡挂载结构体访问器，用于运行时卸载/重新挂载 */
extern struct lsfs_mount_t *platform_sd_mount_get(void);

/* ============================================================================
 * 配置和全局上下文
 * ============================================================================ */

#define SD_MUSIC_DISK_DEVICE        "SD:"
#define SD_MUSIC_MOUNT_POINT        "/" SD_MUSIC_DISK_DEVICE
#define SD_MUSIC_ROOT_DIR           SD_MUSIC_MOUNT_POINT "/"
#define SD_MUSIC_POLL_INTERVAL_MS   1000
#define SD_MUSIC_DEBOUNCE_COUNT     2
#define SD_MUSIC_PROBE_BACKOFF      3
#define SD_MUSIC_SYNC_RESULT_HOLD_MS 5000
#define SD_MUSIC_WORK_STACK_SIZE    16384

typedef struct {
    lisa_mutex_t *lock;
    lisa_thread_t *init_thread;
    lisa_thread_t *poll_thread;
    int full_count;               /**< 最近已知 MP3 数；未全量上传时为本地缓存数量 */
    int saved_count;              /**< 实际存入播放列表的数量（≤ MAX_TOTAL） */
    char current_card_id[SD_MUSIC_CARD_ID_LEN];
    char probe_path[AUIDO_OUT_URL_LEN]; /**< 用于拔卡检测的本地文件路径 */
    volatile bool card_ready;     /**< 已确认本轮 TF 卡文件系统可访问 */
    volatile bool sync_busy;      /**< TF 卡扫描/上报 UI 流程中，屏蔽按键和唤醒 */
    volatile uint64_t sync_busy_until_ms; /**< 结果态交互屏蔽截止时间，0 表示不等待 */
    volatile bool runtime_insert_busy;    /**< 运行态插卡挂载/扫描线程正在执行 */
} sd_music_ctx_t;

static sd_music_ctx_t g_sd_music;

typedef struct {
    char card_id[SD_MUSIC_CARD_ID_LEN];
} sd_music_removed_report_ctx_t;

/* ============================================================================
 * UI 同步状态与交互屏蔽
 * ============================================================================ */

static uint32_t sd_music_sync_msg_id(voice_msg_sd_music_sync_state_e state)
{
    switch (state) {
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_START:
        return VOICE_MSG_APP_SD_MUSIC_SYNC_START;
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_UPLOADING:
        return VOICE_MSG_APP_SD_MUSIC_SYNC_UPLOADING;
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED:
        return VOICE_MSG_APP_SD_MUSIC_SYNC_SUCCESSED;
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED:
        return VOICE_MSG_APP_SD_MUSIC_SYNC_FAILED;
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_FINISHED:
    case VOICE_MSG_SD_MUSIC_SYNC_STATE_IDLE:
    default:
        return VOICE_MSG_APP_SD_MUSIC_SYNC_FINISHED;
    }
}

static void sd_music_clear_sync_busy_if_due(void)
{
    uint64_t until_ms = g_sd_music.sync_busy_until_ms;
    if (until_ms == 0) {
        return;
    }

    if (lisa_os_get_tick_ms() >= until_ms) {
        g_sd_music.sync_busy = false;
        g_sd_music.sync_busy_until_ms = 0;
    }
}

static void sd_music_publish_sync_state(voice_msg_sd_music_sync_state_e state,
                                         int uploaded_count,
                                         int total_count,
                                         int result,
                                         int http_status_code,
                                         int http_error_code,
                                         int skipped_count)
{
    voice_msg_sd_music_sync_state_t msg = {
        .state = (uint32_t)state,
        .uploaded_count = (uploaded_count > 0) ? (uint32_t)uploaded_count : 0U,
        .total_count = (total_count > 0) ? (uint32_t)total_count : 0U,
        .result = result,
        .http_status_code = (http_status_code > 0) ? (uint32_t)http_status_code : 0U,
        .http_error_code = http_error_code,
        .skipped_count = (skipped_count > 0) ? (uint32_t)skipped_count : 0U,
    };

    if (state == VOICE_MSG_SD_MUSIC_SYNC_STATE_START ||
        state == VOICE_MSG_SD_MUSIC_SYNC_STATE_UPLOADING) {
        g_sd_music.sync_busy = true;
        g_sd_music.sync_busy_until_ms = 0;
    } else if (state == VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED ||
               state == VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED) {
        g_sd_music.sync_busy = true;
    }

    voice_msg_pub(sd_music_sync_msg_id(state), &msg, sizeof(msg));
}

static void sd_music_finish_sync(voice_msg_sd_music_sync_state_e state,
                                  int uploaded_count,
                                  int total_count,
                                  int result,
                                  int http_status_code,
                                  int http_error_code,
                                  int skipped_count)
{
    sd_music_publish_sync_state(state, uploaded_count, total_count, result,
                                http_status_code, http_error_code,
                                skipped_count);

    if (state == VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED ||
        state == VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED) {
        g_sd_music.sync_busy_until_ms = lisa_os_get_tick_ms() + SD_MUSIC_SYNC_RESULT_HOLD_MS;
    } else {
        g_sd_music.sync_busy = false;
        g_sd_music.sync_busy_until_ms = 0;
    }
}

/* ============================================================================
 * 本地运行态缓存
 * ============================================================================ */

static void sd_music_clear_local_card_state(void)
{
    voice_music_list_clear(MUSIC_LIST_OFFLINE);
    g_sd_music.full_count = 0;
    g_sd_music.saved_count = 0;
    g_sd_music.current_card_id[0] = '\0';
    g_sd_music.probe_path[0] = '\0';
    g_sd_music.card_ready = false;
    g_sd_music.sync_busy = false;
    g_sd_music.sync_busy_until_ms = 0;
}

static void sd_music_stop_music_on_remove(void)
{
    voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, NULL, 0);

    if (voice_intent_contains(INTENT_MUSIC)) {
        (void)voice_intent_pop(INTENT_MUSIC);
    }

    if (music_player) {
        (void)app_player_stop(music_player);
        (void)app_player_reset(music_player);
    }
}

/* ============================================================================
 * KV 卡状态和快速指纹
 *
 * 通过持久化 CID + 快速存储指纹（剩余空间），
 * 在每次插卡后对比上次记录，仅在指纹变更时才上传文件列表并持久化。
 * 运行时拔卡只清理在位标记，不清理快速指纹，避免同卡未变更时重复上报；
 * 上电无卡时清理全部 SD KV，并补报 removed 让云端状态收敛。
 *
 * KV 键：
 *   "sd.card.cid"     — 上次成功同步的卡 CID（string）
 *   "sd.card.stamp"   — 上次成功同步的快速存储指纹（string）
 *   "sd.card.present" — 上次确认在位的卡 CID，用于重启后补报 removed（string）
 * ============================================================================ */

static bool sd_music_has_card_changed(const char *cid, const char *stamp)
{
    char *prev_cid = NULL;
    char *prev_stamp = NULL;
    bool changed = true;

    if (lisa_kv_get_string(KV_KEY_SD_CID, &prev_cid) != 0) goto out;
    if (lisa_kv_get_string(KV_KEY_SD_STAMP, &prev_stamp) != 0) goto out;

    if (prev_cid && prev_stamp
        && strcmp(cid, prev_cid) == 0
        && strcmp(stamp, prev_stamp) == 0) {
        changed = false;
    }

out:
    if (prev_cid)  lisa_kv_free(prev_cid);
    if (prev_stamp) lisa_kv_free(prev_stamp);
    return changed;
}

static bool sd_music_get_present_card_id(char *card_id, int card_id_len)
{
    char *prev_present = NULL;
    bool has_present = false;

    if (card_id && card_id_len > 0) {
        card_id[0] = '\0';
    }

    if (lisa_kv_get_string(KV_KEY_SD_PRESENT, &prev_present) == 0 &&
        prev_present && prev_present[0]) {
        if (card_id && card_id_len > 0) {
            snprintf(card_id, card_id_len, "%s", prev_present);
        }
        has_present = true;
    }

    if (prev_present) lisa_kv_free(prev_present);
    return has_present;
}

static bool sd_music_get_saved_card_id(char *card_id, int card_id_len)
{
    char *prev_cid = NULL;
    bool has_cid = false;

    if (card_id && card_id_len > 0) {
        card_id[0] = '\0';
    }

    if (lisa_kv_get_string(KV_KEY_SD_CID, &prev_cid) == 0 &&
        prev_cid && prev_cid[0]) {
        if (card_id && card_id_len > 0) {
            snprintf(card_id, card_id_len, "%s", prev_cid);
        }
        has_cid = true;
    }

    if (prev_cid) lisa_kv_free(prev_cid);
    return has_cid;
}

static void sd_music_persist_card_present(const char *cid)
{
    if (cid && cid[0]) {
        lisa_kv_set_string(KV_KEY_SD_PRESENT, cid);
    } else {
        lisa_kv_del(KV_KEY_SD_PRESENT);
    }
}

static void sd_music_clear_card_state(void)
{
    lisa_kv_del(KV_KEY_SD_CID);
    lisa_kv_del(KV_KEY_SD_STAMP);
    lisa_kv_del(KV_KEY_SD_PRESENT);
}

static void sd_music_persist_card_state(const char *cid, const char *stamp)
{
    lisa_kv_set_string(KV_KEY_SD_CID, cid);
    lisa_kv_set_string(KV_KEY_SD_STAMP, stamp);
    sd_music_persist_card_present(cid);
    LOGI("KV 已更新: cid=%s, stamp=%s", cid, stamp);
}

static bool sd_music_has_saved_card_state(void)
{
    char *prev_cid = NULL;
    char *prev_stamp = NULL;
    bool has_state = false;

    if (lisa_kv_get_string(KV_KEY_SD_CID, &prev_cid) == 0 &&
        lisa_kv_get_string(KV_KEY_SD_STAMP, &prev_stamp) == 0 &&
        prev_cid && prev_cid[0] && prev_stamp && prev_stamp[0]) {
        has_state = true;
    }

    if (prev_cid) lisa_kv_free(prev_cid);
    if (prev_stamp) lisa_kv_free(prev_stamp);
    return has_state;
}

static bool sd_music_has_saved_card_kv(void)
{
    char *prev_cid = NULL;
    char *prev_stamp = NULL;
    bool has_kv = false;

    if (lisa_kv_get_string(KV_KEY_SD_CID, &prev_cid) == 0 &&
        prev_cid && prev_cid[0]) {
        has_kv = true;
    }
    if (lisa_kv_get_string(KV_KEY_SD_STAMP, &prev_stamp) == 0 &&
        prev_stamp && prev_stamp[0]) {
        has_kv = true;
    }

    if (prev_cid) lisa_kv_free(prev_cid);
    if (prev_stamp) lisa_kv_free(prev_stamp);
    return has_kv;
}

static bool sd_music_reconcile_boot_removed_event(const char *reason)
{
    char present_card_id[SD_MUSIC_CARD_ID_LEN] = {0};
    char removed_card_id[SD_MUSIC_CARD_ID_LEN] = {0};
    bool was_card_present = sd_music_get_present_card_id(present_card_id,
                                                          sizeof(present_card_id));
    bool has_saved_card_kv = sd_music_has_saved_card_kv();
    bool has_removed_card_id = false;

    if (!has_saved_card_kv && !was_card_present) {
        return false;
    }

    if (was_card_present && present_card_id[0]) {
        snprintf(removed_card_id, sizeof(removed_card_id), "%s", present_card_id);
        has_removed_card_id = true;
    } else {
        has_removed_card_id = sd_music_get_saved_card_id(removed_card_id,
                                                          sizeof(removed_card_id));
    }

    LOGI("%s，清理 SD KV 并补报 removed 事件", reason);
    if (has_removed_card_id) {
        if (sd_music_http_report_card_event(removed_card_id, "removed", 0,
                                            NULL, NULL) != 0) {
            LOGW("removed 事件上报失败，仍按已拔卡处理并清理 SD KV");
        }
    } else {
        LOGW("存在 SD KV，但缺少 card_id，跳过 removed 上报");
    }

    sd_music_clear_card_state();
    return true;
}

/* ============================================================================
 * 扫描/上传回调上下文
 * ============================================================================ */

typedef struct {
    music_item_t *accum;          /**< 累积曲目缓冲区（PSRAM 分配） */
    int accum_count;              /**< 已累积数量 */
    int accum_cap;                /**< 缓冲区容量 */
} sd_scan_batch_ctx_t;

typedef struct {
    const char *card_id;
    const char *scan_id;
    int uploaded_count;
    int total_count;
    int http_status_code;
    int http_error_code;
    int skipped_count;
    bool last_batch_reported;
} sd_upload_batch_ctx_t;

/**
 * @brief 流式扫描回调 — 逐批累加曲目
 *
 * 不做 HTTP 上报。上报决策由快速存储指纹控制。
 */
static int sd_music_scan_batch_callback(music_item_t *tracks, int count,
                                         bool is_last, void *user_data)
{
    sd_scan_batch_ctx_t *ctx = (sd_scan_batch_ctx_t *)user_data;
    (void)is_last;

    if (count == 0) return 0;

    for (int i = 0; i < count && ctx->accum_count < ctx->accum_cap; i++) {
        memcpy(&ctx->accum[ctx->accum_count], &tracks[i], sizeof(music_item_t));
        ctx->accum_count++;
    }

    return 0;
}

static void sd_music_build_scan_id(char *buf, int buf_len)
{
    static uint64_t last_scan_ms = 0;
    struct timeval tv = {0};
    uint64_t scan_ms = 0;

    if (ls_sys_get_time(&tv) == 0 && tv.tv_sec > 0) {
        scan_ms = (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
    }

    if (scan_ms <= last_scan_ms) {
        scan_ms = last_scan_ms + 1;
    }
    last_scan_ms = scan_ms;

    snprintf(buf, buf_len, "scan-%llu", (unsigned long long)scan_ms);
}

static int sd_music_upload_scan_callback(music_item_t *tracks, int count,
                                          bool is_last, void *user_data)
{
    sd_upload_batch_ctx_t *ctx = (sd_upload_batch_ctx_t *)user_data;

    if (count < 0) return 0;
    if (count == 0 && !is_last) return 0;

    if (count > 0) {
        voice_music_list_set(MUSIC_LIST_OFFLINE, tracks, count);
    }

    int http_status_code = 0;
    int http_error_code = 0;
    int ret = sd_music_http_report_files(ctx->card_id, ctx->scan_id, count,
                                         is_last, &http_status_code,
                                         &http_error_code);
    if (ret != 0) {
        ctx->http_status_code = http_status_code;
        ctx->http_error_code = http_error_code;
        LOGW("文件批次上报失败 [offset=%d, count=%d, is_last=%d]",
             ctx->uploaded_count, count, is_last ? 1 : 0);
        return ret;
    }

    ctx->uploaded_count += count;
    if (is_last) {
        ctx->last_batch_reported = true;
    }
    sd_music_publish_sync_state(VOICE_MSG_SD_MUSIC_SYNC_STATE_UPLOADING,
                                ctx->uploaded_count, ctx->total_count, 0,
                                0, 0, ctx->skipped_count);
    return 0;
}

static int sd_music_count_scan_callback(music_item_t *tracks, int count,
                                         bool is_last, void *user_data)
{
    (void)tracks;
    (void)count;
    (void)is_last;
    (void)user_data;
    return 0;
}

/* ============================================================================
 * 卡快照与快速指纹读取
 *
 * 快速指纹只使用文件系统剩余空间：free:<bytes>。
 * 这是近似判断，用于减少上电和重复插卡时的全量扫描/上报成本。
 * ============================================================================ */

static int sd_music_read_storage_stamp(uint64_t *capacity,
                                       char *storage_stamp,
                                       int storage_stamp_len)
{
    if (!capacity || !storage_stamp || storage_stamp_len <= 0) {
        return -1;
    }

    uint64_t fs_total = 0;
    uint64_t fs_free = 0;
    if (sd_music_card_get_storage_usage(&fs_total, &fs_free) != 0) {
        LOGW("快速存储指纹获取失败");
        return -1;
    }

    *capacity = sd_music_card_get_capacity();
    if (*capacity == 0) {
        *capacity = fs_total;
    }

    snprintf(storage_stamp, storage_stamp_len, "free:%llu",
             (unsigned long long)fs_free);
    LOGI("快速存储指纹: total=%llu, free=%llu, stamp=%s",
         (unsigned long long)fs_total,
         (unsigned long long)fs_free,
         storage_stamp);
    return 0;
}

/* ============================================================================
 * 公共同步动作
 *
 * inserted 事件始终先上报；随后扫描本地播放列表。
 * 文件列表是否全量上传由调用方传入的 stamp_changed 决定。
 * ============================================================================ */

static int sd_music_report_inserted_and_sync_files(const char *path,
                                                   const char *card_id,
                                                   uint64_t capacity,
                                                   const char *storage_stamp,
                                                   bool stamp_changed)
{
    int known_total = 0;
    int http_status_code = 0;
    int http_error_code = 0;
    if (sd_music_http_report_card_event(card_id, "inserted", capacity,
                                        &http_status_code,
                                        &http_error_code) != 0) {
        LOGW("inserted 事件上报失败，跳过扫描和文件列表上传并保留本地插卡状态");
        g_sd_music.saved_count = 0;
        g_sd_music.probe_path[0] = '\0';
        g_sd_music.card_ready = true;
        sd_music_finish_sync(VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED, 0, known_total,
                             VOICE_MSG_SD_MUSIC_SYNC_RESULT_FAILED,
                             http_status_code, http_error_code, 0);
        return -1;
    }
    sd_music_persist_card_present(card_id);

    /* 流式扫描（仅累积前 100 首，供本地播放） */
    int accum_cap = SD_MUSIC_SCANNER_MAX_TOTAL;
    music_item_t *accum = (music_item_t *)lisa_mem_calloc((uint32_t)accum_cap,
                                                           sizeof(music_item_t));
    if (!accum) {
        LOGE("分配扫描累积缓冲区失败, cap=%d", accum_cap);
        g_sd_music.current_card_id[0] = '\0';
        g_sd_music.probe_path[0] = '\0';
        g_sd_music.card_ready = false;
        sd_music_finish_sync(VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED, 0, known_total,
                             VOICE_MSG_SD_MUSIC_SYNC_RESULT_FAILED, 0, 0, 0);
        return -1;
    }

    sd_scan_batch_ctx_t ctx = {
        .accum       = accum,
        .accum_count = 0,
        .accum_cap   = accum_cap,
    };

    int ret = sd_music_scanner_scan(path, accum_cap,
                                    sd_music_scan_batch_callback,
                                    &ctx, NULL);

    if (ret != 0) {
        LOGW("本地播放列表扫描失败: %d", ret);
        lisa_mem_free(accum);
        g_sd_music.current_card_id[0] = '\0';
        g_sd_music.probe_path[0] = '\0';
        g_sd_music.card_ready = false;
        sd_music_finish_sync(VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED, 0,
                             known_total, ret, 0, 0, 0);
        return ret;
    }

    if (ctx.accum_count == 0) {
        voice_music_list_clear(MUSIC_LIST_OFFLINE);
        g_sd_music.saved_count = 0;
        g_sd_music.probe_path[0] = '\0';
        LOGI("扫描完成: 无 MP3 文件");
    } else {
        /* 写入本地播放列表 */
        voice_music_list_set(MUSIC_LIST_OFFLINE, accum, ctx.accum_count);
        g_sd_music.saved_count = ctx.accum_count;
        snprintf(g_sd_music.probe_path, sizeof(g_sd_music.probe_path),
                 "%s", accum[0].m_url);
    }
    known_total = ctx.accum_count;
    g_sd_music.full_count = known_total;
    g_sd_music.card_ready = true;

    /* 快速指纹未变 → 跳过文件列表上报 */
    if (!stamp_changed) {
        LOGI("inserted 已上报，快速指纹未变 (known=%d, stamp=%s)，跳过文件列表上报",
             known_total, storage_stamp);
        lisa_mem_free(accum);
        sd_music_finish_sync(VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED, 0, known_total,
                             VOICE_MSG_SD_MUSIC_SYNC_RESULT_DATA_UNCHANGED,
                             0, 0, 0);
        return 0;
    }

    /* 内容变更 → 上报全量文件列表 */
    LOGI("inserted 已上报，快速指纹变化 (known=%d, stamp=%s)，开始全量流式上报文件列表",
         known_total, storage_stamp);

    char scan_id[48];
    sd_music_build_scan_id(scan_id, sizeof(scan_id));
    sd_music_scanner_stats_t count_stats = {0};
    sd_music_scanner_stats_t upload_stats = {0};

    int count_ret = sd_music_scanner_scan(path, 0,
                                          sd_music_count_scan_callback,
                                          NULL, &count_stats);
    if (count_ret != 0) {
        LOGW("文件总数统计失败: %d", count_ret);
        lisa_mem_free(accum);
        sd_music_finish_sync(VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED, 0,
                             0, count_ret, 0, 0, 0);
        return count_ret;
    }

    sd_upload_batch_ctx_t upload_ctx = {
        .card_id = card_id,
        .scan_id = scan_id,
        .uploaded_count = 0,
        .total_count = (int)count_stats.reported_count,
        .skipped_count = 0,
        .last_batch_reported = false,
    };
    sd_music_publish_sync_state(VOICE_MSG_SD_MUSIC_SYNC_STATE_UPLOADING, 0,
                                upload_ctx.total_count,
                                VOICE_MSG_SD_MUSIC_SYNC_RESULT_OK, 0, 0, 0);
    int upload_ret = sd_music_scanner_scan(path, 0,
                                           sd_music_upload_scan_callback,
                                           &upload_ctx, &upload_stats);
    upload_ctx.skipped_count = (int)upload_stats.skipped_count;
    if (upload_ret == 0 && !upload_ctx.last_batch_reported) {
        upload_ret = sd_music_upload_scan_callback(NULL, 0, true, &upload_ctx);
    }

    /* 上传时 voice_music_list 会临时切成当前批次，这里恢复本地播放列表。 */
    if (ctx.accum_count > 0) {
        voice_music_list_set(MUSIC_LIST_OFFLINE, accum, ctx.accum_count);
    } else {
        voice_music_list_clear(MUSIC_LIST_OFFLINE);
    }

    voice_msg_sd_music_sync_state_e final_state;
    int final_result;
    if (upload_ret == 0) {
        /* 持久化快速存储指纹。 */
        sd_music_persist_card_state(card_id, storage_stamp);
        g_sd_music.full_count = upload_ctx.uploaded_count;
        LOGI("上报完成，scan_id=%s, uploaded=%d, skipped=%d, KV 已更新 (stamp=%s)",
             scan_id, upload_ctx.uploaded_count, upload_ctx.skipped_count,
             storage_stamp);
        final_state = VOICE_MSG_SD_MUSIC_SYNC_STATE_SUCCESSED;
        final_result = (upload_ctx.uploaded_count == 0 &&
                        upload_ctx.skipped_count == 0) ?
                       VOICE_MSG_SD_MUSIC_SYNC_RESULT_NO_MP3 :
                       VOICE_MSG_SD_MUSIC_SYNC_RESULT_OK;
    } else {
        LOGW("上报未完成，scan_id=%s, uploaded=%d",
             scan_id, upload_ctx.uploaded_count);
        final_state = VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED;
        final_result = upload_ret;
    }

    lisa_mem_free(accum);
    sd_music_finish_sync(final_state, upload_ctx.uploaded_count,
                         upload_ctx.total_count,
                         final_result, upload_ctx.http_status_code,
                         upload_ctx.http_error_code,
                         upload_ctx.skipped_count);
    return upload_ret;
}

/* ============================================================================
 * 上电状态收敛
 *
 * 刚上电：
 *   - 无卡 + 无 KV       → 无事发生
 *   - 无卡 + 有 KV       → 上报 removed，清除 KV
 *   - 有卡 + 无 KV       → 上报 inserted，上报文件列表，保存 KV
 *   - 有卡 + 有 KV 未变  → 无事发生
 *   - 有卡 + 有 KV 变化  → 上报 inserted，上报文件列表，保存 KV
 * ============================================================================ */

static int sd_music_handle_boot_present_card(const char *card_id)
{
    uint64_t capacity = 0;
    char storage_stamp[64] = {0};

    snprintf(g_sd_music.current_card_id, sizeof(g_sd_music.current_card_id),
             "%s", card_id);
    g_sd_music.card_ready = false;

    if (sd_music_read_storage_stamp(&capacity, storage_stamp,
                                    sizeof(storage_stamp)) != 0) {
        g_sd_music.current_card_id[0] = '\0';
        g_sd_music.probe_path[0] = '\0';
        g_sd_music.card_ready = false;
        g_sd_music.sync_busy = false;
        g_sd_music.sync_busy_until_ms = 0;
        return -1;
    }

    bool has_saved_card_state = sd_music_has_saved_card_state();
    bool stamp_changed = sd_music_has_card_changed(card_id, storage_stamp);

    if (has_saved_card_state && !stamp_changed) {
        g_sd_music.full_count = 0;
        g_sd_music.saved_count = 0;
        g_sd_music.probe_path[0] = '\0';
        g_sd_music.card_ready = true;
        sd_music_persist_card_present(card_id);
        g_sd_music.sync_busy = false;
        g_sd_music.sync_busy_until_ms = 0;

        LOGI("上电有卡 + KV 未变化，跳过 inserted 和文件列表上报: stamp=%s",
             storage_stamp);
        return 0;
    }

    LOGI("上电有卡 + %s，开始 inserted 和文件列表上报: stamp=%s",
         has_saved_card_state ? "KV 有变化" : "无有效 KV",
         storage_stamp);
    sd_music_publish_sync_state(VOICE_MSG_SD_MUSIC_SYNC_STATE_START,
                                0, 0, 0, 0, 0, 0);
    return sd_music_report_inserted_and_sync_files(NULL, card_id, capacity,
                                                   storage_stamp, true);
}

static int sd_music_handle_boot_state(void)
{
    char card_id[SD_MUSIC_CARD_ID_LEN] = {0};
    sd_music_card_get_cid_str(card_id, sizeof(card_id));

    if (card_id[0] == '\0') {
        sd_music_clear_local_card_state();
        if (!sd_music_reconcile_boot_removed_event("上电未检测到 TF 卡")) {
            LOGI("上电无卡 + 无历史 SD KV，无事发生");
        }
        return 0;
    }

    LOGI("上电检测到 TF 卡: card_id=%s", card_id);
    return sd_music_handle_boot_present_card(card_id);
}

/* ============================================================================
 * 运行时插拔处理
 *
 * 运行时插卡：先上报 inserted，再扫描本地列表；文件列表是否上传由 KV 决定。
 * 运行时拔卡：上报 removed，不清KV
 * ============================================================================ */

static int sd_music_handle_runtime_present_card(const char *path)
{
    char card_id[SD_MUSIC_CARD_ID_LEN] = {0};
    uint64_t capacity = 0;
    char storage_stamp[64] = {0};

    sd_music_card_get_cid_str(card_id, sizeof(card_id));
    if (card_id[0] == '\0') {
        LOGW("运行时同步未检测到 TF 卡 CID");
        sd_music_clear_local_card_state();
        sd_music_finish_sync(VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED, 0, 0,
                             VOICE_MSG_SD_MUSIC_SYNC_RESULT_FAILED, 0, 0, 0);
        return -1;
    }

    snprintf(g_sd_music.current_card_id, sizeof(g_sd_music.current_card_id),
             "%s", card_id);
    g_sd_music.card_ready = false;
    g_sd_music.full_count = 0;

    sd_music_publish_sync_state(VOICE_MSG_SD_MUSIC_SYNC_STATE_START,
                                0, 0, 0, 0, 0, 0);

    if (sd_music_read_storage_stamp(&capacity, storage_stamp,
                                    sizeof(storage_stamp)) != 0) {
        g_sd_music.current_card_id[0] = '\0';
        g_sd_music.probe_path[0] = '\0';
        g_sd_music.card_ready = false;
        sd_music_finish_sync(VOICE_MSG_SD_MUSIC_SYNC_STATE_FAILED, 0, 0,
                             VOICE_MSG_SD_MUSIC_SYNC_RESULT_FAILED, 0, 0, 0);
        return -1;
    }

    bool stamp_changed = sd_music_has_card_changed(card_id, storage_stamp);
    return sd_music_report_inserted_and_sync_files(path, card_id, capacity,
                                                   storage_stamp,
                                                   stamp_changed);
}

static void sd_music_removed_report_thread(void *arg)
{
    sd_music_removed_report_ctx_t *ctx = (sd_music_removed_report_ctx_t *)arg;

    if (ctx && ctx->card_id[0]) {
        if (sd_music_http_report_card_event(ctx->card_id, "removed", 0,
                                            NULL, NULL) != 0) {
            LOGW("removed 事件异步上报失败");
        }
    }

    if (ctx) {
        lisa_mem_free(ctx);
    }
    lisa_thread_delete(NULL);
}

static void sd_music_report_removed_async(const char *card_id)
{
    if (!card_id || card_id[0] == '\0') {
        return;
    }

    sd_music_removed_report_ctx_t *ctx =
        (sd_music_removed_report_ctx_t *)lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        LOGW("分配 removed 上报上下文失败");
        return;
    }
    snprintf(ctx->card_id, sizeof(ctx->card_id), "%s", card_id);

    lisa_thread_attr_t attr = {
        .name       = "sd_report",
        .stack_size = SD_MUSIC_WORK_STACK_SIZE,
        .priority   = 3,
    };

    if (lisa_thread_create(&attr, sd_music_removed_report_thread, ctx) == NULL) {
        LOGW("创建 removed 上报线程失败");
        lisa_mem_free(ctx);
    }
}

static void sd_music_handle_runtime_remove(void)
{
    LOGI(">>> SD 卡已拔出 <<<");

    char card_id[SD_MUSIC_CARD_ID_LEN] = {0};
    sd_music_card_get_cid_str(card_id, sizeof(card_id));
    if (!card_id[0] && g_sd_music.current_card_id[0]) {
        snprintf(card_id, sizeof(card_id), "%s", g_sd_music.current_card_id);
    }

    /* 只清理在位标记，保留快速指纹用于下次插入去重。 */
    sd_music_persist_card_present(NULL);
    sd_music_stop_music_on_remove();
    sd_music_clear_local_card_state();

    struct lsfs_mount_t *mp = platform_sd_mount_get();
    if (mp) {
        int unmount_ret = lsfs_unmount(mp);
        if (unmount_ret != 0) {
            LOGW("SD 卡文件系统卸载返回: %d", unmount_ret);
        }
    }
    sd_music_card_sdmmc_reset();
    sd_music_report_removed_async(card_id);
}

static bool sd_music_mount_runtime_card(void)
{
    sd_music_card_sdmmc_reset();

    lisa_device_t *dev = lisa_device_get("sdmmc0");
    if (!dev || lisa_sdmmc_probe(dev) != 0) {
        LOGI("SD 卡硬件探测失败，稍后重试");
        return false;
    }

    struct lsfs_mount_t *mp = platform_sd_mount_get();
    if (!mp) {
        LOGW("无法获取 SD 卡挂载点");
        return false;
    }

    int mount_ret = lsfs_mount(mp);
    if (mount_ret != 0 && !sd_music_card_test_dir(SD_MUSIC_ROOT_DIR)) {
        LOGI("SD 卡挂载失败，稍后重试");
        return false;
    }

    return true;
}

static bool sd_music_handle_runtime_insert(int *scan_ret_out)
{
    if (scan_ret_out) {
        *scan_ret_out = -1;
    }

    LOGI(">>> 检测到 SD 卡插入，开始挂载... <<<");
    if (!sd_music_mount_runtime_card()) {
        return false;
    }

    LOGI("SD 卡挂载成功，开始运行态扫描");
    sd_music_card_log_info();
    int scan_ret = service_sd_music_scan(NULL);
    if (scan_ret_out) {
        *scan_ret_out = scan_ret;
    }

    if (g_sd_music.card_ready && scan_ret != 0) {
        LOGW("TF 卡扫描/同步失败但文件系统仍可访问，保持插卡状态: %d",
             scan_ret);
    }

    return g_sd_music.card_ready;
}

static void sd_music_runtime_insert_thread(void *arg)
{
    int scan_ret = -1;
    bool ready;

    (void)arg;

    ready = sd_music_handle_runtime_insert(&scan_ret);
    if (ready) {
        LOGI("运行时插卡恢复完成: scan_ret=%d", scan_ret);
    } else {
        LOGI("运行时插卡恢复未完成: scan_ret=%d", scan_ret);
    }

    g_sd_music.runtime_insert_busy = false;
    lisa_thread_delete(NULL);
}

static bool sd_music_start_runtime_insert_worker(void)
{
    if (g_sd_music.runtime_insert_busy) {
        return true;
    }

    g_sd_music.runtime_insert_busy = true;

    lisa_thread_attr_t attr = {
        .name       = "sd_work",
        .stack_size = SD_MUSIC_WORK_STACK_SIZE,
        .priority   = 3,
    };

    if (lisa_thread_create(&attr, sd_music_runtime_insert_thread, NULL) == NULL) {
        g_sd_music.runtime_insert_busy = false;
        LOGW("创建运行时插卡恢复线程失败");
        return false;
    }

    return true;
}

/* ============================================================================
 * 后台轮询
 *
 * 检测策略：
 *   - 卡在位时：通过 probe_path 读 MP3 文件；无 MP3 时读物理扇区强制 I/O 检测
 *   - 卡不在位时：lib_sdc_card_exist() 快检触发运行时挂载恢复线程
 * ============================================================================ */

static void sd_music_poll_thread(void *arg)
{
    (void)arg;
    bool was_present = g_sd_music.card_ready;
    int debounce_count = 0;
    int probe_backoff = 0;
    bool removal_toast_sent = false;

    LOGI("轮询线程已启动（初始%s卡，间隔=%dms，防抖=%d次，退避=%d周期）",
         was_present ? "有" : "无",
         SD_MUSIC_POLL_INTERVAL_MS, SD_MUSIC_DEBOUNCE_COUNT, SD_MUSIC_PROBE_BACKOFF);

    while (1) {
        sd_music_clear_sync_busy_if_due();

        if (was_present) {
            /* ---- 卡在位：通过文件或物理扇区 I/O 检测卡是否仍在 ---- */
            const char *test_path = g_sd_music.probe_path[0] ?
                                    g_sd_music.probe_path : NULL;

            bool accessible = sd_music_card_test_access(test_path);

            if (accessible) {
                debounce_count = 0;
                removal_toast_sent = false;
            } else {
                if (!removal_toast_sent) {
                    voice_msg_pub(VOICE_MSG_APP_SD_MUSIC_CARD_REMOVED, NULL, 0);
                    sd_music_stop_music_on_remove();
                    removal_toast_sent = true;
                }
                debounce_count++;
                if (debounce_count >= SD_MUSIC_DEBOUNCE_COUNT) {
                    sd_music_handle_runtime_remove();
                    was_present = false;
                    debounce_count = 0;
                    removal_toast_sent = false;
                    LOGI("SD 卡拔出清理完成，等待重新插入");
                }
            }
        } else {
            /* ---- 卡不在位：用寄存器快检感知插入 ---- */
            if (g_sd_music.card_ready) {
                was_present = true;
                debounce_count = 0;
                removal_toast_sent = false;
                probe_backoff = 0;
            } else if (g_sd_music.runtime_insert_busy) {
                /* 插卡挂载/扫描/上报在 sd_work 中执行，poll 线程保持轻量。 */
            } else if (probe_backoff > 0) {
                probe_backoff--;
            } else if (lib_sdc_card_exist(0) == ERR_SD_NO_ERROR) {
                if (sd_music_start_runtime_insert_worker()) {
                    probe_backoff = SD_MUSIC_PROBE_BACKOFF;
                } else {
                    probe_backoff = 1;
                }
            }
        }

        lisa_thread_mdelay(SD_MUSIC_POLL_INTERVAL_MS);
    }
}

/* ============================================================================
 * 线程生命周期
 * ============================================================================ */

static void sd_music_start_poll_thread_once(void)
{
    if (g_sd_music.poll_thread != NULL) {
        return;
    }

    lisa_thread_attr_t attr = {
        .name       = "sd_poll",
        .stack_size = 16384,
        .priority   = 3,
    };

    g_sd_music.poll_thread = lisa_thread_create(&attr, sd_music_poll_thread, NULL);
    if (g_sd_music.poll_thread == NULL) {
        LOGW("创建 SD 卡轮询线程失败，插拔检测不可用");
    }
}

static void sd_music_initial_scan_thread(void *arg)
{
    (void)arg;

    /*
     * 首次扫描/上报可能包含 HTTP 请求，不能直接跑在 voice.ebus 回调里；
     * 否则 UI 同步事件会排队等待当前回调结束，启动阶段看不到同步页面。
     */
    lisa_mutex_lock(g_sd_music.lock, LISA_WAIT_FOREVER);
    int init_scan_ret = sd_music_handle_boot_state();
    lisa_mutex_unlock(g_sd_music.lock);

    LOGI("SD 卡启动状态处理完成: ret=%d, card_ready=%d",
         init_scan_ret, g_sd_music.card_ready ? 1 : 0);

    sd_music_start_poll_thread_once();
    g_sd_music.init_thread = NULL;
    lisa_thread_delete(NULL);
}

/* ============================================================================
 * 公开 API
 * ============================================================================ */

int service_sd_music_init(void)
{
    if (g_sd_music.lock != NULL) {
        return 0;
    }

    g_sd_music.lock = lisa_mutex_create();
    if (g_sd_music.lock == NULL) {
        LOGE("创建互斥锁失败");
        return -1;
    }

    /*
     * sd_init 异步处理 boot 初始状态：
     * - 无卡 + 无 KV：无事发生
     * - 无卡 + 有 KV：补报 removed 并清 KV
     * - 有卡 + 无 KV：上报 inserted + 文件列表并保存 KV
     * - 有卡 + 有 KV 未变：无事发生
     * - 有卡 + 有 KV 变化：上报 inserted + 文件列表并保存 KV
     * 完成后再启动 sd_poll，运行时插拔逻辑只在 poll 中处理。
     */
    lisa_thread_attr_t attr = {
        .name       = "sd_init",
        .stack_size = 16384,
        .priority   = 3,
    };
    g_sd_music.init_thread = lisa_thread_create(&attr, sd_music_initial_scan_thread, NULL);
    if (g_sd_music.init_thread == NULL) {
        LOGE("创建 SD 卡首次扫描线程失败，改为同步处理启动 SD 状态");
        lisa_mutex_lock(g_sd_music.lock, LISA_WAIT_FOREVER);
        (void)sd_music_handle_boot_state();
        lisa_mutex_unlock(g_sd_music.lock);
        sd_music_start_poll_thread_once();
        return -1;
    }

    return 0;
}

int service_sd_music_scan(const char *path)
{
    lisa_mutex_lock(g_sd_music.lock, LISA_WAIT_FOREVER);
    int ret = sd_music_handle_runtime_present_card(path);
    lisa_mutex_unlock(g_sd_music.lock);
    return ret;
}

bool service_sd_music_is_syncing(void)
{
    sd_music_clear_sync_busy_if_due();
    return g_sd_music.sync_busy;
}

bool app_voice_interaction_blocked(void)
{
    return service_sd_music_is_syncing();
}
