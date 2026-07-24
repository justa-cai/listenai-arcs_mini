/**
 * @file lisa_modem_module.c
 * @brief Lisa Modem Module Network Interface implementation
 */

#include "lisa_modem_module.h"
#include "at_client.h"
#include "at_mem.h"
#include "transport/at_transport.h"
#include "transport/at_transport_uart.h"
#include "core/modem_driver_ops.h"
#include "drivers/common/modem_dispatcher.h"
#if CONFIG_LISA_MODEM_DRIVER_ML307
#include "drivers/ml307/ml307_endpoint.h"
#endif
#if CONFIG_LISA_MODEM_DRIVER_EC801E
#include "drivers/ec801e/ec801e_endpoint.h"
#endif
#if defined(CONFIG_LISA_NET) && CONFIG_LISA_NET
#include "netdev.h"
#endif
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "semphr.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/time.h>

#define TAG "lisa_modem"
#include "lisa_log.h"
#include "lisa_modem_perf_log.h"

#define MODEM_PROBE_PRIMARY_BAUD        921600U
#define MODEM_PROBE_SECONDARY_BAUD      460800U
#define MODEM_PROBE_FALLBACK_BAUD       115200U
#define MODEM_COMPAT_DEFAULT_TIMEOUT_MS 5000U
#define MODEM_MAX_SOCKETS               8
#define MODEM_TIMEOUT_UNSET             UINT32_MAX
#define MODEM_TCP_TX_BUFFER_SIZE        8192U
#define MODEM_TCP_TX_AGG_THRESHOLD      512U
#define MODEM_TCP_TX_AGG_TIMEOUT_MS     4U

static bool modem_probe_sync_uart_baud(at_client_t *client)
{
    static const uint32_t probe_bauds[] = {
        MODEM_PROBE_PRIMARY_BAUD,
        MODEM_PROBE_SECONDARY_BAUD,
        MODEM_PROBE_FALLBACK_BAUD,
    };
    at_transport_t *transport;
    const int max_passes = 5;
    const int max_retries_per_baudrate = 3;
    const size_t probe_baud_count = sizeof(probe_bauds) / sizeof(probe_bauds[0]);

    if (!client) {
        return false;
    }

    transport = at_client_get_transport(client);
    if (!transport) {
        return false;
    }

    for (int pass = 0; pass < max_passes; ++pass) {
        for (size_t i = 0; i < probe_baud_count; ++i) {
            uint32_t baud = probe_bauds[i];

            if (at_transport_ioctl(transport, AT_TRANSPORT_IOCTL_SET_BAUDRATE, &baud) != 0) {
                LISA_LOGE(TAG, "Failed to set baudrate");
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));

            for (int retry = 0; retry < max_retries_per_baudrate; ++retry) {
                if (at_client_send_cmd(client, "AT", 1000, true)) {
                    LISA_LOGI(TAG, "AT ready at baudrate=%u", baud);
                    return true;
                }

                LISA_LOGI(TAG, "AT retry %d/%d at baudrate %u",
                          retry + 1, max_retries_per_baudrate, baud);
                vTaskDelay(pdMS_TO_TICKS(200));
            }

            if (!(pass == max_passes - 1 &&
                  i == (probe_baud_count - 1U))) {
                uint32_t next_baud = probe_bauds[(i + 1U) % probe_baud_count];
                LISA_LOGI(TAG, "Switching baudrate to %u", next_baud);
            }
        }
    }

    LISA_LOGE(TAG, "AT not ready after %d baudrate switches",
              max_passes * (int)probe_baud_count - 1);
    return false;
}

typedef struct {
    int sockfd;
    uint32_t generation;
    int driver_id;
    bool in_use;
    int domain;
    int protocol;
    bool nonblock;
    bool tls_enabled;
    uint32_t send_timeout_ms;
    uint32_t recv_timeout_ms;
    SemaphoreHandle_t tx_done_sem;
    const uint8_t *tx_data;
    size_t tx_length;
    size_t tx_offset;
    int tx_result;
    uint32_t tx_timeout_ms;
    bool tx_active;
    bool tx_has_destination;
    modem_addr_t tx_destination;
    uint8_t *tx_buffer;
    size_t tx_buffer_head;
    size_t tx_buffer_tail;
    size_t tx_buffer_used;
    uint32_t tx_flush_threshold;
    uint32_t tx_flush_timeout_ms;
    TickType_t tx_flush_deadline_tick;
    bool tx_flush_deadline_active;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
} lisa_modem_socket_record_t;

typedef struct {
    SemaphoreHandle_t done_sem;
    int result;
} lisa_modem_control_sync_t;

typedef struct {
    SemaphoreHandle_t done_sem;
    int result;
    const char *domain;
    char *ip_addr;
    size_t size;
} lisa_modem_dns_sync_t;

typedef struct {
    SemaphoreHandle_t done_sem;
    int result;
    char *value;
    size_t size;
} lisa_modem_identity_sync_t;

typedef struct {
    SemaphoreHandle_t done_sem;
    int result;
    int *rssi;
    int *ber;
} lisa_modem_signal_sync_t;

struct lisa_modem {
    void *driver_ctx;
    const modem_driver_ops_t *ops;
    at_client_t *client;
    at_transport_t *transport;
    bool owns_transport;
    bool has_probe_result;
    bool initialized;
    modem_probe_result_t probe_result;
    lisa_modem_status_t status;
    char name[16];
    SemaphoreHandle_t socket_table_mutex;
    modem_dispatcher_t *dispatcher;
    lisa_modem_socket_record_t sockets[MODEM_MAX_SOCKETS];
};

static lisa_modem_t *s_default_modem = NULL;
static lisa_modem_status_t s_last_status = {
    .last_error = LISA_MODEM_ERR_NOT_INITIALIZED,
};
static bool s_modem_present = false;

#ifdef LISA_MODEM_TEST
static bool s_lisa_modem_test_in_dispatcher_control = false;
#endif

static void lisa_modem_publish_status(lisa_modem_t *modem, const lisa_modem_status_t *status)
{
    if (!status) {
        return;
    }

    if (modem) {
        modem->status = *status;
    }
    s_last_status = *status;
}

static void lisa_modem_status_fail(lisa_modem_t *modem, lisa_modem_status_t *status,
                                   lisa_modem_error_t error)
{
    if (!status) {
        return;
    }

    status->last_error = error;
    lisa_modem_publish_status(modem, status);
}

static void lisa_modem_refresh_driver_status(lisa_modem_t *modem)
{
    lisa_modem_status_t status;

    if (!modem) {
        return;
    }

    status = modem->status;
    if (modem->ops && modem->ops->get_status && modem->driver_ctx) {
        modem->ops->get_status(modem->driver_ctx, &status);
    }
    modem->status = status;
    s_last_status = status;
}

static int lisa_modem_register_default_netdev(const char *name)
{
#if defined(CONFIG_LISA_NET) && CONFIG_LISA_NET
    return app_netdev_register(name, 500);
#else
    (void)name;
    return 0;
#endif
}

static void lisa_modem_unregister_default_netdev(const char *name)
{
#if defined(CONFIG_LISA_NET) && CONFIG_LISA_NET
    netdev_unregister_by_name(name);
#else
    (void)name;
#endif
}

static bool lisa_modem_validate_sockaddr_input(const struct sockaddr *addr, int addrlen)
{
    return addr && addrlen > 0;
}

static bool lisa_modem_convert_addrlen_in(const int *addrlen, socklen_t *socklen)
{
    if (!addrlen || !socklen || *addrlen < 0) {
        return false;
    }

    *socklen = (socklen_t)(*addrlen);
    return true;
}

static void lisa_modem_socket_table_lock(lisa_modem_t *modem)
{
    if (modem && modem->socket_table_mutex) {
        (void)xSemaphoreTake(modem->socket_table_mutex, portMAX_DELAY);
    }
}

static void lisa_modem_socket_table_unlock(lisa_modem_t *modem)
{
    if (modem && modem->socket_table_mutex) {
        (void)xSemaphoreGive(modem->socket_table_mutex);
    }
}

static void lisa_modem_socket_record_reset(lisa_modem_socket_record_t *record, int sockfd,
                                           uint32_t generation)
{
    SemaphoreHandle_t tx_done_sem;
    uint8_t *tx_buffer;

    if (!record) {
        return;
    }

    tx_done_sem = record->tx_done_sem;
    tx_buffer = record->tx_buffer;
    memset(record, 0, sizeof(*record));
    record->sockfd = sockfd;
    record->generation = generation;
    record->driver_id = -1;
    record->send_timeout_ms = MODEM_TIMEOUT_UNSET;
    record->recv_timeout_ms = MODEM_TIMEOUT_UNSET;
    record->tx_result = -1;
    record->tx_done_sem = tx_done_sem;
    record->tx_buffer = tx_buffer;
    record->tx_flush_threshold = MODEM_TCP_TX_AGG_THRESHOLD;
    record->tx_flush_timeout_ms = MODEM_TCP_TX_AGG_TIMEOUT_MS;
}

static void lisa_modem_socket_table_init(lisa_modem_t *modem)
{
    int i;

    if (!modem) {
        return;
    }

    for (i = 0; i < MODEM_MAX_SOCKETS; ++i) {
        lisa_modem_socket_record_reset(&modem->sockets[i], i, 1U);
    }
}

static int lisa_modem_normalize_protocol(int type, int protocol)
{
    if (protocol != 0) {
        return protocol;
    }

    switch (type) {
        case SOCK_STREAM:
            return IPPROTO_TCP;
        case SOCK_DGRAM:
            return IPPROTO_UDP;
        default:
            return -1;
    }
}

static lisa_modem_socket_record_t *lisa_modem_get_socket_record_locked(lisa_modem_t *modem, int sockfd)
{
    lisa_modem_socket_record_t *record;

    if (!modem || sockfd < 0 || sockfd >= MODEM_MAX_SOCKETS) {
        return NULL;
    }

    record = &modem->sockets[sockfd];
    return record->in_use ? record : NULL;
}

