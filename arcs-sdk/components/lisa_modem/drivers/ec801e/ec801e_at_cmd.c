/**
 * @file ec801e_at_cmd.c
 * @brief EC801E AT transaction helpers
 */

#include "ec801e_at_cmd.h"
#include "ec801e_endpoint_internal.h"
#include "at_mem.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "task.h"

#define TAG "ec801e_at"
#include "lisa_log.h"

#define EC801E_TCP_SEND_CHUNK_SIZE 1460U

static void ec801e_at_cmd_schedule_prefetch(ec801e_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->ctx || !endpoint->ctx->dispatcher ||
        endpoint->public_sockfd < 0 || endpoint->generation == 0U) {
        return;
    }
    if (!modem_endpoint_runtime_should_pull(&endpoint->runtime)) {
        return;
    }

    (void)modem_dispatcher_mark_rx_ready(endpoint->ctx->dispatcher,
                                         endpoint->public_sockfd,
                                         endpoint->generation);
}

static void ec801e_at_cmd_refresh_pending(ec801e_endpoint_t *endpoint)
{
    if (!endpoint) {
        return;
    }

    modem_endpoint_runtime_mark_pending(&endpoint->runtime, endpoint->data_pending);
    ec801e_at_cmd_schedule_prefetch(endpoint);
}

typedef struct {
    int connect_id;
    int err;
} qioopen_out_t;

static ec801e_endpoint_t *ec801e_find_endpoint(ec801e_endpoint_ctx_t *ctx, int endpoint_id)
{
    if (!ctx) {
        return NULL;
    }

    for (int i = 0; i < EC801E_MAX_ENDPOINTS; ++i) {
        if (ctx->endpoints[i].in_use && ctx->endpoints[i].id == endpoint_id) {
            return &ctx->endpoints[i];
        }
    }

    return NULL;
}

static void ec801e_udp_peer_clear(ec801e_udp_peer_t *peer)
{
    if (peer) {
        memset(peer, 0, sizeof(*peer));
    }
}

static void ec801e_udp_peer_set(ec801e_udp_peer_t *peer, const char *host, uint16_t port)
{
    if (!peer) {
        return;
    }

    ec801e_udp_peer_clear(peer);
    if (!host) {
        return;
    }

    peer->valid = true;
    peer->addr.family = AF_INET;
    strncpy(peer->addr.host, host, sizeof(peer->addr.host) - 1);
    peer->addr.host[sizeof(peer->addr.host) - 1] = '\0';
    peer->addr.port = port;
}

static void ec801e_at_cmd_push_rx(ec801e_endpoint_t *endpoint, const char *data, size_t len)
{
    if (!endpoint || !data || len == 0) {
        return;
    }

    (void)modem_endpoint_runtime_write_rx(&endpoint->runtime, (const uint8_t *)data, (uint32_t)len);
    if (endpoint->data_sem) {
        xSemaphoreGive(endpoint->data_sem);
    }
}

static bool parse_qiopen(at_arg_value_t *args, size_t count, void *user_data)
{
    qioopen_out_t *out = (qioopen_out_t *)user_data;

    if (!out || count < 2 || args[0].type != AT_ARG_TYPE_INT || args[1].type != AT_ARG_TYPE_INT) {
        return false;
    }

    out->connect_id = args[0].data.int_val;
    out->err = args[1].data.int_val;
    return true;
}

static int ec801e_qird_decode_and_push(ec801e_endpoint_t *endpoint, const char *hex_line)
{
    char *decoded;
    size_t decoded_len = 0;

    if (!endpoint || !hex_line || endpoint->pending_qird.data_len == 0) {
        return 0;
    }

    decoded = at_decode_hex(hex_line, strlen(hex_line), &decoded_len);
    if (!decoded) {
        return -1;
    }

    if (endpoint->pending_qird.source.valid) {
        endpoint->udp_last_source = endpoint->pending_qird.source;
    }

    ec801e_at_cmd_push_rx(endpoint, decoded, decoded_len);
    at_mem_free(decoded);
    return (int)decoded_len;
}

