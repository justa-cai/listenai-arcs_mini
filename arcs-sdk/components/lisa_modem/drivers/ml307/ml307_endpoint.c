/**
 * @file ml307_endpoint.c
 * @brief Private ML307 endpoint context implementation
 */

#include "drivers/ml307/ml307_endpoint_internal.h"
#include "drivers/ml307/ml307_at_cmd.h"
#include "drivers/ml307/ml307_netreg.h"
#include "core/modem_probe_utils.h"
#include "at_mem.h"
#include <errno.h>
#include <string.h>

#define TAG "ml307_endpoint"
#include "lisa_log.h"
#include "lisa_modem_perf_log.h"

#define DEFAULT_INIT_BAUDRATE        115200
#define DEFAULT_TARGET_BAUDRATE      921600
#define ML307_DEFAULT_RECV_TIMEOUT_MS 1500
#define ML307_DEFAULT_PULL_TIMEOUT_MS   50
#define ML307_RX_HIGH_WATERMARK(bytes) (((bytes) * 5U) / 6U)
#define ML307_RX_LOW_WATERMARK(bytes)  (((bytes) * 4U) / 5U)

static uint32_t ml307_endpoint_tick_elapsed_ms(TickType_t start, TickType_t end)
{
    return (uint32_t)((end - start) * portTICK_PERIOD_MS);
}

static ml307_endpoint_t *ml307_endpoint_get(ml307_endpoint_ctx_t *ctx, int endpoint_id)
{
    if (!ctx || endpoint_id < 1 || endpoint_id > ML307_MAX_ENDPOINTS) {
        return NULL;
    }

    return &ctx->endpoints[endpoint_id - 1];
}

static void ml307_endpoint_schedule_rx_ready(ml307_endpoint_t *endpoint)
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

static void ml307_endpoint_modem_urc_handler(const char *command, at_arg_value_t *arguments,
                                             size_t arg_count, void *user_data)
{
    ml307_endpoint_ctx_t *ctx = (ml307_endpoint_ctx_t *)user_data;
    int i;

    if (!ctx || !command) {
        return;
    }

    for (i = 0; i < ML307_MAX_ENDPOINTS; ++i) {
        ml307_endpoint_t *endpoint = &ctx->endpoints[i];

        if (!endpoint->in_use || !endpoint->initialized) {
            continue;
        }

        ml307_at_cmd_handle_urc(endpoint, command, arguments, arg_count);
    }

    ml307_netreg_handle_modem_urc(ctx, command, arguments, arg_count);
}

static network_status_t ml307_endpoint_check_network(ml307_endpoint_ctx_t *ctx)
{
    return ml307_netreg_network_check(ctx);
}

static int ml307_endpoint_instance_init(ml307_endpoint_ctx_t *ctx, ml307_endpoint_t *endpoint,
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
    endpoint->blocking = true;
    endpoint->public_sockfd = -1;
    endpoint->recv_timeout_ms = ML307_DEFAULT_RECV_TIMEOUT_MS;
    endpoint->pull_timeout_ms = ML307_DEFAULT_PULL_TIMEOUT_MS;
    endpoint->send_timeout_ms = ML307_SEND_TIMEOUT_MS;
    ring_buf_init(&endpoint->ring_buf, ML307_ENDPOINT_RECV_BUFFER_SIZE, endpoint->recv_buffer_data);
    /*
     * Keep cached-mode TCP refill more continuous for streaming workloads:
     * pause only when the ring is almost full, and resume once it drops to
     * about half full so playback does not outrun refill between bursts.
     */
    modem_endpoint_runtime_init(&endpoint->runtime, &endpoint->ring_buf,
                                ML307_RX_HIGH_WATERMARK(ML307_ENDPOINT_RECV_BUFFER_SIZE),
                                ML307_RX_LOW_WATERMARK(ML307_ENDPOINT_RECV_BUFFER_SIZE));

    endpoint->event_group = xEventGroupCreate();
    endpoint->recv_event = xEventGroupCreate();
    endpoint->data_sem = xSemaphoreCreateBinary();
    if (!endpoint->event_group || !endpoint->recv_event || !endpoint->data_sem) {
        if (endpoint->data_sem) {
            vSemaphoreDelete(endpoint->data_sem);
        }
        if (endpoint->recv_event) {
            vEventGroupDelete(endpoint->recv_event);
        }
        if (endpoint->event_group) {
            vEventGroupDelete(endpoint->event_group);
        }
        memset(endpoint, 0, sizeof(*endpoint));
        return -1;
    }

    endpoint->initialized = true;
    return 0;
}

