/**
 * @file ec801e_endpoint.c
 * @brief Private EC801E endpoint context implementation
 */

#include "ec801e_endpoint_internal.h"
#include "ec801e_at_cmd.h"
#include "ec801e_netreg.h"
#include "core/modem_probe_utils.h"
#include "at_mem.h"
#include <errno.h>
#include <string.h>

#define TAG "ec801e_endpoint"
#include "lisa_log.h"

#define DEFAULT_INIT_BAUDRATE           921600U
#define DEFAULT_TARGET_BAUDRATE         921600U
#define EC801E_RX_HIGH_WATERMARK(bytes) (((bytes) * 5U) / 6U)
#define EC801E_RX_LOW_WATERMARK(bytes)  (((bytes) * 4U) / 5U)

static bool ec801e_endpoint_query_matches(at_client_t *client, const char *query_cmd,
                                          const char *expected_fragment,
                                          char *response, size_t response_size)
{
    if (!client || !query_cmd || !expected_fragment || !response || response_size == 0U) {
        return false;
    }

    if (!at_client_exec_text_cmd(client, query_cmd, response, response_size, 1000U)) {
        response[0] = '\0';
        return false;
    }

    return strstr(response, expected_fragment) != NULL;
}

static bool ec801e_endpoint_ensure_socket_format(at_client_t *client,
                                                 const char *query_cmd,
                                                 const char *set_cmd,
                                                 const char *expected_fragment)
{
    char response[128];

    if (ec801e_endpoint_query_matches(client, query_cmd, expected_fragment,
                                      response, sizeof(response))) {
        return true;
    }

    if (response[0] != '\0') {
        LISA_LOGW(TAG, "Unexpected EC801E socket format before set: %s => %s",
                  query_cmd, response);
    }

    if (!at_client_exec_cmd(client, &(at_cmd_desc_t){
            .cmd = set_cmd,
            .timeout_ms = 1000,
        }, NULL)) {
        if (ec801e_endpoint_query_matches(client, query_cmd, expected_fragment,
                                          response, sizeof(response))) {
            return true;
        }

        if (response[0] != '\0') {
            LISA_LOGE(TAG, "Failed to configure EC801E socket formatting: %s (current: %s)",
                      set_cmd, response);
        } else {
            LISA_LOGE(TAG, "Failed to configure EC801E socket formatting: %s",
                      set_cmd);
        }
        return false;
    }

    if (!ec801e_endpoint_query_matches(client, query_cmd, expected_fragment,
                                       response, sizeof(response))) {
        if (response[0] != '\0') {
            LISA_LOGE(TAG, "EC801E socket formatting mismatch after set: %s => %s",
                      query_cmd, response);
        } else {
            LISA_LOGE(TAG, "EC801E socket formatting verification failed: %s",
                      query_cmd);
        }
        return false;
    }

    return true;
}

static void ec801e_endpoint_signal_prefetch(ec801e_endpoint_t *endpoint)
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

static ec801e_endpoint_t *ec801e_endpoint_get(ec801e_endpoint_ctx_t *ctx, int endpoint_id)
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

static void ec801e_endpoint_urc_handler(const char *command, at_arg_value_t *arguments,
                                        size_t arg_count, void *user_data)
{
    ec801e_endpoint_ctx_t *ctx = (ec801e_endpoint_ctx_t *)user_data;

    if (!ctx || !command) {
        return;
    }

    ec801e_at_cmd_handle_urc(ctx, command, arguments, arg_count);
    ec801e_netreg_handle_modem_urc(ctx, command, arguments, arg_count);
}