static int ec801e_qird_push_text(ec801e_endpoint_t *endpoint, const uint8_t *data, size_t len)
{
    if (!endpoint || !data || len == 0U) {
        return 0;
    }

    if (endpoint->pending_qird.source.valid) {
        endpoint->udp_last_source = endpoint->pending_qird.source;
    }

    ec801e_at_cmd_push_rx(endpoint, (const char *)data, len);
    return (int)len;
}

static void ec801e_at_cmd_handle_qiurc(ec801e_endpoint_ctx_t *ctx, at_arg_value_t *arguments, size_t arg_count)
{
    const char *urc_type;
    ec801e_endpoint_t *endpoint;

    if (!ctx || arg_count < 1 ||
        arguments[0].type != AT_ARG_TYPE_STRING || !arguments[0].data.string_val.value) {
        return;
    }

    urc_type = arguments[0].data.string_val.value;
    if ((strcmp(urc_type, "recv") == 0 || strcmp(urc_type, "closed") == 0) &&
        arg_count >= 2 && arguments[1].type == AT_ARG_TYPE_INT) {
        endpoint = ec801e_find_endpoint(ctx, arguments[1].data.int_val);
        if (!endpoint) {
            return;
        }

        if (strcmp(urc_type, "recv") == 0) {
            endpoint->data_pending = true;
            ec801e_at_cmd_refresh_pending(endpoint);
        } else {
            endpoint->connected = false;
            endpoint->instance_active = false;
            endpoint->data_pending = false;
            endpoint->pending_qird.valid = false;
            ec801e_at_cmd_refresh_pending(endpoint);
        }
    }
}

static void ec801e_at_cmd_handle_qiopen(ec801e_endpoint_ctx_t *ctx, at_arg_value_t *arguments, size_t arg_count)
{
    ec801e_endpoint_t *endpoint;
    int result;

    if (!ctx || arg_count < 2 || arguments[0].type != AT_ARG_TYPE_INT || arguments[1].type != AT_ARG_TYPE_INT) {
        return;
    }

    endpoint = ec801e_find_endpoint(ctx, arguments[0].data.int_val);
    if (!endpoint) {
        return;
    }

    result = arguments[1].data.int_val;
    endpoint->last_error = result;
    endpoint->instance_active = (result == 0);
    endpoint->connected = (result == 0);
}

static void ec801e_at_cmd_handle_qisend(ec801e_endpoint_ctx_t *ctx, at_arg_value_t *arguments, size_t arg_count)
{
    ec801e_endpoint_t *endpoint;

    if (!ctx || arg_count < 2 || arguments[0].type != AT_ARG_TYPE_INT || arguments[1].type != AT_ARG_TYPE_INT) {
        return;
    }

    endpoint = ec801e_find_endpoint(ctx, arguments[0].data.int_val);
    if (!endpoint) {
        return;
    }

    endpoint->last_qisend_status = arguments[1].data.int_val;
}

static void ec801e_at_cmd_handle_qird(ec801e_endpoint_ctx_t *ctx, at_arg_value_t *arguments, size_t arg_count)
{
    ec801e_endpoint_t *endpoint;

    if (!ctx || arg_count < 1 || arguments[0].type != AT_ARG_TYPE_INT) {
        return;
    }

    for (int i = 0; i < EC801E_MAX_ENDPOINTS; ++i) {
        endpoint = &ctx->endpoints[i];
        if (!endpoint->in_use || !endpoint->initialized || !endpoint->pending_qird.valid) {
            continue;
        }

        endpoint->pending_qird.data_len = (size_t)arguments[0].data.int_val;
        ec801e_udp_peer_clear(&endpoint->pending_qird.source);

        if (endpoint->protocol == IPPROTO_UDP &&
            arg_count >= 3 &&
            arguments[1].type == AT_ARG_TYPE_STRING &&
            arguments[1].data.string_val.value &&
            arguments[2].type == AT_ARG_TYPE_INT) {
            ec801e_udp_peer_set(&endpoint->pending_qird.source,
                                arguments[1].data.string_val.value,
                                (uint16_t)arguments[2].data.int_val);
        }
        return;
    }
}

