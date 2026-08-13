#define LOG_TAG "adb.sync"

#include "adb_services.h"
#include "adb.h"
#include "adb_device.h"
#include "adb_utils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lsfs.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "adb_sync_ext_disk.h"
#include "adb_sync_stage.h"
#if defined(CONFIG_BOOT_ADB)
#include "boot_adb_path_policy.h"
#endif

extern int lsfs_preallocate(struct lsfs_file_t *fp, off_t length) __attribute__((weak));

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
#define ADB_SYNC_BOOT_CHERRYUSB_STAGE_FILE_PATH 1
#define ADB_SYNC_BOOT_CHERRYUSB_IO_BUFFER_SIZE ADB_SYNC_STAGE_PACKET_SIZE
#define ADB_SYNC_BOOT_CHERRYUSB_RAW_ALIGN_BUFFER_SIZE (4U * 512U)
#define ADB_SYNC_BOOT_CHERRYUSB_SKIP_RESERVE 1
#endif

#if defined(CONFIG_BOOT_ADB) || (defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP)
#include "esp_heap_caps.h"
#endif

#if defined(CONFIG_BOOT_ADB)
#ifndef CONFIG_BOOT_ADB_SDMMC_FS_ROOT
#define CONFIG_BOOT_ADB_SDMMC_FS_ROOT "/SD:/adb/"
#endif
#define ADB_SYNC_FS_ROOT CONFIG_BOOT_ADB_SDMMC_FS_ROOT
#else
#ifndef CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT
#define CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT "/RAM:/adb/"
#endif
#define ADB_SYNC_FS_ROOT CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT
#endif
#define ADB_SYNC_NULL_SINK_PREFIX "/NULL/"
#define ADB_SYNC_RAM_SINK_PREFIX  "/RAMSINK/"
#define ADB_SYNC_INRAM_SINK_PREFIX "/INRAMSINK/"
#define ADB_SYNC_RAM_SINK_CAPACITY (1024U * 1024U)

#define SYNC_ID(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

#define ADB_SYNC_ID_LIST SYNC_ID('L', 'I', 'S', 'T')
#define ADB_SYNC_ID_RECV SYNC_ID('R', 'E', 'C', 'V')
#define ADB_SYNC_ID_SEND SYNC_ID('S', 'E', 'N', 'D')
#define ADB_SYNC_ID_STAT SYNC_ID('S', 'T', 'A', 'T')
#define ADB_SYNC_ID_FAIL SYNC_ID('F', 'A', 'I', 'L')
#define ADB_SYNC_ID_DATA SYNC_ID('D', 'A', 'T', 'A')
#define ADB_SYNC_ID_DONE SYNC_ID('D', 'O', 'N', 'E')
#define ADB_SYNC_ID_OKAY SYNC_ID('O', 'K', 'A', 'Y')
#define ADB_SYNC_ID_QUIT SYNC_ID('Q', 'U', 'I', 'T')
#define ADB_SYNC_ID_DENT SYNC_ID('D', 'E', 'N', 'T')

#if defined(CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE)
#define ADB_SYNC_STAGE_ALIGNMENT CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE
#else
#define ADB_SYNC_STAGE_ALIGNMENT 64U
#endif

#define ADB_SYNC_STAGE_PACKET_SIZE (64U * 1024U)
/*
 * The host-side sync protocol still emits 64 KiB DATA chunks even when the
 * underlying ADB transport payload is smaller, so keep the request parser
 * aligned with the sync-layer framing rather than the WRTE packet size.
 */
#define ADB_SYNC_MAX_REQ_CHUNK_SIZE ADB_SYNC_STAGE_PACKET_SIZE

static inline uint32_t adb_sync_send_chunk_limit(uint32_t max_payload)
{
    if (max_payload > ADB_SYNC_STAGE_PACKET_SIZE) {
        return ADB_SYNC_STAGE_PACKET_SIZE;
    }

    return max_payload;
}

#define ADB_SYNC_TASK_PRIORITY (CONFIG_ADB_TASK_PRIORITY - 1)

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
/*
 * sync_task keeps ample headroom in recovery; trim the stack so SRAM-only
 * FATFS still has room to allocate its per-file FIL object.
 */
#define ADB_SYNC_TASK_STACK_DEPTH 1024U
/*
 * Boot sync still runs in WRTE/OKAY pacing. Two queued packets give the
 * producer one extra 64 KiB window before OKAY stalls on the consumer, while
 * still fitting inside recovery's internal-RAM budget.
 */
#define ADB_SYNC_RX_QUEUE_DEPTH 1U
#define ADB_SYNC_BOOT_CHERRYUSB_STAGE_BUFFER_SIZE ADB_SYNC_BOOT_CHERRYUSB_IO_BUFFER_SIZE
#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
/*
 * Three 64 KiB async buffers leave the producer waiting on almost every SDRAW
 * chunk; keep the buffers in PSRAM and deepen the queue enough to overlap USB
 * ingress with TF writes.
 */
#define ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT 3U
#define ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE ADB_SYNC_STAGE_PACKET_SIZE
#define ADB_SYNC_ASYNC_WRITER_STACK_DEPTH 1024U
#if (CONFIG_ADB_TASK_PRIORITY > 1)
#define ADB_SYNC_ASYNC_WRITER_PRIORITY (ADB_SYNC_TASK_PRIORITY - 1)
#else
#define ADB_SYNC_ASYNC_WRITER_PRIORITY ADB_SYNC_TASK_PRIORITY
#endif
#else
#define ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT 2U
#if (MAX_PAYLOAD < ADB_SYNC_STAGE_PACKET_SIZE)
#define ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE MAX_PAYLOAD
#else
#define ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE ADB_SYNC_STAGE_PACKET_SIZE
#endif
#define ADB_SYNC_ASYNC_WRITER_STACK_DEPTH 1024U
#if (CONFIG_ADB_TASK_PRIORITY > 1)
#define ADB_SYNC_ASYNC_WRITER_PRIORITY (ADB_SYNC_TASK_PRIORITY - 1)
#else
#define ADB_SYNC_ASYNC_WRITER_PRIORITY ADB_SYNC_TASK_PRIORITY
#endif
#endif
#define ADB_SYNC_INRAM_SINK_CAPACITY_MAX (32U * 1024U)
#define ADB_SYNC_INRAM_SINK_CAPACITY_MIN (4U * 1024U)
#define ADB_SYNC_INRAM_SINK_HEADROOM 1024U
#else
#define ADB_SYNC_TASK_STACK_DEPTH (1024U * 2U)
#define ADB_SYNC_RX_QUEUE_DEPTH 20U
#endif

#if defined(ADB_SYNC_BOOT_CHERRYUSB_STAGE_FILE_PATH)
#define ADB_SYNC_STAGE_BUFFER_SIZE ADB_SYNC_BOOT_CHERRYUSB_STAGE_BUFFER_SIZE
#define ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE 1
#elif defined(CONFIG_BOOT_ADB) && defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP && \
    !defined(ADB_SYNC_BOOT_CHERRYUSB_DISABLE_ASYNC_WRITER)
#define ADB_SYNC_STAGE_BUFFER_SIZE ADB_SYNC_STAGE_PACKET_SIZE
#define ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT 3U
#define ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE ADB_SYNC_STAGE_PACKET_SIZE
#define ADB_SYNC_ASYNC_WRITER_STACK_DEPTH 1024U
#if (CONFIG_ADB_TASK_PRIORITY > 1)
#define ADB_SYNC_ASYNC_WRITER_PRIORITY (ADB_SYNC_TASK_PRIORITY - 1)
#else
#define ADB_SYNC_ASYNC_WRITER_PRIORITY ADB_SYNC_TASK_PRIORITY
#endif
#elif defined(CONFIG_BOOT_ADB)
#define ADB_SYNC_STAGE_BUFFER_SIZE (60U * 1024U)
#else
#define ADB_SYNC_STAGE_BUFFER_SIZE ADB_SYNC_STAGE_PACKET_SIZE
#endif

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP && \
    !defined(ADB_SYNC_BOOT_CHERRYUSB_DISABLE_ASYNC_WRITER) && \
    defined(ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT)
#define ADB_SYNC_ASYNC_WRITER_ENABLED 1
#endif

/*
 * Reserve a larger window up front in recovery so long sequential pushes do
 * not have to re-expand and relink the file every 4 MiB.
 */
#define ADB_SYNC_FILE_PREALLOC_STEP (32U * 1024U * 1024U)
/*
 * Keep the CLMT small enough to fit recovery's internal heap while still
 * covering dozens of fragments if the TF card is moderately fragmented.
 */
#define ADB_SYNC_FILE_FASTSEEK_MAP_ITEMS 128U
struct adb_sync_req {
	uint32_t id;
	uint32_t len;
};

struct adb_sync_rsp_data {
	uint32_t id;
	uint32_t len;
};

struct adb_sync_rsp_stat {
	uint32_t id;
	uint32_t mode;
	uint32_t size;
	uint32_t time;
};

struct adb_sync_send_data {
	uint32_t id;
	uint32_t chunk_size;
};

struct adb_sync_done_data {
	uint32_t id;
	uint32_t time;
};

struct adb_sync_dent_data {
	uint32_t id;
	uint32_t mode;
	uint32_t size;
	uint32_t time;
	uint32_t namelen;
};

struct adb_sync_ctx {
	QueueHandle_t rx_queue;
	struct adb_service *s;
	TaskHandle_t task;
	SemaphoreHandle_t exit_sem;
	uint32_t rd_pos;
	adb_packet_t *curr_pkt;
	volatile bool closing;
#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    StackType_t *task_stack;
    StaticTask_t *task_tcb;
#endif
#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
    uint8_t *reserved_file_stage;
    uint8_t *reserved_raw_align;
#endif
};

struct adb_sync_file_stage_ctx {
    struct lsfs_file_t *fp;
    uint32_t *total_size;
    uint32_t reserved_size;
    bool fastseek_enabled;
};

struct adb_sync_ext_disk_stage_ctx {
    struct adb_sync_ext_disk_ctx *disk_ctx;
    uint32_t *total_size;
};

struct adb_sync_ext_disk_async_ctx {
    struct adb_sync_stage *stage;
    bool *stage_ready;
    struct adb_sync_ext_disk_stage_ctx *stage_ctx;
};

typedef void *(*adb_sync_sink_alloc_fn)(size_t align, size_t size);
typedef int (*adb_sync_sink_consume_fn)(struct adb_sync_ctx *ctx,
                                        void *sink_ctx,
                                        uint32_t *remaining,
                                        uint32_t *total_size);

struct adb_sync_ring_sink {
    uint8_t *buf;
    uint32_t capacity;
    uint32_t wr_pos;
};

struct adb_sync_packet_span {
    adb_packet_t *release_pkt;
    uint8_t *data;
    uint32_t len;
};

#define ADB_SYNC_EXT_DISK_GATHER_MAX_SPANS 4U

#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
struct adb_sync_async_buffer {
    uint8_t *buf;
    uint32_t size;
    void (*free_fn)(void *ptr);
};

typedef int (*adb_sync_async_buffer_flushv_fn)(void *user_data,
                                               struct adb_sync_async_buffer *const *buffers,
                                               uint32_t buffer_count);

struct adb_sync_async_writer {
    QueueHandle_t free_queue;
    QueueHandle_t ready_queue;
    QueueHandle_t done_queue;
    adb_sync_stage_flush_fn flush;
    adb_sync_async_buffer_flushv_fn flushv;
    void *flush_user_data;
    struct adb_sync_async_buffer *active;
    struct adb_sync_async_buffer buffers[ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT];
    bool task_started;
};
#endif

static struct adb_sync_ring_sink g_adb_sync_ram_sink;
#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
static uint8_t *g_adb_sync_reserved_file_stage_buf;
static uint32_t g_adb_sync_reserved_file_stage_busy;
static uint8_t *g_adb_sync_reserved_raw_align_buf;
static uint32_t g_adb_sync_reserved_raw_align_busy;
#endif

