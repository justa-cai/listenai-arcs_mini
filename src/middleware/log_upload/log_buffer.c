#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "sys_init.h"
#include "sysutils.h"

#define TAG "log_buffer"
#include "lisa_log.h"
#include "log_buffer.h"

#define LOG_BUFFER_BACKEND_NAME "log.upload.buffer"

static uint8_t g_log_buffer_storage[LOG_BUFFER_CAPACITY] __psram_noinit__;

struct log_buffer_state {
    uint32_t next_write_offset;
    uint32_t valid_bytes;
    bool snapshot_active;
};

static struct log_buffer_state s_log_buffer_state;
static SemaphoreHandle_t s_log_buffer_ctrl_mutex;

/* 返回两个无符号整数中的较小值，用于环形缓冲区长度裁剪。 */
static uint32_t log_buffer_min_u32(uint32_t lhs, uint32_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

/* 判断当前位置是否是 ANSI 颜色控制序列，命中后返回整段长度。 */
static bool log_buffer_match_ansi_sgr_sequence(const uint8_t *data, uint32_t len, uint32_t *seq_len)
{
    uint32_t offset = 2;

    if (!data || !seq_len || len < 3 || data[0] != '\033' || data[1] != '[') {
        return false;
    }

    while (offset < len && (((data[offset] >= '0' && data[offset] <= '9') || data[offset] == ';'))) {
        offset++;
    }

    if (offset < len && data[offset] == 'm') {
        *seq_len = offset + 1;
        return true;
    }

    return false;
}

/* 判断当前位置是否是更通用的 ANSI CSI 控制序列，命中后返回整段长度。 */
static bool log_buffer_match_ansi_csi_sequence(const uint8_t *data, uint32_t len, uint32_t *seq_len)
{
    uint32_t offset = 2;

    if (!data || !seq_len || len < 3 || data[0] != '\033' || data[1] != '[') {
        return false;
    }

    while (offset < len && data[offset] >= 0x30 && data[offset] <= 0x3F) {
        offset++;
    }
    while (offset < len && data[offset] >= 0x20 && data[offset] <= 0x2F) {
        offset++;
    }

    if (offset < len && data[offset] >= 0x40 && data[offset] <= 0x7E) {
        *seq_len = offset + 1;
        return true;
    }

    return false;
}

/* 校验当前位置是否是一个完整合法的 UTF-8 字符，并返回字节数。 */
static uint32_t log_buffer_match_utf8_sequence(const uint8_t *data, uint32_t len)
{
    if (!data || len == 0) {
        return 0;
    }

    if (data[0] <= 0x7F) {
        return 1;
    }

    if (len >= 2 &&
        data[0] >= 0xC2 && data[0] <= 0xDF &&
        data[1] >= 0x80 && data[1] <= 0xBF) {
        return 2;
    }

    if (len >= 3) {
        if (data[0] == 0xE0 &&
            data[1] >= 0xA0 && data[1] <= 0xBF &&
            data[2] >= 0x80 && data[2] <= 0xBF) {
            return 3;
        }
        if ((data[0] >= 0xE1 && data[0] <= 0xEC) || (data[0] >= 0xEE && data[0] <= 0xEF)) {
            if (data[1] >= 0x80 && data[1] <= 0xBF &&
                data[2] >= 0x80 && data[2] <= 0xBF) {
                return 3;
            }
        }
        if (data[0] == 0xED &&
            data[1] >= 0x80 && data[1] <= 0x9F &&
            data[2] >= 0x80 && data[2] <= 0xBF) {
            return 3;
        }
    }

    if (len >= 4) {
        if (data[0] == 0xF0 &&
            data[1] >= 0x90 && data[1] <= 0xBF &&
            data[2] >= 0x80 && data[2] <= 0xBF &&
            data[3] >= 0x80 && data[3] <= 0xBF) {
            return 4;
        }
        if (data[0] >= 0xF1 && data[0] <= 0xF3 &&
            data[1] >= 0x80 && data[1] <= 0xBF &&
            data[2] >= 0x80 && data[2] <= 0xBF &&
            data[3] >= 0x80 && data[3] <= 0xBF) {
            return 4;
        }
        if (data[0] == 0xF4 &&
            data[1] >= 0x80 && data[1] <= 0x8F &&
            data[2] >= 0x80 && data[2] <= 0xBF &&
            data[3] >= 0x80 && data[3] <= 0xBF) {
            return 4;
        }
    }

    return 0;
}

/* 把净化后的日志写入 512KB 环形缓冲区，只保留最新的数据。 */
static void log_buffer_write_bytes(const uint8_t *data, uint32_t len)
{
    uint32_t first_chunk_size;
    uint32_t wrapped_chunk_size;

    if (!data || len == 0) {
        return;
    }

    if (len >= LOG_BUFFER_CAPACITY) {
        memcpy(g_log_buffer_storage, data + len - LOG_BUFFER_CAPACITY, LOG_BUFFER_CAPACITY);
        s_log_buffer_state.next_write_offset = 0;
        s_log_buffer_state.valid_bytes = LOG_BUFFER_CAPACITY;
        return;
    }

    first_chunk_size = log_buffer_min_u32(len, LOG_BUFFER_CAPACITY - s_log_buffer_state.next_write_offset);
    memcpy(&g_log_buffer_storage[s_log_buffer_state.next_write_offset], data, first_chunk_size);

    wrapped_chunk_size = len - first_chunk_size;
    if (wrapped_chunk_size > 0) {
        memcpy(g_log_buffer_storage, data + first_chunk_size, wrapped_chunk_size);
    }

    s_log_buffer_state.next_write_offset =
        (s_log_buffer_state.next_write_offset + len) % LOG_BUFFER_CAPACITY;
    s_log_buffer_state.valid_bytes =
        log_buffer_min_u32(LOG_BUFFER_CAPACITY, s_log_buffer_state.valid_bytes + len);
}

/*
 * 只保留可直接作为 UTF-8 文本上传的内容：
 * - 丢掉 ANSI/CSI 控制序列
 * - ASCII 仅保留可打印字符和常见空白
 * - 多字节内容必须是合法 UTF-8
 */
static void log_buffer_append_sanitized_text(const uint8_t *data, uint32_t len)
{
    uint32_t segment_start = 0;
    uint32_t offset = 0;

    while (offset < len) {
        uint32_t seq_len = 0;
        uint32_t utf8_len = 0;

        if (log_buffer_match_ansi_sgr_sequence(data + offset, len - offset, &seq_len) ||
            log_buffer_match_ansi_csi_sequence(data + offset, len - offset, &seq_len)) {
            if (offset > segment_start) {
                log_buffer_write_bytes(data + segment_start, offset - segment_start);
            }
            offset += seq_len;
            segment_start = offset;
            continue;
        }

        if (data[offset] <= 0x7F) {
            if (data[offset] == '\n' || data[offset] == '\r' || data[offset] == '\t' ||
                (data[offset] >= 0x20 && data[offset] <= 0x7E)) {
                offset++;
                continue;
            }
        } else {
            utf8_len = log_buffer_match_utf8_sequence(data + offset, len - offset);
            if (utf8_len > 0) {
                offset += utf8_len;
                continue;
            }
        }

        if (offset > segment_start) {
            log_buffer_write_bytes(data + segment_start, offset - segment_start);
        }
        offset++;
        segment_start = offset;
    }

    if (segment_start < len) {
        log_buffer_write_bytes(data + segment_start, len - segment_start);
    }
}

/* 在暂停写入期间生成一个稳定快照，供上传线程读取完整日志。 */
static void log_buffer_fill_snapshot_locked(struct log_buffer_snapshot *snapshot)
{
    uint32_t oldest_log_offset;

    snapshot->capacity = LOG_BUFFER_CAPACITY;
    snapshot->valid_bytes = s_log_buffer_state.valid_bytes;
    snapshot->next_write_offset = s_log_buffer_state.next_write_offset;
    snapshot->first.data = NULL;
    snapshot->first.size = 0;
    snapshot->second.data = NULL;
    snapshot->second.size = 0;

    if (s_log_buffer_state.valid_bytes == 0) {
        return;
    }

    if (s_log_buffer_state.valid_bytes < LOG_BUFFER_CAPACITY) {
        snapshot->first.data = g_log_buffer_storage;
        snapshot->first.size = s_log_buffer_state.valid_bytes;
        return;
    }

    oldest_log_offset = s_log_buffer_state.next_write_offset;
    snapshot->first.data = &g_log_buffer_storage[oldest_log_offset];
    snapshot->first.size = LOG_BUFFER_CAPACITY - oldest_log_offset;

    if (oldest_log_offset > 0) {
        snapshot->second.data = g_log_buffer_storage;
        snapshot->second.size = oldest_log_offset;
    }
}

/* 暂停日志后端，防止上传过程中底层缓冲区继续被覆盖。 */
static int log_buffer_pause_backend_locked(void)
{
    int ret = lisa_log_backend_pause(LOG_BUFFER_BACKEND_NAME);

    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to pause log buffer backend: ret=%d", ret);
        return -EIO;
    }

    return 0;
}