static lisa_modem_socket_record_t *lisa_modem_alloc_socket_record_locked(lisa_modem_t *modem,
                                                                         int domain, int protocol)
{
    int i;

    if (!modem) {
        return NULL;
    }

    for (i = 0; i < MODEM_MAX_SOCKETS; ++i) {
        lisa_modem_socket_record_t *record = &modem->sockets[i];

        if (record->in_use) {
            continue;
        }

        lisa_modem_socket_record_reset(record, i, record->generation);
        record->in_use = true;
        record->domain = domain;
        record->protocol = protocol;
        if (protocol != IPPROTO_TCP) {
            record->tx_flush_threshold = 0U;
            record->tx_flush_timeout_ms = 0U;
        }
        return record;
    }

    return NULL;
}

static void lisa_modem_release_socket_record_locked(lisa_modem_t *modem, int sockfd)
{
    uint32_t next_generation;

    if (!modem || sockfd < 0 || sockfd >= MODEM_MAX_SOCKETS) {
        return;
    }

    next_generation = modem->sockets[sockfd].generation + 1U;
    lisa_modem_socket_record_reset(&modem->sockets[sockfd], sockfd, next_generation);
}

static bool lisa_modem_socket_generation_matches_locked(lisa_modem_t *modem, int sockfd,
                                                        uint32_t generation)
{
    if (!modem || sockfd < 0 || sockfd >= MODEM_MAX_SOCKETS) {
        return false;
    }

    return modem->sockets[sockfd].in_use && modem->sockets[sockfd].generation == generation;
}

static bool lisa_modem_snapshot_socket_record(lisa_modem_t *modem, int sockfd,
                                              lisa_modem_socket_record_t *snapshot)
{
    lisa_modem_socket_record_t *record;

    if (!modem || !snapshot) {
        return false;
    }

    lisa_modem_socket_table_lock(modem);
    record = lisa_modem_get_socket_record_locked(modem, sockfd);
    if (!record) {
        lisa_modem_socket_table_unlock(modem);
        return false;
    }

    *snapshot = *record;
    lisa_modem_socket_table_unlock(modem);
    return true;
}

static bool lisa_modem_socket_generation_matches(lisa_modem_t *modem, int sockfd, uint32_t generation)
{
    bool matches = false;

    if (!modem || sockfd < 0 || sockfd >= MODEM_MAX_SOCKETS) {
        return false;
    }

    lisa_modem_socket_table_lock(modem);
    matches = lisa_modem_socket_generation_matches_locked(modem, sockfd, generation);
    lisa_modem_socket_table_unlock(modem);
    return matches;
}

static bool lisa_modem_sockaddr_to_addr(const struct sockaddr *addr, socklen_t addrlen, modem_addr_t *out)
{
    const struct sockaddr_in *addr_in;

    (void)addrlen;

    if (!addr || !out || addr->sa_family != AF_INET) {
        return false;
    }

    addr_in = (const struct sockaddr_in *)addr;
    memset(out, 0, sizeof(*out));
    out->family = addr->sa_family;
    out->port = ntohs(addr_in->sin_port);
    return inet_ntop(AF_INET, &addr_in->sin_addr, out->host, sizeof(out->host)) != NULL;
}

static bool lisa_modem_addr_to_sockaddr(const modem_addr_t *addr, struct sockaddr *out, socklen_t *outlen)
{
    struct sockaddr_in addr_in;

    if (!addr || !out || !outlen || addr->family != AF_INET ||
        *outlen < (socklen_t)sizeof(addr_in)) {
        return false;
    }

    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(addr->port);
    if (inet_pton(AF_INET, addr->host, &addr_in.sin_addr) != 1) {
        return false;
    }

    memcpy(out, &addr_in, sizeof(addr_in));
    *outlen = sizeof(addr_in);
    return true;
}

static void lisa_modem_mark_peer(lisa_modem_socket_record_t *record,
                                 const struct sockaddr *addr, socklen_t addrlen)
{
    if (!record || !addr || addrlen <= 0) {
        return;
    }

    if ((size_t)addrlen > sizeof(record->peer_addr)) {
        addrlen = sizeof(record->peer_addr);
    }

    memcpy(&record->peer_addr, addr, (size_t)addrlen);
    record->peer_addr_len = addrlen;
}

static int lisa_modem_copy_peer(const lisa_modem_socket_record_t *record,
                                struct sockaddr *addr, socklen_t *addrlen)
{
    socklen_t copy_len;

    if (!record || !addrlen || record->peer_addr_len <= 0) {
        return -1;
    }

    if (addr) {
        copy_len = record->peer_addr_len;
        if (copy_len > *addrlen) {
            copy_len = *addrlen;
        }
        memcpy(addr, &record->peer_addr, (size_t)copy_len);
    }

    *addrlen = record->peer_addr_len;
    return 0;
}

static uint32_t lisa_modem_timeval_to_ms(const struct timeval *tv)
{
    if (!tv) {
        return 0;
    }

    return (uint32_t)(tv->tv_sec * 1000 + tv->tv_usec / 1000);
}

static int lisa_modem_update_timeout(lisa_modem_t *modem, int sockfd, uint32_t generation,
                                     int driver_id, bool is_send, uint32_t timeout_ms)
{
    uint32_t *stored_timeout;
    lisa_modem_socket_record_t *record;

    if (!modem) {
        return -1;
    }

    lisa_modem_socket_table_lock(modem);
    record = lisa_modem_get_socket_record_locked(modem, sockfd);
    if (!record || record->generation != generation) {
        lisa_modem_socket_table_unlock(modem);
        return -1;
    }

    stored_timeout = is_send ? &record->send_timeout_ms : &record->recv_timeout_ms;
    if (*stored_timeout == timeout_ms) {
        lisa_modem_socket_table_unlock(modem);
        return 0;
    }

    *stored_timeout = timeout_ms;
    lisa_modem_socket_table_unlock(modem);
    if (modem->ops && modem->ops->set_timeout) {
        return modem->ops->set_timeout(modem->driver_ctx, driver_id, is_send, timeout_ms);
    }

    return 0;
}

static uint32_t lisa_modem_effective_recv_timeout_ms(const lisa_modem_socket_record_t *record,
                                                     uint32_t timeout_ms)
{
    if (!record) {
        return timeout_ms;
    }

    if (record->nonblock) {
        return 0U;
    }

    if (record->recv_timeout_ms != MODEM_TIMEOUT_UNSET) {
        return record->recv_timeout_ms;
    }

    return timeout_ms > 0U ? timeout_ms : MODEM_COMPAT_DEFAULT_TIMEOUT_MS;
}

static uint32_t lisa_modem_effective_send_timeout_ms(const lisa_modem_socket_record_t *record,
                                                     uint32_t timeout_ms)
{
    if (!record) {
        return (timeout_ms > 0U && timeout_ms != MODEM_TIMEOUT_UNSET)
             ? timeout_ms
             : MODEM_COMPAT_DEFAULT_TIMEOUT_MS;
    }

    if (timeout_ms > 0U && timeout_ms != MODEM_TIMEOUT_UNSET) {
        return timeout_ms;
    }
    if (record->send_timeout_ms != MODEM_TIMEOUT_UNSET) {
        return record->send_timeout_ms;
    }

    return MODEM_COMPAT_DEFAULT_TIMEOUT_MS;
}