void ec801e_at_cmd_handle_urc(ec801e_endpoint_ctx_t *ctx, const char *command,
                              at_arg_value_t *arguments, size_t arg_count)
{
    if (!ctx || !command) {
        return;
    }

    if (strcmp(command, "QIURC") == 0) {
        ec801e_at_cmd_handle_qiurc(ctx, arguments, arg_count);
    } else if (strcmp(command, "QIOPEN") == 0) {
        ec801e_at_cmd_handle_qiopen(ctx, arguments, arg_count);
    } else if (strcmp(command, "QISEND") == 0) {
        ec801e_at_cmd_handle_qisend(ctx, arguments, arg_count);
    } else if (strcmp(command, "QIRD") == 0) {
        ec801e_at_cmd_handle_qird(ctx, arguments, arg_count);
    }
}

bool ec801e_at_cmd_connect(ec801e_endpoint_t *endpoint, const char *host, uint16_t port)
{
    qioopen_out_t out = { -1, -1 };

    if (!endpoint || !endpoint->client || !endpoint->ctx || !host) {
        return false;
    }

    if (endpoint->protocol == IPPROTO_UDP) {
        if (!ec801e_at_cmd_open_udp_service(endpoint)) {
            return false;
        }

        ec801e_udp_peer_set(&endpoint->udp_peer, host, port);
        endpoint->connected = true;
        return true;
    }

    if (endpoint->instance_active) {
        (void)ec801e_at_cmd_disconnect(endpoint);
    }

    out.connect_id = -1;
    out.err = -1;
    endpoint->last_error = 0;
    endpoint->data_pending = false;

    if (!at_client_exec_cmdf(endpoint->client, &(at_cmd_desc_t){
            .cmd = "AT+QIOPEN=%d,%d,\"TCP\",\"%s\",%u,0,0",
            .expect_urc = "QIOPEN",
            .parse = parse_qiopen,
            .timeout_ms = EC801E_CONNECT_TIMEOUT_MS,
        }, &out,
        endpoint->ctx->active_pdp_cid > 0 ? endpoint->ctx->active_pdp_cid : 1,
        endpoint->id, host, (unsigned int)port)) {
        endpoint->last_error = at_client_get_cme_error(endpoint->client);
        return false;
    }

    endpoint->last_error = out.err;
    endpoint->instance_active = (out.connect_id == endpoint->id && out.err == 0);
    endpoint->connected = endpoint->instance_active;
    endpoint->data_pending = false;
    ec801e_at_cmd_refresh_pending(endpoint);
    return endpoint->connected;
}

bool ec801e_at_cmd_open_udp_service(ec801e_endpoint_t *endpoint)
{
    qioopen_out_t out = { -1, -1 };

    if (!endpoint || !endpoint->client || !endpoint->ctx) {
        return false;
    }

    if (endpoint->instance_active) {
        return true;
    }

    if (endpoint->local_port == 0) {
        endpoint->local_port = (uint16_t)(EC801E_DEFAULT_UDP_LOCAL_PORT + endpoint->id);
    }

    if (!at_client_exec_cmdf(endpoint->client, &(at_cmd_desc_t){
            .cmd = "AT+QIOPEN=%d,%d,\"UDP SERVICE\",\"127.0.0.1\",0,%u,0",
            .expect_urc = "QIOPEN",
            .parse = parse_qiopen,
            .timeout_ms = EC801E_CONNECT_TIMEOUT_MS,
        }, &out,
        endpoint->ctx->active_pdp_cid > 0 ? endpoint->ctx->active_pdp_cid : 1,
        endpoint->id, (unsigned int)endpoint->local_port)) {
        endpoint->last_error = at_client_get_cme_error(endpoint->client);
        return false;
    }

    endpoint->last_error = out.err;
    endpoint->instance_active = (out.connect_id == endpoint->id && out.err == 0);
    endpoint->connected = endpoint->instance_active;
    endpoint->data_pending = false;
    ec801e_at_cmd_refresh_pending(endpoint);
    return endpoint->instance_active;
}