/* 恢复日志后端写入，结束一次快照读取窗口。 */
static void log_buffer_resume_backend_locked(void)
{
    int ret = lisa_log_backend_resume(LOG_BUFFER_BACKEND_NAME);

    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to resume log buffer backend: ret=%d", ret);
    }
}

/* 作为 lisa_log 后端回调，把原始日志净化后写入上传缓冲区。 */
static void log_buffer_store_log(const uint8_t *log, uint32_t len, void *data)
{
    (void)data;

    /*
     * 这个后端保持 best-effort：
     * 1. 任务上下文里的串行化由 lisa_log 自己负责；
     * 2. 中断和 critical section 里不额外做内存写入，直接跳过。
     */
    if (xPortIsInsideInterrupt() || xPortIsInsideCritical()) {
        return;
    }

    log_buffer_append_sanitized_text(log, len);
}

/* 模块初始化：注册日志后端，并准备快照控制用的互斥锁。 */
static int log_buffer_init(void)
{
    int ret;

    if (s_log_buffer_ctrl_mutex == NULL) {
        s_log_buffer_ctrl_mutex = xSemaphoreCreateMutex();
        if (s_log_buffer_ctrl_mutex == NULL) {
            LISA_LOGE(TAG, "Failed to create log buffer mutex");
            return -ENOMEM;
        }
    }

    ret = lisa_log_backend_add(LOG_BUFFER_BACKEND_NAME, log_buffer_store_log, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to register log buffer backend: ret=%d", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Log buffer backend ready, capacity=%u", (unsigned int)LOG_BUFFER_CAPACITY);
    return 0;
}