static bool lisa_modem_tick_deadline_reached(TickType_t now, TickType_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static void lisa_modem_socket_tx_reset(lisa_modem_socket_record_t *record)
{
    if (!record) {
        return;
    }

    record->tx_data = NULL;
    record->tx_length = 0U;
    record->tx_offset = 0U;
    record->tx_result = -1;
    record->tx_timeout_ms = 0U;
    record->tx_active = false;
    record->tx_has_destination = false;
    memset(&record->tx_destination, 0, sizeof(record->tx_destination));
    record->tx_buffer_head = 0U;
    record->tx_buffer_tail = 0U;
    record->tx_buffer_used = 0U;
    record->tx_flush_deadline_tick = 0;
    record->tx_flush_deadline_active = false;
}

static size_t lisa_modem_socket_tx_buffer_free_locked(const lisa_modem_socket_record_t *record)
{
    return record ? (MODEM_TCP_TX_BUFFER_SIZE - record->tx_buffer_used) : 0U;
}

static size_t lisa_modem_socket_tx_buffer_contiguous_used_locked(const lisa_modem_socket_record_t *record)
{
    if (!record || record->tx_buffer_used == 0U) {
        return 0U;
    }

    if (record->tx_buffer_head < record->tx_buffer_tail) {
        return record->tx_buffer_tail - record->tx_buffer_head;
    }

    return MODEM_TCP_TX_BUFFER_SIZE - record->tx_buffer_head;
}

static size_t lisa_modem_socket_tx_buffer_write_locked(lisa_modem_socket_record_t *record,
                                                       const uint8_t *data, size_t length)
{
    size_t free_space;
    size_t first_copy;
    size_t second_copy;

    if (!record || !record->tx_buffer || !data || length == 0U) {
        return 0U;
    }

    free_space = lisa_modem_socket_tx_buffer_free_locked(record);
    if (free_space == 0U) {
        return 0U;
    }
    if (length > free_space) {
        length = free_space;
    }

    first_copy = MODEM_TCP_TX_BUFFER_SIZE - record->tx_buffer_tail;
    if (first_copy > length) {
        first_copy = length;
    }
    memcpy(record->tx_buffer + record->tx_buffer_tail, data, first_copy);
    record->tx_buffer_tail = (record->tx_buffer_tail + first_copy) % MODEM_TCP_TX_BUFFER_SIZE;

    second_copy = length - first_copy;
    if (second_copy > 0U) {
        memcpy(record->tx_buffer + record->tx_buffer_tail, data + first_copy, second_copy);
        record->tx_buffer_tail = (record->tx_buffer_tail + second_copy) % MODEM_TCP_TX_BUFFER_SIZE;
    }

    record->tx_buffer_used += length;
    return length;
}

static void lisa_modem_socket_tx_buffer_consume_locked(lisa_modem_socket_record_t *record, size_t length)
{
    if (!record || length == 0U) {
        return;
    }

    if (length >= record->tx_buffer_used) {
        record->tx_buffer_head = 0U;
        record->tx_buffer_tail = 0U;
        record->tx_buffer_used = 0U;
        record->tx_flush_deadline_active = false;
        record->tx_flush_deadline_tick = 0;
        return;
    }

    record->tx_buffer_head = (record->tx_buffer_head + length) % MODEM_TCP_TX_BUFFER_SIZE;
    record->tx_buffer_used -= length;
}

static bool lisa_modem_socket_tx_should_flush_locked(const lisa_modem_socket_record_t *record, TickType_t now)
{
    if (!record || record->tx_buffer_used == 0U) {
        return false;
    }
    if (record->tx_flush_threshold == 0U || record->tx_buffer_used >= record->tx_flush_threshold) {
        return true;
    }
    if (record->tx_flush_timeout_ms == 0U) {
        return true;
    }

    return record->tx_flush_deadline_active &&
           lisa_modem_tick_deadline_reached(now, record->tx_flush_deadline_tick);
}

static void lisa_modem_socket_tx_arm_deadline_locked(lisa_modem_socket_record_t *record, TickType_t now)
{
    if (!record || record->tx_buffer_used == 0U || record->tx_flush_timeout_ms == 0U) {
        return;
    }
    if (record->tx_flush_deadline_active) {
        return;
    }

    record->tx_flush_deadline_tick = now + pdMS_TO_TICKS(record->tx_flush_timeout_ms);
    record->tx_flush_deadline_active = true;
}

static int lisa_modem_apply_tls(lisa_modem_t *modem, const lisa_modem_socket_record_t *record)
{
    if (!record || !record->tls_enabled) {
        return 0;
    }

    if (!modem || !modem->ops || !modem->ops->set_tls) {
        return -ENOTSUP;
    }

    return modem->ops->set_tls(modem->driver_ctx, record->driver_id, true);
}

static int lisa_modem_socket_open_impl(lisa_modem_t *modem, int domain, int type, int protocol)
{
    lisa_modem_socket_record_t *record;
    lisa_modem_socket_record_t snapshot;
    int normalized_protocol;
    int driver_id;

    if (!modem || !modem->ops || !modem->ops->open_fn) {
        return -1;
    }

    normalized_protocol = lisa_modem_normalize_protocol(type, protocol);
    if (normalized_protocol < 0) {
        return -1;
    }

    lisa_modem_socket_table_lock(modem);
    record = lisa_modem_alloc_socket_record_locked(modem, domain, normalized_protocol);
    if (!record) {
        lisa_modem_socket_table_unlock(modem);
        return -1;
    }
    snapshot = *record;
    lisa_modem_socket_table_unlock(modem);

    driver_id = modem->ops->open_fn(modem->driver_ctx, domain, normalized_protocol);
    if (driver_id < 0) {
        lisa_modem_socket_table_lock(modem);
        if (lisa_modem_socket_generation_matches_locked(modem, snapshot.sockfd, snapshot.generation)) {
            lisa_modem_release_socket_record_locked(modem, snapshot.sockfd);
        }
        lisa_modem_socket_table_unlock(modem);
        return -1;
    }

    lisa_modem_socket_table_lock(modem);
    record = lisa_modem_get_socket_record_locked(modem, snapshot.sockfd);
    if (record && record->generation == snapshot.generation) {
        record->driver_id = driver_id;
    }
    lisa_modem_socket_table_unlock(modem);
    if (modem->ops->bind_socket) {
        modem->ops->bind_socket(modem->driver_ctx, driver_id, snapshot.sockfd, snapshot.generation);
    }
    return snapshot.sockfd;
}

static bool lisa_modem_socket_connect_impl(lisa_modem_t *modem, int sockfd,
                                           const struct sockaddr *addr, int addrlen)
{
    lisa_modem_socket_record_t snapshot;
    modem_addr_t remote_addr;

    if (!modem || !lisa_modem_validate_sockaddr_input(addr, addrlen)) {
        return false;
    }

    if (!lisa_modem_snapshot_socket_record(modem, sockfd, &snapshot) ||
        !modem->ops || !modem->ops->connect_fn) {
        return false;
    }

    if (!lisa_modem_sockaddr_to_addr(addr, (socklen_t)addrlen, &remote_addr)) {
        return false;
    }

    if (lisa_modem_apply_tls(modem, &snapshot) != 0 ||
        modem->ops->connect_fn(modem->driver_ctx, snapshot.driver_id, &remote_addr) != 0) {
        return false;
    }

    lisa_modem_socket_table_lock(modem);
    if (lisa_modem_socket_generation_matches_locked(modem, sockfd, snapshot.generation)) {
        lisa_modem_mark_peer(&modem->sockets[sockfd], addr, (socklen_t)addrlen);
        lisa_modem_socket_table_unlock(modem);
        return true;
    }
    lisa_modem_socket_table_unlock(modem);
    return false;
}

static int lisa_modem_socket_close_impl(lisa_modem_t *modem, int sockfd)
{
    lisa_modem_socket_record_t snapshot;
    lisa_modem_control_sync_t sync = {0};
    modem_dispatcher_control_request_t request = {0};
    int ret = -1;

    if (!modem || !modem->ops || !modem->ops->close_fn) {
        return -1;
    }

    if (!lisa_modem_snapshot_socket_record(modem, sockfd, &snapshot)) {
        return -1;
    }

    if (!modem->dispatcher) {
        return -1;
    }

    if (snapshot.protocol == IPPROTO_TCP) {
        TickType_t start_tick = xTaskGetTickCount();
        TickType_t timeout_ticks =
            pdMS_TO_TICKS(lisa_modem_effective_send_timeout_ms(&snapshot, snapshot.send_timeout_ms));

        while (true) {
            bool drained = false;

            lisa_modem_socket_table_lock(modem);
            if (!lisa_modem_socket_generation_matches_locked(modem, sockfd, snapshot.generation)) {
                lisa_modem_socket_table_unlock(modem);
                return -1;
            }
            drained = (modem->sockets[sockfd].tx_buffer_used == 0U);
            lisa_modem_socket_table_unlock(modem);
            if (drained) {
                break;
            }

            (void)modem_dispatcher_mark_tx_ready(modem->dispatcher, sockfd, snapshot.generation);
            if (snapshot.tx_done_sem) {
                (void)xSemaphoreTake(snapshot.tx_done_sem, pdMS_TO_TICKS(1U));
            } else {
                vTaskDelay(pdMS_TO_TICKS(1U));
            }

            if (timeout_ticks > 0U && (xTaskGetTickCount() - start_tick) >= timeout_ticks) {
                break;
            }
        }
    }

    sync.result = INT_MIN;
    sync.done_sem = xSemaphoreCreateBinary();
    if (!sync.done_sem) {
        return -1;
    }

    request.kind = MODEM_DISPATCHER_CONTROL_CLOSE;
    request.endpoint_id = sockfd;
    request.generation = snapshot.generation;
    request.value0 = (uintptr_t)snapshot.driver_id;
    request.context = &sync;

    if (!modem_dispatcher_enqueue_control(modem->dispatcher, &request)) {
        vSemaphoreDelete(sync.done_sem);
        return -1;
    }

#ifdef LISA_MODEM_TEST
    while (sync.result == INT_MIN) {
        if (modem_dispatcher_service_once(modem->dispatcher) == MODEM_DISPATCHER_SERVICE_IDLE) {
            break;
        }
    }
    ret = sync.result;
#else
    if (xSemaphoreTake(sync.done_sem, portMAX_DELAY) == pdTRUE) {
        ret = sync.result;
    }
#endif

    vSemaphoreDelete(sync.done_sem);
    return ret;
}

static int lisa_modem_socket_send_impl(lisa_modem_t *modem, int sockfd,
                                       const void *data, size_t length,
                                       const struct sockaddr *dest_addr, int addrlen,
                                       uint32_t timeout_ms)
{
    lisa_modem_socket_record_t snapshot;
    modem_addr_t remote_addr;
    const modem_addr_t *remote_addr_ptr = NULL;
    int ret;

    if (!modem || !modem->ops || !modem->ops->send_fn) {
        return -1;
    }

    if (!lisa_modem_snapshot_socket_record(modem, sockfd, &snapshot)) {
        return -1;
    }

    if (dest_addr) {
        if (!lisa_modem_validate_sockaddr_input(dest_addr, addrlen) ||
            !lisa_modem_sockaddr_to_addr(dest_addr, (socklen_t)addrlen, &remote_addr)) {
            return -1;
        }
        remote_addr_ptr = &remote_addr;
    }

    if (snapshot.protocol == IPPROTO_TCP && modem->ops->send_chunk && modem->dispatcher) {
        const uint8_t *src = (const uint8_t *)data;
        size_t queued = 0U;
        TickType_t start_tick = xTaskGetTickCount();
        TickType_t timeout_ticks =
            pdMS_TO_TICKS(lisa_modem_effective_send_timeout_ms(&snapshot, timeout_ms));

        lisa_modem_socket_table_lock(modem);
        if (!lisa_modem_socket_generation_matches_locked(modem, sockfd, snapshot.generation)) {
            lisa_modem_socket_table_unlock(modem);
            return -1;
        }
        if (!modem->sockets[sockfd].tx_done_sem) {
            modem->sockets[sockfd].tx_done_sem = xSemaphoreCreateBinary();
            if (!modem->sockets[sockfd].tx_done_sem) {
                lisa_modem_socket_table_unlock(modem);
                return -1;
            }
        }
        if (!modem->sockets[sockfd].tx_buffer) {
            modem->sockets[sockfd].tx_buffer = (uint8_t *)at_mem_alloc(MODEM_TCP_TX_BUFFER_SIZE);
            if (!modem->sockets[sockfd].tx_buffer) {
                lisa_modem_socket_table_unlock(modem);
                return -1;
            }
        }
        modem->sockets[sockfd].tx_result = -1;
        modem->sockets[sockfd].tx_timeout_ms = lisa_modem_effective_send_timeout_ms(&snapshot, timeout_ms);
        modem->sockets[sockfd].tx_has_destination = (remote_addr_ptr != NULL);
        if (remote_addr_ptr) {
            modem->sockets[sockfd].tx_destination = *remote_addr_ptr;
        } else {
            memset(&modem->sockets[sockfd].tx_destination, 0, sizeof(modem->sockets[sockfd].tx_destination));
        }
        lisa_modem_socket_table_unlock(modem);

        while (queued < length) {
            size_t wrote = 0U;

            lisa_modem_socket_table_lock(modem);
            if (!lisa_modem_socket_generation_matches_locked(modem, sockfd, snapshot.generation)) {
                lisa_modem_socket_table_unlock(modem);
                return -1;
            }

            wrote = lisa_modem_socket_tx_buffer_write_locked(&modem->sockets[sockfd],
                                                             src + queued, length - queued);
            if (wrote > 0U) {
                TickType_t now = xTaskGetTickCount();

                modem->sockets[sockfd].tx_active = true;
                lisa_modem_socket_tx_arm_deadline_locked(&modem->sockets[sockfd], now);
                /* Treat the send timeout as a no-progress timeout for payloads
                 * larger than the bounded TX ring buffer. */
                start_tick = now;
            }
            lisa_modem_socket_table_unlock(modem);

            if (wrote > 0U) {
                queued += wrote;
                if (!modem_dispatcher_mark_tx_ready(modem->dispatcher, sockfd, snapshot.generation)) {
                    return queued > 0U ? (int)queued : -1;
                }
                continue;
            }

            if (!modem_dispatcher_mark_tx_ready(modem->dispatcher, sockfd, snapshot.generation)) {
                return queued > 0U ? (int)queued : -1;
            }
            (void)xSemaphoreTake(modem->sockets[sockfd].tx_done_sem, pdMS_TO_TICKS(1U));

            if (timeout_ticks > 0U && (xTaskGetTickCount() - start_tick) >= timeout_ticks) {
                return queued > 0U ? (int)queued : -1;
            }
#ifdef LISA_MODEM_TEST
            while (modem_dispatcher_service_once(modem->dispatcher) != MODEM_DISPATCHER_SERVICE_IDLE) {
            }
#endif
        }
        ret = (int)queued;
    } else {
        ret = modem->ops->send_fn(modem->driver_ctx, snapshot.driver_id, data, length,
                                  timeout_ms, remote_addr_ptr);
    }
    if (ret >= 0 && dest_addr && snapshot.protocol == IPPROTO_TCP) {
        lisa_modem_socket_table_lock(modem);
        if (lisa_modem_socket_generation_matches_locked(modem, sockfd, snapshot.generation)) {
            lisa_modem_mark_peer(&modem->sockets[sockfd], dest_addr, (socklen_t)addrlen);
        }
        lisa_modem_socket_table_unlock(modem);
    }

    return ret;
}

static int lisa_modem_socket_recv_impl(lisa_modem_t *modem, int sockfd,
                                       void *buffer, size_t length,
                                       struct sockaddr *src_addr, int *addrlen,
                                       uint32_t timeout_ms)
{
    lisa_modem_socket_record_t snapshot;
    uint32_t effective_timeout_ms;
    modem_addr_t remote_addr;
    modem_addr_t *remote_addr_ptr = NULL;
    socklen_t socklen = 0;
    socklen_t *socklen_ptr = NULL;
    int ret;

    if (!modem || !modem->ops || !modem->ops->recv_fn) {
        return -1;
    }

    if (!lisa_modem_snapshot_socket_record(modem, sockfd, &snapshot)) {
        return -1;
    }

    if (addrlen) {
        if (!lisa_modem_convert_addrlen_in(addrlen, &socklen)) {
            return -1;
        }
        socklen_ptr = &socklen;
    }

    if (socklen_ptr) {
        memset(&remote_addr, 0, sizeof(remote_addr));
        remote_addr_ptr = &remote_addr;
    }

    effective_timeout_ms = lisa_modem_effective_recv_timeout_ms(&snapshot, timeout_ms);
    ret = modem->ops->recv_fn(modem->driver_ctx, snapshot.driver_id, buffer, length,
                              effective_timeout_ms, remote_addr_ptr);

    if (ret >= 0 && src_addr && socklen_ptr && remote_addr_ptr) {
        if (remote_addr_ptr->family != 0) {
            (void)lisa_modem_addr_to_sockaddr(remote_addr_ptr, src_addr, socklen_ptr);
        } else if (snapshot.protocol == IPPROTO_TCP && snapshot.peer_addr_len > 0) {
            socklen_t copy_len = snapshot.peer_addr_len;

            if (copy_len > *socklen_ptr) {
                copy_len = *socklen_ptr;
            }
            memcpy(src_addr, &snapshot.peer_addr, (size_t)copy_len);
            *socklen_ptr = snapshot.peer_addr_len;
        }
    }

    if (addrlen) {
        *addrlen = (int)socklen;
    }

    return ret;
}

static int lisa_modem_socket_setsockopt_impl(lisa_modem_t *modem, int sockfd, int level,
                                             int optname, const void *optval, int optlen)
{
    lisa_modem_socket_record_t snapshot;
    bool enable_tls;

    if (!modem || !optval || optlen <= 0) {
        return -1;
    }

    if (!lisa_modem_snapshot_socket_record(modem, sockfd, &snapshot)) {
        return -1;
    }

    if (level != SOL_SOCKET) {
        return -ENOTSUP;
    }

    if (optname == SO_SNDTIMEO && optlen >= (int)sizeof(struct timeval)) {
        return lisa_modem_update_timeout(modem, sockfd, snapshot.generation, snapshot.driver_id, true,
                                         lisa_modem_timeval_to_ms((const struct timeval *)optval));
    }

    if (optname == SO_RCVTIMEO && optlen >= (int)sizeof(struct timeval)) {
        return lisa_modem_update_timeout(modem, sockfd, snapshot.generation, snapshot.driver_id, false,
                                         lisa_modem_timeval_to_ms((const struct timeval *)optval));
    }

    if (optname == SO_SSL_CONFIG && optlen >= (int)sizeof(bool)) {
        enable_tls = *(const bool *)optval;
        lisa_modem_socket_table_lock(modem);
        if (!lisa_modem_socket_generation_matches_locked(modem, sockfd, snapshot.generation)) {
            lisa_modem_socket_table_unlock(modem);
            return -1;
        }
        modem->sockets[sockfd].tls_enabled = enable_tls;
        snapshot.tls_enabled = enable_tls;
        lisa_modem_socket_table_unlock(modem);

        if (!modem->ops || !modem->ops->set_tls) {
            return enable_tls ? -ENOTSUP : 0;
        }

        if (!enable_tls) {
            return modem->ops->set_tls(modem->driver_ctx, snapshot.driver_id, false);
        }

        return lisa_modem_apply_tls(modem, &snapshot);
    }

    return -ENOTSUP;
}

static int lisa_modem_socket_ioctl_impl(lisa_modem_t *modem, int sockfd, long cmd, void *arg)
{
    lisa_modem_socket_record_t snapshot;

    if (!modem || !modem->ops || !modem->ops->set_nonblock) {
        return -1;
    }

    if (!lisa_modem_snapshot_socket_record(modem, sockfd, &snapshot)) {
        return -1;
    }

    if (cmd == FIONBIO && arg) {
        bool nonblock = (*(int *)arg != 0);

        lisa_modem_socket_table_lock(modem);
        if (!lisa_modem_socket_generation_matches_locked(modem, sockfd, snapshot.generation)) {
            lisa_modem_socket_table_unlock(modem);
            return -1;
        }
        modem->sockets[sockfd].nonblock = nonblock;
        lisa_modem_socket_table_unlock(modem);
        return modem->ops->set_nonblock(modem->driver_ctx, snapshot.driver_id, nonblock);
    }

    return -ENOTSUP;
}

static int lisa_modem_socket_getpeername_impl(lisa_modem_t *modem, int sockfd,
                                              struct sockaddr *addr, int *addrlen)
{
    lisa_modem_socket_record_t snapshot;
    socklen_t peer_len;

    if (!modem || !addrlen || !lisa_modem_convert_addrlen_in(addrlen, &peer_len)) {
        return -1;
    }

    if (!lisa_modem_snapshot_socket_record(modem, sockfd, &snapshot) ||
        lisa_modem_copy_peer(&snapshot, addr, &peer_len) != 0) {
        return -1;
    }

    *addrlen = (int)peer_len;
    return 0;
}

static bool lisa_modem_dispatcher_generation_matches(void *user_data, int endpoint_id, uint32_t generation)
{
    return lisa_modem_socket_generation_matches((lisa_modem_t *)user_data, endpoint_id, generation);
}

static modem_dispatcher_work_result_t
lisa_modem_dispatcher_handle_control(void *user_data, const modem_dispatcher_control_request_t *request)
{
    lisa_modem_t *modem = (lisa_modem_t *)user_data;
    int ret = -1;

#ifdef LISA_MODEM_TEST
    s_lisa_modem_test_in_dispatcher_control = true;
#endif

    if (!modem || !request || !modem->ops) {
        lisa_modem_control_sync_t *sync = request ? (lisa_modem_control_sync_t *)request->context : NULL;

        if (sync && sync->done_sem) {
            sync->result = -1;
            (void)xSemaphoreGive(sync->done_sem);
        }
#ifdef LISA_MODEM_TEST
        s_lisa_modem_test_in_dispatcher_control = false;
#endif
        return MODEM_DISPATCHER_WORK_DROP;
    }

    if (request->kind == MODEM_DISPATCHER_CONTROL_CLOSE && modem->ops->close_fn) {
        lisa_modem_control_sync_t *sync = (lisa_modem_control_sync_t *)request->context;
        const int driver_id = (int)request->value0;
        bool valid = false;

        if (modem->ops->bind_socket) {
            modem->ops->bind_socket(modem->driver_ctx, driver_id, -1, 0U);
        }

        lisa_modem_socket_table_lock(modem);
        if (lisa_modem_socket_generation_matches_locked(modem, request->endpoint_id, request->generation)) {
            lisa_modem_release_socket_record_locked(modem, request->endpoint_id);
            valid = true;
        }
        lisa_modem_socket_table_unlock(modem);

        if (valid) {
            ret = modem->ops->close_fn(modem->driver_ctx, driver_id);
        }
        if (sync && sync->done_sem) {
            sync->result = ret;
            (void)xSemaphoreGive(sync->done_sem);
        }
    } else if (request->kind == MODEM_DISPATCHER_CONTROL_DNS_RESOLVE && modem->ops->dns_resolve) {
        lisa_modem_dns_sync_t *sync = (lisa_modem_dns_sync_t *)request->context;

        ret = (sync && sync->domain && sync->ip_addr && sync->size > 0U &&
               modem->ops->dns_resolve(modem->driver_ctx, sync->domain, sync->ip_addr, sync->size))
            ? 1
            : 0;
        if (sync && sync->done_sem) {
            sync->result = ret;
            (void)xSemaphoreGive(sync->done_sem);
        }
    } else if (request->kind == MODEM_DISPATCHER_CONTROL_GET_IMEI ||
               request->kind == MODEM_DISPATCHER_CONTROL_GET_ICCID) {
        lisa_modem_identity_sync_t *sync = (lisa_modem_identity_sync_t *)request->context;
        bool ok = false;

        if (sync && sync->value && sync->size > 0U) {
            if (request->kind == MODEM_DISPATCHER_CONTROL_GET_IMEI && modem->ops->get_imei) {
                ok = modem->ops->get_imei(modem->driver_ctx, sync->value, sync->size);
            } else if (request->kind == MODEM_DISPATCHER_CONTROL_GET_ICCID && modem->ops->get_iccid) {
                ok = modem->ops->get_iccid(modem->driver_ctx, sync->value, sync->size);
            }
        }

        ret = ok ? 1 : 0;
        if (sync && sync->done_sem) {
            sync->result = ret;
            (void)xSemaphoreGive(sync->done_sem);
        }
    } else if (request->kind == MODEM_DISPATCHER_CONTROL_GET_SIGNAL_QUALITY) {
        lisa_modem_signal_sync_t *sync = (lisa_modem_signal_sync_t *)request->context;
        bool ok = sync && sync->rssi && sync->ber && modem->ops->get_signal_quality &&
                  modem->ops->get_signal_quality(modem->driver_ctx, sync->rssi, sync->ber);

        ret = ok ? 1 : 0;
        if (sync && sync->done_sem) {
            sync->result = ret;
            (void)xSemaphoreGive(sync->done_sem);
        }
    }

#ifdef LISA_MODEM_TEST
    s_lisa_modem_test_in_dispatcher_control = false;
#endif
    return MODEM_DISPATCHER_WORK_COMPLETE;
}

static modem_dispatcher_work_result_t
lisa_modem_dispatcher_handle_tx(void *user_data, int endpoint_id, uint32_t generation)
{
    lisa_modem_t *modem = (lisa_modem_t *)user_data;
    const modem_addr_t *to = NULL;
    const uint8_t *data;
    size_t remaining;
    size_t chunk_length;
    int driver_id;
    uint32_t timeout_ms;
    int ret;
    bool complete = false;
    modem_addr_t destination;

    if (!modem || !modem->ops || !modem->ops->send_chunk) {
        return MODEM_DISPATCHER_WORK_DROP;
    }

    lisa_modem_socket_table_lock(modem);
    if (!lisa_modem_socket_generation_matches_locked(modem, endpoint_id, generation)) {
        lisa_modem_socket_table_unlock(modem);
        return MODEM_DISPATCHER_WORK_DROP;
    }
    if (modem->sockets[endpoint_id].protocol == IPPROTO_TCP) {
        TickType_t now = xTaskGetTickCount();

        if (modem->sockets[endpoint_id].tx_buffer_used == 0U) {
            modem->sockets[endpoint_id].tx_active = false;
            lisa_modem_socket_table_unlock(modem);
            return MODEM_DISPATCHER_WORK_DROP;
        }
        if (!lisa_modem_socket_tx_should_flush_locked(&modem->sockets[endpoint_id], now)) {
            lisa_modem_socket_table_unlock(modem);
            return MODEM_DISPATCHER_WORK_WAIT;
        }

        data = modem->sockets[endpoint_id].tx_buffer + modem->sockets[endpoint_id].tx_buffer_head;
        remaining = lisa_modem_socket_tx_buffer_contiguous_used_locked(&modem->sockets[endpoint_id]);
        chunk_length = remaining;
        if (modem->ops->caps.max_tx_chunk > 0U && chunk_length > modem->ops->caps.max_tx_chunk) {
            chunk_length = modem->ops->caps.max_tx_chunk;
        }
        modem->sockets[endpoint_id].tx_flush_deadline_active = false;
    } else {
        if (!modem->sockets[endpoint_id].tx_active ||
            !modem->sockets[endpoint_id].tx_data ||
            modem->sockets[endpoint_id].tx_offset >= modem->sockets[endpoint_id].tx_length) {
            modem->sockets[endpoint_id].tx_active = false;
            lisa_modem_socket_table_unlock(modem);
            return MODEM_DISPATCHER_WORK_DROP;
        }

        data = modem->sockets[endpoint_id].tx_data + modem->sockets[endpoint_id].tx_offset;
        remaining = modem->sockets[endpoint_id].tx_length - modem->sockets[endpoint_id].tx_offset;
        chunk_length = remaining;
        if (modem->ops->caps.max_tx_chunk > 0U && chunk_length > modem->ops->caps.max_tx_chunk) {
            chunk_length = modem->ops->caps.max_tx_chunk;
        }
    }
    driver_id = modem->sockets[endpoint_id].driver_id;
    timeout_ms = modem->sockets[endpoint_id].tx_timeout_ms;
    if (modem->sockets[endpoint_id].tx_has_destination) {
        destination = modem->sockets[endpoint_id].tx_destination;
        to = &destination;
    }
    lisa_modem_socket_table_unlock(modem);

    ret = modem->ops->send_chunk(modem->driver_ctx, driver_id, data, chunk_length, timeout_ms, to);

    lisa_modem_socket_table_lock(modem);
    if (!lisa_modem_socket_generation_matches_locked(modem, endpoint_id, generation)) {
        lisa_modem_socket_table_unlock(modem);
        return MODEM_DISPATCHER_WORK_DROP;
    }

    if (ret <= 0) {
        lisa_modem_socket_tx_reset(&modem->sockets[endpoint_id]);
        modem->sockets[endpoint_id].tx_result = -1;
        complete = true;
        if (modem->sockets[endpoint_id].tx_done_sem) {
            (void)xSemaphoreGive(modem->sockets[endpoint_id].tx_done_sem);
        }
    } else {
        if (modem->sockets[endpoint_id].protocol == IPPROTO_TCP) {
            lisa_modem_socket_tx_buffer_consume_locked(&modem->sockets[endpoint_id], (size_t)ret);
            modem->sockets[endpoint_id].tx_result = (int)ret;
            if (modem->sockets[endpoint_id].tx_buffer_used == 0U) {
                modem->sockets[endpoint_id].tx_active = false;
                complete = true;
            }
            if (modem->sockets[endpoint_id].tx_done_sem) {
                (void)xSemaphoreGive(modem->sockets[endpoint_id].tx_done_sem);
            }
        } else {
            modem->sockets[endpoint_id].tx_offset += (size_t)ret;
            if (modem->sockets[endpoint_id].tx_offset >= modem->sockets[endpoint_id].tx_length) {
                modem->sockets[endpoint_id].tx_active = false;
                modem->sockets[endpoint_id].tx_result = (int)modem->sockets[endpoint_id].tx_offset;
                complete = true;
            }
        }
    }

    if (complete && modem->sockets[endpoint_id].tx_done_sem) {
        (void)xSemaphoreGive(modem->sockets[endpoint_id].tx_done_sem);
    }
    lisa_modem_socket_table_unlock(modem);
    return complete ? MODEM_DISPATCHER_WORK_COMPLETE : MODEM_DISPATCHER_WORK_REQUEUE;
}

static modem_dispatcher_work_result_t
lisa_modem_dispatcher_handle_rx(void *user_data, int endpoint_id, uint32_t generation)
{
    lisa_modem_t *modem = (lisa_modem_t *)user_data;
    lisa_modem_socket_record_t snapshot;
    int ret;

    if (!modem || !modem->ops || !modem->ops->pull_rx) {
        return MODEM_DISPATCHER_WORK_DROP;
    }
    if (!lisa_modem_snapshot_socket_record(modem, endpoint_id, &snapshot) ||
        snapshot.generation != generation) {
        return MODEM_DISPATCHER_WORK_DROP;
    }

    ret = modem->ops->pull_rx(modem->driver_ctx, snapshot.driver_id);
    if (ret > 0) {
        return MODEM_DISPATCHER_WORK_REQUEUE;
    }
    if (ret == 0) {
        return MODEM_DISPATCHER_WORK_COMPLETE;
    }
    return MODEM_DISPATCHER_WORK_DROP;
}

static const modem_dispatcher_ops_t s_lisa_modem_dispatcher_ops = {
    .generation_matches = lisa_modem_dispatcher_generation_matches,
    .handle_control = lisa_modem_dispatcher_handle_control,
    .handle_tx = lisa_modem_dispatcher_handle_tx,
    .handle_rx = lisa_modem_dispatcher_handle_rx,
};

static bool lisa_modem_host_to_sockaddr(lisa_modem_t *modem, const char *host, uint16_t port,
                                        struct sockaddr_in *addr)
{
    char ip_addr[64] = {0};

    if (!modem || !host || !addr) {
        return false;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr->sin_addr) == 1) {
        return true;
    }

    if (!lisa_modem_dns_resolve_on(modem, host, ip_addr, sizeof(ip_addr))) {
        return false;
    }

    return inet_pton(AF_INET, ip_addr, &addr->sin_addr) == 1;
}