static int ec801e_endpoint_instance_init(ec801e_endpoint_ctx_t *ctx, ec801e_endpoint_t *endpoint,
                                         int endpoint_id, int protocol)
{
    if (!ctx || !endpoint) {
        return -1;
    }

    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->id = endpoint_id;
    endpoint->protocol = protocol;
    endpoint->in_use = true;
    endpoint->client = ctx->client;
    endpoint->ctx = ctx;
    endpoint->initialized = true;
    endpoint->blocking = true;
    endpoint->public_sockfd = -1;
    endpoint->send_timeout_ms = EC801E_SEND_TIMEOUT_MS;
    endpoint->recv_timeout_ms = EC801E_DEFAULT_RECV_TIMEOUT_MS;
    endpoint->pull_timeout_ms = EC801E_DEFAULT_PULL_TIMEOUT_MS;
    endpoint->last_qisend_status = -1;
    endpoint->local_port = (uint16_t)(EC801E_DEFAULT_UDP_LOCAL_PORT + endpoint_id);
    ring_buf_init(&endpoint->ring_buf, EC801E_ENDPOINT_RECV_BUFFER_SIZE, endpoint->recv_buffer_data);
    modem_endpoint_runtime_init(&endpoint->runtime, &endpoint->ring_buf,
                                EC801E_RX_HIGH_WATERMARK(EC801E_ENDPOINT_RECV_BUFFER_SIZE),
                                EC801E_RX_LOW_WATERMARK(EC801E_ENDPOINT_RECV_BUFFER_SIZE));
    endpoint->data_sem = xSemaphoreCreateBinary();
    if (!endpoint->data_sem) {
        memset(endpoint, 0, sizeof(*endpoint));
        return -1;
    }
    return 0;
}

static void ec801e_endpoint_instance_deinit(ec801e_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->initialized) {
        return;
    }

    if (endpoint->data_sem) {
        vSemaphoreDelete(endpoint->data_sem);
    }
    memset(endpoint, 0, sizeof(*endpoint));
}

ec801e_endpoint_ctx_t *ec801e_endpoint_create(at_client_t *client)
{
    ec801e_endpoint_ctx_t *ctx;

    if (!client) {
        return NULL;
    }

    ctx = (ec801e_endpoint_ctx_t *)at_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->client = client;
    lisa_modem_status_clear(&ctx->status);
    ctx->dns_mutex = xSemaphoreCreateMutex();
    if (!ctx->dns_mutex) {
        at_mem_free(ctx);
        return NULL;
    }

    ctx->urc_node = at_client_register_urc(client, ec801e_endpoint_urc_handler, ctx);
    if (!ctx->urc_node) {
        vSemaphoreDelete(ctx->dns_mutex);
        at_mem_free(ctx);
        return NULL;
    }

    return ctx;
}

bool ec801e_endpoint_init(ec801e_endpoint_ctx_t *ctx)
{
    uint32_t current_baud = 0;

    if (!ctx || ctx->initialized) {
        return false;
    }

    LISA_LOGI(TAG, "Initializing EC801E endpoint context...");

    if (at_client_uart_get_baudrate(ctx->client, &current_baud)) {
        if (current_baud != DEFAULT_TARGET_BAUDRATE) {
            if (!at_client_uart_baudrate_adapt(ctx->client,
                                               current_baud,
                                               DEFAULT_TARGET_BAUDRATE)) {
                LISA_LOGE(TAG, "Baudrate adaptation failed");
                ctx->status.last_error = LISA_MODEM_ERR_UART_BAUD_ADAPT_FAILED;
                lisa_modem_status_note_cme(&ctx->status, at_client_get_cme_error(ctx->client));
                return false;
            }
        } else {
            LISA_LOGI(TAG, "UART already synced at target baudrate=%u", current_baud);
        }
    } else {
        if (!at_client_uart_baudrate_adapt(ctx->client,
                                           DEFAULT_INIT_BAUDRATE,
                                           DEFAULT_TARGET_BAUDRATE)) {
            LISA_LOGE(TAG, "Baudrate adaptation failed");
            ctx->status.last_error = LISA_MODEM_ERR_UART_BAUD_ADAPT_FAILED;
            lisa_modem_status_note_cme(&ctx->status, at_client_get_cme_error(ctx->client));
            return false;
        }
    }

    if (!ec801e_endpoint_ensure_socket_format(ctx->client,
                                              "AT+QICFG=\"dataformat\"",
                                              EC801E_QICFG_DATAFORMAT_CMD,
                                              EC801E_QICFG_DATAFORMAT_EXPECT) ||
        !ec801e_endpoint_ensure_socket_format(ctx->client,
                                              "AT+QICFG=\"viewmode\"",
                                              "AT+QICFG=\"viewmode\",0",
                                              "\"viewmode\",0") ||
        !ec801e_endpoint_ensure_socket_format(ctx->client,
                                              "AT+QICFG=\"sendinfo\"",
                                              "AT+QICFG=\"sendinfo\",1",
                                              "\"sendinfo\",1") ||
        !ec801e_endpoint_ensure_socket_format(ctx->client,
                                              "AT+QISDE?",
                                              "AT+QISDE=0",
                                              "+QISDE: 0")) {
        ctx->status.last_error = LISA_MODEM_ERR_SOCKET_CONFIG_FAILED;
        lisa_modem_status_note_cme(&ctx->status, at_client_get_cme_error(ctx->client));
        return false;
    }

    if (NETWORK_STATUS_READY != ec801e_netreg_network_check(ctx)) {
        LISA_LOGE(TAG, "4G network is not ready");
        if (ctx->status.last_error == LISA_MODEM_ERR_NOT_INITIALIZED) {
            ctx->status.last_error = LISA_MODEM_ERR_NETWORK_REGISTER_FAILED;
        }
        return false;
    }

    ctx->initialized = true;
    ctx->status.last_error = LISA_MODEM_ERR_READY;
    LISA_LOGI(TAG, "EC801E endpoint context initialized successfully");
    return true;
}