static void ml307_endpoint_instance_deinit(ml307_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->initialized) {
        return;
    }

    if (endpoint->data_sem) {
        vSemaphoreDelete(endpoint->data_sem);
    }
    if (endpoint->recv_event) {
        vEventGroupDelete(endpoint->recv_event);
    }
    if (endpoint->event_group) {
        vEventGroupDelete(endpoint->event_group);
    }

    memset(endpoint, 0, sizeof(*endpoint));
}

ml307_endpoint_ctx_t *ml307_endpoint_create(at_client_t *client)
{
    ml307_endpoint_ctx_t *ctx = NULL;

    if (!client) {
        return NULL;
    }

    ctx = (ml307_endpoint_ctx_t *)at_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->client = client;
    ctx->runtime_config = (ml307_runtime_config_t){
        .tcp_send_chunk_size = ML307_TCP_SEND_CHUNK_SIZE_DEFAULT,
        .tcp_pull_chunk_size = ML307_TCP_PULL_CHUNK_SIZE_DEFAULT,
        .send_chunk_delay_ms = ML307_SEND_CHUNK_DELAY_MS_DEFAULT,
    };
    ctx->event_group = xEventGroupCreate();
    if (!ctx->event_group) {
        at_mem_free(ctx);
        return NULL;
    }

    ctx->dns_mutex = xSemaphoreCreateMutex();
    if (!ctx->dns_mutex) {
        vEventGroupDelete(ctx->event_group);
        at_mem_free(ctx);
        return NULL;
    }

    if (at_client_set_line_stream_handler(client, ml307_at_cmd_get_line_stream_handler(), ctx) != 0) {
        vSemaphoreDelete(ctx->dns_mutex);
        vEventGroupDelete(ctx->event_group);
        at_mem_free(ctx);
        return NULL;
    }

    ctx->urc_node = at_client_register_urc(client, ml307_endpoint_modem_urc_handler, ctx);
    if (!ctx->urc_node) {
        (void)at_client_set_line_stream_handler(client, NULL, NULL);
        vSemaphoreDelete(ctx->dns_mutex);
        vEventGroupDelete(ctx->event_group);
        at_mem_free(ctx);
        return NULL;
    }

    LISA_LOGI(TAG, "ML307 endpoint context created");
    return ctx;
}