static const modem_driver_ops_t *lisa_modem_detect_driver(at_client_t *client,
                                                          modem_probe_result_t *probe_result,
                                                          lisa_modem_status_t *status)
{
#if !CONFIG_LISA_MODEM_DRIVER_ML307 && !CONFIG_LISA_MODEM_DRIVER_EC801E
    (void)client;
    (void)probe_result;
    s_modem_present = false;
    if (status) {
        status->last_error = LISA_MODEM_ERR_DRIVER_NOT_FOUND;
        lisa_modem_publish_status(NULL, status);
    }
    return NULL;
#else
    const modem_driver_ops_t *drivers[] = {
#if CONFIG_LISA_MODEM_DRIVER_ML307
        ml307_endpoint_get_ops(),
#endif
#if CONFIG_LISA_MODEM_DRIVER_EC801E
        ec801e_endpoint_get_ops(),
#endif
    };
    const modem_driver_ops_t *best_ops = NULL;
    modem_probe_result_t best_result = {0};
    size_t i;

    if (!client) {
        if (status) {
            status->last_error = LISA_MODEM_ERR_INVALID_ARG;
            lisa_modem_publish_status(NULL, status);
        }
        return NULL;
    }

    if (!modem_probe_sync_uart_baud(client)) {
        LISA_LOGE(TAG, "Failed to sync modem UART before driver probe");
        s_modem_present = false;
        if (status) {
            status->last_error = LISA_MODEM_ERR_UART_AT_SYNC_FAILED;
            lisa_modem_status_note_cme(status, at_client_get_cme_error(client));
            lisa_modem_publish_status(NULL, status);
        }
        return NULL;
    }

    for (i = 0; i < sizeof(drivers) / sizeof(drivers[0]); ++i) {
        const modem_driver_ops_t *ops = drivers[i];
        modem_probe_result_t current_result = {0};

        if (!ops || !ops->probe || !ops->probe(client, &current_result)) {
            continue;
        }

        if (!best_ops || current_result.match_score > best_result.match_score) {
            best_ops = ops;
            best_result = current_result;
        }
    }

    if (probe_result && best_ops) {
        *probe_result = best_result;
    }
    if (status) {
        if (!best_ops) {
            status->last_error = LISA_MODEM_ERR_DRIVER_NOT_FOUND;
            lisa_modem_publish_status(NULL, status);
        }
    }

    s_modem_present = (best_ops != NULL);
    return best_ops;
#endif
}

