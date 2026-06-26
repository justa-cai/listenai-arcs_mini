/**
 * @file ml307_endpoint_internal.h
 * @brief Private ML307 endpoint internals
 */

#ifndef LISA_MODEM_DRIVERS_ML307_ENDPOINT_INTERNAL_H
#define LISA_MODEM_DRIVERS_ML307_ENDPOINT_INTERNAL_H

#include "drivers/common/modem_runtime_common.h"
#include "drivers/common/modem_dispatcher.h"
#include "drivers/ml307/ml307_endpoint.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef modem_runtime_wait_hook_t ml307_endpoint_wait_hook_t;

typedef enum {
    ML307_LINE_STREAM_NONE = 0,
    ML307_LINE_STREAM_MIPRD,
    ML307_LINE_STREAM_MIPURC_RTCP,
} ml307_line_stream_kind_t;

typedef struct {
    ml307_line_stream_kind_t kind;
    ml307_endpoint_t *endpoint;
    size_t field_index;
    char token[32];
    size_t token_len;
    bool parsing_payload;
    bool final_field_payload;
    bool final_field_digits_only;
    size_t final_field_len;
    int rtcp_declared_len;
    size_t payload_declared_len;
    size_t payload_received_len;
    bool payload_len_known;
    size_t tcp_remaining_len;
    bool tcp_remaining_len_known;
    ml307_udp_peer_t udp_source;
    char udp_source_host[sizeof(((modem_addr_t *)0)->host)];
    bool udp_source_has_host;
    uint8_t pending_hex_nibble;
    bool has_pending_hex_nibble;
    uint8_t decode_buf[4096];
    size_t decode_buf_len;
} ml307_line_stream_state_t;

struct ml307_endpoint_ctx {
    at_client_t *client;
    bool initialized;
    lisa_modem_status_t status;
    modem_network_status_t network_status;
    bool network_ready;
    uint8_t active_pdp_cid;
    char ip_address[16];

    EventGroupHandle_t event_group;
    at_urc_callback_node_t *urc_node;
    SemaphoreHandle_t dns_mutex;
    modem_dispatcher_t *dispatcher;
    ml307_line_stream_state_t line_stream;

    ml307_endpoint_t endpoints[ML307_MAX_ENDPOINTS];
};

#ifdef __cplusplus
}
#endif

#endif