void ec801e_endpoint_shutdown(ec801e_endpoint_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return;
    }

    for (int i = 0; i < EC801E_MAX_ENDPOINTS; ++i) {
        if (ctx->endpoints[i].in_use) {
            (void)ec801e_endpoint_close(ctx, ctx->endpoints[i].id);
        }
    }

    ctx->initialized = false;
}

void ec801e_endpoint_destroy(ec801e_endpoint_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    ec801e_endpoint_shutdown(ctx);

    if (ctx->urc_node) {
        at_client_unregister_urc(ctx->client, ctx->urc_node);
    }
    if (ctx->dns_mutex) {
        vSemaphoreDelete(ctx->dns_mutex);
    }

    at_mem_free(ctx);
}

bool ec801e_endpoint_dns_resolve(ec801e_endpoint_ctx_t *ctx, const char *domain, char *ip_addr, size_t size)
{
    return ec801e_netreg_dns_resolve(ctx, domain, ip_addr, size);
}

static bool ec801e_endpoint_copy_identifier(const char *response, char *out, size_t size)
{
    const char *p;
    size_t len = 0U;

    if (!response || !out || size == 0U) {
        return false;
    }
    out[0] = '\0';

    p = strchr(response, ':');
    p = p ? (p + 1) : response;
    while (*p && (*p < '0' || *p > '9')) {
        p++;
    }
    while (p[len] >= '0' && p[len] <= '9') {
        len++;
    }
    if (len == 0U || len >= size) {
        return false;
    }

    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static bool ec801e_endpoint_query_identifier(ec801e_endpoint_ctx_t *ctx, const char *command,
                                             char *out, size_t size)
{
    char response[96] = {0};

    if (!ctx || !command || !out || size == 0U) {
        return false;
    }
    out[0] = '\0';

    if (!at_client_exec_text_cmd(ctx->client, command, response, sizeof(response), 1000U)) {
        return false;
    }

    return ec801e_endpoint_copy_identifier(response, out, size);
}

bool ec801e_endpoint_get_imei(ec801e_endpoint_ctx_t *ctx, char *imei, size_t size)
{
    return ec801e_endpoint_query_identifier(ctx, "AT+CGSN", imei, size) ||
           ec801e_endpoint_query_identifier(ctx, "AT+CGSN=1", imei, size);
}

bool ec801e_endpoint_get_iccid(ec801e_endpoint_ctx_t *ctx, char *iccid, size_t size)
{
    return ec801e_endpoint_query_identifier(ctx, "AT+QCCID", iccid, size) ||
           ec801e_endpoint_query_identifier(ctx, "AT+ICCID", iccid, size);
}

typedef struct {
    int *rssi;
    int *ber;
} ec801e_csq_out_t;

static bool ec801e_endpoint_parse_csq(at_arg_value_t *args, size_t count, void *user_data)
{
    ec801e_csq_out_t *out = (ec801e_csq_out_t *)user_data;

    if (!out || !out->rssi || !out->ber ||
        count < 2 || args[0].type != AT_ARG_TYPE_INT || args[1].type != AT_ARG_TYPE_INT) {
        return false;
    }

    *out->rssi = args[0].data.int_val;
    *out->ber = args[1].data.int_val;
    return true;
}

bool ec801e_endpoint_get_signal_quality(ec801e_endpoint_ctx_t *ctx, int *rssi, int *ber)
{
    ec801e_csq_out_t out = { rssi, ber };

    if (!ctx || !rssi || !ber) {
        return false;
    }

    *rssi = 99;
    *ber = 99;
    return at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
        .cmd = "AT+CSQ", .expect_urc = "CSQ",
        .parse = ec801e_endpoint_parse_csq, .timeout_ms = 1000U,
    }, &out);
}

