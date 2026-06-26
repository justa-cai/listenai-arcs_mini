/**
 * @file modem_driver_ops.h
 * @brief Generic AT modem driver ops
 */

#ifndef LISA_MODEM_CORE_MODEM_DRIVER_OPS_H
#define LISA_MODEM_CORE_MODEM_DRIVER_OPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include "at_client.h"
#include "lisa_modem_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct modem_dispatcher modem_dispatcher_t;

typedef struct {
    sa_family_t family;
    char host[64];
    uint16_t port;
} modem_addr_t;

typedef enum {
    MODEM_RX_MODE_UNKNOWN = 0,
    MODEM_RX_MODE_URC_WITH_DATA,
    MODEM_RX_MODE_URC_DATA_READY,
    MODEM_RX_MODE_HYBRID,
} modem_rx_mode_t;

typedef enum {
    MODEM_TX_MODE_UNKNOWN = 0,
    MODEM_TX_MODE_PROMPT_RAW,
    MODEM_TX_MODE_INLINE_ENCODED,
} modem_tx_mode_t;

typedef struct {
    uint8_t max_bearers;
    uint8_t max_endpoints;
    uint8_t max_tls_profiles;
    uint16_t max_tx_chunk;
    uint16_t max_rx_pull;
    modem_rx_mode_t rx_mode;
    modem_tx_mode_t tx_mode;
    bool supports_listener;
    bool supports_udp_service;
    bool supports_tls_profile_reuse;
    bool supports_sendto_without_reconnect;
    bool supports_access_mode_cached;
    bool supports_access_mode_direct;
    bool supports_access_mode_transparent;
    bool supports_service_http;
    bool supports_service_mqtt;
    bool supports_ipv6;
    bool supports_multi_bearer;
    bool needs_connect_before_udp_send;
    bool supports_sleep_urc;
} modem_caps_t;

#define MODEM_PROBE_TEXT_LEN 48

typedef struct {
    char driver_name[MODEM_PROBE_TEXT_LEN];
    char manufacturer[MODEM_PROBE_TEXT_LEN];
    char model[MODEM_PROBE_TEXT_LEN];
    char revision[MODEM_PROBE_TEXT_LEN];
    uint8_t match_score;
} modem_probe_result_t;

typedef struct modem_driver_ops {
    const char *name;
    modem_caps_t caps;

    bool (*probe)(at_client_t *client, modem_probe_result_t *result);
    void *(*create)(at_client_t *client);
    void (*destroy)(void *driver_ctx);
    bool (*init)(void *driver_ctx);
    void (*deinit)(void *driver_ctx);
    void (*attach_dispatcher)(void *driver_ctx, modem_dispatcher_t *dispatcher);
    void (*bind_socket)(void *driver_ctx, int driver_endpoint_id, int sockfd, uint32_t generation);
    void (*get_status)(void *driver_ctx, lisa_modem_status_t *status);

    bool (*dns_resolve)(void *driver_ctx, const char *domain, char *ip_addr, size_t size);
    bool (*get_imei)(void *driver_ctx, char *imei, size_t size);
    bool (*get_iccid)(void *driver_ctx, char *iccid, size_t size);
    bool (*get_signal_quality)(void *driver_ctx, int *rssi, int *ber);

    int (*open_fn)(void *driver_ctx, int domain, int protocol);
    int (*connect_fn)(void *driver_ctx, int driver_endpoint_id, const modem_addr_t *addr);
    int (*close_fn)(void *driver_ctx, int driver_endpoint_id);
    int (*send_fn)(void *driver_ctx, int driver_endpoint_id, const void *data, size_t length,
                   uint32_t timeout_ms, const modem_addr_t *to);
    int (*send_chunk)(void *driver_ctx, int driver_endpoint_id, const void *data, size_t length,
                      uint32_t timeout_ms, const modem_addr_t *to);
    int (*recv_fn)(void *driver_ctx, int driver_endpoint_id, void *buffer, size_t length,
                   uint32_t timeout_ms, modem_addr_t *from);
    int (*pull_rx)(void *driver_ctx, int driver_endpoint_id);
    int (*set_timeout)(void *driver_ctx, int driver_endpoint_id, bool is_send, uint32_t timeout_ms);
    int (*set_nonblock)(void *driver_ctx, int driver_endpoint_id, bool nonblock);
    int (*set_tls)(void *driver_ctx, int driver_endpoint_id, bool enabled);
} modem_driver_ops_t;

#ifdef __cplusplus
}
#endif

#endif