bool ml307_endpoint_init(ml307_endpoint_ctx_t *ctx)
{
    uint32_t current_baud = 0;

    if (!ctx || ctx->initialized) {
        return false;
    }

    LISA_LOGI(TAG, "Initializing ML307 endpoint context...");

    if (at_client_uart_get_baudrate(ctx->client, &current_baud)) {
        if (current_baud != DEFAULT_TARGET_BAUDRATE) {
            if (!at_client_uart_baudrate_adapt(ctx->client,
                                               DEFAULT_INIT_BAUDRATE,
                                               DEFAULT_TARGET_BAUDRATE)) {
                LISA_LOGE(TAG, "Baudrate adaptation failed");
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
            return false;
        }
    }

    if (NETWORK_STATUS_READY != ml307_endpoint_check_network(ctx)) {
        LISA_LOGE(TAG, "4G network is not ready");
        return false;
    }

    ctx->initialized = true;
    LISA_LOGI(TAG, "ML307 endpoint context initialized successfully");
    return true;
}

void ml307_endpoint_shutdown(ml307_endpoint_ctx_t *ctx)
{
    int i;

    if (!ctx || !ctx->initialized) {
        return;
    }

    for (i = 0; i < ML307_MAX_ENDPOINTS; ++i) {
        ml307_endpoint_t *endpoint = &ctx->endpoints[i];

        if (!endpoint->in_use) {
            continue;
        }

        ml307_endpoint_close(ctx, endpoint->id);
    }

    ctx->initialized = false;
}

void ml307_endpoint_destroy(ml307_endpoint_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    LISA_LOGI(TAG, "Destroying ML307 endpoint context...");

    ml307_endpoint_shutdown(ctx);

    (void)at_client_set_line_stream_handler(ctx->client, NULL, NULL);
    if (ctx->urc_node) {
        at_client_unregister_urc(ctx->client, ctx->urc_node);
    }
    if (ctx->dns_mutex) {
        vSemaphoreDelete(ctx->dns_mutex);
    }
    if (ctx->event_group) {
        vEventGroupDelete(ctx->event_group);
    }

    at_mem_free(ctx);
}

bool ml307_endpoint_dns_resolve(ml307_endpoint_ctx_t *ctx, const char *domain, char *ip_addr, size_t size)
{
    return ml307_netreg_dns_resolve(ctx, domain, ip_addr, size);
}

int ml307_endpoint_open(ml307_endpoint_ctx_t *ctx, int domain, int protocol)
{
    int i;

    if (!ctx || domain != AF_INET) {
        return -1;
    }
    if (protocol != IPPROTO_TCP && protocol != IPPROTO_UDP) {
        return -1;
    }

    for (i = 0; i < ML307_MAX_ENDPOINTS; ++i) {
        ml307_endpoint_t *endpoint = &ctx->endpoints[i];

        if (endpoint->in_use) {
            continue;
        }

        if (ml307_endpoint_instance_init(ctx, endpoint, i + 1, protocol) != 0) {
            return -1;
        }

        LISA_LOGD(TAG, "Allocated endpoint id=%d protocol=%d", endpoint->id, protocol);
        return endpoint->id;
    }

    LISA_LOGE(TAG, "No free endpoint available");
    return -1;
}

bool ml307_endpoint_connect(ml307_endpoint_ctx_t *ctx, int endpoint_id, const char *host, uint16_t port)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get(ctx, endpoint_id);
    bool connected;

    if (!endpoint || !endpoint->in_use || !host) {
        return false;
    }

    connected = ml307_at_cmd_connect(endpoint, host, (int)port);
    if (connected && endpoint->protocol == IPPROTO_UDP) {
        endpoint->udp_peer.valid = true;
        endpoint->udp_peer.addr.family = AF_INET;
        strncpy(endpoint->udp_peer.addr.host, host, sizeof(endpoint->udp_peer.addr.host) - 1);
        endpoint->udp_peer.addr.host[sizeof(endpoint->udp_peer.addr.host) - 1] = '\0';
        endpoint->udp_peer.addr.port = port;
    }

    return connected;
}

int ml307_endpoint_close(ml307_endpoint_ctx_t *ctx, int endpoint_id)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use) {
        return -1;
    }

    ml307_at_cmd_disconnect(endpoint);
    ml307_endpoint_instance_deinit(endpoint);
    return 0;
}

int ml307_endpoint_send(ml307_endpoint_ctx_t *ctx, int endpoint_id,
                        const void *data, size_t length, uint32_t timeout_ms,
                        const char *host, uint16_t port)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use || !data || length == 0) {
        return -1;
    }

    if (endpoint->protocol == IPPROTO_UDP && host) {
        endpoint->send_timeout_ms = timeout_ms;
        return ml307_at_cmd_sendto(endpoint, host, port, (const char *)data, length);
    }

    if (endpoint->protocol == IPPROTO_UDP) {
        if (!endpoint->udp_peer.valid) {
            errno = ENOTCONN;
            return -1;
        }

        if (!endpoint->connected &&
            !ml307_at_cmd_connect(endpoint, endpoint->udp_peer.addr.host, (int)endpoint->udp_peer.addr.port)) {
            return -1;
        }
    }

    endpoint->send_timeout_ms = timeout_ms;
    return ml307_at_cmd_send(endpoint, (const char *)data, length);
}

