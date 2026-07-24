/**
 * @file ml307_endpoint.h
 * @brief Private ML307 endpoint context and endpoint operations
 */

#ifndef LISA_MODEM_DRIVERS_ML307_ENDPOINT_H
#define LISA_MODEM_DRIVERS_ML307_ENDPOINT_H

#include "at_client.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"
#include "ring_buffer.h"
#include "drivers/common/modem_endpoint_runtime.h"
#include "core/modem_bits.h"
#include "core/modem_driver_ops.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ML307_MAX_ENDPOINTS            5
#define ML307_CONNECT_TIMEOUT_MS       10000
#define ML307_MIPOPEN_TIMEOUT_SECONDS  (ML307_CONNECT_TIMEOUT_MS / 1000U)
#define ML307_MIPOPEN_WAIT_TIMEOUT_MS  (ML307_CONNECT_TIMEOUT_MS + 5000U)
#define ML307_SEND_TIMEOUT_MS          5000
#define ML307_UDP_MAX_PACKET_SIZE      730
#define ML307_DATA_FORMAT_TEXT         0
#define ML307_DATA_FORMAT_HEX          1

#ifndef ML307_SEND_DATA_FORMAT
#define ML307_SEND_DATA_FORMAT         ML307_DATA_FORMAT_TEXT
#endif

#ifndef ML307_RECV_DATA_FORMAT
#define ML307_RECV_DATA_FORMAT         ML307_DATA_FORMAT_TEXT
#endif

#if (ML307_SEND_DATA_FORMAT != ML307_DATA_FORMAT_TEXT) && \
    (ML307_SEND_DATA_FORMAT != ML307_DATA_FORMAT_HEX)
#error "Unsupported ML307_SEND_DATA_FORMAT"
#endif

#if (ML307_RECV_DATA_FORMAT != ML307_DATA_FORMAT_TEXT) && \
    (ML307_RECV_DATA_FORMAT != ML307_DATA_FORMAT_HEX)
#error "Unsupported ML307_RECV_DATA_FORMAT"
#endif

#define ML307_STRINGIFY_VALUE(x)       #x
#define ML307_STRINGIFY(x)             ML307_STRINGIFY_VALUE(x)
/*
 * One TCP cached-mode MIPRD can fetch up to 4096 bytes. Keep enough local RX
 * space for a small dispatcher burst so high-backlog sockets do not stall
 * after every single pull.
 */
#define ML307_ENDPOINT_RECV_BUFFER_SIZE (1024 * 32)

#define ML307_ENDPOINT_CONNECTED          BIT0
#define ML307_ENDPOINT_DISCONNECTED       BIT1
#define ML307_ENDPOINT_ERROR              BIT2
#define ML307_ENDPOINT_SEND_COMPLETE      BIT3
#define ML307_ENDPOINT_INITIALIZED        BIT4
#define ML307_ENDPOINT_PREFETCH_AVAILABLE BIT7

typedef struct ml307_endpoint_ctx ml307_endpoint_ctx_t;

typedef struct {
    bool valid;
    modem_addr_t addr;
} ml307_udp_peer_t;

typedef struct ml307_endpoint {
    int id;
    int protocol;
    bool in_use;
    bool is_tls;
    bool connected;
    bool connecting;
    bool instance_active;
    bool initialized;
    bool blocking;
    volatile bool prefetching;      /**< True while pull_rx/prefetch is running */
    int last_error;
    int last_sent_bytes;
    uint32_t send_timeout_ms;
    uint32_t recv_timeout_ms;
    uint32_t pull_timeout_ms;
    uint32_t generation;
    int public_sockfd;
    ml307_udp_peer_t udp_peer;
    ml307_udp_peer_t udp_last_source;

    at_client_t *client;
    ml307_endpoint_ctx_t *ctx;
    EventGroupHandle_t event_group;
    EventGroupHandle_t recv_event;
    SemaphoreHandle_t data_sem;
    SemaphoreHandle_t prefetch_mutex;

    struct ring_buf ring_buf;
    uint8_t recv_buffer_data[ML307_ENDPOINT_RECV_BUFFER_SIZE];
    modem_endpoint_runtime_t runtime;
    union {
        size_t available_data_len;
        size_t unread_packet_count;
    } rx_hint;
    TickType_t perf_last_rx_push_tick;
    TickType_t perf_last_recv_ok_tick;
    TickType_t perf_prefetch_first_payload_tick;
    TickType_t perf_prefetch_first_push_tick;
    TickType_t perf_prefetch_last_push_tick;
    TickType_t perf_prefetch_line_done_tick;
    TickType_t perf_rx_rate_window_start_tick;
    uint32_t perf_last_push_bytes;
    uint32_t perf_rx_push_seq;
    uint32_t perf_rx_push_total_bytes;
    uint32_t perf_rx_rate_window_bytes;
    uint32_t perf_rx_rate_last_1s_bps;
    uint32_t perf_empty_prefetch_streak;
    uint32_t perf_consecutive_recv_timeouts;
    uint32_t perf_consecutive_partial_reads;
} ml307_endpoint_t;

ml307_endpoint_ctx_t *ml307_endpoint_create(at_client_t *client);
bool ml307_endpoint_init(ml307_endpoint_ctx_t *ctx);
void ml307_endpoint_shutdown(ml307_endpoint_ctx_t *ctx);
void ml307_endpoint_destroy(ml307_endpoint_ctx_t *ctx);
bool ml307_endpoint_dns_resolve(ml307_endpoint_ctx_t *ctx, const char *domain, char *ip_addr, size_t size);
bool ml307_endpoint_get_imei(ml307_endpoint_ctx_t *ctx, char *imei, size_t size);
bool ml307_endpoint_get_iccid(ml307_endpoint_ctx_t *ctx, char *iccid, size_t size);
bool ml307_endpoint_get_signal_quality(ml307_endpoint_ctx_t *ctx, int *rssi, int *ber);
bool ml307_endpoint_start_gnss(ml307_endpoint_ctx_t *ctx);
bool ml307_endpoint_get_gps_location(ml307_endpoint_ctx_t *ctx, double *lat, double *lon);

int ml307_endpoint_open(ml307_endpoint_ctx_t *ctx, int domain, int protocol);
bool ml307_endpoint_connect(ml307_endpoint_ctx_t *ctx, int endpoint_id, const char *host, uint16_t port);
int ml307_endpoint_close(ml307_endpoint_ctx_t *ctx, int endpoint_id);
int ml307_endpoint_send(ml307_endpoint_ctx_t *ctx, int endpoint_id,
                        const void *data, size_t length, uint32_t timeout_ms,
                        const char *host, uint16_t port);
int ml307_endpoint_recv(ml307_endpoint_ctx_t *ctx, int endpoint_id,
                        void *buffer, size_t length, uint32_t timeout_ms,
                        modem_addr_t *from);
int ml307_endpoint_set_timeout(ml307_endpoint_ctx_t *ctx, int endpoint_id,
                               bool is_send, uint32_t timeout_ms);
int ml307_endpoint_set_nonblock(ml307_endpoint_ctx_t *ctx, int endpoint_id, bool nonblock);
int ml307_endpoint_set_tls(ml307_endpoint_ctx_t *ctx, int endpoint_id, bool enabled);
const modem_driver_ops_t *ml307_endpoint_get_ops(void);

#ifdef __cplusplus
}
#endif

#endif
