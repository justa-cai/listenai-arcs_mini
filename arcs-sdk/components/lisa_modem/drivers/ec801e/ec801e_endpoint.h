/**
 * @file ec801e_endpoint.h
 * @brief Private EC801E endpoint context and endpoint operations
 */

#ifndef LISA_MODEM_DRIVERS_EC801E_ENDPOINT_H
#define LISA_MODEM_DRIVERS_EC801E_ENDPOINT_H

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

#define EC801E_MAX_ENDPOINTS              6
#define EC801E_CONNECT_TIMEOUT_MS         150000U
#define EC801E_SEND_TIMEOUT_MS            5000U
#define EC801E_DEFAULT_RECV_TIMEOUT_MS    1500U
#define EC801E_QIRD_TCP_CHUNK_SIZE        1460U
#define EC801E_QIRD_HEX_BUFFER_SIZE       ((EC801E_QIRD_TCP_CHUNK_SIZE * 2U) + 16U)
#define EC801E_ENDPOINT_RECV_BUFFER_SIZE  (1024U * 16U)
#define EC801E_DEFAULT_UDP_LOCAL_PORT     40000U
#define EC801E_DATA_FORMAT_TEXT           0
#define EC801E_DATA_FORMAT_HEX            1

#ifndef EC801E_SEND_DATA_FORMAT
#define EC801E_SEND_DATA_FORMAT           EC801E_DATA_FORMAT_TEXT
#endif

#ifndef EC801E_RECV_DATA_FORMAT
#define EC801E_RECV_DATA_FORMAT           EC801E_DATA_FORMAT_TEXT
#endif

#if (EC801E_SEND_DATA_FORMAT != EC801E_DATA_FORMAT_TEXT) && \
    (EC801E_SEND_DATA_FORMAT != EC801E_DATA_FORMAT_HEX)
#error "Unsupported EC801E_SEND_DATA_FORMAT"
#endif

#if (EC801E_RECV_DATA_FORMAT != EC801E_DATA_FORMAT_TEXT) && \
    (EC801E_RECV_DATA_FORMAT != EC801E_DATA_FORMAT_HEX)
#error "Unsupported EC801E_RECV_DATA_FORMAT"
#endif

#ifndef EC801E_DEFAULT_PULL_TIMEOUT_MS
#if EC801E_RECV_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
#define EC801E_DEFAULT_PULL_TIMEOUT_MS    120U
#else
#define EC801E_DEFAULT_PULL_TIMEOUT_MS    50U
#endif
#endif

#define EC801E_STRINGIFY_VALUE(x)          #x
#define EC801E_STRINGIFY(x)                EC801E_STRINGIFY_VALUE(x)
#define EC801E_QICFG_DATAFORMAT_CMD       "AT+QICFG=\"dataformat\"," \
                                           EC801E_STRINGIFY(EC801E_SEND_DATA_FORMAT) "," \
                                           EC801E_STRINGIFY(EC801E_RECV_DATA_FORMAT)
#define EC801E_QICFG_DATAFORMAT_EXPECT    "\"dataformat\"," \
                                           EC801E_STRINGIFY(EC801E_SEND_DATA_FORMAT) "," \
                                           EC801E_STRINGIFY(EC801E_RECV_DATA_FORMAT)

typedef struct ec801e_endpoint_ctx ec801e_endpoint_ctx_t;

typedef struct {
    bool valid;
    modem_addr_t addr;
} ec801e_udp_peer_t;

typedef struct {
    bool valid;
    size_t data_len;
    ec801e_udp_peer_t source;
} ec801e_qird_result_t;

typedef struct ec801e_endpoint {
    int id;
    int protocol;
    bool in_use;
    bool connected;
    bool instance_active;
    bool initialized;
    bool blocking;
    bool is_tls;
    bool data_pending;
    int last_error;
    int last_qisend_status;
    uint32_t send_timeout_ms;
    uint32_t recv_timeout_ms;
    uint32_t pull_timeout_ms;
    uint32_t generation;
    int public_sockfd;
    uint16_t local_port;
    ec801e_udp_peer_t udp_peer;
    ec801e_udp_peer_t udp_last_source;
    ec801e_qird_result_t pending_qird;

    at_client_t *client;
    ec801e_endpoint_ctx_t *ctx;
    SemaphoreHandle_t data_sem;

    struct ring_buf ring_buf;
    uint8_t recv_buffer_data[EC801E_ENDPOINT_RECV_BUFFER_SIZE];
    modem_endpoint_runtime_t runtime;
} ec801e_endpoint_t;

ec801e_endpoint_ctx_t *ec801e_endpoint_create(at_client_t *client);
bool ec801e_endpoint_init(ec801e_endpoint_ctx_t *ctx);
void ec801e_endpoint_shutdown(ec801e_endpoint_ctx_t *ctx);
void ec801e_endpoint_destroy(ec801e_endpoint_ctx_t *ctx);
bool ec801e_endpoint_dns_resolve(ec801e_endpoint_ctx_t *ctx, const char *domain, char *ip_addr, size_t size);
bool ec801e_endpoint_get_imei(ec801e_endpoint_ctx_t *ctx, char *imei, size_t size);
bool ec801e_endpoint_get_iccid(ec801e_endpoint_ctx_t *ctx, char *iccid, size_t size);
bool ec801e_endpoint_get_signal_quality(ec801e_endpoint_ctx_t *ctx, int *rssi, int *ber);

int ec801e_endpoint_open(ec801e_endpoint_ctx_t *ctx, int domain, int protocol);
bool ec801e_endpoint_connect(ec801e_endpoint_ctx_t *ctx, int endpoint_id, const char *host, uint16_t port);
int ec801e_endpoint_close(ec801e_endpoint_ctx_t *ctx, int endpoint_id);
int ec801e_endpoint_send(ec801e_endpoint_ctx_t *ctx, int endpoint_id,
                         const void *data, size_t length, uint32_t timeout_ms,
                         const char *host, uint16_t port);
int ec801e_endpoint_recv(ec801e_endpoint_ctx_t *ctx, int endpoint_id,
                         void *buffer, size_t length, uint32_t timeout_ms,
                         modem_addr_t *from);
int ec801e_endpoint_set_timeout(ec801e_endpoint_ctx_t *ctx, int endpoint_id,
                                bool is_send, uint32_t timeout_ms);
int ec801e_endpoint_set_nonblock(ec801e_endpoint_ctx_t *ctx, int endpoint_id, bool nonblock);
int ec801e_endpoint_set_tls(ec801e_endpoint_ctx_t *ctx, int endpoint_id, bool enabled);
const modem_driver_ops_t *ec801e_endpoint_get_ops(void);

#ifdef __cplusplus
}
#endif

#endif