int ml307_endpoint_recv(ml307_endpoint_ctx_t *ctx, int endpoint_id,
                        void *buffer, size_t length, uint32_t timeout_ms,
                        modem_addr_t *from)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get(ctx, endpoint_id);
    int ret;
    TickType_t t_recv_start;
    TickType_t t_recv_done;
    bool partial_read;
    bool refill_ready;

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

    t_recv_start = xTaskGetTickCount();

    ret = modem_runtime_recv_ring(&endpoint->ring_buf, (char *)buffer, length,
                                  timeout_ms, endpoint->data_sem,
                                  NULL, NULL);
    refill_ready = false;
    if (ret > 0) {
        refill_ready = modem_endpoint_runtime_note_read(&endpoint->runtime);
        if (refill_ready) {
            ml307_endpoint_schedule_rx_ready(endpoint);
        }
    }

    t_recv_done = xTaskGetTickCount();
    partial_read = (ret > 0 && (size_t)ret < length);

    if (ret > 0) {
        endpoint->perf_last_recv_ok_tick = t_recv_done;
        endpoint->perf_consecutive_recv_timeouts = 0U;
        endpoint->perf_consecutive_partial_reads = partial_read
                                                 ? (endpoint->perf_consecutive_partial_reads + 1U)
                                                 : 0U;
    } else {
        endpoint->perf_consecutive_recv_timeouts++;
        endpoint->perf_consecutive_partial_reads = 0U;
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

int ml307_endpoint_set_timeout(ml307_endpoint_ctx_t *ctx, int endpoint_id,
                               bool is_send, uint32_t timeout_ms)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get(ctx, endpoint_id);

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

int ml307_endpoint_set_nonblock(ml307_endpoint_ctx_t *ctx, int endpoint_id, bool nonblock)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use) {
        return -1;
    }

    endpoint->blocking = !nonblock;
    endpoint->recv_timeout_ms = endpoint->blocking ? ML307_DEFAULT_RECV_TIMEOUT_MS : 0;
    return 0;
}

int ml307_endpoint_set_tls(ml307_endpoint_ctx_t *ctx, int endpoint_id, bool enabled)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use || endpoint->protocol != IPPROTO_TCP) {
        return -1;
    }

    endpoint->is_tls = enabled;
    return 0;
}

int ml307_endpoint_set_runtime_config(ml307_endpoint_ctx_t *ctx, const ml307_runtime_config_t *config)
{
    if (!ctx || !config ||
        config->tcp_send_chunk_size == 0U ||
        config->tcp_pull_chunk_size == 0U) {
        return -1;
    }

    ctx->runtime_config.tcp_send_chunk_size =
        config->tcp_send_chunk_size > ML307_TCP_SEND_CHUNK_SIZE_MAX
            ? ML307_TCP_SEND_CHUNK_SIZE_MAX
            : config->tcp_send_chunk_size;
    ctx->runtime_config.tcp_pull_chunk_size =
        config->tcp_pull_chunk_size > ML307_TCP_PULL_CHUNK_SIZE_MAX
            ? ML307_TCP_PULL_CHUNK_SIZE_MAX
            : config->tcp_pull_chunk_size;
    ctx->runtime_config.send_chunk_delay_ms = config->send_chunk_delay_ms;

    LISA_LOGI(TAG, "ML307 runtime config: send_chunk=%u pull_chunk=%u send_delay=%u ms",
              ctx->runtime_config.tcp_send_chunk_size,
              ctx->runtime_config.tcp_pull_chunk_size,
              ctx->runtime_config.send_chunk_delay_ms);
    return 0;
}

void ml307_endpoint_get_runtime_config(ml307_endpoint_ctx_t *ctx, ml307_runtime_config_t *config)
{
    if (!ctx || !config) {
        return;
    }

    *config = ctx->runtime_config;
}

static bool ml307_endpoint_driver_probe(at_client_t *client, modem_probe_result_t *result)
{
    return modem_probe_common(client, result, "ml307", "ML307");
}

static void *ml307_endpoint_driver_create(at_client_t *client)
{
    return ml307_endpoint_create(client);
}

static void ml307_endpoint_driver_destroy(void *driver_ctx)
{
    ml307_endpoint_destroy((ml307_endpoint_ctx_t *)driver_ctx);
}

static bool ml307_endpoint_driver_init(void *driver_ctx)
{
    return driver_ctx ? ml307_endpoint_init((ml307_endpoint_ctx_t *)driver_ctx) : false;
}