static const char *lisa_modem_resolve_name(const char *requested_name,
                                           const modem_driver_ops_t *ops,
                                           const modem_probe_result_t *probe_result)
{
    if (requested_name && requested_name[0] != '\0' &&
        strcmp(requested_name, "modem") != 0) {
        return requested_name;
    }

    if (probe_result && probe_result->driver_name[0] != '\0') {
        return probe_result->driver_name;
    }

    if (ops && ops->name && ops->name[0] != '\0') {
        return ops->name;
    }

    return requested_name && requested_name[0] != '\0' ? requested_name : "modem";
}

static lisa_modem_t *lisa_modem_create_resolved(at_client_t *client, const char *name,
                                                const modem_driver_ops_t *ops,
                                                const modem_probe_result_t *probe_result,
                                                const lisa_modem_status_t *initial_status)
{
    lisa_modem_t *modem;
    lisa_modem_status_t status;

    if (!client || !name || !ops || !ops->create || !ops->init) {
        lisa_modem_status_clear(&status);
        status.last_error = LISA_MODEM_ERR_INVALID_ARG;
        lisa_modem_publish_status(NULL, &status);
        return NULL;
    }

    if (initial_status) {
        status = *initial_status;
    } else {
        lisa_modem_status_clear(&status);
    }

    modem = (lisa_modem_t *)at_mem_calloc(1, sizeof(lisa_modem_t));
    if (!modem) {
        lisa_modem_status_fail(NULL, &status, LISA_MODEM_ERR_NO_MEMORY);
        return NULL;
    }

    strncpy(modem->name, name, sizeof(modem->name) - 1);
    modem->name[sizeof(modem->name) - 1] = '\0';
    modem->client = client;
    modem->ops = ops;
    modem->status = status;
    lisa_modem_socket_table_init(modem);
    modem->socket_table_mutex = xSemaphoreCreateMutex();
    if (!modem->socket_table_mutex) {
        lisa_modem_status_fail(modem, &modem->status, LISA_MODEM_ERR_NO_MEMORY);
        at_mem_free(modem);
        return NULL;
    }
    modem->dispatcher = modem_dispatcher_create(&(modem_dispatcher_config_t){
        .max_endpoints = MODEM_MAX_SOCKETS,
        .control_queue_capacity = MODEM_MAX_SOCKETS,
        .ops = &s_lisa_modem_dispatcher_ops,
        .user_data = modem,
        .task_name = "lisa_modem_disp",
        .task_stack_size = 2048U,
        /*
         * The dispatcher is the AT transaction owner. Keep it above the RX pump
         * so final OK can wake and resume the waiting command path immediately
         * instead of being starved by continuous incoming UART data.
         */
        .task_priority = 10U,
    });
    if (!modem->dispatcher) {
        lisa_modem_status_fail(modem, &modem->status, LISA_MODEM_ERR_NO_MEMORY);
        vSemaphoreDelete(modem->socket_table_mutex);
        at_mem_free(modem);
        return NULL;
    }
    if (!modem_dispatcher_start(modem->dispatcher)) {
        lisa_modem_status_fail(modem, &modem->status, LISA_MODEM_ERR_NO_MEMORY);
        modem_dispatcher_destroy(modem->dispatcher);
        vSemaphoreDelete(modem->socket_table_mutex);
        at_mem_free(modem);
        return NULL;
    }
    if (probe_result) {
        modem->probe_result = *probe_result;
        modem->has_probe_result = true;
    }

    modem->driver_ctx = ops->create(client);
    if (!modem->driver_ctx) {
        LISA_LOGE(TAG, "Failed to create modem runtime for driver '%s'", ops->name);
        lisa_modem_status_fail(modem, &modem->status, LISA_MODEM_ERR_DRIVER_CREATE_FAILED);
        modem_dispatcher_destroy(modem->dispatcher);
        vSemaphoreDelete(modem->socket_table_mutex);
        at_mem_free(modem);
        return NULL;
    }
    if (ops->attach_dispatcher) {
        ops->attach_dispatcher(modem->driver_ctx, modem->dispatcher);
    }

    if (!ops->init(modem->driver_ctx)) {
        LISA_LOGE(TAG, "Failed to initialize modem '%s' with driver '%s'", modem->name, ops->name);
        lisa_modem_refresh_driver_status(modem);
        if (modem->status.last_error == LISA_MODEM_ERR_NOT_INITIALIZED) {
            modem->status.last_error = LISA_MODEM_ERR_DRIVER_INIT_FAILED;
        }
        lisa_modem_publish_status(modem, &modem->status);
        if (ops->destroy) {
            ops->destroy(modem->driver_ctx);
        }
        modem_dispatcher_destroy(modem->dispatcher);
        vSemaphoreDelete(modem->socket_table_mutex);
        at_mem_free(modem);
        return NULL;
    }

    modem->initialized = true;
    s_modem_present = true;
    lisa_modem_refresh_driver_status(modem);
    modem->status.last_error = LISA_MODEM_ERR_READY;
    lisa_modem_publish_status(modem, &modem->status);
    if (modem->has_probe_result) {
        LISA_LOGI(TAG, "Modem '%s' detected as '%s' (%s)", modem->name,
                  modem->probe_result.driver_name, modem->probe_result.model);
    }
    LISA_LOGI(TAG, "Modem '%s' created with driver '%s'", modem->name, ops->name);
    return modem;
}