static inline uint32_t adb_sync_buffer_claim(struct adb_sync_ctx *ctx, uint8_t **data, uint32_t size)
{
	uint32_t len;

	if (ctx->curr_pkt == NULL) {
		adb_packet_t *p = NULL;
		if (xQueueReceive(ctx->rx_queue, &p, portMAX_DELAY) != pdTRUE) {
			ADB_LOGE("sync buffer read timeout\n");
			return -1;
		}
		ctx->rd_pos = 0;
		ctx->curr_pkt = p;
	}

    if ((ctx->rd_pos + size) > ctx->curr_pkt->msg.data_length) {
        len = ctx->curr_pkt->msg.data_length - ctx->rd_pos;
    } else {
        len = size;
    }

    *data = ctx->curr_pkt->data + ctx->rd_pos;

	return len;
}

static inline void adb_sync_buffer_commit(struct adb_sync_ctx *ctx, uint32_t size)
{
    ctx->rd_pos += size;

    if (ctx->rd_pos >= ctx->curr_pkt->msg.data_length) {
        adb_packet_free(ctx->curr_pkt);
        ctx->curr_pkt = NULL;
        ctx->rd_pos = 0;
    }
}

static inline int adb_sync_buffer_read(struct adb_sync_ctx *ctx, uint8_t *data, uint32_t size)
{
    int len;

    while (size) {
        uint8_t *p = NULL;
        len = adb_sync_buffer_claim(ctx, &p, size);
        memcpy(data, p, len);
		adb_sync_buffer_commit(ctx, len);
        data += len;
        size -= len;
    }

    return size;
}

static int adb_sync_discard_bytes(struct adb_sync_ctx *ctx, uint32_t size)
{
    while (size != 0U) {
        uint8_t *p = NULL;
        uint32_t len;

        len = adb_sync_buffer_claim(ctx, &p, size);
        if (len == 0U || len == (uint32_t)-1) {
            return -1;
        }

        adb_sync_buffer_commit(ctx, len);
        size -= len;
    }

    return 0;
}

static int adb_sync_discard_send_payload(struct adb_sync_ctx *ctx)
{
    while (1) {
        struct adb_sync_send_data req = {0};
        int r;

        adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(req));
        if (req.id == ADB_SYNC_ID_DONE) {
            return 0;
        }

        if (req.id != ADB_SYNC_ID_DATA) {
            ADB_LOGE("do_send, discard unknown req id:%x\n", req.id);
            return -1;
        }

        r = adb_sync_discard_bytes(ctx, req.chunk_size);
        if (r != 0) {
            return r;
        }
    }
}

static int adb_sync_buffer_gather_spans(struct adb_sync_ctx *ctx,
                                        struct adb_sync_packet_span *spans,
                                        uint32_t max_spans,
                                        uint32_t total_size,
                                        uint32_t *span_count)
{
    uint32_t count = 0U;

    if (ctx == NULL || spans == NULL || span_count == NULL) {
        return -EINVAL;
    }

    while (total_size != 0U) {
        uint32_t available;
        uint32_t len;

        if (count >= max_spans) {
            return -ENOSPC;
        }

        if (ctx->curr_pkt == NULL) {
            adb_packet_t *p = NULL;

            if (xQueueReceive(ctx->rx_queue, &p, portMAX_DELAY) != pdTRUE) {
                ADB_LOGE("sync buffer gather timeout\n");
                return -1;
            }
            ctx->rd_pos = 0U;
            ctx->curr_pkt = p;
        }

        available = ctx->curr_pkt->msg.data_length - ctx->rd_pos;
        len = total_size < available ? total_size : available;
        spans[count].release_pkt = NULL;
        spans[count].data = ctx->curr_pkt->data + ctx->rd_pos;
        spans[count].len = len;

        ctx->rd_pos += len;
        total_size -= len;
        if (ctx->rd_pos >= ctx->curr_pkt->msg.data_length) {
            spans[count].release_pkt = ctx->curr_pkt;
            ctx->curr_pkt = NULL;
            ctx->rd_pos = 0U;
        }

        count++;
    }

    *span_count = count;
    return 0;
}

static void adb_sync_buffer_release_spans(const struct adb_sync_packet_span *spans, uint32_t span_count)
{
    adb_packet_t *released[4] = {0};
    uint32_t released_count = 0U;

    if (spans == NULL) {
        return;
    }

    for (uint32_t i = 0; i < span_count; ++i) {
        adb_packet_t *pkt = spans[i].release_pkt;
        bool already_released = false;

        if (pkt == NULL) {
            continue;
        }

        for (uint32_t j = 0; j < released_count; ++j) {
            if (released[j] == pkt) {
                already_released = true;
                break;
            }
        }

        if (!already_released) {
            adb_packet_free(pkt);
            if (released_count < (sizeof(released) / sizeof(released[0]))) {
                released[released_count++] = pkt;
            }
        }
    }
}

static uint8_t *adb_get_full_path(const char *file_name)
{
	uint8_t *full_path = NULL;
	uint32_t len;
	bool use_default_root;

#if defined(CONFIG_BOOT_ADB)
	bool fs_enabled = boot_adb_path_policy_fs_enabled();

	if (!boot_adb_path_policy_is_allowed(file_name, fs_enabled)) {
		return NULL;
	}

	use_default_root = boot_adb_path_policy_should_prefix_default_root(file_name, fs_enabled);
#else
	use_default_root = file_name != NULL && file_name[0] != '/';
#endif

	if (use_default_root) {
		len = strlen(ADB_SYNC_FS_ROOT) + strlen(file_name) + 1;
	} else {
		len = strlen(file_name) + 1;
	}

	full_path = ADB_MALLOC(len);

	if (full_path == NULL) {
		return NULL;
	}

	memset(full_path, 0, len);
	if (use_default_root) {
		strcat(full_path, ADB_SYNC_FS_ROOT);
	}

	strcat(full_path, file_name);

	return full_path;
}

static uint8_t *adb_get_dir_path(const char *file_name)
{
    int idx = (int)strlen(file_name) - 1;

    while (idx >= 0) {
        if (file_name[idx] == '/') {
            break;
        }
        idx--;
    }

    if (idx < 0) {
        return NULL;
    }

    uint32_t len = idx + 1;

    uint8_t *dir_path = (uint8_t *)malloc(len + 1);
    if (dir_path == NULL) {
        return NULL;
    }

    memcpy(dir_path, file_name, len);
    dir_path[len] = '\0';

    return dir_path;
}

static bool adb_sync_path_has_prefix(const char *path, const char *prefix)
{
    return path != NULL && prefix != NULL && strncmp(path, prefix, strlen(prefix)) == 0;
}

static bool adb_sync_is_null_sink_path(const char *path)
{
    return adb_sync_path_has_prefix(path, ADB_SYNC_NULL_SINK_PREFIX);
}

static bool adb_sync_is_ram_sink_path(const char *path)
{
    return adb_sync_path_has_prefix(path, ADB_SYNC_RAM_SINK_PREFIX);
}

static bool adb_sync_is_inram_sink_path(const char *path)
{
    return adb_sync_path_has_prefix(path, ADB_SYNC_INRAM_SINK_PREFIX);
}

static int adb_sync_null_sink_consume(struct adb_sync_ctx *ctx,
                                      void *sink_ctx,
                                      uint32_t *remaining,
                                      uint32_t *total_size)
{
    (void)sink_ctx;

    while (*remaining != 0U) {
        uint8_t *p = NULL;
        int n;

        n = (int)adb_sync_buffer_claim(ctx, &p, *remaining);
        if (n <= 0) {
            return -1;
        }

        if (*total_size > (UINT32_MAX - (uint32_t)n)) {
            return -EOVERFLOW;
        }

        (void)p;
        adb_sync_buffer_commit(ctx, (uint32_t)n);
        *remaining -= (uint32_t)n;
        *total_size += (uint32_t)n;
    }

    return 0;
}

static void adb_sync_ring_sink_reset(struct adb_sync_ring_sink *sink)
{
    if (sink == NULL) {
        return;
    }

    sink->wr_pos = 0U;
}

static void adb_sync_ring_sink_deinit(struct adb_sync_ring_sink *sink, void (*free_fn)(void *ptr))
{
    if (sink == NULL) {
        return;
    }

    if (sink->buf != NULL && free_fn != NULL) {
        free_fn(sink->buf);
    }

    memset(sink, 0, sizeof(*sink));
}

static int adb_sync_ring_sink_init(struct adb_sync_ring_sink *sink,
                                   adb_sync_sink_alloc_fn alloc_fn,
                                   const uint32_t *capacities,
                                   size_t capacity_count)
{
    size_t i;

    if (sink == NULL || alloc_fn == NULL || capacities == NULL || capacity_count == 0U) {
        return -EINVAL;
    }

    if (sink->buf == NULL) {
        for (i = 0; i < capacity_count; i++) {
            if (capacities[i] == 0U) {
                continue;
            }

            sink->buf = alloc_fn(ADB_SYNC_STAGE_ALIGNMENT, capacities[i]);
            if (sink->buf != NULL) {
                sink->capacity = capacities[i];
                break;
            }
        }
    }

    if (sink->buf == NULL || sink->capacity == 0U) {
        return -ENOMEM;
    }

    adb_sync_ring_sink_reset(sink);
    return 0;
}

static int adb_sync_ram_sink_init(struct adb_sync_ring_sink *sink)
{
    static const uint32_t capacities[] = {
        ADB_SYNC_RAM_SINK_CAPACITY,
    };

    return adb_sync_ring_sink_init(sink,
                                   exram_malloc,
                                   capacities,
                                   sizeof(capacities) / sizeof(capacities[0]));
}

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
static uint32_t adb_sync_inram_sink_capacity_get(size_t largest_internal_block)
{
    size_t usable;

    if (largest_internal_block <=
        ((size_t)ADB_SYNC_INRAM_SINK_HEADROOM + (size_t)ADB_SYNC_STAGE_ALIGNMENT)) {
        return 0U;
    }

    usable = largest_internal_block - (size_t)ADB_SYNC_INRAM_SINK_HEADROOM -
             (size_t)ADB_SYNC_STAGE_ALIGNMENT;
    if (usable > (size_t)ADB_SYNC_INRAM_SINK_CAPACITY_MAX) {
        usable = (size_t)ADB_SYNC_INRAM_SINK_CAPACITY_MAX;
    }

    usable &= ~((size_t)ADB_SYNC_STAGE_ALIGNMENT - 1U);
    if (usable < (size_t)ADB_SYNC_INRAM_SINK_CAPACITY_MIN) {
        return 0U;
    }

    return (uint32_t)usable;
}