#define EC801E_TCP_SEND_PAYLOAD_CHUNK_SIZE EC801E_TCP_SEND_CHUNK_SIZE

int ec801e_at_cmd_disconnect(ec801e_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->client) {
        return -1;
    }

    if (endpoint->instance_active) {
        (void)at_client_exec_cmdf(endpoint->client, &(at_cmd_desc_t){
            .cmd = "AT+QICLOSE=%d",
            .timeout_ms = 10000,
        }, NULL, endpoint->id);
    }

    endpoint->connected = false;
    endpoint->instance_active = false;
    endpoint->last_qisend_status = -1;
    ec801e_at_cmd_refresh_pending(endpoint);
    return 0;
}

int ec801e_at_cmd_send(ec801e_endpoint_t *endpoint, const void *data, size_t length,
                       const char *host, uint16_t port)
{
    size_t total_sent = 0U;

    if (!endpoint || !endpoint->client || !data || length == 0U) {
        errno = EINVAL;
        return -1;
    }

    while (total_sent < length) {
        size_t chunk_len = length - total_sent;
        int ret;

        if (chunk_len > EC801E_TCP_SEND_PAYLOAD_CHUNK_SIZE) {
            chunk_len = EC801E_TCP_SEND_PAYLOAD_CHUNK_SIZE;
        }

        ret = ec801e_at_cmd_send_chunk(endpoint,
                                       (const uint8_t *)data + total_sent,
                                       chunk_len,
                                       host, port);
        if (ret <= 0) {
            return (total_sent > 0U) ? (int)total_sent : -1;
        }
        total_sent += (size_t)ret;

        if (total_sent < length) {
            vTaskDelay(1);
        }
    }

    return (int)total_sent;
}

int ec801e_at_cmd_send_chunk(ec801e_endpoint_t *endpoint, const void *data, size_t length,
                             const char *host, uint16_t port)
{
    char command[128];
    const uint8_t *send_data = (const uint8_t *)data;
    size_t send_data_len = length;
#if EC801E_SEND_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
    char *hex_data = NULL;
    size_t hex_len = 0U;
#endif
    bool ok;

    if (!endpoint || !endpoint->client || !data || length == 0U ||
        length > EC801E_TCP_SEND_PAYLOAD_CHUNK_SIZE) {
        errno = EINVAL;
        return -1;
    }

#if EC801E_SEND_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
    hex_data = at_encode_hex((const char *)data, length, &hex_len);
    if (!hex_data) {
        errno = ENOMEM;
        return -1;
    }
    send_data = (const uint8_t *)hex_data;
    send_data_len = hex_len;
#endif

    if (endpoint->protocol == IPPROTO_TCP) {
        if (!endpoint->connected) {
#if EC801E_SEND_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
            at_mem_free(hex_data);
#endif
            errno = ENOTCONN;
            return -1;
        }

        snprintf(command, sizeof(command), "AT+QISEND=%d,%u",
                 endpoint->id, (unsigned int)length);
    } else {
        if (!ec801e_at_cmd_open_udp_service(endpoint)) {
#if EC801E_SEND_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
            at_mem_free(hex_data);
#endif
            return -1;
        }
        if (!host) {
#if EC801E_SEND_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
            at_mem_free(hex_data);
#endif
            errno = ENOTCONN;
            return -1;
        }

        snprintf(command, sizeof(command), "AT+QISEND=%d,%u,\"%s\",%u",
                 endpoint->id, (unsigned int)length, host, (unsigned int)port);
    }

    endpoint->last_qisend_status = -1;
    ok = at_client_send_cmd_with_data(endpoint->client, command,
                                      endpoint->send_timeout_ms > 0 ? endpoint->send_timeout_ms : EC801E_SEND_TIMEOUT_MS,
                                      true, send_data, send_data_len);
#if EC801E_SEND_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
    at_mem_free(hex_data);
#endif
    if (!ok) {
        endpoint->last_error = at_client_get_cme_error(endpoint->client);
        errno = EIO;
        return -1;
    }
    if (endpoint->last_qisend_status > 0) {
        errno = EAGAIN;
        return -1;
    }

    endpoint->instance_active = true;
    if (endpoint->protocol == IPPROTO_TCP) {
        endpoint->connected = true;
    }
    return (int)length;
}