lisa_modem_t *lisa_modem_create(at_client_t *client, const char *name)
{
    modem_probe_result_t probe_result = {0};
    lisa_modem_status_t status;
    const modem_driver_ops_t *ops;
    const char *resolved_name = NULL;

    lisa_modem_status_clear(&status);
    ops = lisa_modem_detect_driver(client, &probe_result, &status);
    if (!ops) {
        LISA_LOGE(TAG, "Failed to detect modem driver for '%s'", name ? name : "modem");
        if (status.last_error == LISA_MODEM_ERR_NOT_INITIALIZED) {
            status.last_error = LISA_MODEM_ERR_DRIVER_NOT_FOUND;
            lisa_modem_publish_status(NULL, &status);
        }
        return NULL;
    }

    resolved_name = lisa_modem_resolve_name(name, ops, &probe_result);
    return lisa_modem_create_resolved(client, resolved_name, ops, &probe_result, &status);
}

lisa_modem_t *lisa_modem_create_with_driver(at_client_t *client, const char *name,
                                            const modem_driver_ops_t *ops)
{
    lisa_modem_status_t status;

    lisa_modem_status_clear(&status);
    return lisa_modem_create_resolved(client, name, ops, NULL, &status);
}

lisa_modem_t *lisa_modem_create_uart(const char *uart_dev, uint32_t baudrate)
{
    return lisa_modem_create_uart_with_driver(uart_dev, baudrate, NULL);
}

lisa_modem_t *lisa_modem_create_uart_with_driver(const char *uart_dev, uint32_t baudrate,
                                                 const modem_driver_ops_t *ops)
{
    at_transport_uart_config_t uart_cfg = AT_TRANSPORT_UART_CONFIG_DEFAULT();
    at_transport_t *transport;
    lisa_modem_t *modem;
    lisa_modem_status_t status;

    lisa_modem_status_clear(&status);
    if (!uart_dev) {
        status.last_error = LISA_MODEM_ERR_INVALID_ARG;
        lisa_modem_publish_status(NULL, &status);
        return NULL;
    }

    uart_cfg.baudrate = baudrate > 0 ? baudrate : MODEM_PROBE_PRIMARY_BAUD;

    transport = at_transport_uart_create(uart_dev, &uart_cfg);
    if (!transport) {
        LISA_LOGE(TAG, "Failed to create UART transport for %s", uart_dev);
        status.last_error = LISA_MODEM_ERR_TRANSPORT_CREATE_FAILED;
        lisa_modem_publish_status(NULL, &status);
        return NULL;
    }

    modem = lisa_modem_create_with_transport_and_driver(transport, true, ops);
    return modem;
}