int ec801e_endpoint_open(ec801e_endpoint_ctx_t *ctx, int domain, int protocol)
{
    if (!ctx || domain != AF_INET) {
        return -1;
    }
    if (protocol != IPPROTO_TCP && protocol != IPPROTO_UDP) {
        return -1;
    }

    for (int i = 0; i < EC801E_MAX_ENDPOINTS; ++i) {
        if (ctx->endpoints[i].in_use) {
            continue;
        }

        if (ec801e_endpoint_instance_init(ctx, &ctx->endpoints[i], i, protocol) != 0) {
            return -1;
        }

        return ctx->endpoints[i].id;
    }

    return -1;
}

bool ec801e_endpoint_connect(ec801e_endpoint_ctx_t *ctx, int endpoint_id, const char *host, uint16_t port)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get(ctx, endpoint_id);
    bool connected;

    if (!endpoint || !endpoint->in_use || !host) {
        return false;
    }

    connected = ec801e_at_cmd_connect(endpoint, host, port);
    if (!connected && ctx) {
        ctx->status.last_error = lisa_modem_cme_may_indicate_traffic_exceeded(endpoint->last_error)
                               ? LISA_MODEM_ERR_TRAFFIC_EXCEEDED
                               : LISA_MODEM_ERR_AT_COMMAND_FAILED;
    }
    return connected;
}

int ec801e_endpoint_close(ec801e_endpoint_ctx_t *ctx, int endpoint_id)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use) {
        return -1;
    }

    (void)ec801e_at_cmd_disconnect(endpoint);
    ec801e_endpoint_instance_deinit(endpoint);
    return 0;
}

int ec801e_endpoint_send(ec801e_endpoint_ctx_t *ctx, int endpoint_id,
                         const void *data, size_t length, uint32_t timeout_ms,
                         const char *host, uint16_t port)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use || !data || length == 0) {
        errno = EINVAL;
        return -1;
    }

    endpoint->send_timeout_ms = timeout_ms;
    if (endpoint->protocol == IPPROTO_UDP) {
        if (host) {
            int ret = ec801e_at_cmd_send(endpoint, data, length, host, port);
            if (ret < 0 && ctx) {
                ctx->status.last_error = lisa_modem_cme_may_indicate_traffic_exceeded(endpoint->last_error)
                                       ? LISA_MODEM_ERR_TRAFFIC_EXCEEDED
                                       : LISA_MODEM_ERR_AT_COMMAND_FAILED;
            }
            return ret;
        }
        if (!endpoint->udp_peer.valid) {
            errno = ENOTCONN;
            return -1;
        }
        int ret = ec801e_at_cmd_send(endpoint, data, length,
                                     endpoint->udp_peer.addr.host, endpoint->udp_peer.addr.port);
        if (ret < 0 && ctx) {
            ctx->status.last_error = lisa_modem_cme_may_indicate_traffic_exceeded(endpoint->last_error)
                                   ? LISA_MODEM_ERR_TRAFFIC_EXCEEDED
                                   : LISA_MODEM_ERR_AT_COMMAND_FAILED;
        }
        return ret;
    }

    int ret = ec801e_at_cmd_send(endpoint, data, length, NULL, 0);
    if (ret < 0 && ctx) {
        ctx->status.last_error = lisa_modem_cme_may_indicate_traffic_exceeded(endpoint->last_error)
                               ? LISA_MODEM_ERR_TRAFFIC_EXCEEDED
                               : LISA_MODEM_ERR_AT_COMMAND_FAILED;
    }
    return ret;
}

