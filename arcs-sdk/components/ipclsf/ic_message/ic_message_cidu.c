#include "ic_message.h"

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "cache.h"
#include "venusa_ap.h"

#ifndef NULL
#define NULL 0
#endif

#define IC_MESSAGE_CIDU_MAGIC        (0x49434455U) /* "ICDU" */
#define IC_MESSAGE_CIDU_VERSION      (1U)
#define IC_MESSAGE_CIDU_CORE_COUNT   (2U)
#define IC_MESSAGE_CIDU_PAYLOAD_SIZE (10U)
#define IC_MESSAGE_CIDU_CACHE_LINE   (32U)

#define IC_CIDU_ALIGN_UP(value, align)   (((value) + ((align) - 1U)) & ~((align) - 1U))
#define IC_CIDU_ALIGN_DOWN(value, align) ((value) & ~((align) - 1U))

#if !defined(CONFIG_IPC_CIDU_RING_ENTRY_COUNT) || (CONFIG_IPC_CIDU_RING_ENTRY_COUNT < 2)
#error "CONFIG_IPC_CIDU_RING_ENTRY_COUNT must be at least 2"
#endif

#if !defined(CONFIG_IPC_CIDU_SHMEM_BASE) || !defined(CONFIG_IPC_CIDU_SHMEM_SIZE)
#error "CONFIG_IPC_CIDU_SHMEM_BASE and CONFIG_IPC_CIDU_SHMEM_SIZE must be configured"
#endif

#if ((CONFIG_IPC_CIDU_SHMEM_BASE % IC_MESSAGE_CIDU_CACHE_LINE) != 0)
#error "CONFIG_IPC_CIDU_SHMEM_BASE must be cache-line aligned"
#endif

#if ((CONFIG_IPC_CIDU_SHMEM_SIZE % IC_MESSAGE_CIDU_CACHE_LINE) != 0)
#error "CONFIG_IPC_CIDU_SHMEM_SIZE must be cache-line aligned"
#endif

typedef struct {
    uint8_t id;
    uint8_t type;
    uint8_t len;
    uint8_t reserved;
    uint8_t payload[IC_MESSAGE_CIDU_PAYLOAD_SIZE];
    uint8_t padding[IC_MESSAGE_CIDU_CACHE_LINE - 4U - IC_MESSAGE_CIDU_PAYLOAD_SIZE];
} __attribute__((aligned(IC_MESSAGE_CIDU_CACHE_LINE))) ic_message_cidu_entry_t;

typedef struct {
    volatile uint32_t value;
    uint8_t padding[IC_MESSAGE_CIDU_CACHE_LINE - 4U];
} __attribute__((aligned(IC_MESSAGE_CIDU_CACHE_LINE))) ic_message_cidu_index_t;

typedef struct {
    ic_message_cidu_index_t write_idx;
    ic_message_cidu_index_t read_idx;
    ic_message_cidu_entry_t entries[CONFIG_IPC_CIDU_RING_ENTRY_COUNT];
} __attribute__((aligned(IC_MESSAGE_CIDU_CACHE_LINE))) ic_message_cidu_ring_t;

typedef struct {
    volatile uint32_t magic;
    volatile uint32_t version;
    volatile uint32_t entry_count;
    volatile uint32_t init_mask;
    uint8_t padding[IC_MESSAGE_CIDU_CACHE_LINE - 16U];
    ic_message_cidu_ring_t rings[IC_MESSAGE_CIDU_CORE_COUNT];
} __attribute__((aligned(IC_MESSAGE_CIDU_CACHE_LINE))) ic_message_cidu_shared_t;

typedef char ic_message_cidu_shmem_size_check[
    (sizeof(ic_message_cidu_shared_t) <= CONFIG_IPC_CIDU_SHMEM_SIZE) ? 1 : -1];
typedef char ic_message_cidu_entry_size_check[
    (sizeof(ic_message_cidu_entry_t) == IC_MESSAGE_CIDU_CACHE_LINE) ? 1 : -1];
typedef char ic_message_cidu_index_size_check[
    (sizeof(ic_message_cidu_index_t) == IC_MESSAGE_CIDU_CACHE_LINE) ? 1 : -1];