void lisa_modem_destroy(lisa_modem_t *modem)
{
    if (!modem) {
        return;
    }

    if (s_default_modem == modem) {
        s_default_modem = NULL;
    }
    lisa_modem_unregister_default_netdev(modem->name);
    if (modem->dispatcher) {
        modem_dispatcher_destroy(modem->dispatcher);
    }

    if (modem->initialized && modem->ops && modem->ops->deinit) {
        modem->ops->deinit(modem->driver_ctx);
    }
    if (modem->ops && modem->ops->destroy && modem->driver_ctx) {
        modem->ops->destroy(modem->driver_ctx);
    }

    if (modem->owns_transport) {
        if (modem->client) {
            at_client_destroy(modem->client);
        }
        if (modem->transport) {
            at_transport_destroy(modem->transport);
        }
    }

    if (modem->socket_table_mutex) {
        vSemaphoreDelete(modem->socket_table_mutex);
    }
    for (int i = 0; i < MODEM_MAX_SOCKETS; ++i) {
        if (modem->sockets[i].tx_done_sem) {
            vSemaphoreDelete(modem->sockets[i].tx_done_sem);
        }
        at_mem_free(modem->sockets[i].tx_buffer);
    }
    at_mem_free(modem);
}

#ifdef LISA_MODEM_TEST
uint32_t lisa_modem_test_socket_generation(lisa_modem_t *modem, int sockfd)
{
    uint32_t generation = 0;

    if (!modem || sockfd < 0 || sockfd >= MODEM_MAX_SOCKETS) {
        return 0;
    }

    lisa_modem_socket_table_lock(modem);
    generation = modem->sockets[sockfd].generation;
    lisa_modem_socket_table_unlock(modem);
    return generation;
}

bool lisa_modem_test_socket_generation_matches(lisa_modem_t *modem, int sockfd, uint32_t generation)
{
    return lisa_modem_socket_generation_matches(modem, sockfd, generation);
}

bool lisa_modem_test_is_in_dispatcher_control(void)
{
    return s_lisa_modem_test_in_dispatcher_control;
}
#endif

lisa_modem_t *lisa_modem_create_with_transport(at_transport_t *transport, bool owns_transport)
{
    lisa_modem_t *modem = lisa_modem_create_with_transport_and_driver(transport, owns_transport, NULL);

    if (modem && !s_default_modem) {
        s_default_modem = modem;
    }

    return modem;
}

lisa_modem_t *lisa_modem_create_with_transport_and_driver(at_transport_t *transport, bool owns_transport,
                                                          const modem_driver_ops_t *ops)
{
    at_client_t *client;
    lisa_modem_t *modem;
    lisa_modem_status_t status;

    lisa_modem_status_clear(&status);
    if (!transport) {
        status.last_error = LISA_MODEM_ERR_INVALID_ARG;
        lisa_modem_publish_status(NULL, &status);
        return NULL;
    }

    client = at_client_create(&(at_client_config_t){
        .rx_buf_initial_size = 512U,
        .resp_buf_size = 4096U,
        .task_priority = 9U,
        .task_stack_size = 2048U,
    });
    if (!client) {
        status.last_error = LISA_MODEM_ERR_CLIENT_CREATE_FAILED;
        lisa_modem_publish_status(NULL, &status);
        if (owns_transport) {
            at_transport_destroy(transport);
        }
        return NULL;
    }

    at_client_set_debug(client, LISA_MODEM_AT_CLIENT_DEBUG_ENABLE);

    if (at_client_bind(client, transport) != 0) {
        status.last_error = LISA_MODEM_ERR_CLIENT_BIND_FAILED;
        lisa_modem_publish_status(NULL, &status);
        at_client_destroy(client);
        if (owns_transport) {
            at_transport_destroy(transport);
        }
        return NULL;
    }

    modem = ops ? lisa_modem_create_with_driver(client, ops->name, ops)
                : lisa_modem_create(client, "modem");
    if (!modem) {
        at_client_destroy(client);
        if (owns_transport) {
            at_transport_destroy(transport);
        }
        return NULL;
    }

    modem->client = client;
    modem->transport = transport;
    modem->owns_transport = owns_transport;
    return modem;
}

lisa_modem_t *lisa_modem_get_default(void)
{
    return s_default_modem;
}

bool lisa_modem_is_present(void)
{
    return s_default_modem != NULL || s_modem_present;
}

at_client_t *lisa_modem_get_client(lisa_modem_t *modem)
{
    return modem ? modem->client : NULL;
}

bool lisa_modem_get_status_on(lisa_modem_t *modem, lisa_modem_status_t *status)
{
    if (!status) {
        return false;
    }

    if (!modem) {
        *status = s_last_status;
        return true;
    }

    lisa_modem_refresh_driver_status(modem);
    *status = modem->status;
    return true;
}

bool lisa_modem_get_status(lisa_modem_status_t *status)
{
    if (!status) {
        return false;
    }

    if (s_default_modem) {
        return lisa_modem_get_status_on(s_default_modem, status);
    }

    *status = s_last_status;
    return true;
}

bool lisa_modem_dns_resolve_on(lisa_modem_t *modem, const char *domain, char *ip_addr, size_t size)
{
    lisa_modem_dns_sync_t sync = {0};
    modem_dispatcher_control_request_t request = {0};
    bool result = false;

    if (!modem || !modem->ops || !modem->ops->dns_resolve || !domain || !ip_addr || size == 0U) {
        return false;
    }

    if (!modem->dispatcher) {
        return modem->ops->dns_resolve(modem->driver_ctx, domain, ip_addr, size);
    }

    sync.result = INT_MIN;
    sync.done_sem = xSemaphoreCreateBinary();
    if (!sync.done_sem) {
        return false;
    }
    sync.domain = domain;
    sync.ip_addr = ip_addr;
    sync.size = size;

    request.kind = MODEM_DISPATCHER_CONTROL_DNS_RESOLVE;
    request.endpoint_id = -1;
    request.generation = 0U;
    request.context = &sync;

    if (!modem_dispatcher_enqueue_control(modem->dispatcher, &request)) {
        vSemaphoreDelete(sync.done_sem);
        return false;
    }

#ifdef LISA_MODEM_TEST
    while (sync.result == INT_MIN) {
        if (modem_dispatcher_service_once(modem->dispatcher) == MODEM_DISPATCHER_SERVICE_IDLE) {
            break;
        }
    }
    result = (sync.result == 1);
#else
    if (xSemaphoreTake(sync.done_sem, portMAX_DELAY) == pdTRUE) {
        result = (sync.result == 1);
    }
#endif

    vSemaphoreDelete(sync.done_sem);
    return result;
}

static bool lisa_modem_identity_query_on(lisa_modem_t *modem, bool imei, char *value, size_t size)
{
    lisa_modem_identity_sync_t sync = {0};
    modem_dispatcher_control_request_t request = {0};
    bool result = false;

    if (!value || size == 0U) {
        return false;
    }
    value[0] = '\0';

    if (!modem || !modem->ops ||
        (imei && !modem->ops->get_imei) ||
        (!imei && !modem->ops->get_iccid)) {
        return false;
    }

    if (!modem->dispatcher) {
        return imei ? modem->ops->get_imei(modem->driver_ctx, value, size)
                    : modem->ops->get_iccid(modem->driver_ctx, value, size);
    }

    sync.result = INT_MIN;
    sync.done_sem = xSemaphoreCreateBinary();
    if (!sync.done_sem) {
        return false;
    }
    sync.value = value;
    sync.size = size;

    request.kind = imei ? MODEM_DISPATCHER_CONTROL_GET_IMEI
                        : MODEM_DISPATCHER_CONTROL_GET_ICCID;
    request.endpoint_id = -1;
    request.generation = 0U;
    request.context = &sync;

    if (!modem_dispatcher_enqueue_control(modem->dispatcher, &request)) {
        vSemaphoreDelete(sync.done_sem);
        return false;
    }

#ifdef LISA_MODEM_TEST
    while (sync.result == INT_MIN) {
        if (modem_dispatcher_service_once(modem->dispatcher) == MODEM_DISPATCHER_SERVICE_IDLE) {
            break;
        }
    }
    result = (sync.result == 1);
#else
    if (xSemaphoreTake(sync.done_sem, portMAX_DELAY) == pdTRUE) {
        result = (sync.result == 1);
    }
#endif

    vSemaphoreDelete(sync.done_sem);
    return result;
}

bool lisa_modem_get_imei_on(lisa_modem_t *modem, char *imei, size_t size)
{
    return lisa_modem_identity_query_on(modem, true, imei, size);
}

bool lisa_modem_get_iccid_on(lisa_modem_t *modem, char *iccid, size_t size)
{
    return lisa_modem_identity_query_on(modem, false, iccid, size);
}

bool lisa_modem_get_signal_quality_on(lisa_modem_t *modem, int *rssi, int *ber)
{
    lisa_modem_signal_sync_t sync = {0};
    modem_dispatcher_control_request_t request = {0};
    bool result = false;

    if (!rssi || !ber) {
        return false;
    }
    *rssi = 99;
    *ber = 99;

    if (!modem || !modem->ops || !modem->ops->get_signal_quality) {
        return false;
    }

    if (!modem->dispatcher) {
        return modem->ops->get_signal_quality(modem->driver_ctx, rssi, ber);
    }

    sync.result = INT_MIN;
    sync.done_sem = xSemaphoreCreateBinary();
    if (!sync.done_sem) {
        return false;
    }
    sync.rssi = rssi;
    sync.ber = ber;

    request.kind = MODEM_DISPATCHER_CONTROL_GET_SIGNAL_QUALITY;
    request.endpoint_id = -1;
    request.generation = 0U;
    request.context = &sync;

    if (!modem_dispatcher_enqueue_control(modem->dispatcher, &request)) {
        vSemaphoreDelete(sync.done_sem);
        return false;
    }

#ifdef LISA_MODEM_TEST
    while (sync.result == INT_MIN) {
        if (modem_dispatcher_service_once(modem->dispatcher) == MODEM_DISPATCHER_SERVICE_IDLE) {
            break;
        }
    }
    result = (sync.result == 1);
#else
    if (xSemaphoreTake(sync.done_sem, portMAX_DELAY) == pdTRUE) {
        result = (sync.result == 1);
    }
#endif

    vSemaphoreDelete(sync.done_sem);
    return result;
}