static int adb_sync_inram_sink_init(struct adb_sync_ring_sink *sink)
{
    size_t largest_internal_block;
    uint32_t capacity;

    if (sink == NULL) {
        return -EINVAL;
    }

    largest_internal_block = heap_caps_get_largest_free_block(
        MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
    capacity = adb_sync_inram_sink_capacity_get(largest_internal_block);
    if (capacity == 0U) {
        ADB_LOGE("do_send, inram sink no headroom, largest_internal=%lu\n",
                 (unsigned long)largest_internal_block);
        return -ENOMEM;
    }

    memset(sink, 0, sizeof(*sink));
    sink->buf = inram_malloc(ADB_SYNC_STAGE_ALIGNMENT, capacity);
    if (sink->buf == NULL) {
        return -ENOMEM;
    }

    sink->capacity = capacity;
    adb_sync_ring_sink_reset(sink);
    ADB_LOGI("do_send, inram sink largest_internal=%lu capacity=%u headroom=%u\n",
             (unsigned long)largest_internal_block,
             (unsigned int)capacity,
             (unsigned int)ADB_SYNC_INRAM_SINK_HEADROOM);
    return 0;
}
#endif

static void adb_sync_ring_sink_write(struct adb_sync_ring_sink *sink, const uint8_t *data, uint32_t len)
{
    if (sink == NULL || sink->buf == NULL || sink->capacity == 0U || data == NULL) {
        return;
    }

    while (len != 0U) {
        uint32_t copy_len;

        copy_len = sink->capacity - sink->wr_pos;
        if (copy_len > len) {
            copy_len = len;
        }

        memcpy(&sink->buf[sink->wr_pos], data, copy_len);
        sink->wr_pos += copy_len;
        if (sink->wr_pos == sink->capacity) {
            sink->wr_pos = 0U;
        }

        data += copy_len;
        len -= copy_len;
    }
}

static int adb_sync_ring_sink_consume(struct adb_sync_ctx *ctx,
                                      void *sink_ctx,
                                      uint32_t *remaining,
                                      uint32_t *total_size)
{
    struct adb_sync_ring_sink *sink = (struct adb_sync_ring_sink *)sink_ctx;

    if (sink == NULL) {
        return -EINVAL;
    }

    while (*remaining != 0U) {
        uint8_t *p = NULL;
        int n;

        n = (int)adb_sync_buffer_claim(ctx, &p, *remaining);
        if (n <= 0) {
            return -1;
        }

        if (*total_size > (UINT32_MAX - (uint32_t)n)) {
            return -EOVERFLOW;
        }

        adb_sync_ring_sink_write(sink, p, (uint32_t)n);
        adb_sync_buffer_commit(ctx, (uint32_t)n);
        *remaining -= (uint32_t)n;
        *total_size += (uint32_t)n;
    }

    return 0;
}

static void adb_sync_rsp_okay(struct adb_service *s);

static int adb_sync_handle_sink_path(struct adb_service *s,
                                     struct adb_sync_ctx *ctx,
                                     const char *tag,
                                     void *sink_ctx,
                                     adb_sync_sink_consume_fn consume,
                                     uint32_t *total_size)
{
    while (1) {
        struct adb_sync_send_data req;
        int r;

        adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_send_data));

        if (req.id != ADB_SYNC_ID_DATA) {
            if (req.id == ADB_SYNC_ID_DONE) {
                ADB_LOGI("do_send, %s done, timestamp:%d size=%u\n",
                         tag,
                         req.chunk_size,
                         (unsigned int)*total_size);
                adb_sync_rsp_okay(s);
                return 0;
            }

            ADB_LOGE("do_send, %s unknown req id:%x\n", tag, req.id);
            return -EINVAL;
        }

        if (req.chunk_size > ADB_SYNC_MAX_REQ_CHUNK_SIZE) {
            ADB_LOGE("do_send, %s invalid chunksize: %u\n", tag, req.chunk_size);
            return -EINVAL;
        }

        r = consume(ctx, sink_ctx, &req.chunk_size, total_size);
        if (r != 0) {
            ADB_LOGE("do_send, %s consume error: %d\n", tag, r);
            return r;
        }
    }
}

static bool adb_sync_is_closing(const struct adb_service *s)
{
	struct adb_sync_ctx *ctx = s != NULL ? s->data : NULL;

	return ctx != NULL && ctx->closing;
}

static void adb_sync_queue_drain(struct adb_sync_ctx *ctx)
{
	adb_packet_t *pkt = NULL;

	if (ctx == NULL || ctx->rx_queue == NULL) {
		return;
	}

	while (xQueueReceive(ctx->rx_queue, &pkt, 0) == pdTRUE) {
		if (pkt != NULL) {
			adb_packet_free(pkt);
		}
	}
}

static void adb_sync_queue_request_exit(struct adb_sync_ctx *ctx)
{
	adb_packet_t *pkt = NULL;
	struct adb_sync_req quit_req = {
		.id = ADB_SYNC_ID_QUIT,
		.len = 0U,
	};

	if (ctx == NULL || ctx->rx_queue == NULL) {
		return;
	}

	pkt = adb_packet_alloc(sizeof(quit_req));
	if (pkt == NULL) {
		return;
	}

	pkt->msg.data_length = sizeof(quit_req);
	memcpy(pkt->data, &quit_req, sizeof(quit_req));
	if (xQueueSend(ctx->rx_queue, &pkt, 0) != pdTRUE) {
		adb_packet_free(pkt);
	}
}

static void adb_sync_rsp_okay(struct adb_service *s)
{
	struct adb_sync_rsp_data okay_data = {
		.id = ADB_SYNC_ID_OKAY,
		.len = 0,
	};

	if (adb_sync_is_closing(s)) {
		return;
	}

	adb_service_write_remote(s, (uint8_t *)&okay_data, sizeof(struct adb_sync_rsp_data));
}

static void adb_sync_rsp_fail(struct adb_service *s)
{
	struct adb_sync_rsp_data fail_data = {
		.id = ADB_SYNC_ID_FAIL,
		.len = 0,
	};

	if (adb_sync_is_closing(s)) {
		return;
	}

	adb_service_write_remote(s, (uint8_t *)&fail_data, sizeof(fail_data));
}

static void adb_sync_rsp_stat(struct adb_service *s, struct adb_sync_rsp_stat *stat)
{
	if (adb_sync_is_closing(s)) {
		return;
	}

	adb_service_write_remote(s, (uint8_t *)stat, sizeof(struct adb_sync_rsp_stat));
}

static void adb_file_close(void *f)
{
	int r = lsfs_close((struct lsfs_file_t *)f);

	if (r != 0) {
		ADB_LOGE("adb file close failed, %d\n", r);
	}
}

static int adb_sync_stage_init_default(struct adb_sync_stage *stage)
{
    return adb_sync_stage_init(stage, ADB_SYNC_STAGE_BUFFER_SIZE, ADB_SYNC_STAGE_ALIGNMENT);
}

static bool adb_sync_stage_try_enable(struct adb_sync_stage *stage, const char *tag)
{
    if (adb_sync_stage_init_default(stage) == 0) {
        ADB_LOGI("do_send, %s staging enabled, size=%u\n",
                 tag, (unsigned int)stage->capacity);
        return true;
    }

    ADB_LOGW("do_send, %s staging buffer init failed, fallback to direct write\n", tag);
    return false;
}