int ec801e_endpoint_recv(ec801e_endpoint_ctx_t *ctx, int endpoint_id,
                         void *buffer, size_t length, uint32_t timeout_ms,
                         modem_addr_t *from)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get(ctx, endpoint_id);
    int ret;

    (void)ctx;

    if (!endpoint || !endpoint->in_use || !buffer || length == 0) {
        errno = EAGAIN;
        return -1;
    }
    if (endpoint->protocol == IPPROTO_TCP && !endpoint->connected) {
        errno = EAGAIN;
        return -1;
    }

    if (from) {
        memset(from, 0, sizeof(*from));
    }

    ret = modem_runtime_recv_ring(&endpoint->ring_buf, (char *)buffer, length,
                                  timeout_ms, endpoint->data_sem,
                                  NULL, NULL);
    if (ret > 0) {
        if (modem_endpoint_runtime_note_read(&endpoint->runtime)) {
            ec801e_endpoint_signal_prefetch(endpoint);
        }
    }
    if (ret < 0) {
        errno = EAGAIN;
    } else if (ret > 0 && endpoint->protocol == IPPROTO_UDP) {
        if (from && endpoint->udp_last_source.valid) {
            *from = endpoint->udp_last_source.addr;
        }
        endpoint->udp_last_source.valid = false;
    }
    return ret;
}

int ec801e_endpoint_set_timeout(ec801e_endpoint_ctx_t *ctx, int endpoint_id,
                                bool is_send, uint32_t timeout_ms)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use) {
        return -1;
    }

    if (is_send) {
        endpoint->send_timeout_ms = timeout_ms;
    } else {
        endpoint->recv_timeout_ms = timeout_ms;
    }
    return 0;
}

int ec801e_endpoint_set_nonblock(ec801e_endpoint_ctx_t *ctx, int endpoint_id, bool nonblock)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use) {
        return -1;
    }

    endpoint->blocking = !nonblock;
    endpoint->recv_timeout_ms = endpoint->blocking ? EC801E_DEFAULT_RECV_TIMEOUT_MS : 0;
    return 0;
}

int ec801e_endpoint_set_tls(ec801e_endpoint_ctx_t *ctx, int endpoint_id, bool enabled)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use || endpoint->protocol != IPPROTO_TCP || enabled) {
        return -1;
    }

    endpoint->is_tls = false;
    return 0;
}

static bool ec801e_endpoint_driver_probe(at_client_t *client, modem_probe_result_t *result)
{
    return modem_probe_common(client, result, "ec801e", "EC801E");
}

static void *ec801e_endpoint_driver_create(at_client_t *client)
{
    return ec801e_endpoint_create(client);
}

static void ec801e_endpoint_driver_destroy(void *driver_ctx)
{
    ec801e_endpoint_destroy((ec801e_endpoint_ctx_t *)driver_ctx);
}

static bool ec801e_endpoint_driver_init(void *driver_ctx)
{
    return driver_ctx ? ec801e_endpoint_init((ec801e_endpoint_ctx_t *)driver_ctx) : false;
}

static void ec801e_endpoint_driver_deinit(void *driver_ctx)
{
    if (driver_ctx) {
        ec801e_endpoint_shutdown((ec801e_endpoint_ctx_t *)driver_ctx);
    }
}

static void ec801e_endpoint_driver_attach_dispatcher(void *driver_ctx, modem_dispatcher_t *dispatcher)
{
    ec801e_endpoint_ctx_t *ctx = (ec801e_endpoint_ctx_t *)driver_ctx;

    if (ctx) {
        ctx->dispatcher = dispatcher;
    }
}

static void ec801e_endpoint_driver_bind_socket(void *driver_ctx, int driver_endpoint_id,
                                               int sockfd, uint32_t generation)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get((ec801e_endpoint_ctx_t *)driver_ctx, driver_endpoint_id);

    if (!endpoint) {
        return;
    }

    endpoint->public_sockfd = sockfd;
    endpoint->generation = generation;
}

static bool ec801e_endpoint_driver_dns_resolve(void *driver_ctx, const char *domain, char *ip_addr, size_t size)
{
    return driver_ctx
         ? ec801e_endpoint_dns_resolve((ec801e_endpoint_ctx_t *)driver_ctx, domain, ip_addr, size)
         : false;
}

static bool ec801e_endpoint_driver_get_imei(void *driver_ctx, char *imei, size_t size)
{
    return driver_ctx ? ec801e_endpoint_get_imei((ec801e_endpoint_ctx_t *)driver_ctx, imei, size) : false;
}