static void ml307_endpoint_driver_deinit(void *driver_ctx)
{
    if (driver_ctx) {
        ml307_endpoint_shutdown((ml307_endpoint_ctx_t *)driver_ctx);
    }
}

static void ml307_endpoint_driver_attach_dispatcher(void *driver_ctx, modem_dispatcher_t *dispatcher)
{
    ml307_endpoint_ctx_t *ctx = (ml307_endpoint_ctx_t *)driver_ctx;

    if (ctx) {
        ctx->dispatcher = dispatcher;
    }
}

static void ml307_endpoint_driver_bind_socket(void *driver_ctx, int driver_endpoint_id,
                                              int sockfd, uint32_t generation)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id);

    if (!endpoint) {
        return;
    }

    endpoint->public_sockfd = sockfd;
    endpoint->generation = generation;
}

static bool ml307_endpoint_driver_dns_resolve(void *driver_ctx, const char *domain, char *ip_addr, size_t size)
{
    return driver_ctx
         ? ml307_endpoint_dns_resolve((ml307_endpoint_ctx_t *)driver_ctx, domain, ip_addr, size)
         : false;
}

static int ml307_endpoint_driver_open(void *driver_ctx, int domain, int protocol)
{
    return driver_ctx ? ml307_endpoint_open((ml307_endpoint_ctx_t *)driver_ctx, domain, protocol) : -1;
}

static int ml307_endpoint_driver_connect(void *driver_ctx, int driver_endpoint_id, const modem_addr_t *addr)
{
    return driver_ctx && addr
         ? (ml307_endpoint_connect((ml307_endpoint_ctx_t *)driver_ctx,
                                   driver_endpoint_id, addr->host, addr->port) ? 0 : -1)
         : -1;
}