#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
static int adb_sync_reserved_file_stage_prepare_internal(void)
{
    size_t required_internal_block;
    size_t largest_internal_block;

    if (g_adb_sync_reserved_file_stage_buf != NULL) {
        return 0;
    }

    required_internal_block = (size_t)ADB_SYNC_STAGE_BUFFER_SIZE +
                              (size_t)ADB_SYNC_STAGE_ALIGNMENT;
    largest_internal_block = heap_caps_get_largest_free_block(
        MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
    if (largest_internal_block < required_internal_block) {
        return -ENOMEM;
    }

    g_adb_sync_reserved_file_stage_buf = inram_malloc(ADB_SYNC_STAGE_ALIGNMENT,
                                                      ADB_SYNC_STAGE_BUFFER_SIZE);
    if (g_adb_sync_reserved_file_stage_buf == NULL) {
        return -ENOMEM;
    }

    return 0;
}

static int adb_sync_reserved_raw_align_prepare_internal(void)
{
    if (g_adb_sync_reserved_raw_align_buf != NULL) {
        return 0;
    }

    g_adb_sync_reserved_raw_align_buf = inram_malloc(ADB_SYNC_STAGE_ALIGNMENT,
                                                     ADB_SYNC_BOOT_CHERRYUSB_RAW_ALIGN_BUFFER_SIZE);
    if (g_adb_sync_reserved_raw_align_buf == NULL) {
        ADB_LOGW("sync raw align buffer prealloc failed, size=%u\n",
                 (unsigned int)ADB_SYNC_BOOT_CHERRYUSB_RAW_ALIGN_BUFFER_SIZE);
        return -ENOMEM;
    }

    ADB_LOGI("sync raw align buffer prepared, size=%u\n",
             (unsigned int)ADB_SYNC_BOOT_CHERRYUSB_RAW_ALIGN_BUFFER_SIZE);
    return 0;
}

static bool adb_sync_stage_try_enable_reserved(struct adb_sync_stage *stage,
                                               uint8_t *buf,
                                               const char *tag)
{
    if (buf != NULL &&
        adb_sync_stage_attach(stage, buf, ADB_SYNC_STAGE_BUFFER_SIZE) == 0) {
        ADB_LOGI("do_send, %s staging enabled, size=%u (reserved)\n",
                 tag,
                 (unsigned int)stage->capacity);
        return true;
    }

    ADB_LOGW("do_send, %s reserved staging unavailable, fallback to direct write\n", tag);
    return false;
}

static uint8_t *adb_sync_reserved_file_stage_get(struct adb_sync_ctx *ctx)
{
    uint8_t *buf = NULL;

    if (ctx == NULL) {
        return NULL;
    }

    if (ctx->reserved_file_stage == NULL) {
        (void)adb_sync_reserved_file_stage_prepare_internal();

        portENTER_CRITICAL();
        if (g_adb_sync_reserved_file_stage_buf != NULL &&
            g_adb_sync_reserved_file_stage_busy == 0U) {
            g_adb_sync_reserved_file_stage_busy = 1U;
            ctx->reserved_file_stage = g_adb_sync_reserved_file_stage_buf;
        }
        portEXIT_CRITICAL();

        if (ctx->reserved_file_stage == NULL) {
            ADB_LOGW("sync reserved file stage busy/unavailable, fallback to direct write\n");
        }
    }

    buf = ctx->reserved_file_stage;
    return buf;
}

static uint8_t *adb_sync_reserved_raw_align_get(struct adb_sync_ctx *ctx)
{
    uint8_t *buf = NULL;

    if (ctx == NULL) {
        return NULL;
    }

    if (ctx->reserved_raw_align == NULL) {
        if (g_adb_sync_reserved_raw_align_buf == NULL &&
            adb_sync_reserved_raw_align_prepare_internal() != 0) {
            return NULL;
        }

        portENTER_CRITICAL();
        if (g_adb_sync_reserved_raw_align_busy == 0U) {
            g_adb_sync_reserved_raw_align_busy = 1U;
            ctx->reserved_raw_align = g_adb_sync_reserved_raw_align_buf;
        }
        portEXIT_CRITICAL();

        if (ctx->reserved_raw_align == NULL) {
            ADB_LOGW("sync raw align buffer busy/unavailable, fallback to direct write\n");
        }
    }

    buf = ctx->reserved_raw_align;
    return buf;
}

static void adb_sync_reserved_file_stage_put(struct adb_sync_ctx *ctx)
{
    if (ctx == NULL || ctx->reserved_file_stage == NULL) {
        return;
    }

    portENTER_CRITICAL();
    if (ctx->reserved_file_stage == g_adb_sync_reserved_file_stage_buf) {
        g_adb_sync_reserved_file_stage_busy = 0U;
    }
    portEXIT_CRITICAL();
    ctx->reserved_file_stage = NULL;
}

static void adb_sync_reserved_raw_align_put(struct adb_sync_ctx *ctx)
{
    if (ctx == NULL || ctx->reserved_raw_align == NULL) {
        return;
    }

    portENTER_CRITICAL();
    if (ctx->reserved_raw_align == g_adb_sync_reserved_raw_align_buf) {
        g_adb_sync_reserved_raw_align_busy = 0U;
    }
    portEXIT_CRITICAL();
    ctx->reserved_raw_align = NULL;
}
#endif

static void adb_sync_stage_cleanup(struct adb_sync_stage *stage, bool *stage_ready)
{
    if (stage == NULL || stage_ready == NULL || !*stage_ready) {
        return;
    }

    adb_sync_stage_deinit(stage);
    *stage_ready = false;
}

static int adb_sync_file_stage_flush(void *user_data, const uint8_t *data, uint32_t len);
static int adb_sync_ext_disk_stage_flush(void *user_data, const uint8_t *data, uint32_t len);
static int adb_sync_ext_disk_async_writer_flush(void *user_data, const uint8_t *data, uint32_t len);
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
static int adb_sync_ext_disk_async_writer_flushv(void *user_data,
                                                 struct adb_sync_async_buffer *const *buffers,
                                                 uint32_t buffer_count);
#endif

static bool adb_sync_file_use_reserve(void)
{
#if defined(CONFIG_BOOT_ADB)
    return true;
#else
    /* Non-boot path: no prealloc. FATFS truncate-expand to 32 MB on SD is
     * extremely slow and often fails due to space constraints. */
    return false;
#endif
}

static bool adb_sync_file_use_direct_path(void)
{
#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
#if defined(CONFIG_PSRAM_HEAP) && CONFIG_PSRAM_HEAP
    /*
     * Recovery can spill the stage buffer to PSRAM, so forcing direct writes
     * here only turns long TF pushes into many small lsfs_write() calls.
     */
    return false;
#else
    size_t largest_internal_block = heap_caps_get_largest_free_block(
        MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
    size_t required_internal_block = (size_t)ADB_SYNC_STAGE_BUFFER_SIZE +
                                     (size_t)ADB_SYNC_MAX_REQ_CHUNK_SIZE +
                                     (size_t)ADB_SYNC_STAGE_ALIGNMENT;

#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
    if (g_adb_sync_reserved_file_stage_buf != NULL) {
        /*
         * The global reserved stage buffer persists across transfers, so a
         * follow-up push only needs headroom for the next incoming WRTE
         * packet, not another full stage allocation.
         */
        required_internal_block = (size_t)ADB_SYNC_MAX_REQ_CHUNK_SIZE +
                                  (size_t)ADB_SYNC_STAGE_ALIGNMENT;
    }
#endif

    if (largest_internal_block < required_internal_block) {
        ADB_LOGW("do_send, file direct path forced, largest_internal=%u need=%u\n",
                 (unsigned int)largest_internal_block,
                 (unsigned int)required_internal_block);
        return true;
    }
#endif
#endif

    return false;
}

static bool adb_sync_file_use_async_writer(void)
{
#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    return false;
#else
    return true;
#endif
}

static int adb_sync_file_fastseek_enable(struct adb_sync_file_stage_ctx *stage_ctx)
{
    int r;

    if (stage_ctx == NULL || stage_ctx->fp == NULL || stage_ctx->fastseek_enabled) {
        return 0;
    }

    r = lsfs_fastseek_link(stage_ctx->fp, ADB_SYNC_FILE_FASTSEEK_MAP_ITEMS);
    if (r == 0) {
        stage_ctx->fastseek_enabled = true;
        return 0;
    }

    ADB_LOGW("do_send, fastseek link skipped: %d\n", r);
    return 0;
}

static int adb_sync_file_fastseek_disable(struct adb_sync_file_stage_ctx *stage_ctx)
{
    int r;

    if (stage_ctx == NULL || stage_ctx->fp == NULL || !stage_ctx->fastseek_enabled) {
        return 0;
    }

    r = lsfs_fastseek_unlink(stage_ctx->fp);
    if (r == 0) {
        stage_ctx->fastseek_enabled = false;
    }

    return r;
}

static int adb_sync_file_preallocate(struct adb_sync_file_stage_ctx *stage_ctx, uint32_t size)
{
    if (lsfs_preallocate != NULL) {
        return lsfs_preallocate(stage_ctx->fp, (off_t)size);
    }

    /*
     * Older fs revisions do not export lsfs_preallocate(); fall back to
     * truncate so finalize() can trim the file back to the real payload size.
     */
    return lsfs_truncate(stage_ctx->fp, (off_t)size);
}

#if defined(ADB_SYNC_BOOT_CHERRYUSB_SKIP_RESERVE)
static int adb_sync_file_reserve(struct adb_sync_file_stage_ctx *stage_ctx, uint32_t len)
{
    (void)stage_ctx;
    (void)len;
    return 0;
}
#else
static int adb_sync_file_reserve(struct adb_sync_file_stage_ctx *stage_ctx, uint32_t len)
{
    uint32_t aligned_size;
    uint32_t needed_size;
    uint32_t remainder;
    int r;

    if (stage_ctx == NULL || stage_ctx->fp == NULL || stage_ctx->total_size == NULL) {
        return -EINVAL;
    }

    if (len == 0U) {
        return 0;
    }

    if (*stage_ctx->total_size > (UINT32_MAX - len)) {
        return -EOVERFLOW;
    }

    needed_size = *stage_ctx->total_size + len;
    if (needed_size <= stage_ctx->reserved_size) {
        return 0;
    }

    aligned_size = needed_size;
    remainder = aligned_size % ADB_SYNC_FILE_PREALLOC_STEP;
    if (remainder != 0U) {
        if (aligned_size > (UINT32_MAX - (ADB_SYNC_FILE_PREALLOC_STEP - remainder))) {
            return -EOVERFLOW;
        }
        aligned_size += ADB_SYNC_FILE_PREALLOC_STEP - remainder;
    }

    r = adb_sync_file_preallocate(stage_ctx, aligned_size);
    if (r != 0) {
        return r;
    }

    stage_ctx->reserved_size = aligned_size;
    return adb_sync_file_fastseek_enable(stage_ctx);
}
#endif

static int adb_sync_file_write_exact(struct adb_sync_file_stage_ctx *stage_ctx, const uint8_t *data, uint32_t len)
{
    int written;
    int r;

    if (stage_ctx == NULL || stage_ctx->fp == NULL || stage_ctx->total_size == NULL) {
        return -EINVAL;
    }

    if (adb_sync_file_use_reserve()) {
        r = adb_sync_file_reserve(stage_ctx, len);
        if (r != 0) {
            return r;
        }
    }

    written = lsfs_write(stage_ctx->fp, data, len);
    if (written != (int)len) {
        if (written == 0) {
            return -1;
        }
        return written;
    }

    *stage_ctx->total_size += (uint32_t)written;
    ADB_LOGD("do_send, file saved %u bytes", *stage_ctx->total_size);

    return 0;
}

static int adb_sync_file_stage_flush(void *user_data, const uint8_t *data, uint32_t len)
{
    struct adb_sync_file_stage_ctx *stage_ctx = (struct adb_sync_file_stage_ctx *)user_data;

    if (stage_ctx == NULL || stage_ctx->fp == NULL || stage_ctx->total_size == NULL) {
        return -1;
    }

    return adb_sync_file_write_exact(stage_ctx, data, len);
}

static int adb_sync_ext_disk_write_exact(struct adb_sync_ext_disk_stage_ctx *stage_ctx,
                                         const uint8_t *data,
                                         uint32_t len)
{
    int r;

    if (stage_ctx == NULL || stage_ctx->disk_ctx == NULL || stage_ctx->total_size == NULL) {
        return -EINVAL;
    }

    r = adb_sync_ext_disk_write(stage_ctx->disk_ctx, data, len);
    if (r != 0) {
        return r;
    }

    *stage_ctx->total_size += len;
    return 0;
}

static int adb_sync_ext_disk_write_buffer_exact(struct adb_sync_ext_disk_stage_ctx *stage_ctx,
                                                const uint8_t *data,
                                                uint32_t len)
{
    if (stage_ctx == NULL || stage_ctx->disk_ctx == NULL || data == NULL || len == 0U) {
        return -EINVAL;
    }

    return adb_sync_ext_disk_write_exact(stage_ctx, data, len);
}

static int adb_sync_ext_disk_write_spans(struct adb_sync_ext_disk_stage_ctx *stage_ctx,
                                         const struct adb_sync_packet_span *spans,
                                         uint32_t span_count)
{
    int r;

    for (uint32_t i = 0; i < span_count; ++i) {
        r = adb_sync_ext_disk_write_exact(stage_ctx, spans[i].data, spans[i].len);
        if (r != 0) {
            return r;
        }
    }

    return 0;
}

static int adb_sync_ext_disk_write_from_rx(struct adb_sync_ctx *ctx,
                                           struct adb_sync_ext_disk_stage_ctx *stage_ctx,
                                           uint32_t total_size)
{
    struct adb_sync_packet_span spans[ADB_SYNC_EXT_DISK_GATHER_MAX_SPANS];
    uint32_t span_count = 0U;
    int r;

    if (ctx == NULL || stage_ctx == NULL || stage_ctx->disk_ctx == NULL) {
        return -EINVAL;
    }

    memset(spans, 0, sizeof(spans));
    r = adb_sync_buffer_gather_spans(ctx,
                                     spans,
                                     ADB_SYNC_EXT_DISK_GATHER_MAX_SPANS,
                                     total_size,
                                     &span_count);
    if (r != 0) {
        return r;
    }

    r = adb_sync_ext_disk_write_spans(stage_ctx, spans, span_count);
    adb_sync_buffer_release_spans(spans, span_count);
    return r;
}

static int adb_sync_ext_disk_stage_flush(void *user_data, const uint8_t *data, uint32_t len)
{
    struct adb_sync_ext_disk_stage_ctx *stage_ctx = (struct adb_sync_ext_disk_stage_ctx *)user_data;

    if (stage_ctx == NULL || stage_ctx->disk_ctx == NULL || stage_ctx->total_size == NULL) {
        return -EINVAL;
    }

    return adb_sync_ext_disk_write_buffer_exact(stage_ctx, data, len);
}

static int adb_sync_ext_disk_async_writer_flush(void *user_data, const uint8_t *data, uint32_t len)
{
    struct adb_sync_ext_disk_async_ctx *async_ctx = (struct adb_sync_ext_disk_async_ctx *)user_data;

    if (async_ctx == NULL || async_ctx->stage_ctx == NULL) {
        return -EINVAL;
    }

    if (async_ctx->stage != NULL && async_ctx->stage_ready != NULL && *async_ctx->stage_ready) {
        return adb_sync_stage_write(async_ctx->stage,
                                    data,
                                    len,
                                    adb_sync_ext_disk_stage_flush,
                                    async_ctx->stage_ctx);
    }

    return adb_sync_ext_disk_write_buffer_exact(async_ctx->stage_ctx, data, len);
}

#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
static int adb_sync_ext_disk_async_writer_flushv(void *user_data,
                                                 struct adb_sync_async_buffer *const *buffers,
                                                 uint32_t buffer_count)
{
    uint32_t i;
    int r;

    if (buffers == NULL) {
        return -EINVAL;
    }

    /*
     * Keep the async writer draining ready buffers in batches even though
     * the SDRAW backend now falls back to repeated sector_write() calls.
     */
    for (i = 0U; i < buffer_count; ++i) {
        if (buffers[i] == NULL || buffers[i]->size == 0U) {
            continue;
        }

        r = adb_sync_ext_disk_async_writer_flush(user_data, buffers[i]->buf, buffers[i]->size);
        if (r != 0) {
            return r;
        }
    }

    return 0;
}
#endif

static int adb_sync_file_async_writer_flush(void *user_data, const uint8_t *data, uint32_t len)
{
    struct adb_sync_file_stage_ctx *stage_ctx = (struct adb_sync_file_stage_ctx *)user_data;

    if (stage_ctx == NULL || stage_ctx->fp == NULL || stage_ctx->total_size == NULL) {
        return -1;
    }

    return adb_sync_file_write_exact(stage_ctx, data, len);
}

static int adb_sync_file_finalize(struct adb_sync_file_stage_ctx *stage_ctx)
{
    int r;

    if (stage_ctx == NULL || stage_ctx->fp == NULL || stage_ctx->total_size == NULL) {
        return -EINVAL;
    }

    if (!adb_sync_file_use_reserve()) {
        return 0;
    }

    r = adb_sync_file_fastseek_disable(stage_ctx);
    if (r != 0) {
        return r;
    }

    if (stage_ctx->reserved_size <= *stage_ctx->total_size) {
        return 0;
    }

    return lsfs_truncate(stage_ctx->fp, (off_t)*stage_ctx->total_size);
}

static int adb_sync_stage_fill_from_rx(struct adb_sync_ctx *ctx,
                                       struct adb_sync_stage *stage,
                                       uint32_t *remaining,
                                       adb_sync_stage_flush_fn flush,
                                       void *user_data)
{
    while (*remaining != 0U) {
        uint8_t *p = NULL;
        uint32_t copy_len;
        uint32_t free_size;
        int r;

        if (stage->size == stage->capacity) {
            r = adb_sync_stage_finish(stage, flush, user_data);
            if (r != 0) {
                return r;
            }
        }

        free_size = stage->capacity - stage->size;
        copy_len = adb_sync_buffer_claim(ctx, &p, *remaining);
        if (copy_len > free_size) {
            copy_len = free_size;
        }
        if (copy_len == 0U) {
            return -1;
        }

        /*
         * Copy data out of the WRTE packet first, then flush the staging
         * buffer so the internal-RAM RX packet can be released before FS latency.
         */
        memcpy(stage->buf + stage->size, p, copy_len);
        stage->size += copy_len;
        adb_sync_buffer_commit(ctx, copy_len);
        *remaining -= copy_len;

        if (stage->size == stage->capacity) {
            r = adb_sync_stage_finish(stage, flush, user_data);
            if (r != 0) {
                return r;
            }
        }
    }

    return 0;
}

#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
static int adb_sync_async_buffer_alloc(struct adb_sync_async_buffer *buf, bool prefer_inram)
{
    if (buf == NULL) {
        return -EINVAL;
    }

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    if (prefer_inram) {
        buf->buf = adb_boot_try_inram_malloc(ADB_SYNC_STAGE_ALIGNMENT, ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE);
        if (buf->buf != NULL) {
            buf->free_fn = inram_free;
            return 0;
        }
    }

    buf->buf = psram_malloc_align(ADB_SYNC_STAGE_ALIGNMENT, ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE);
    if (buf->buf == NULL) {
        buf->buf = adb_boot_try_inram_malloc(ADB_SYNC_STAGE_ALIGNMENT, ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE);
        if (buf->buf == NULL) {
            return -ENOMEM;
        }

        buf->free_fn = inram_free;
        return 0;
    }

    buf->free_fn = psram_free;
    return 0;
#else
    buf->buf = adb_boot_try_inram_malloc(ADB_SYNC_STAGE_ALIGNMENT, ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE);
    if (buf->buf != NULL) {
        buf->free_fn = inram_free;
        return 0;
    }

    buf->buf = psram_malloc_align(ADB_SYNC_STAGE_ALIGNMENT, ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE);
    if (buf->buf == NULL) {
        return -ENOMEM;
    }

    buf->free_fn = psram_free;
    return 0;
#endif
}

static void adb_sync_async_writer_log_buffers(const struct adb_sync_async_writer *writer,
                                              size_t largest_internal_before)
{
    uint32_t inram_count = 0U;
    uint32_t psram_count = 0U;

    if (writer == NULL) {
        return;
    }

    for (uint32_t i = 0; i < ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT; i++) {
        if (writer->buffers[i].free_fn == inram_free) {
            inram_count++;
        } else if (writer->buffers[i].free_fn == psram_free) {
            psram_count++;
        }
    }

    (void)inram_count;
    (void)psram_count;
    (void)largest_internal_before;
}

static void adb_sync_async_writer_deinit(struct adb_sync_async_writer *writer)
{
    uint32_t i;

    if (writer == NULL) {
        return;
    }

    if (writer->done_queue != NULL) {
        vQueueDelete(writer->done_queue);
    }
    if (writer->ready_queue != NULL) {
        vQueueDelete(writer->ready_queue);
    }
    if (writer->free_queue != NULL) {
        vQueueDelete(writer->free_queue);
    }

    for (i = 0; i < ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT; i++) {
        if (writer->buffers[i].buf != NULL && writer->buffers[i].free_fn != NULL) {
            writer->buffers[i].free_fn(writer->buffers[i].buf);
        }
    }

    memset(writer, 0, sizeof(*writer));
}

static int adb_sync_async_writer_init_fail(struct adb_sync_async_writer *writer, int rc)
{
    adb_sync_async_writer_deinit(writer);
    return rc;
}

static uint32_t adb_sync_async_writer_collect_batch(struct adb_sync_async_writer *writer,
                                                    struct adb_sync_async_buffer *first,
                                                    struct adb_sync_async_buffer **batch,
                                                    uint32_t batch_capacity,
                                                    bool *stop_requested)
{
    struct adb_sync_async_buffer *buf = first;
    uint32_t batch_count = 0U;

    if (batch == NULL || batch_capacity == 0U || stop_requested == NULL) {
        return 0U;
    }

    *stop_requested = false;
    if (buf == NULL) {
        *stop_requested = true;
        return 0U;
    }

    batch[batch_count++] = buf;
    if (writer == NULL || writer->flushv == NULL) {
        return batch_count;
    }

    while (batch_count < batch_capacity) {
        if (xQueueReceive(writer->ready_queue, &buf, 0) != pdTRUE) {
            break;
        }

        if (buf == NULL) {
            *stop_requested = true;
            break;
        }

        batch[batch_count++] = buf;
    }

    return batch_count;
}

static int adb_sync_async_writer_flush_batch(struct adb_sync_async_writer *writer,
                                             struct adb_sync_async_buffer *const *batch,
                                             uint32_t batch_count)
{
    if (writer == NULL || batch == NULL) {
        return -EINVAL;
    }

    if (batch_count == 0U) {
        return 0;
    }

    if (writer->flushv != NULL) {
        return writer->flushv(writer->flush_user_data, batch, batch_count);
    }

    if (batch[0] == NULL || batch[0]->size == 0U) {
        return 0;
    }

    return writer->flush(writer->flush_user_data, batch[0]->buf, batch[0]->size);
}

static int adb_sync_async_writer_release_batch(struct adb_sync_async_writer *writer,
                                               struct adb_sync_async_buffer *const *batch,
                                               uint32_t batch_count)
{
    struct adb_sync_async_buffer *buf;

    if (writer == NULL || batch == NULL) {
        return -EINVAL;
    }

    for (uint32_t i = 0; i < batch_count; ++i) {
        buf = batch[i];
        if (buf == NULL) {
            continue;
        }

        buf->size = 0U;
        if (xQueueSend(writer->free_queue, &buf, portMAX_DELAY) != pdTRUE) {
            return -1;
        }
    }

    return 0;
}

static void adb_sync_async_writer_task(void *arg)
{
    struct adb_sync_async_writer *writer = (struct adb_sync_async_writer *)arg;
    struct adb_sync_async_buffer *buf = NULL;
    struct adb_sync_async_buffer *batch[ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT];
    uint32_t batch_count;
    bool stop_requested;
    int result = 0;

    while (xQueueReceive(writer->ready_queue, &buf, portMAX_DELAY) == pdTRUE) {
        batch_count = adb_sync_async_writer_collect_batch(writer,
                                                          buf,
                                                          batch,
                                                          ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT,
                                                          &stop_requested);
        if (batch_count == 0U) {
            break;
        }

        if (result == 0) {
            result = adb_sync_async_writer_flush_batch(writer, batch, batch_count);
        }

        if (adb_sync_async_writer_release_batch(writer, batch, batch_count) != 0) {
            if (result == 0) {
                result = -1;
            }
            break;
        }

        if (stop_requested) {
            break;
        }
    }

    xQueueSend(writer->done_queue, &result, portMAX_DELAY);
    vTaskDelete(NULL);
}

static int adb_sync_async_writer_take_active(struct adb_sync_async_writer *writer)
{
    if (writer == NULL) {
        return -EINVAL;
    }

    if (writer->active != NULL) {
        return 0;
    }

    if (xQueueReceive(writer->free_queue, &writer->active, portMAX_DELAY) != pdTRUE || writer->active == NULL) {
        writer->active = NULL;
        return -1;
    }

    writer->active->size = 0U;
    return 0;
}

static int adb_sync_async_writer_submit_active(struct adb_sync_async_writer *writer)
{
    struct adb_sync_async_buffer *buf;

    if (writer == NULL || writer->active == NULL) {
        return 0;
    }

    buf = writer->active;
    writer->active = NULL;

    if (buf->size == 0U) {
        return (xQueueSend(writer->free_queue, &buf, portMAX_DELAY) == pdTRUE) ? 0 : -1;
    }

    if (xQueueSend(writer->ready_queue, &buf, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    return 0;
}

static int adb_sync_async_writer_wait_done(struct adb_sync_async_writer *writer)
{
    int result = 0;

    if (writer == NULL || !writer->task_started) {
        return 0;
    }

    if (xQueueReceive(writer->done_queue, &result, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    writer->task_started = false;
    return result;
}

static int adb_sync_async_writer_signal_stop(struct adb_sync_async_writer *writer)
{
    struct adb_sync_async_buffer *buf = NULL;

    if (writer == NULL || !writer->task_started) {
        return 0;
    }

    return (xQueueSend(writer->ready_queue, &buf, portMAX_DELAY) == pdTRUE) ? 0 : -1;
}

static int adb_sync_async_writer_finish(struct adb_sync_async_writer *writer)
{
    int r;

    if (writer == NULL) {
        return -EINVAL;
    }

    r = adb_sync_async_writer_submit_active(writer);
    if (r != 0) {
        return r;
    }

    r = adb_sync_async_writer_signal_stop(writer);
    if (r != 0) {
        return r;
    }

    return adb_sync_async_writer_wait_done(writer);
}

static int adb_sync_async_writer_abort(struct adb_sync_async_writer *writer)
{
    int r;

    if (writer == NULL || !writer->task_started) {
        return 0;
    }

    if (writer->active != NULL) {
        writer->active->size = 0U;
    }
    writer->active = NULL;

    r = adb_sync_async_writer_signal_stop(writer);
    if (r != 0) {
        return r;
    }

    return adb_sync_async_writer_wait_done(writer);
}

static int adb_sync_async_writer_init(struct adb_sync_async_writer *writer,
                                      adb_sync_stage_flush_fn flush,
                                      adb_sync_async_buffer_flushv_fn flushv,
                                      void *flush_user_data)
{
    uint32_t i;
    size_t largest_internal_before;

    if (writer == NULL || flush == NULL) {
        return -EINVAL;
    }

    memset(writer, 0, sizeof(*writer));
    writer->flush = flush;
    writer->flushv = flushv;
    writer->flush_user_data = flush_user_data;
    largest_internal_before = heap_caps_get_largest_free_block(
        MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
    writer->free_queue = xQueueCreate(ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT, sizeof(struct adb_sync_async_buffer *));
    if (writer->free_queue == NULL) {
        return adb_sync_async_writer_init_fail(writer, -ENOMEM);
    }

    writer->ready_queue = xQueueCreate(ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT, sizeof(struct adb_sync_async_buffer *));
    if (writer->ready_queue == NULL) {
        return adb_sync_async_writer_init_fail(writer, -ENOMEM);
    }

    writer->done_queue = xQueueCreate(1, sizeof(int));
    if (writer->done_queue == NULL) {
        return adb_sync_async_writer_init_fail(writer, -ENOMEM);
    }

    for (i = 0; i < ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT; i++) {
        struct adb_sync_async_buffer *buf = &writer->buffers[i];

        if (adb_sync_async_buffer_alloc(buf, (i == 0U) && (flushv == NULL)) != 0) {
            return adb_sync_async_writer_init_fail(writer, -ENOMEM);
        }
        buf->size = 0U;

        if (xQueueSend(writer->free_queue, &buf, 0) != pdTRUE) {
            return adb_sync_async_writer_init_fail(writer, -1);
        }
    }

    if (xTaskCreate(adb_sync_async_writer_task, "sync_file_wr",
                    ADB_SYNC_ASYNC_WRITER_STACK_DEPTH, writer,
                    ADB_SYNC_ASYNC_WRITER_PRIORITY, NULL) != pdPASS) {
        return adb_sync_async_writer_init_fail(writer, -1);
    }

    writer->task_started = true;
    adb_sync_async_writer_log_buffers(writer, largest_internal_before);
    return 0;
}

static bool adb_sync_async_writer_try_enable(struct adb_sync_async_writer *writer,
                                             adb_sync_stage_flush_fn flush,
                                             adb_sync_async_buffer_flushv_fn flushv,
                                             void *flush_user_data,
                                             const char *tag)
{
    int rc = adb_sync_async_writer_init(writer, flush, flushv, flush_user_data);

    if (rc == 0) {
        ADB_LOGI("do_send, %s async staging enabled, buffers=%u size=%u\n",
                 tag,
                 (unsigned int)ADB_SYNC_ASYNC_STAGE_BUFFER_COUNT,
                 (unsigned int)ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE);
        return true;
    }

    ADB_LOGW("do_send, %s async staging init failed rc=%d, fallback to sync stage\n",
             tag,
             rc);
    return false;
}

static int adb_sync_async_writer_fill_from_rx(struct adb_sync_ctx *ctx,
                                              struct adb_sync_async_writer *writer,
                                              uint32_t *remaining)
{
    while (*remaining != 0U) {
        uint8_t *p = NULL;
        uint32_t copy_len;
        uint32_t free_size;
        int r;

        r = adb_sync_async_writer_take_active(writer);
        if (r != 0) {
            return r;
        }

        free_size = ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE - writer->active->size;
        copy_len = adb_sync_buffer_claim(ctx, &p, *remaining);
        if (copy_len > free_size) {
            copy_len = free_size;
        }
        if (copy_len == 0U) {
            return -1;
        }

        memcpy(writer->active->buf + writer->active->size, p, copy_len);
        writer->active->size += copy_len;
        adb_sync_buffer_commit(ctx, copy_len);
        *remaining -= copy_len;

        if (writer->active->size == ADB_SYNC_ASYNC_STAGE_BUFFER_SIZE) {
            r = adb_sync_async_writer_submit_active(writer);
            if (r != 0) {
                return r;
            }
        }
    }

    return 0;
}
#endif

static void do_stat(struct adb_service *s, struct adb_sync_req *req)
{
	struct adb_sync_ctx *ctx = s->data;
	uint8_t *file_name = ADB_MALLOC(req->len + 1);
	struct lsfs_dirent *ls_stat = NULL;
	struct adb_sync_rsp_stat stat = {
		.id = ADB_SYNC_ID_STAT,
	};
	int r;
	uint8_t *full_path = NULL;

	if (file_name == NULL) {
		ADB_LOGE("do stat,file_name ADB_MALLOC error\n");
		adb_sync_rsp_fail(s);
		return;
	}
	file_name[req->len] = '\0';
	adb_sync_buffer_read(ctx, file_name, req->len);

	full_path = adb_get_full_path(file_name);
	if (full_path == NULL) {
		ADB_LOGW("sync stat unsupported path: %s\n", file_name);
		adb_sync_rsp_stat(s, &stat);
		goto done;
	}
	ADB_LOGI("sync stat, stat file name: %s, full path: %s\n", file_name, full_path);

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint64_t start_addr = 0;
		uint64_t size = 0;
		r = adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		if (r == 0) {
			stat.mode = 33188;
			stat.size = (uint32_t)size;
			stat.time = 0;
		}
	} else {
		ls_stat = ADB_MALLOC(sizeof(struct lsfs_dirent));
		if (ls_stat == NULL) {
			ADB_LOGE("do stat, ls_stat ADB_MALLOC error\n");
			adb_sync_rsp_fail(s);
			goto failed;
		}
		memset(ls_stat, 0, sizeof(struct lsfs_dirent));
		r = lsfs_stat(full_path, ls_stat);
		if (r == 0) {
			/* normal file, 0o100644 */
			stat.mode = ls_stat->type == 0 ? 33188 : 16877;
			stat.size = ls_stat->size;

			/* TOTO: time */
			stat.time = 0;
			ADB_LOGI("file details: name=%s, size=%d, type=%d\n", ls_stat->name, ls_stat->size,
				ls_stat->type);
		} else {
			ADB_LOGE("Failed to get file stat, name:%s, error:%d\n", file_name, r);
		}
	}

	adb_sync_rsp_stat(s, &stat);
failed:
done:
	ADB_FREE(file_name);
	ADB_FREE(ls_stat);
	ADB_FREE(full_path);
}

static void do_send(struct adb_service *s, struct adb_sync_req *req)
{
	struct adb_sync_ctx *ctx = s->data;
	uint32_t total_size = 0;
    struct lsfs_file_t fp = {0};
    struct adb_sync_stage file_stage = {0};
    struct adb_sync_stage ext_disk_stage = {0};
    struct adb_sync_file_stage_ctx file_stage_ctx = {0};
    struct adb_sync_ext_disk_stage_ctx ext_disk_stage_ctx = {0};
    struct adb_sync_ext_disk_async_ctx ext_disk_async_ctx = {0};
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
    struct adb_sync_async_writer file_async_writer = {0};
    struct adb_sync_async_writer ext_disk_async_writer = {0};
#endif
	bool file_opened = false;
    bool file_stage_ready = false;
    bool ext_disk_stage_ready = false;
    bool need_discard = false;
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
    bool file_async_ready = false;
    bool ext_disk_async_ready = false;
#endif

	uint8_t *file_name = ADB_MALLOC(req->len + 1);
	int r;

	if (file_name == NULL) {
		ADB_LOGE("do_send, file_name ADB_MALLOC error\n");
		goto failed;
	}
	file_name[req->len] = '\0';

	adb_sync_buffer_read(ctx, file_name, req->len);
    need_discard = true;

	/* format: filename,mode */
	for (int i = 0; i < req->len; i++) {
		if (file_name[i] == ',') {
			file_name[i] = '\0';
			break;
		}
	}

	uint8_t *full_path = adb_get_full_path(file_name);
	if (full_path == NULL) {
		ADB_LOGE("do_send, adb_get_full_path error\n");
		goto failed;
	}

	ADB_LOGI("sync send, file name: %s, full path: %s\n", file_name, full_path);

    if (adb_sync_is_null_sink_path((const char *)full_path)) {
        ADB_LOGI("do_send, null sink enabled, path=%s\n", full_path);
        r = adb_sync_handle_sink_path(s,
                                      ctx,
                                      "null sink",
                                      NULL,
                                      adb_sync_null_sink_consume,
                                      &total_size);
        if (r == 0) {
            need_discard = false;
            goto done;
        }
        goto failed;
    }

    if (adb_sync_is_ram_sink_path((const char *)full_path)) {
        r = adb_sync_ram_sink_init(&g_adb_sync_ram_sink);
        if (r != 0) {
            ADB_LOGE("do_send, ram sink init error: %d\n", r);
            goto failed;
        }

        ADB_LOGI("do_send, ram sink enabled, path=%s, capacity=%u\n",
                 full_path,
                 (unsigned int)g_adb_sync_ram_sink.capacity);
        r = adb_sync_handle_sink_path(s,
                                      ctx,
                                      "ram sink",
                                      &g_adb_sync_ram_sink,
                                      adb_sync_ring_sink_consume,
                                      &total_size);
        if (r == 0) {
            need_discard = false;
            goto done;
        }
        goto failed;
    }

    if (adb_sync_is_inram_sink_path((const char *)full_path)) {
#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
        struct adb_sync_ring_sink inram_sink = {0};

        r = adb_sync_inram_sink_init(&inram_sink);
        if (r != 0) {
            ADB_LOGE("do_send, inram sink init error: %d\n", r);
            goto failed;
        }

        ADB_LOGI("do_send, inram sink enabled, path=%s, capacity=%u\n",
                 full_path,
                 (unsigned int)inram_sink.capacity);
        r = adb_sync_handle_sink_path(s,
                                      ctx,
                                      "inram sink",
                                      &inram_sink,
                                      adb_sync_ring_sink_consume,
                                      &total_size);
        adb_sync_ring_sink_deinit(&inram_sink, inram_free);
        if (r == 0) {
            need_discard = false;
            goto done;
        }
        goto failed;
#else
        ADB_LOGE("do_send, inram sink only supports boot cherryusb path\n");
        goto failed;
#endif
    }

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint64_t start_addr = 0;
		uint64_t size = 0;
		r = adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		if (r != 0) {
			ADB_LOGE("do_send, invalid raw path: %s\n", file_name);
			goto failed;
		}
		ADB_LOGI("do_send, ext disk access, disk_name:%s, start_addr:0x%llx, size:0x%llx\n", disk_name,
			 (unsigned long long)start_addr, (unsigned long long)size);
		struct adb_sync_ext_disk_ctx *disk_ctx = adb_sync_ext_disk_ctx_init(disk_name, start_addr, size, true);

		if (disk_ctx == NULL) {
			ADB_LOGE("do send, ext disk init error\n");
			goto failed;
		}
        ext_disk_stage_ctx.disk_ctx = disk_ctx;
        ext_disk_stage_ctx.total_size = &total_size;

        if (!disk_ctx->direct_flash) {
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
            ext_disk_async_ctx.stage = &ext_disk_stage;
            ext_disk_async_ctx.stage_ready = &ext_disk_stage_ready;
            ext_disk_async_ctx.stage_ctx = &ext_disk_stage_ctx;
            ext_disk_async_ready = adb_sync_async_writer_try_enable(&ext_disk_async_writer,
                                                                    adb_sync_ext_disk_async_writer_flush,
                                                                    adb_sync_ext_disk_async_writer_flushv,
                                                                    &ext_disk_async_ctx,
                                                                    "ext disk");
#endif
#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
            if (!ext_disk_async_ready) {
                uint8_t *raw_align_buf = adb_sync_reserved_file_stage_get(ctx);
                uint32_t raw_align_size = ADB_SYNC_STAGE_BUFFER_SIZE;

                if (raw_align_buf == NULL) {
                    raw_align_buf = adb_sync_reserved_raw_align_get(ctx);
                    raw_align_size = ADB_SYNC_BOOT_CHERRYUSB_RAW_ALIGN_BUFFER_SIZE;
                }

                adb_sync_ext_disk_set_align_buf(disk_ctx, raw_align_buf, raw_align_size);
            }
#endif
        }

		while (1) {
			struct adb_sync_send_data req;
			adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_send_data));

			if (req.id != ADB_SYNC_ID_DATA) {
				if (req.id == ADB_SYNC_ID_DONE) {
					ADB_LOGI("do_send, done, timestamp:%d\n", req.chunk_size);
                    need_discard = false;
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
                    if (ext_disk_async_ready) {
                        r = adb_sync_async_writer_finish(&ext_disk_async_writer);
                        if (r != 0) {
                            ADB_LOGE("do_send, ext disk async finalize error: %d\n", r);
                            adb_sync_async_writer_deinit(&ext_disk_async_writer);
                            ext_disk_async_ready = false;
                            adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
                            adb_sync_ext_disk_ctx_free(disk_ctx);
                            goto failed;
                        }
                        adb_sync_async_writer_deinit(&ext_disk_async_writer);
                        ext_disk_async_ready = false;
                    }
#endif
                    if (ext_disk_stage_ready) {
                        r = adb_sync_stage_finish(&ext_disk_stage,
                                                  adb_sync_ext_disk_stage_flush,
                                                  &ext_disk_stage_ctx);
                        if (r != 0) {
                            ADB_LOGE("do_send, ext disk staging flush error: %d\n", r);
                            adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
                            adb_sync_ext_disk_ctx_free(disk_ctx);
                            goto failed;
                        }
                        adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
                    }
					adb_sync_ext_disk_ctx_free(disk_ctx);
					adb_sync_rsp_okay(s);
					goto done;
				} else {
					ADB_LOGE("do_send, unknown req id:%x\n", req.id);
                    need_discard = false;
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
                    if (ext_disk_async_ready) {
                        r = adb_sync_async_writer_abort(&ext_disk_async_writer);
                        if (r != 0) {
                            ADB_LOGW("do_send, ext disk async abort error: %d\n", r);
                        }
                        adb_sync_async_writer_deinit(&ext_disk_async_writer);
                        ext_disk_async_ready = false;
                    }
#endif
                    adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
					adb_sync_ext_disk_ctx_free(disk_ctx);
					goto failed;
				}
			}
			if (req.chunk_size > ADB_SYNC_MAX_REQ_CHUNK_SIZE) {
				ADB_LOGE("do_send, invalid chunksize: %d\n", req.chunk_size);
                need_discard = false;
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
                if (ext_disk_async_ready) {
                    r = adb_sync_async_writer_abort(&ext_disk_async_writer);
                    if (r != 0) {
                        ADB_LOGW("do_send, ext disk async abort error: %d\n", r);
                    }
                    adb_sync_async_writer_deinit(&ext_disk_async_writer);
                    ext_disk_async_ready = false;
                }
#endif
                adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
				adb_sync_ext_disk_ctx_free(disk_ctx);
				goto failed;
			}
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
            if (ext_disk_async_ready) {
                need_discard = false;
                r = adb_sync_async_writer_fill_from_rx(ctx, &ext_disk_async_writer, &req.chunk_size);
                if (r != 0) {
                    ADB_LOGE("do_send, ext disk async write error: %d\n", r);
                    r = adb_sync_async_writer_abort(&ext_disk_async_writer);
                    if (r != 0) {
                        ADB_LOGW("do_send, ext disk async abort error: %d\n", r);
                    }
                    adb_sync_async_writer_deinit(&ext_disk_async_writer);
                    ext_disk_async_ready = false;
                    adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
                    adb_sync_ext_disk_ctx_free(disk_ctx);
                    goto failed;
                }
                need_discard = true;
                continue;
            }
#endif
            if (!disk_ctx->direct_flash && !ext_disk_stage_ready) {
                need_discard = false;
                r = adb_sync_ext_disk_write_from_rx(ctx, &ext_disk_stage_ctx, req.chunk_size);
                if (r != 0) {
                    ADB_LOGE("do_send, ext disk write error: %d\n", r);
                    adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
                    adb_sync_ext_disk_ctx_free(disk_ctx);
                    goto failed;
                }
                req.chunk_size = 0U;
                need_discard = true;
                continue;
            }
            need_discard = false;
			while (req.chunk_size) {
				uint8_t *p = NULL;
				int n = 0;
				n = adb_sync_buffer_claim(ctx, &p, req.chunk_size);
				req.chunk_size -= n;
                if (ext_disk_stage_ready) {
                    r = adb_sync_stage_write(&ext_disk_stage,
                                             p,
                                             (uint32_t)n,
                                             adb_sync_ext_disk_stage_flush,
                                             &ext_disk_stage_ctx);
                } else {
                    r = adb_sync_ext_disk_write_exact(&ext_disk_stage_ctx, p, (uint32_t)n);
                }
				adb_sync_buffer_commit(ctx, n);
				if (r) {
					ADB_LOGE("do_send, ext disk write error: %d\n", r);
                    adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
					adb_sync_ext_disk_ctx_free(disk_ctx);
					goto failed;
				}
			}
            need_discard = true;
		}
	}

	uint8_t *dir_path = adb_get_dir_path(full_path);
	ADB_LOGI("sync send, dir path: %s\n", dir_path);
	if (dir_path == NULL) {
		ADB_LOGE("do_send, adb_get_dir_path error\n");
		goto failed;
	}

	struct lsfs_dir_t dir;
	uint8_t *p = dir_path + 1;
	for (; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			ADB_LOGI("sync send, mkdir dir path: %s\n", dir_path);
			lsfs_dir_t_init(&dir);
			r = lsfs_opendir(&dir, dir_path);
			if (r) {
				r = lsfs_mkdir(dir_path);
				if (r) {
					ADB_LOGE("lsfs_mkdir error: %d\n", r);
					lsfs_closedir(&dir);
					ADB_FREE(dir_path);
					goto failed;
				}
			} else {
				lsfs_closedir(&dir);
			}
			*p = '/';
		}
	}
	ADB_FREE(dir_path);

	r = lsfs_open(&fp, full_path, LSFS_O_WRITE | LSFS_O_CREATE | LSFS_O_TRUNC);
	if (r != 0) {
		ADB_LOGE("open file error: %d\n", r);
		goto failed;
	}
	file_opened = true;
    file_stage_ctx.fp = &fp;
    file_stage_ctx.total_size = &total_size;
    if (!adb_sync_file_use_direct_path()) {
        if (adb_sync_file_use_async_writer()) {
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
            file_async_ready = adb_sync_async_writer_try_enable(&file_async_writer,
                                                                adb_sync_file_async_writer_flush,
                                                                NULL,
                                                                &file_stage_ctx,
                                                                "file");
#endif
        }
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
        if (!file_async_ready)
#endif
        {
#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
            file_stage_ready = adb_sync_stage_try_enable_reserved(&file_stage,
                                                                  adb_sync_reserved_file_stage_get(ctx),
                                                                  "file");
            if (!file_stage_ready) {
                file_stage_ready = adb_sync_stage_try_enable(&file_stage, "file");
            }
#else
            file_stage_ready = adb_sync_stage_try_enable(&file_stage, "file");
#endif
        }
    }

	/**
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * ....
	 * DONE(4) TIME(4)
	 */
	while (1) {
		struct adb_sync_send_data req;
		adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_send_data));

		if (ctx->closing && req.id == ADB_SYNC_ID_QUIT) {
			need_discard = false;
			goto done;
		}

		if (req.id != ADB_SYNC_ID_DATA) {
			if (req.id == ADB_SYNC_ID_DONE) {
				ADB_LOGI("do_send, done, timestamp:%d\n", req.chunk_size);
                need_discard = false;
				break;
			} else {
				ADB_LOGE("do_send, unknown req id:%x\n", req.id);
                need_discard = false;
				goto failed;
			}
		}

		if (req.chunk_size > ADB_SYNC_MAX_REQ_CHUNK_SIZE) {
			ADB_LOGE("do_send, invalid chunksize: %d\n", req.chunk_size);
            need_discard = false;
			goto failed;
        }

#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
        if (file_async_ready) {
            need_discard = false;
            r = adb_sync_async_writer_fill_from_rx(ctx, &file_async_writer, &req.chunk_size);
            if (r != 0) {
                ADB_LOGE("lsfs async write error: %d\n", r);
                goto failed;
            }
            need_discard = true;
            continue;
        }
#endif

        if (file_stage_ready) {
            need_discard = false;
            r = adb_sync_stage_fill_from_rx(ctx, &file_stage, &req.chunk_size,
                                            adb_sync_file_stage_flush, &file_stage_ctx);
            if (r != 0) {
                ADB_LOGE("lsfs write error: %d\n", r);
                goto failed;
            }
            need_discard = true;
            continue;
        }

        need_discard = false;
		while (req.chunk_size) {
			uint8_t *p = NULL;
			int n = 0;

			n = adb_sync_buffer_claim(ctx, &p, req.chunk_size);
			req.chunk_size -= n;
            r = adb_sync_file_write_exact(&file_stage_ctx, p, (uint32_t)n);
			adb_sync_buffer_commit(ctx, n);
			if (r != 0) {
				ADB_LOGE("lsfs write error: %d\n", r);
				goto failed;
			}
		}
        need_discard = true;
	}

#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
    if (file_async_ready) {
        r = adb_sync_async_writer_finish(&file_async_writer);
        if (r != 0) {
            ADB_LOGE("lsfs async finalize error: %d\n", r);
            goto failed;
        }
        adb_sync_async_writer_deinit(&file_async_writer);
        file_async_ready = false;
    }
#endif

    if (file_stage_ready) {
        r = adb_sync_stage_finish(&file_stage, adb_sync_file_stage_flush, &file_stage_ctx);
        if (r != 0) {
            ADB_LOGE("lsfs staging flush error: %d\n", r);
            goto failed;
        }
        adb_sync_stage_cleanup(&file_stage, &file_stage_ready);
    }

    r = adb_sync_file_finalize(&file_stage_ctx);
    if (r != 0) {
        ADB_LOGE("do_send, file finalize error: %d\n", r);
        goto failed;
    }
    adb_file_close(&fp);
	file_opened = false;
    need_discard = false;
    adb_sync_rsp_okay(s);
    goto done;

failed:
    if (need_discard) {
        r = adb_sync_discard_send_payload(ctx);
        if (r != 0) {
            ADB_LOGE("do_send, discard payload failed: %d\n", r);
        }
    }
	adb_sync_rsp_fail(s);

done:
	ADB_FREE(file_name);
	ADB_FREE(full_path);

	if (file_opened) {
#if defined(ADB_SYNC_ASYNC_WRITER_ENABLED)
        if (file_async_ready) {
            r = adb_sync_async_writer_abort(&file_async_writer);
            if (r != 0) {
                ADB_LOGW("do_send, async abort error: %d\n", r);
            }
            adb_sync_async_writer_deinit(&file_async_writer);
            file_async_ready = false;
        }
#endif
        r = adb_sync_file_finalize(&file_stage_ctx);
        if (r != 0) {
            ADB_LOGW("do_send, file finalize on failure error: %d\n", r);
        }
		adb_file_close(&fp);
	}
    adb_sync_stage_cleanup(&file_stage, &file_stage_ready);
    adb_sync_stage_cleanup(&ext_disk_stage, &ext_disk_stage_ready);
#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
    adb_sync_reserved_file_stage_put(ctx);
    adb_sync_reserved_raw_align_put(ctx);
#endif
}