/* 对外接口：获取一份稳定日志快照，期间新日志会暂时丢弃。 */
int log_buffer_snapshot_acquire(struct log_buffer_snapshot *snapshot)
{
    int ret;

    if (!snapshot) {
        return -EINVAL;
    }

    if (xPortIsInsideInterrupt()) {
        return -EPERM;
    }

    if (s_log_buffer_ctrl_mutex == NULL) {
        return -EIO;
    }

    xSemaphoreTake(s_log_buffer_ctrl_mutex, portMAX_DELAY);

    if (s_log_buffer_state.snapshot_active) {
        xSemaphoreGive(s_log_buffer_ctrl_mutex);
        return -EBUSY;
    }

    ret = log_buffer_pause_backend_locked();
    if (ret != 0) {
        xSemaphoreGive(s_log_buffer_ctrl_mutex);
        return ret;
    }

    s_log_buffer_state.snapshot_active = true;
    log_buffer_fill_snapshot_locked(snapshot);
    xSemaphoreGive(s_log_buffer_ctrl_mutex);

    LISA_LOGI(TAG, "Snapshot acquired: valid=%u first=%u second=%u next=%u",
              snapshot->valid_bytes,
              snapshot->first.size,
              snapshot->second.size,
              snapshot->next_write_offset);
    return 0;
}

/* 对外接口：释放快照窗口，恢复日志后端继续写入。 */
void log_buffer_snapshot_release(void)
{
    if (xPortIsInsideInterrupt() || s_log_buffer_ctrl_mutex == NULL) {
        return;
    }

    xSemaphoreTake(s_log_buffer_ctrl_mutex, portMAX_DELAY);
    if (s_log_buffer_state.snapshot_active) {
        log_buffer_resume_backend_locked();
        s_log_buffer_state.snapshot_active = false;
        LISA_LOGI(TAG, "Snapshot released");
    }
    xSemaphoreGive(s_log_buffer_ctrl_mutex);
}

/* 对外接口：清空已缓存的上传日志，只保留之后产生的新日志。 */
void log_buffer_reset(void)
{
    bool paused_here = false;
    int ret;

    if (xPortIsInsideInterrupt() || s_log_buffer_ctrl_mutex == NULL) {
        return;
    }

    xSemaphoreTake(s_log_buffer_ctrl_mutex, portMAX_DELAY);

    if (!s_log_buffer_state.snapshot_active) {
        ret = log_buffer_pause_backend_locked();
        if (ret != 0) {
            xSemaphoreGive(s_log_buffer_ctrl_mutex);
            return;
        }
        paused_here = true;
    }

    s_log_buffer_state.next_write_offset = 0;
    s_log_buffer_state.valid_bytes = 0;
    LISA_LOGI(TAG, "Log buffer reset");

    if (paused_here) {
        log_buffer_resume_backend_locked();
    }

    xSemaphoreGive(s_log_buffer_ctrl_mutex);
}

SYS_INIT(log_buffer_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