static ic_message_handle_info_t handle_infos[IC_MESSAGE_ID_MAX] = {0};
static uint8_t ic_message_inited = 0;
static uint32_t local_core_id = 0;
static uint32_t peer_core_id = 1;
static TaskHandle_t worker_task = NULL;
static SemaphoreHandle_t send_mutex = NULL;

static inline ic_message_cidu_shared_t *ic_message_cidu_shmem(void)
{
    return (ic_message_cidu_shared_t *)(uintptr_t)CONFIG_IPC_CIDU_SHMEM_BASE;
}

static void ic_message_cidu_cache_flush(const void *addr, uint32_t len)
{
    uintptr_t start = IC_CIDU_ALIGN_DOWN((uintptr_t)addr, IC_MESSAGE_CIDU_CACHE_LINE);
    uintptr_t end = IC_CIDU_ALIGN_UP((uintptr_t)addr + len, IC_MESSAGE_CIDU_CACHE_LINE);

    HAL_FlushDCache_by_Addr((void *)start, (uint32_t)(end - start));
}

static void ic_message_cidu_cache_invalidate(const void *addr, uint32_t len)
{
    uintptr_t start = IC_CIDU_ALIGN_DOWN((uintptr_t)addr, IC_MESSAGE_CIDU_CACHE_LINE);
    uintptr_t end = IC_CIDU_ALIGN_UP((uintptr_t)addr + len, IC_MESSAGE_CIDU_CACHE_LINE);

    HAL_InvalidateDCache_by_Addr((void *)start, (uint32_t)(end - start));
}

static uint32_t ic_message_cidu_next_idx(uint32_t idx)
{
    idx++;
    if (idx >= CONFIG_IPC_CIDU_RING_ENTRY_COUNT) {
        idx = 0;
    }
    return idx;
}

static uint32_t ic_message_cidu_load_index(const ic_message_cidu_index_t *idx)
{
    ic_message_cidu_cache_invalidate(idx, sizeof(*idx));
    return idx->value;
}

static void ic_message_cidu_store_index(ic_message_cidu_index_t *idx, uint32_t value)
{
    idx->value = value;
    ic_message_cidu_cache_flush(idx, sizeof(*idx));
    __sync_synchronize();
}

static int32_t ic_message_cidu_push(uint8_t id, uint8_t type, const void *msg, uint32_t len)
{
    ic_message_cidu_shared_t *shared = ic_message_cidu_shmem();
    ic_message_cidu_ring_t *ring = &shared->rings[local_core_id];
    uint32_t write_idx;
    uint32_t read_idx;
    uint32_t next_idx;
    ic_message_cidu_entry_t *entry;

    write_idx = ic_message_cidu_load_index(&ring->write_idx);
    read_idx = ic_message_cidu_load_index(&ring->read_idx);
    next_idx = ic_message_cidu_next_idx(write_idx);

    if (next_idx == read_idx) {
        return IC_MESSAGE_ERR_URPC;
    }

    entry = &ring->entries[write_idx];
    memset(entry, 0, sizeof(*entry));
    entry->id = id;
    entry->type = type;
    entry->len = (uint8_t)len;
    memcpy(entry->payload, msg, len);

    ic_message_cidu_cache_flush(entry, sizeof(*entry));
    __sync_synchronize();

    ic_message_cidu_store_index(&ring->write_idx, next_idx);
    CIDU_TriggerInterCoreInt(local_core_id, peer_core_id);
    return IC_MESSAGE_ERR_NONE;
}

static void ic_message_cidu_dispatch(const ic_message_cidu_entry_t *entry)
{
    uint8_t id = entry->id;

    if (id >= IC_MESSAGE_ID_MAX) {
        return;
    }

    if (handle_infos[id].cb != NULL) {
        ic_message_msg_info_t msg;

        msg.len = entry->len;
        msg.msg_type = entry->type;
        msg.msg = (void *)entry->payload;
        (void)((ic_message_callback_t)handle_infos[id].cb)(&handle_infos[id], &msg);
    }
}

