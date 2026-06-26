#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 保存最近一段业务日志，容量可通过 Kconfig 配置，默认 512KB。 */
#define LOG_BUFFER_CAPACITY (CONFIG_LOG_UPLOAD_BUFFER_CAPACITY_KB * 1024U)

struct log_buffer_span {
    const uint8_t *data;
    uint32_t size;
};

struct log_buffer_snapshot {
    struct log_buffer_span first;
    struct log_buffer_span second;
    uint32_t capacity;
    uint32_t valid_bytes;
    uint32_t next_write_offset;
};

/* 对外接口：获取一份稳定日志快照。调用后到 release 前，新日志会临时丢弃。 */
int log_buffer_snapshot_acquire(struct log_buffer_snapshot *snapshot);
/* 对外接口：释放快照窗口，恢复日志后端继续写入。 */
void log_buffer_snapshot_release(void);
/* 对外接口：清空当前缓冲日志，只保留之后的新日志。 */
void log_buffer_reset(void);

#ifdef __cplusplus
}
#endif