bool lisa_modem_get_gps_location_on(lisa_modem_t *modem, double *lat, double *lon)
{
    if (!modem || !modem->driver_ctx || !modem->ops || !modem->ops->get_gps_location) {
        return false;
    }

    return modem->ops->get_gps_location(modem->driver_ctx, lat, lon);
}

bool lisa_modem_start_gnss_on(lisa_modem_t *modem)
{
    if (!modem || !modem->driver_ctx || !modem->ops || !modem->ops->start_gnss) {
        return false;
    }

    return modem->ops->start_gnss(modem->driver_ctx);
}

int lisa_modem_socket_open_on(lisa_modem_t *modem, int domain, int type, int protocol)
{
    return lisa_modem_socket_open_impl(modem, domain, type, protocol);
}

bool lisa_modem_socket_connect_on(lisa_modem_t *modem, int sockfd, const struct sockaddr *addr, int addrlen)
{
    return lisa_modem_socket_connect_impl(modem, sockfd, addr, addrlen);
}

int lisa_modem_socket_close_on(lisa_modem_t *modem, int sockfd)
{
    return lisa_modem_socket_close_impl(modem, sockfd);
}

int lisa_modem_socket_send_on(lisa_modem_t *modem, int sockfd, const void *data, size_t length,
                              uint32_t timeout_ms)
{
    return lisa_modem_socket_send_impl(modem, sockfd, data, length, NULL, 0, timeout_ms);
}

int lisa_modem_socket_sendto_on(lisa_modem_t *modem, int sockfd, const void *data, size_t length, int flags,
                                const struct sockaddr *dest_addr, int addrlen, uint32_t timeout_ms)
{
    (void)flags;
    if (!lisa_modem_validate_sockaddr_input(dest_addr, addrlen)) {
        return -1;
    }

    return lisa_modem_socket_send_impl(modem, sockfd, data, length, dest_addr, addrlen, timeout_ms);
}

int lisa_modem_socket_recv_on(lisa_modem_t *modem, int sockfd, void *buffer, size_t length,
                              uint32_t timeout_ms)
{
    return lisa_modem_socket_recv_impl(modem, sockfd, buffer, length, NULL, NULL, timeout_ms);
}

int lisa_modem_socket_recvfrom_on(lisa_modem_t *modem, int sockfd, void *buffer, size_t length, int flags,
                                  struct sockaddr *src_addr, int *addrlen, uint32_t timeout_ms)
{
    (void)flags;
    return lisa_modem_socket_recv_impl(modem, sockfd, buffer, length, src_addr, addrlen, timeout_ms);
}

int lisa_modem_setsockopt_on(lisa_modem_t *modem, int sockfd, int level, int optname,
                             const void *optval, int optlen)
{
    return lisa_modem_socket_setsockopt_impl(modem, sockfd, level, optname, optval, optlen);
}

int lisa_modem_ioctlsocket_on(lisa_modem_t *modem, int sockfd, long cmd, void *arg)
{
    return lisa_modem_socket_ioctl_impl(modem, sockfd, cmd, arg);
}

int lisa_modem_getpeername_on(lisa_modem_t *modem, int sockfd, struct sockaddr *addr, int *addrlen)
{
    return lisa_modem_socket_getpeername_impl(modem, sockfd, addr, addrlen);
}

bool lisa_modem_dns_resolve(const char *domain, char *ip_addr, size_t size)
{
    return lisa_modem_dns_resolve_on(s_default_modem, domain, ip_addr, size);
}

bool lisa_modem_get_imei(char *imei, size_t size)
{
    return lisa_modem_get_imei_on(s_default_modem, imei, size);
}

bool lisa_modem_get_iccid(char *iccid, size_t size)
{
    return lisa_modem_get_iccid_on(s_default_modem, iccid, size);
}

bool lisa_modem_get_signal_quality(int *rssi, int *ber)
{
    return lisa_modem_get_signal_quality_on(s_default_modem, rssi, ber);
}

bool lisa_modem_start_gnss(void)
{
    return lisa_modem_start_gnss_on(s_default_modem);
}

bool lisa_modem_get_gps_location(double *lat, double *lon)
{
    return lisa_modem_get_gps_location_on(s_default_modem, lat, lon);
}

int lisa_modem_socket_open(int domain, int type, int protocol)
{
    return lisa_modem_socket_open_impl(s_default_modem, domain, type, protocol);
}

bool lisa_modem_socket_connect(int sockfd, const struct sockaddr *addr, int addrlen)
{
    return lisa_modem_socket_connect_impl(s_default_modem, sockfd, addr, addrlen);
}

int lisa_modem_socket_close(int sockfd)
{
    return lisa_modem_socket_close_impl(s_default_modem, sockfd);
}

int lisa_modem_socket_send(int sockfd, const void *data, size_t length, uint32_t timeout_ms)
{
    return lisa_modem_socket_send_impl(s_default_modem, sockfd, data, length, NULL, 0, timeout_ms);
}

int lisa_modem_socket_sendto(int sockfd, const void *data, size_t length, int flags,
                             const struct sockaddr *dest_addr, int addrlen, uint32_t timeout_ms)
{
    (void)flags;
    if (!lisa_modem_validate_sockaddr_input(dest_addr, addrlen)) {
        return -1;
    }

    return lisa_modem_socket_send_impl(s_default_modem, sockfd, data, length, dest_addr, addrlen, timeout_ms);
}

int lisa_modem_socket_recv(int sockfd, void *buffer, size_t length, uint32_t timeout_ms)
{
    return lisa_modem_socket_recv_impl(s_default_modem, sockfd, buffer, length, NULL, NULL, timeout_ms);
}

int lisa_modem_socket_recvfrom(int sockfd, void *buffer, size_t length, int flags,
                               struct sockaddr *src_addr, int *addrlen, uint32_t timeout_ms)
{
    (void)flags;
    return lisa_modem_socket_recv_impl(s_default_modem, sockfd, buffer, length, src_addr, addrlen, timeout_ms);
}

int lisa_modem_setsockopt(int sockfd, int level, int optname, const void *optval, int optlen)
{
    return lisa_modem_socket_setsockopt_impl(s_default_modem, sockfd, level, optname, optval, optlen);
}

int lisa_modem_ioctlsocket(int sockfd, long cmd, void *arg)
{
    return lisa_modem_socket_ioctl_impl(s_default_modem, sockfd, cmd, arg);
}

int lisa_modem_getpeername(int sockfd, struct sockaddr *addr, int *addrlen)
{
    return lisa_modem_socket_getpeername_impl(s_default_modem, sockfd, addr, addrlen);
}

int lisa_modem_tcp_socket(bool is_ssl)
{
    int sockfd;

    sockfd = lisa_modem_socket_open(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    if (is_ssl) {
        if (lisa_modem_setsockopt(sockfd, SOL_SOCKET, SO_SSL_CONFIG, &is_ssl, (int)sizeof(is_ssl)) != 0) {
            (void)lisa_modem_socket_close(sockfd);
            return -1;
        }
    }

    return sockfd;
}

void lisa_modem_tcp_deinit(int tcp_id)
{
    (void)tcp_id;
}

bool lisa_modem_tcp_connect(int tcp_id, const char *host, int port, bool is_ssl)
{
    struct sockaddr_in addr;

    if (is_ssl && lisa_modem_setsockopt(tcp_id, SOL_SOCKET, SO_SSL_CONFIG, &is_ssl, (int)sizeof(is_ssl)) != 0) {
        return false;
    }

    if (!lisa_modem_host_to_sockaddr(s_default_modem, host, (uint16_t)port, &addr)) {
        return false;
    }

    return lisa_modem_socket_connect(tcp_id, (const struct sockaddr *)&addr, (int)sizeof(addr));
}

int lisa_modem_tcp_connect_with_addr(int tcp_id, struct sockaddr *addr, int len)
{
    return lisa_modem_socket_connect(tcp_id, addr, len) ? 0 : -1;
}

int lisa_modem_tcp_closesocket(int tcp_id)
{
    return lisa_modem_socket_close(tcp_id);
}

int lisa_modem_tcp_send(int tcp_id, const char *data, size_t length, uint32_t timeout_ms)
{
    return lisa_modem_socket_send(tcp_id, data, length, timeout_ms);
}

int lisa_modem_tcp_recv(int tcp_id, char *buffer, size_t length, uint32_t timeout_ms)
{
    return lisa_modem_socket_recv(tcp_id, buffer, length, timeout_ms);
}

int lisa_modem_udp_socket(int domain, int type, int protocol)
{
    return lisa_modem_socket_open(domain, type, protocol);
}

void lisa_modem_udp_deinit(int udp_id)
{
    (void)udp_id;
}

int lisa_modem_udp_closesocket(int udp_id)
{
    return lisa_modem_socket_close(udp_id);
}

int lisa_modem_udp_sendto(int udp_id, const char *data, size_t length, int flags,
                          const struct sockaddr *dest_addr, int addrlen)
{
    return lisa_modem_socket_sendto(udp_id, data, length, flags, dest_addr, addrlen,
                                    MODEM_COMPAT_DEFAULT_TIMEOUT_MS);
}

int lisa_modem_udp_recvform(int udp_id, char *buffer, size_t length, int flags,
                            struct sockaddr *src_addr, int *addrlen)
{
    return lisa_modem_socket_recvfrom(udp_id, buffer, length, flags, src_addr, addrlen,
                                      MODEM_COMPAT_DEFAULT_TIMEOUT_MS);
}

bool lisa_modem_module_init(const char *uart_dev)
{
    if (s_default_modem) {
        return true;
    }

    s_default_modem = lisa_modem_create_uart(uart_dev, 0);
    if (!s_default_modem) {
        return false;
    }

    if (lisa_modem_register_default_netdev(s_default_modem->name) != 0) {
        s_default_modem->status.last_error = LISA_MODEM_ERR_NETDEV_REGISTER_FAILED;
        lisa_modem_publish_status(s_default_modem, &s_default_modem->status);
        lisa_modem_destroy(s_default_modem);
        s_default_modem = NULL;
        return false;
    }

    return true;
}

bool lisa_modem_module_deinit(void)
{
    if (!s_default_modem) {
        return false;
    }

    lisa_modem_destroy(s_default_modem);
    s_default_modem = NULL;
    return true;
}