static void do_recv(struct adb_service *s, struct adb_sync_req *req)
{
	uint8_t *full_path = NULL;
	uint8_t *path = NULL;
	uint8_t *read_buf = NULL;
	int r;
	const uint32_t chunk_size = adb_sync_send_chunk_limit(MAX_PAYLOAD);
	struct adb_sync_ctx *ctx = s->data;

	read_buf = ADB_MALLOC(chunk_size);
	if (read_buf == NULL) {
		ADB_LOGE("do recv, read buff malloc error\n");
		adb_sync_rsp_fail(s);
		return;
	}

	path = ADB_MALLOC(req->len + 1);
	if (path == NULL) {
		ADB_LOGE("do recv, path ADB_MALLOC error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}
	path[req->len] = '\0';
	adb_sync_buffer_read(ctx, path, req->len);

	full_path = adb_get_full_path(path);
	if (full_path == NULL) {
		ADB_LOGE("do recv, get full path error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}

	ADB_LOGI("sync recv, path: %s, full path: %s\n", path, full_path);
	struct adb_sync_send_data sd = {0};
	sd.id = ADB_SYNC_ID_DATA;

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint64_t start_addr = 0;
		uint64_t size = 0;

		r = adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		if (r != 0) {
			ADB_LOGE("do_recv, invalid raw path: %s\n", path);
			adb_sync_rsp_fail(s);
			goto failed;
		}
		ADB_LOGI("do_recv, ext disk access, disk_name:%s, start_addr:0x%llx, size:0x%llx\n", disk_name,
			 (unsigned long long)start_addr, (unsigned long long)size);
		struct adb_sync_ext_disk_ctx *disk_ctx = adb_sync_ext_disk_ctx_init(disk_name, start_addr, size, false);
		if (disk_ctx == NULL) {
			ADB_LOGE("do recv, ext disk init error\n");
			adb_sync_rsp_fail(s);
			goto failed;
		}
		uint32_t read_size = chunk_size / disk_ctx->sec_size * disk_ctx->sec_size;

		while (1) {
			r = adb_sync_ext_disk_read(disk_ctx, read_buf, size > read_size ? read_size : size);
			if (r < 0) {
				ADB_LOGE("do recv, ext disk read error: %d\n", r);
				adb_sync_rsp_fail(s);
				adb_sync_ext_disk_ctx_free(disk_ctx);
				goto failed;
			}

			sd.id = ADB_SYNC_ID_DATA;
			sd.chunk_size = r;
			adb_service_write_remote(s, (uint8_t *)&sd,
						sizeof(struct adb_sync_send_data));
			adb_service_write_remote(s, (uint8_t *)read_buf, r);
			size -= r;

			if (size == 0) {
				ADB_LOGI("do recv, ext disk read done\n");
				sd.id = ADB_SYNC_ID_DONE;
				sd.chunk_size = 0;

				adb_service_write_remote(s, (uint8_t *)&sd,
							sizeof(struct adb_sync_send_data));
				adb_sync_ext_disk_ctx_free(disk_ctx);
				goto done;
			}
		}
	}

	struct lsfs_file_t fp = {0};
	r = lsfs_open(&fp, full_path, LSFS_O_READ | LSFS_O_RDWR);
	if (r) {
		adb_sync_rsp_fail(s);
		ADB_LOGE("do recv, lsfs open error: %d\n", r);
		goto failed;
	}

	while (1) {
		r = lsfs_read(&fp, read_buf, chunk_size);
		if (r < 0) {
			ADB_LOGE("do recv, lsfs read error: %d\n", r);
			adb_sync_rsp_fail(s);
			lsfs_close(&fp);
			goto failed;
		} else if (r == 0) {
			ADB_LOGI("do recv, lsfs read done\n");
			lsfs_close(&fp);
			sd.id = ADB_SYNC_ID_DONE;
			sd.chunk_size = 0;

			adb_service_write_remote(s, (uint8_t *)&sd,
						 sizeof(struct adb_sync_send_data));
			break;
		} else {
			sd.id = ADB_SYNC_ID_DATA;
			sd.chunk_size = r;
			adb_service_write_remote(s, (uint8_t *)&sd,
						 sizeof(struct adb_sync_send_data));
			adb_service_write_remote(s, (uint8_t *)read_buf, r);
		}
	}

failed:
done:
	ADB_FREE(full_path);
	ADB_FREE(path);
	ADB_FREE(read_buf);
}

static void __attribute__((section(".psram.text"))) do_list(struct adb_service *s,
                                                            struct adb_sync_req *req)
{
	uint8_t *path = ADB_MALLOC(req->len + 1);
	uint8_t *full_path = NULL;
	struct lsfs_dir_t dir;
	struct lsfs_dirent entry;
	struct adb_sync_ctx *ctx = s->data;

	if (path == NULL) {
		ADB_LOGE("ADB_MALLOC error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}

	ADB_LOGI("sync list, req len: %d\n", req->len);
	adb_sync_buffer_read(ctx, path, req->len);
	path[req->len] = '\0';

	ADB_LOGI("sync list, path: %s\n", path);

	full_path = adb_get_full_path((const char *)path);
	if (full_path == NULL) {
		ADB_LOGE("sync list unsupported path: %s\n", path);
		adb_sync_rsp_fail(s);
		goto failed;
	}

	lsfs_dir_t_init(&dir);

	struct adb_sync_dent_data dent = {0};

	dent.id = ADB_SYNC_ID_DENT;

	int r = lsfs_opendir(&dir, (const char *)full_path);
	if (r == 0) {
		while (1) {
			r = lsfs_readdir(&dir, &entry);
			if (r != 0 || entry.name[0] == 0) {
				break;
			}

			dent.namelen = strlen(entry.name);
			dent.size = entry.size;
			dent.mode = entry.type == 0 ? 33188 : 16877;
			ADB_LOGI("list, name:%s, size:%d, type:%d\n", entry.name, entry.size,
				entry.type);
			adb_service_write_remote(s, (uint8_t *)&dent,
						 sizeof(struct adb_sync_dent_data));
			adb_service_write_remote(s, (uint8_t *)entry.name, dent.namelen);
		}
	} else {
		ADB_LOGE("lsfs opendir error: %d\n", r);
	}
	lsfs_closedir(&dir);
failed:
	memset(&dent, 0, sizeof(struct adb_sync_dent_data));
	dent.id = ADB_SYNC_ID_DONE;
	adb_service_write_remote(s, (uint8_t *)&dent, sizeof(struct adb_sync_dent_data));

	if (path) {
		ADB_FREE(path);
	}
	if (full_path) {
		ADB_FREE(full_path);
	}
}

static void adb_sync_task(void *arg)
{
	struct adb_sync_ctx *ctx = arg;
	struct adb_service *s = ctx->s;

	assert(s);
	assert(ctx);

	uint8_t quit = 0;
	while (!quit) {
		struct adb_sync_req req;
		adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_req));
		uint8_t *cmd = (uint8_t *)&req.id;
		ADB_LOGI("adb sync, cmd: %c%c%c%c\n", cmd[0], cmd[1], cmd[2], cmd[3]);
		switch (req.id) {
		case ADB_SYNC_ID_LIST:
			do_list(s, &req);
			break;
		case ADB_SYNC_ID_RECV:
			do_recv(s, &req);
			break;
		case ADB_SYNC_ID_SEND:
			do_send(s, &req);
			break;
		case ADB_SYNC_ID_STAT:
			do_stat(s, &req);
			break;
		case ADB_SYNC_ID_QUIT:
			quit = 1;
			break;
		default:
			ADB_LOGE("adb sync, unknown req id:%x\n", req.id);
			quit = 1;
			break;
		}

		if (ctx->closing) {
			quit = 1;
		}
	}
	ADB_LOGI("adb sync, quit\n");
	if (!ctx->closing) {
		adb_close(s->local_id, s->remote_id);
	}
	xSemaphoreGive(ctx->exit_sem);
	for (;;) {
		vTaskSuspend(NULL);
	}
}

static int adb_sync_open(struct adb_service *s, const uint8_t *args)
{
	struct adb_sync_ctx *ctx = NULL;

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    ctx = adb_boot_try_inram_malloc(ADB_SYNC_STAGE_ALIGNMENT, sizeof(*ctx));
#endif
    if (ctx == NULL) {
        ctx = ADB_MALLOC(sizeof(*ctx));
    }

	if (ctx == NULL) {
		return -1;
	}

	memset(ctx, 0, sizeof(struct adb_sync_ctx));

	ADB_LOGI("adb sync open, local_id:%d, remote_id:%d\n", s->local_id, s->remote_id);

	ctx->exit_sem = xSemaphoreCreateBinary();
	if (ctx->exit_sem == NULL) {
		ADB_FREE(ctx);
		ADB_LOGE("sync exit sem create failed\n");
		return -1;
	}

	ctx->rx_queue = xQueueCreate(ADB_SYNC_RX_QUEUE_DEPTH, sizeof(adb_packet_t *));

	if (ctx->rx_queue == NULL) {
		vSemaphoreDelete(ctx->exit_sem);
		ADB_FREE(ctx);
		ADB_LOGE("sync queue create failed\n");
		return -1;
	}

	ctx->s = s;
	s->data = ctx;

	BaseType_t xReturn;

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    ctx->task_stack = inram_calloc(32, ADB_SYNC_TASK_STACK_DEPTH, sizeof(StackType_t));
    ctx->task_tcb = inram_calloc(sizeof(void *), 1, sizeof(StaticTask_t));
    ctx->task = NULL;
    if (ctx->task_stack != NULL && ctx->task_tcb != NULL) {
        ctx->task = xTaskCreateStatic(adb_sync_task, "sync_task",
                                      ADB_SYNC_TASK_STACK_DEPTH, ctx,
                                      ADB_SYNC_TASK_PRIORITY,
                                      ctx->task_stack, ctx->task_tcb);
    }
    if (ctx->task != NULL) {
        xReturn = pdPASS;
    } else {
        if (ctx->task_tcb != NULL) {
            inram_free(ctx->task_tcb);
            ctx->task_tcb = NULL;
        }
        if (ctx->task_stack != NULL) {
            inram_free(ctx->task_stack);
            ctx->task_stack = NULL;
        }
        xReturn = xTaskCreate(adb_sync_task, "sync_task",
                              ADB_SYNC_TASK_STACK_DEPTH, ctx,
                              ADB_SYNC_TASK_PRIORITY, &ctx->task);
    }
#else
	xReturn = xTaskCreate(adb_sync_task, "sync_task",
                              ADB_SYNC_TASK_STACK_DEPTH, ctx,
                              ADB_SYNC_TASK_PRIORITY, &ctx->task);
#endif
	if (xReturn != pdPASS) {
		vQueueDelete(ctx->rx_queue);
		vSemaphoreDelete(ctx->exit_sem);
		ADB_FREE(ctx);
		ADB_LOGE("sync task create failed\n");
		return -1;
	}

	return 0;
}

int adb_write_file(const char *path, uint8_t *data, uint32_t len)
{
	return 0;
}

static int adb_sync_write(struct adb_service *s, adb_packet_t *p)
{
	struct adb_sync_ctx *ctx = s->data;

	if (ctx && ctx->rx_queue && !ctx->closing) {
		xQueueSend(ctx->rx_queue, &p, portMAX_DELAY);
	} else {
		adb_packet_free(p);
		if (ctx != NULL && ctx->closing) {
			ADB_LOGW("sync write ignored while closing\n");
		} else {
			ADB_LOGE("sync write failed\n");
		}
	}

	return 0;
}

static int adb_sync_close(struct adb_service *s)
{
	if (s == NULL || s->data == NULL) {
		return -1;
	}

	struct adb_sync_ctx *ctx = s->data;
	if (ctx->task != NULL && ctx->task != xTaskGetCurrentTaskHandle()) {
		ctx->closing = true;
		adb_sync_queue_drain(ctx);
		adb_sync_queue_request_exit(ctx);

		if (ctx->exit_sem != NULL &&
		    xSemaphoreTake(ctx->exit_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
			ADB_LOGE("sync task exit timeout, force delete\n");
		}
		vTaskDelete(ctx->task);
		ctx->task = NULL;
		vTaskDelay(pdMS_TO_TICKS(5));
	}

	if (ctx->curr_pkt) {
		adb_packet_free(ctx->curr_pkt);
		ctx->curr_pkt = NULL;
	}

	if (ctx->rx_queue) {
		adb_sync_queue_drain(ctx);
		vQueueDelete(ctx->rx_queue);
		ctx->rx_queue = NULL;
	}

	if (ctx->exit_sem != NULL) {
		vSemaphoreDelete(ctx->exit_sem);
		ctx->exit_sem = NULL;
	}

#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
    adb_sync_reserved_file_stage_put(ctx);
    adb_sync_reserved_raw_align_put(ctx);
#endif

#if defined(CONFIG_BOOT_ADB) && defined(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    if (ctx->task_tcb != NULL) {
        inram_free(ctx->task_tcb);
        ctx->task_tcb = NULL;
    }
    if (ctx->task_stack != NULL) {
        inram_free(ctx->task_stack);
        ctx->task_stack = NULL;
    }
#endif

	ADB_FREE(ctx);
	s->data = NULL;

	return 0;
}

static const struct adb_service_handle adb_sync_handle = {
	.name = "sync",
	.open = adb_sync_open,
	.close = adb_sync_close,
	.write = adb_sync_write,
};

int adb_sync_init(void)
{
	return adb_service_hd_register(&adb_sync_handle);
}

int adb_sync_prepare(void)
{
#if defined(ADB_SYNC_BOOT_CHERRYUSB_PREALLOC_STAGE)
    return adb_sync_reserved_raw_align_prepare_internal();
#else
    return 0;
#endif
}