static int ml307_endpoint_driver_close(void *driver_ctx, int driver_endpoint_id)
{
    return driver_ctx ? ml307_endpoint_close((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id) : -1;
}

static int ml307_endpoint_driver_send(void *driver_ctx, int driver_endpoint_id, const void *data, size_t length,
                                      uint32_t timeout_ms, const modem_addr_t *to)
{
    return driver_ctx
         ? ml307_endpoint_send((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id, data, length,
                               timeout_ms, to ? to->host : NULL, to ? to->port : 0)
         : -1;
}

static int ml307_endpoint_send_chunk(ml307_endpoint_ctx_t *ctx, int endpoint_id,
                                     const void *data, size_t length, uint32_t timeout_ms,
                                     const char *host, uint16_t port)
{
    ml307_endpoint_t *endpoint = ml307_endpoint_get(ctx, endpoint_id);

    if (!endpoint || !endpoint->in_use || !data || length == 0U) {
        return -1;
    }

    endpoint->send_timeout_ms = timeout_ms;
    if (endpoint->protocol == IPPROTO_UDP) {
        if (host) {
            bool peer_changed = !endpoint->udp_peer.valid ||
                                strcmp(endpoint->udp_peer.addr.host, host) != 0 ||
                                endpoint->udp_peer.addr.port != port;

            if (!endpoint->connected || peer_changed) {
                if (!ml307_at_cmd_connect(endpoint, host, (int)port)) {
                    return -1;
                }
            }

            endpoint->udp_peer.valid = true;
            endpoint->udp_peer.addr.family = AF_INET;
            strncpy(endpoint->udp_peer.addr.host, host, sizeof(endpoint->udp_peer.addr.host) - 1);
            endpoint->udp_peer.addr.host[sizeof(endpoint->udp_peer.addr.host) - 1] = '\0';
            endpoint->udp_peer.addr.port = port;
        } else {
            if (!endpoint->udp_peer.valid) {
                errno = ENOTCONN;
                return -1;
            }

            if (!endpoint->connected &&
                !ml307_at_cmd_connect(endpoint, endpoint->udp_peer.addr.host,
                                      (int)endpoint->udp_peer.addr.port)) {
                return -1;
            }
        }
    }

    return ml307_at_cmd_send_chunk(endpoint, (const char *)data, length);
}

static int ml307_endpoint_driver_send_chunk(void *driver_ctx, int driver_endpoint_id,
                                            const void *data, size_t length, uint32_t timeout_ms,
                                            const modem_addr_t *to)
{
    return driver_ctx
         ? ml307_endpoint_send_chunk((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id,
                                     data, length, timeout_ms,
                                     to ? to->host : NULL, to ? to->port : 0)
         : -1;
}

static int ml307_endpoint_driver_recv(void *driver_ctx, int driver_endpoint_id, void *buffer, size_t length,
                                      uint32_t timeout_ms, modem_addr_t *from)
{
    return driver_ctx
         ? ml307_endpoint_recv((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id, buffer, length,
                               timeout_ms, from)
         : -1;
}

static int ml307_endpoint_driver_pull_rx(void *driver_ctx, int driver_endpoint_id)
{
    ml307_endpoint_t *endpoint;
    int ret;

    endpoint = ml307_endpoint_get((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id);
    if (!endpoint || !endpoint->initialized) {
        return -1;
    }
    if (!modem_endpoint_runtime_should_pull(&endpoint->runtime)) {
        return 0;
    }

    ret = ml307_at_cmd_prefetch(endpoint);
    if (ret < 0) {
        return -1;
    }

    return modem_endpoint_runtime_should_pull(&endpoint->runtime) ? 1 : 0;
}

static int ml307_endpoint_driver_set_timeout(void *driver_ctx, int driver_endpoint_id,
                                             bool is_send, uint32_t timeout_ms)
{
    return driver_ctx
         ? ml307_endpoint_set_timeout((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id, is_send, timeout_ms)
         : -1;
}

static int ml307_endpoint_driver_set_nonblock(void *driver_ctx, int driver_endpoint_id, bool nonblock)
{
    return driver_ctx
         ? ml307_endpoint_set_nonblock((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id, nonblock)
         : -1;
}

static int ml307_endpoint_driver_set_tls(void *driver_ctx, int driver_endpoint_id, bool enabled)
{
    return driver_ctx
         ? ml307_endpoint_set_tls((ml307_endpoint_ctx_t *)driver_ctx, driver_endpoint_id, enabled)
         : -1;
}

static const modem_driver_ops_t s_ml307_driver_ops = {
    .name = "ml307",
    .caps = {
        .max_bearers = 1,
        .max_endpoints = 5,
        .max_tls_profiles = 1,
        .max_tx_chunk = 1460,
        .max_rx_pull = 4096,
        .rx_mode = MODEM_RX_MODE_HYBRID,
        .tx_mode = MODEM_TX_MODE_PROMPT_RAW,
        .supports_listener = false,
        .supports_udp_service = true,
        .supports_tls_profile_reuse = false,
        .supports_sendto_without_reconnect = false,
        .supports_access_mode_cached = true,
        .supports_access_mode_direct = false,
        .supports_access_mode_transparent = false,
        .supports_service_http = false,
        .supports_service_mqtt = false,
        .supports_ipv6 = false,
        .supports_multi_bearer = false,
        .needs_connect_before_udp_send = true,
        .supports_sleep_urc = true,
    },
    .probe = ml307_endpoint_driver_probe,
    .create = ml307_endpoint_driver_create,
    .destroy = ml307_endpoint_driver_destroy,
    .init = ml307_endpoint_driver_init,
    .deinit = ml307_endpoint_driver_deinit,
    .attach_dispatcher = ml307_endpoint_driver_attach_dispatcher,
    .bind_socket = ml307_endpoint_driver_bind_socket,
    .dns_resolve = ml307_endpoint_driver_dns_resolve,
    .open_fn = ml307_endpoint_driver_open,
    .connect_fn = ml307_endpoint_driver_connect,
    .close_fn = ml307_endpoint_driver_close,
    .send_fn = ml307_endpoint_driver_send,
    .send_chunk = ml307_endpoint_driver_send_chunk,
    .recv_fn = ml307_endpoint_driver_recv,
    .pull_rx = ml307_endpoint_driver_pull_rx,
    .set_timeout = ml307_endpoint_driver_set_timeout,
    .set_nonblock = ml307_endpoint_driver_set_nonblock,
    .set_tls = ml307_endpoint_driver_set_tls,
};

const modem_driver_ops_t *ml307_endpoint_get_ops(void)
{
    return &s_ml307_driver_ops;
}