static void ic_message_cidu_drain(void)
{
    ic_message_cidu_shared_t *shared = ic_message_cidu_shmem();
    ic_message_cidu_ring_t *ring = &shared->rings[peer_core_id];

    while (1) {
        uint32_t write_idx;
        uint32_t read_idx;
        uint32_t next_idx;
        ic_message_cidu_entry_t local_entry;
        ic_message_cidu_entry_t *entry;

        write_idx = ic_message_cidu_load_index(&ring->write_idx);
        read_idx = ic_message_cidu_load_index(&ring->read_idx);

        if (read_idx == write_idx) {
            break;
        }

        entry = &ring->entries[read_idx];
        ic_message_cidu_cache_invalidate(entry, sizeof(*entry));
        memcpy(&local_entry, entry, sizeof(local_entry));

        next_idx = ic_message_cidu_next_idx(read_idx);
        ic_message_cidu_store_index(&ring->read_idx, next_idx);

        if (local_entry.len <= IC_MESSAGE_CIDU_PAYLOAD_SIZE) {
            ic_message_cidu_dispatch(&local_entry);
        }
    }
}

static void ic_message_cidu_worker(void *arg)
{
    (void)arg;

    while (1) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ic_message_cidu_drain();
    }
}

static void ic_message_cidu_clear_pending(void)
{
    uint32_t sender_mask = CIDU_QueryCoreIntSenderMask(local_core_id);

    for (uint32_t sender = 0; sender < IC_MESSAGE_CIDU_CORE_COUNT; sender++) {
        if ((sender_mask & (1U << sender)) != 0U) {
            CIDU_ClearInterCoreIntReq(sender, local_core_id);
        }
    }

    clear_IRQ(CONFIG_IPC_CIDU_IRQ_VECTOR);
}

static void ic_message_cidu_isr(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    ic_message_cidu_clear_pending();

    if (worker_task != NULL) {
        vTaskNotifyGiveFromISR(worker_task, &higher_priority_task_woken);
#if defined(portYIELD_FROM_ISR)
        portYIELD_FROM_ISR(higher_priority_task_woken);
#elif defined(portEND_SWITCHING_ISR)
        portEND_SWITCHING_ISR(higher_priority_task_woken);
#else
        (void)higher_priority_task_woken;
#endif
    }
}

static int32_t ic_message_cidu_init_shared(void)
{
    ic_message_cidu_shared_t *shared = ic_message_cidu_shmem();

    CIDU_AcquireSemaphore_Block(CONFIG_IPC_CIDU_SEMAPHORE_ID, local_core_id);

    ic_message_cidu_cache_invalidate(shared, sizeof(*shared));
    if ((shared->magic != IC_MESSAGE_CIDU_MAGIC) ||
        (shared->version != IC_MESSAGE_CIDU_VERSION) ||
        (shared->entry_count != CONFIG_IPC_CIDU_RING_ENTRY_COUNT)) {
        memset(shared, 0, sizeof(*shared));
        shared->magic = IC_MESSAGE_CIDU_MAGIC;
        shared->version = IC_MESSAGE_CIDU_VERSION;
        shared->entry_count = CONFIG_IPC_CIDU_RING_ENTRY_COUNT;
        ic_message_cidu_cache_flush(shared, sizeof(*shared));
    }

    shared->init_mask |= (1U << local_core_id);
    ic_message_cidu_cache_flush((const void *)&shared->init_mask, sizeof(shared->init_mask));
    __sync_synchronize();

    CIDU_ReleaseSemaphore(CONFIG_IPC_CIDU_SEMAPHORE_ID);
    return IC_MESSAGE_ERR_NONE;
}

static int32_t ic_message_inner_conn_msg_send(uint8_t id)
{
    uint8_t data = id;

    return ic_message_msg_send_by_id(IC_MESSAGE_ID_INNER_CONN, IC_MESSAGE_MSG_TYPE_CMD, &data, sizeof(data));
}

static int32_t ic_message_inner_conn_cb(ic_message_handle_info_t *handle_info, ic_message_msg_info_t *msg_info)
{
    uint8_t id;

    (void)handle_info;

    if ((msg_info == NULL) || (msg_info->msg == NULL) || (msg_info->len < sizeof(uint8_t))) {
        return IC_MESSAGE_ERR_PARAM_INVALID;
    }

    id = *((uint8_t *)msg_info->msg);
    if (id >= IC_MESSAGE_ID_MAX) {
        return IC_MESSAGE_ERR_ID_INVALID;
    }

    handle_infos[id].remote_conn = 1;
    return IC_MESSAGE_ERR_NONE;
}

