#ifndef __ADB_DEBUG_STATS_H__
#define __ADB_DEBUG_STATS_H__

#include <stdint.h>

#define BOOT_ADB_SYNC_DIAG_RECENT_COUNT 4U

#define BOOT_ADB_SYNC_PATH_DIRECT 0U
#define BOOT_ADB_SYNC_PATH_STAGE  1U
#define BOOT_ADB_SYNC_PATH_ASYNC  2U
#define BOOT_ADB_SYNC_PATH_NULL   3U
#define BOOT_ADB_SYNC_PATH_RAM    4U
#define BOOT_ADB_SYNC_PATH_INRAM  5U

#define BOOT_ADB_SYNC_ASYNC_INIT_NONE            0U
#define BOOT_ADB_SYNC_ASYNC_INIT_FREE_QUEUE      1U
#define BOOT_ADB_SYNC_ASYNC_INIT_READY_QUEUE     2U
#define BOOT_ADB_SYNC_ASYNC_INIT_DONE_QUEUE      3U
#define BOOT_ADB_SYNC_ASYNC_INIT_BUFFER_ALLOC    4U
#define BOOT_ADB_SYNC_ASYNC_INIT_FREE_QUEUE_FILL 5U
#define BOOT_ADB_SYNC_ASYNC_INIT_TASK_CREATE     6U

struct adb_sync_debug_stats {
    uint32_t wrte_total;
    uint32_t wrte_recent[BOOT_ADB_SYNC_DIAG_RECENT_COUNT];
    uint32_t claim_total;
    uint32_t claim_recent[BOOT_ADB_SYNC_DIAG_RECENT_COUNT];
    uint32_t stat_total;
    uint32_t send_total;
    uint32_t data_total;
    uint32_t done_total;
    uint32_t data_recent[BOOT_ADB_SYNC_DIAG_RECENT_COUNT];
    uint64_t data_bytes;
    uint32_t file_bytes;
    uint32_t file_path_mode;
    uint32_t async_init_fail_stage;
    int32_t async_init_fail_rc;
    uint32_t async_submit_calls;
    uint64_t async_submit_bytes;
    uint32_t async_wait_free_count;
    uint32_t async_wait_free_ms;
    uint32_t async_wait_free_max_ms;
    uint32_t async_writer_flush_calls;
    uint32_t async_writer_flush_ms;
    uint32_t async_writer_flush_max_ms;
    uint32_t file_write_calls;
    uint32_t file_write_ms;
    uint32_t file_write_max_ms;
    uint32_t stage_flush_calls;
    uint32_t stage_flush_ms;
    uint32_t stage_flush_max_ms;
    uint32_t file_close_ms;
};

struct adb_dev_debug_stats {
    uint32_t unexpected_header_zero_count;
    uint32_t unexpected_header_other_count;
    uint32_t wait_packet_count;
    uint32_t rx_msg_total;
    uint32_t rx_data_total;
    uint32_t last_msg_payload_len;
    uint32_t last_data_read_len;
};

void adb_sync_debug_stats_get(struct adb_sync_debug_stats *stats);
void adb_dev_debug_stats_get(struct adb_dev_debug_stats *stats);

#endif