static bool ec801e_endpoint_driver_get_iccid(void *driver_ctx, char *iccid, size_t size)
{
    return driver_ctx ? ec801e_endpoint_get_iccid((ec801e_endpoint_ctx_t *)driver_ctx, iccid, size) : false;
}

static bool ec801e_endpoint_driver_get_signal_quality(void *driver_ctx, int *rssi, int *ber)
{
    return driver_ctx
         ? ec801e_endpoint_get_signal_quality((ec801e_endpoint_ctx_t *)driver_ctx, rssi, ber)
         : false;
}

static int ec801e_endpoint_driver_open(void *driver_ctx, int domain, int protocol)
{
    return driver_ctx ? ec801e_endpoint_open((ec801e_endpoint_ctx_t *)driver_ctx, domain, protocol) : -1;
}

static int ec801e_endpoint_driver_connect(void *driver_ctx, int driver_endpoint_id, const modem_addr_t *addr)
{
    return driver_ctx && addr
         ? (ec801e_endpoint_connect((ec801e_endpoint_ctx_t *)driver_ctx,
                                    driver_endpoint_id, addr->host, addr->port) ? 0 : -1)
         : -1;
}

static int ec801e_endpoint_driver_close(void *driver_ctx, int driver_endpoint_id)
{
    return driver_ctx ? ec801e_endpoint_close((ec801e_endpoint_ctx_t *)driver_ctx, driver_endpoint_id) : -1;
}

static int ec801e_endpoint_driver_send(void *driver_ctx, int driver_endpoint_id,
                                       const void *data, size_t length, uint32_t timeout_ms,
                                       const modem_addr_t *to)
{
    return driver_ctx
         ? ec801e_endpoint_send((ec801e_endpoint_ctx_t *)driver_ctx, driver_endpoint_id, data, length,
                                timeout_ms, to ? to->host : NULL, to ? to->port : 0)
         : -1;
}

static int ec801e_endpoint_driver_send_chunk(void *driver_ctx, int driver_endpoint_id,
                                             const void *data, size_t length, uint32_t timeout_ms,
                                             const modem_addr_t *to)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get((ec801e_endpoint_ctx_t *)driver_ctx, driver_endpoint_id);

    if (!endpoint || !endpoint->in_use || !data || length == 0U) {
        return -1;
    }

    endpoint->send_timeout_ms = timeout_ms;
    if (endpoint->protocol == IPPROTO_UDP) {
        const char *host = to ? to->host : NULL;
        uint16_t port = to ? to->port : 0U;

        if (!host) {
            if (!endpoint->udp_peer.valid) {
                errno = ENOTCONN;
                return -1;
            }
            host = endpoint->udp_peer.addr.host;
            port = endpoint->udp_peer.addr.port;
        }

        endpoint->udp_peer.valid = true;
        endpoint->udp_peer.addr.family = AF_INET;
        strncpy(endpoint->udp_peer.addr.host, host, sizeof(endpoint->udp_peer.addr.host) - 1U);
        endpoint->udp_peer.addr.host[sizeof(endpoint->udp_peer.addr.host) - 1U] = '\0';
        endpoint->udp_peer.addr.port = port;
        return ec801e_at_cmd_send_chunk(endpoint, data, length, host, port);
    }

    return ec801e_at_cmd_send_chunk(endpoint, data, length, NULL, 0U);
}

static int ec801e_endpoint_driver_recv(void *driver_ctx, int driver_endpoint_id,
                                       void *buffer, size_t length, uint32_t timeout_ms,
                                       modem_addr_t *from)
{
    return driver_ctx
         ? ec801e_endpoint_recv((ec801e_endpoint_ctx_t *)driver_ctx, driver_endpoint_id, buffer, length,
                                timeout_ms, from)
         : -1;
}

static int ec801e_endpoint_driver_pull_rx(void *driver_ctx, int driver_endpoint_id)
{
    ec801e_endpoint_t *endpoint = ec801e_endpoint_get((ec801e_endpoint_ctx_t *)driver_ctx, driver_endpoint_id);
    int ret;

    if (!endpoint || !endpoint->initialized) {
        return -1;
    }
    if (!modem_endpoint_runtime_should_pull(&endpoint->runtime)) {
        return 0;
    }

    ret = ec801e_at_cmd_prefetch(endpoint);
    if (ret < 0) {
        return -1;
    }

    return modem_endpoint_runtime_should_pull(&endpoint->runtime) ? 1 : 0;
}