int ic_message_init(void)
{
    BaseType_t task_ret;
    int32_t ret;

    if (ic_message_inited) {
        return IC_MESSAGE_ERR_NONE;
    }

    local_core_id = (uint32_t)(__get_hart_id() & 0xffU);
#if CONFIG_IPC_CIDU_PEER_CORE_AUTO
    peer_core_id = local_core_id ^ 1U;
#else
    peer_core_id = CONFIG_IPC_CIDU_PEER_CORE_ID;
#endif

    if ((local_core_id >= IC_MESSAGE_CIDU_CORE_COUNT) ||
        (peer_core_id >= IC_MESSAGE_CIDU_CORE_COUNT) ||
        (local_core_id == peer_core_id)) {
        return IC_MESSAGE_ERR_PARAM_INVALID;
    }

    ret = ic_message_cidu_init_shared();
    if (ret != IC_MESSAGE_ERR_NONE) {
        return ret;
    }

    handle_infos[IC_MESSAGE_ID_INNER_CONN].cb = ic_message_inner_conn_cb;
    handle_infos[IC_MESSAGE_ID_INNER_CONN].id = IC_MESSAGE_ID_INNER_CONN;
    handle_infos[IC_MESSAGE_ID_INNER_CONN].user_datas = NULL;
    handle_infos[IC_MESSAGE_ID_INNER_CONN].remote_conn = 1;

    send_mutex = xSemaphoreCreateMutex();
    if (send_mutex == NULL) {
        return IC_MESSAGE_ERR_URPC;
    }

    task_ret = xTaskCreate(ic_message_cidu_worker,
                           "ic_msg_cidu",
                           CONFIG_IPC_CIDU_WORKER_STACK_SIZE,
                           NULL,
                           CONFIG_IPC_CIDU_WORKER_PRIORITY,
                           &worker_task);
    if (task_ret != pdPASS) {
        vSemaphoreDelete(send_mutex);
        send_mutex = NULL;
        return IC_MESSAGE_ERR_URPC;
    }

    ic_message_cidu_clear_pending();
    register_ISR(CONFIG_IPC_CIDU_IRQ_VECTOR, ic_message_cidu_isr, NULL);
    enable_IRQ(CONFIG_IPC_CIDU_IRQ_VECTOR);

    ic_message_inited = 1;
    xTaskNotifyGive(worker_task);

    return IC_MESSAGE_ERR_NONE;
}

int ic_message_register_by_id(uint8_t id, ic_message_callback_t cb, void *user_datas)
{
    if (id >= IC_MESSAGE_ID_MAX) {
        return IC_MESSAGE_ERR_ID_INVALID;
    }

    if (handle_infos[id].cb != NULL) {
        return IC_MESSAGE_ERR_NOT_NULL;
    }

    if (!ic_message_inited) {
        return IC_MESSAGE_ERR_NOT_INITED;
    }

    handle_infos[id].cb = cb;
    handle_infos[id].id = id;
    handle_infos[id].user_datas = user_datas;

    return ic_message_inner_conn_msg_send(id);
}

int ic_message_msg_send_by_id(uint8_t id, uint8_t type, void *msg, uint32_t len)
{
    int32_t ret;

    if (!ic_message_inited) {
        return IC_MESSAGE_ERR_NOT_INITED;
    }

    if (id >= IC_MESSAGE_ID_MAX) {
        return IC_MESSAGE_ERR_ID_INVALID;
    }

    if ((len > IC_MESSAGE_CIDU_PAYLOAD_SIZE) || (msg == NULL)) {
        return IC_MESSAGE_ERR_PARAM_INVALID;
    }

    if ((type != IC_MESSAGE_MSG_TYPE_CMD) && (type != IC_MESSAGE_MSG_TYPE_EVT)) {
        return IC_MESSAGE_ERR_PARAM_INVALID;
    }

    if ((id != IC_MESSAGE_ID_INNER_CONN) && !handle_infos[id].remote_conn) {
        return IC_MESSAGE_ERR_REMOTE_NOT_CONN;
    }

    if (xSemaphoreTake(send_mutex, portMAX_DELAY) != pdTRUE) {
        return IC_MESSAGE_ERR_URPC;
    }

    ret = ic_message_cidu_push(id, type, msg, len);
    xSemaphoreGive(send_mutex);

    return ret;
}

uint8_t ic_message_remote_is_connected(uint8_t id)
{
    return (id < IC_MESSAGE_ID_MAX) && handle_infos[id].remote_conn;
}