int ec801e_at_cmd_prefetch(ec801e_endpoint_t *endpoint)
{
    char command[64];
#if EC801E_RECV_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
    char rx_line[EC801E_QIRD_HEX_BUFFER_SIZE] = {0};
#else
    uint8_t rx_data[EC801E_QIRD_TCP_CHUNK_SIZE] = {0};
    size_t rx_len = 0U;
#endif
    uint32_t max_space;
    uint32_t prefetch_timeout_ms;
    size_t requested_len;

    if (!endpoint || !endpoint->initialized) {
        return 0;
    }

    max_space = ring_buf_space_get(&endpoint->ring_buf);
    if (max_space == 0) {
        return 0;
    }

    prefetch_timeout_ms = modem_endpoint_runtime_effective_pull_timeout(endpoint->pull_timeout_ms,
                                                                        100U,
                                                                        50U);

    endpoint->pending_qird.valid = true;
    endpoint->pending_qird.data_len = 0;
    ec801e_udp_peer_clear(&endpoint->pending_qird.source);

    if (endpoint->protocol == IPPROTO_TCP) {
        if (!endpoint->connected) {
            endpoint->pending_qird.valid = false;
            return 0;
        }

        requested_len = max_space;
        if (requested_len > EC801E_QIRD_TCP_CHUNK_SIZE) {
            requested_len = EC801E_QIRD_TCP_CHUNK_SIZE;
        }
        snprintf(command, sizeof(command), "AT+QIRD=%d,%u", endpoint->id, (unsigned int)requested_len);
    } else if (endpoint->protocol == IPPROTO_UDP) {
        if (!endpoint->instance_active) {
            endpoint->pending_qird.valid = false;
            return 0;
        }
        snprintf(command, sizeof(command), "AT+QIRD=%d", endpoint->id);
    } else {
        endpoint->pending_qird.valid = false;
        return -1;
    }

#if EC801E_RECV_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
    if (!at_client_exec_text_cmd(endpoint->client, command, rx_line, sizeof(rx_line), prefetch_timeout_ms)) {
        if (endpoint->pending_qird.data_len == 0) {
            endpoint->data_pending = false;
            endpoint->pending_qird.valid = false;
            ec801e_at_cmd_refresh_pending(endpoint);
            return 0;
        }
        endpoint->pending_qird.valid = false;
        return -1;
    }
#else
    if (!at_client_exec_binary_cmd(endpoint->client, command, "QIRD",
                                   rx_data, sizeof(rx_data), &rx_len,
                                   prefetch_timeout_ms)) {
        if (endpoint->pending_qird.data_len == 0) {
            endpoint->data_pending = false;
            endpoint->pending_qird.valid = false;
            ec801e_at_cmd_refresh_pending(endpoint);
            return 0;
        }
        endpoint->pending_qird.valid = false;
        return -1;
    }
#endif

    if (endpoint->pending_qird.data_len == 0) {
        endpoint->data_pending = false;
        endpoint->pending_qird.valid = false;
        ec801e_at_cmd_refresh_pending(endpoint);
        return 0;
    }

    endpoint->data_pending = true;
#if EC801E_RECV_DATA_FORMAT == EC801E_DATA_FORMAT_HEX
    if (ec801e_qird_decode_and_push(endpoint, rx_line) < 0) {
        endpoint->pending_qird.valid = false;
        return -1;
    }
#else
    if (rx_len != endpoint->pending_qird.data_len ||
        ec801e_qird_push_text(endpoint, rx_data, rx_len) < 0) {
        endpoint->pending_qird.valid = false;
        return -1;
    }
#endif

    endpoint->pending_qird.valid = false;
    modem_endpoint_runtime_mark_pending(&endpoint->runtime, endpoint->data_pending);
    return 0;
}