static int ec801e_endpoint_driver_set_timeout(void *driver_ctx, int driver_endpoint_id,
                                              bool is_send, uint32_t timeout_ms)
{
    return driver_ctx
         ? ec801e_endpoint_set_timeout((ec801e_endpoint_ctx_t *)driver_ctx,
                                       driver_endpoint_id, is_send, timeout_ms)
         : -1;
}

static int ec801e_endpoint_driver_set_nonblock(void *driver_ctx, int driver_endpoint_id, bool nonblock)
{
    return driver_ctx
         ? ec801e_endpoint_set_nonblock((ec801e_endpoint_ctx_t *)driver_ctx,
                                        driver_endpoint_id, nonblock)
         : -1;
}

static int ec801e_endpoint_driver_set_tls(void *driver_ctx, int driver_endpoint_id, bool enabled)
{
    return driver_ctx
         ? ec801e_endpoint_set_tls((ec801e_endpoint_ctx_t *)driver_ctx,
                                   driver_endpoint_id, enabled)
         : -1;
}

static void ec801e_endpoint_driver_get_status(void *driver_ctx, lisa_modem_status_t *status)
{
    ec801e_endpoint_ctx_t *ctx = (ec801e_endpoint_ctx_t *)driver_ctx;

    if (!ctx || !status) {
        return;
    }

    *status = ctx->status;
}

static const modem_driver_ops_t s_ec801e_driver_ops = {
    .name = "ec801e",
    .caps = {
        .max_bearers = 1,
        .max_endpoints = 6,
        .max_tls_profiles = 1,
        .max_tx_chunk = 1460,
        .max_rx_pull = EC801E_QIRD_TCP_CHUNK_SIZE,
        .rx_mode = MODEM_RX_MODE_HYBRID,
        .tx_mode = MODEM_TX_MODE_PROMPT_RAW,
        .supports_listener = false,
        .supports_udp_service = true,
        .supports_tls_profile_reuse = true,
        .supports_sendto_without_reconnect = true,
        .supports_access_mode_cached = true,
        .supports_access_mode_direct = false,
        .supports_access_mode_transparent = false,
        .supports_service_http = true,
        .supports_service_mqtt = false,
        .supports_ipv6 = false,
        .supports_multi_bearer = false,
        .needs_connect_before_udp_send = false,
        .supports_sleep_urc = true,
    },
    .probe = ec801e_endpoint_driver_probe,
    .create = ec801e_endpoint_driver_create,
    .destroy = ec801e_endpoint_driver_destroy,
    .init = ec801e_endpoint_driver_init,
    .deinit = ec801e_endpoint_driver_deinit,
    .attach_dispatcher = ec801e_endpoint_driver_attach_dispatcher,
    .bind_socket = ec801e_endpoint_driver_bind_socket,
    .get_status = ec801e_endpoint_driver_get_status,
    .dns_resolve = ec801e_endpoint_driver_dns_resolve,
    .get_imei = ec801e_endpoint_driver_get_imei,
    .get_iccid = ec801e_endpoint_driver_get_iccid,
    .get_signal_quality = ec801e_endpoint_driver_get_signal_quality,
    .start_gnss = NULL,
    .get_gps_location = NULL,
    .open_fn = ec801e_endpoint_driver_open,
    .connect_fn = ec801e_endpoint_driver_connect,
    .close_fn = ec801e_endpoint_driver_close,
    .send_fn = ec801e_endpoint_driver_send,
    .send_chunk = ec801e_endpoint_driver_send_chunk,
    .recv_fn = ec801e_endpoint_driver_recv,
    .pull_rx = ec801e_endpoint_driver_pull_rx,
    .set_timeout = ec801e_endpoint_driver_set_timeout,
    .set_nonblock = ec801e_endpoint_driver_set_nonblock,
    .set_tls = ec801e_endpoint_driver_set_tls,
};

const modem_driver_ops_t *ec801e_endpoint_get_ops(void)
{
    return &s_ec801e_driver_ops;
}
