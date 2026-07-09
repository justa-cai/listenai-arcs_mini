/**
 * @file ml307_endpoint.c
 * @brief Private ML307 endpoint context implementation
 */

#include "drivers/ml307/ml307_endpoint_internal.h"
#include "drivers/ml307/ml307_at_cmd.h"
#include "drivers/ml307/ml307_netreg.h"
#include "core/modem_probe_utils.h"
#include "core/modem_bits.h"
#include "at_mem.h"
#include "task.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define TAG "ml307_endpoint"
#include "lisa_log.h"
#include "lisa_modem_perf_log.h"

#define DEFAULT_INIT_BAUDRATE        921600
#define DEFAULT_TARGET_BAUDRATE      921600
#define ML307_DEFAULT_RECV_TIMEOUT_MS 1500
#define ML307_DEFAULT_PULL_TIMEOUT_MS   120
#define ML307_PULL_TIMEOUT_FALLBACK_MS 100U
#define ML307_RX_HIGH_WATERMARK(bytes) (((bytes) * 5U) / 6U)
#define ML307_RX_LOW_WATERMARK(bytes)  (((bytes) * 4U) / 5U)
#define ML307_GNSS_EVENT_RUNNING       BIT2
#define ML307_GNSS_START_TIMEOUT_MS    5000U
#define ML307_GNSS_STATE_SETTLE_MS      200U

static bool ml307_endpoint_exec_ok(ml307_endpoint_ctx_t *ctx, const char *cmd, uint32_t timeout_ms);
static bool ml307_endpoint_set_gnss_enabled(ml307_endpoint_ctx_t *ctx, bool enabled);

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

static bool ml307_endpoint_gnss_status_is_running(int status)
{
    return status == 1 || status == 2;
}

static void ml307_endpoint_handle_gnss_urc(ml307_endpoint_ctx_t *ctx,
                                           const char *command,
                                           at_arg_value_t *arguments,
                                           size_t arg_count)
{
    const char *kind;
    int value;

    if (!ctx || !command || strcmp(command, "MGNSSURC") != 0 ||
        arg_count < 2 || !arguments ||
        arguments[0].type != AT_ARG_TYPE_STRING ||
        !arguments[0].data.string_val.value ||
        arguments[1].type != AT_ARG_TYPE_INT) {
        return;
    }

    kind = arguments[0].data.string_val.value;
    value = arguments[1].data.int_val;
    if (strcmp(kind, "state") == 0) {
        if (ml307_endpoint_gnss_status_is_running(value)) {
            ctx->gnss_running = true;
            xEventGroupSetBits(ctx->event_group, ML307_GNSS_EVENT_RUNNING);
        } else {
            ctx->gnss_running = false;
            xEventGroupClearBits(ctx->event_group, ML307_GNSS_EVENT_RUNNING);
        }
    } else if (strcmp(kind, "error") == 0) {
        LISA_LOGW(TAG, "GNSS URC error=%d", value);
    }
}

static uint32_t ml307_endpoint_effective_pull_timeout_ms(const ml307_endpoint_t *endpoint)
{
    return modem_endpoint_runtime_effective_pull_timeout(endpoint ? endpoint->pull_timeout_ms : 0U,
                                                        ML307_PULL_TIMEOUT_FALLBACK_MS,
                                                        ML307_DEFAULT_PULL_TIMEOUT_MS);
}

static void ml307_endpoint_wait_prefetch_data(ml307_endpoint_t *endpoint)
{
    TickType_t start_tick;
    TickType_t wait_ticks;
    TickType_t poll_ticks;

    if (!endpoint || !endpoint->data_sem) {
        return;
    }

    wait_ticks = pdMS_TO_TICKS(ml307_endpoint_effective_pull_timeout_ms(endpoint));
    poll_ticks = pdMS_TO_TICKS(5U);
    if (poll_ticks == 0) {
        poll_ticks = 1;
    }
    start_tick = xTaskGetTickCount();
    while (endpoint->prefetching &&
           ring_buf_size_get(&endpoint->ring_buf) == 0U &&
           endpoint->rx_hint.available_data_len > 0U &&
           (xTaskGetTickCount() - start_tick) < wait_ticks) {
        TickType_t elapsed = xTaskGetTickCount() - start_tick;
        TickType_t remaining = wait_ticks - elapsed;

        if (remaining == 0 || remaining > wait_ticks) {
            break;
        }
        if (remaining > poll_ticks) {
            remaining = poll_ticks;
        }
        (void)xSemaphoreTake(endpoint->data_sem, remaining);
    }
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

    ml307_endpoint_handle_gnss_urc(ctx, command, arguments, arg_count);
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
    endpoint->prefetch_mutex = xSemaphoreCreateMutex();
    if (!endpoint->event_group || !endpoint->recv_event || !endpoint->data_sem ||
        !endpoint->prefetch_mutex) {
        if (endpoint->prefetch_mutex) {
            vSemaphoreDelete(endpoint->prefetch_mutex);
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
    if (endpoint->prefetch_mutex) {
        vSemaphoreDelete(endpoint->prefetch_mutex);
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
    lisa_modem_status_clear(&ctx->status);
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

    if (NETWORK_STATUS_READY != ml307_endpoint_check_network(ctx)) {
        LISA_LOGE(TAG, "4G network is not ready");
        if (ctx->status.last_error == LISA_MODEM_ERR_NOT_INITIALIZED) {
            ctx->status.last_error = LISA_MODEM_ERR_NETWORK_REGISTER_FAILED;
        }
        return false;
    }

    ctx->initialized = true;
    ctx->status.last_error = LISA_MODEM_ERR_READY;
    LISA_LOGI(TAG, "ML307 endpoint context initialized successfully");
    return true;
}

void ml307_endpoint_shutdown(ml307_endpoint_ctx_t *ctx)
{
    int i;

    if (!ctx || !ctx->initialized) {
        return;
    }

    if (ctx->gnss_running) {
        (void)ml307_endpoint_exec_ok(ctx, "AT+MGNSSLOC=0", 2000U);
        (void)ml307_endpoint_set_gnss_enabled(ctx, false);
        ctx->gnss_running = false;
        xEventGroupClearBits(ctx->event_group, ML307_GNSS_EVENT_RUNNING);
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

static bool ml307_endpoint_copy_identifier(const char *response, char *out, size_t size)
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

static bool ml307_endpoint_query_identifier(ml307_endpoint_ctx_t *ctx, const char *command,
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

    return ml307_endpoint_copy_identifier(response, out, size);
}

bool ml307_endpoint_get_imei(ml307_endpoint_ctx_t *ctx, char *imei, size_t size)
{
    return ml307_endpoint_query_identifier(ctx, "AT+CGSN=1", imei, size) ||
           ml307_endpoint_query_identifier(ctx, "AT+CGSN", imei, size);
}

bool ml307_endpoint_get_iccid(ml307_endpoint_ctx_t *ctx, char *iccid, size_t size)
{
    return ml307_endpoint_query_identifier(ctx, "AT+ICCID", iccid, size);
}

typedef struct {
    int *rssi;
    int *ber;
} ml307_csq_out_t;

static bool ml307_endpoint_parse_csq(at_arg_value_t *args, size_t count, void *user_data)
{
    ml307_csq_out_t *out = (ml307_csq_out_t *)user_data;

    if (!out || !out->rssi || !out->ber ||
        count < 2 || args[0].type != AT_ARG_TYPE_INT || args[1].type != AT_ARG_TYPE_INT) {
        return false;
    }

    *out->rssi = args[0].data.int_val;
    *out->ber = args[1].data.int_val;
    return true;
}

bool ml307_endpoint_get_signal_quality(ml307_endpoint_ctx_t *ctx, int *rssi, int *ber)
{
    ml307_csq_out_t out = { rssi, ber };

    if (!ctx || !rssi || !ber) {
        return false;
    }

    *rssi = 99;
    *ber = 99;
    return at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
        .cmd = "AT+CSQ", .expect_urc = "CSQ",
        .parse = ml307_endpoint_parse_csq, .timeout_ms = 1000U,
    }, &out);
}

typedef struct {
    double *lat;
    double *lon;
    int *fix;
} ml307_gps_out_t;

static bool ml307_endpoint_nmea_coord_to_degrees(const at_arg_value_t *arg, double *degrees)
{
    const char *coord;
    char value[24];
    char *end = NULL;
    size_t len;
    char hemi;
    double raw;
    int deg;
    double minutes;

    if (!arg || !degrees ||
        arg->type != AT_ARG_TYPE_STRING || !arg->data.string_val.value) {
        return false;
    }

    coord = arg->data.string_val.value;
    len = strlen(coord);
    if (len < 3U || len >= sizeof(value)) {
        return false;
    }

    hemi = coord[len - 1U];
    if (hemi != 'N' && hemi != 'S' && hemi != 'E' && hemi != 'W') {
        return false;
    }

    memcpy(value, coord, len - 1U);
    value[len - 1U] = '\0';
    raw = strtod(value, &end);
    if (!end || end == value) {
        return false;
    }

    deg = (int)(raw / 100.0);
    minutes = raw - ((double)deg * 100.0);
    if (minutes < 0.0 || minutes >= 60.0) {
        return false;
    }

    *degrees = (double)deg + (minutes / 60.0);
    if (hemi == 'S' || hemi == 'W') {
        *degrees = -*degrees;
    }
    return true;
}

static bool ml307_endpoint_parse_mgnssloc(at_arg_value_t *args, size_t count, void *user_data)
{
    ml307_gps_out_t *out = (ml307_gps_out_t *)user_data;
    int fix;
    bool lat_ok;
    bool lon_ok;

    /* +MGNSSLOC: <UTC>,<lat>,<lon>,<hdop>,<alt>,<fix>,... */
    if (!out || !out->lat || !out->lon || !out->fix ||
        count < 6 || args[5].type != AT_ARG_TYPE_INT) {
        return false;
    }

    fix = args[5].data.int_val;
    *out->fix = fix;
    lat_ok = ml307_endpoint_nmea_coord_to_degrees(&args[1], out->lat);
    lon_ok = ml307_endpoint_nmea_coord_to_degrees(&args[2], out->lon);
    if (!lat_ok) {
        *out->lat = 0.0;
    }
    if (!lon_ok) {
        *out->lon = 0.0;
    }

    return true;
}

static bool ml307_endpoint_parse_mgnss_status(at_arg_value_t *args, size_t count, void *user_data)
{
    int *status = (int *)user_data;

    if (!status || count < 1 || args[0].type != AT_ARG_TYPE_INT) {
        return false;
    }

    *status = args[0].data.int_val;
    return true;
}

static bool ml307_endpoint_exec_ok(ml307_endpoint_ctx_t *ctx, const char *cmd, uint32_t timeout_ms)
{
    return ctx && cmd && at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
        .cmd = cmd,
        .expect_urc = NULL,
        .parse = NULL,
        .timeout_ms = timeout_ms,
    }, NULL);
}

static bool ml307_endpoint_set_gnss_enabled(ml307_endpoint_ctx_t *ctx, bool enabled)
{
    return ml307_endpoint_exec_ok(ctx, enabled ? "AT+MGNSS=1" : "AT+MGNSS=0", 2000U);
}

static bool ml307_endpoint_query_gnss_status(ml307_endpoint_ctx_t *ctx, int *status)
{
    if (!ctx || !status) {
        return false;
    }

    *status = 0;
    return at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
        .cmd = "AT+MGNSS?",
        .expect_urc = "MGNSS",
        .parse = ml307_endpoint_parse_mgnss_status,
        .timeout_ms = 1000U,
    }, status);
}

static bool ml307_endpoint_refresh_gnss_running(ml307_endpoint_ctx_t *ctx)
{
    int status = 0;

    if (!ml307_endpoint_query_gnss_status(ctx, &status)) {
        return false;
    }

    ctx->gnss_running = ml307_endpoint_gnss_status_is_running(status);
    if (ctx->gnss_running) {
        xEventGroupSetBits(ctx->event_group, ML307_GNSS_EVENT_RUNNING);
    } else {
        xEventGroupClearBits(ctx->event_group, ML307_GNSS_EVENT_RUNNING);
    }
    return ctx->gnss_running;
}

static bool ml307_endpoint_wait_gnss_running(ml307_endpoint_ctx_t *ctx)
{
    TickType_t timeout_ticks;
    EventBits_t bits;

    if (!ctx) {
        return false;
    }

    timeout_ticks = pdMS_TO_TICKS(ML307_GNSS_START_TIMEOUT_MS);
    bits = xEventGroupWaitBits(ctx->event_group,
                               ML307_GNSS_EVENT_RUNNING,
                               pdFALSE,
                               pdFALSE,
                               timeout_ticks);
    if ((bits & ML307_GNSS_EVENT_RUNNING) != 0U) {
        if (ML307_GNSS_STATE_SETTLE_MS > 0U) {
            vTaskDelay(pdMS_TO_TICKS(ML307_GNSS_STATE_SETTLE_MS));
        }
        return true;
    }

    return ml307_endpoint_refresh_gnss_running(ctx);
}

bool ml307_endpoint_start_gnss(ml307_endpoint_ctx_t *ctx)
{
    if (!ctx) {
        return false;
    }

    if (ctx->gnss_running) {
        return true;
    }

    if (!ml307_endpoint_exec_ok(ctx, "AT+MGNSSCFG=\"nmea/mask\",63", 2000U)) {
        return false;
    }

    if (!ml307_endpoint_refresh_gnss_running(ctx)) {
        xEventGroupClearBits(ctx->event_group, ML307_GNSS_EVENT_RUNNING);
        if (!ml307_endpoint_set_gnss_enabled(ctx, true)) {
            return false;
        }
        if (!ml307_endpoint_wait_gnss_running(ctx)) {
            return false;
        }
    }

    return true;
}

bool ml307_endpoint_get_gps_location(ml307_endpoint_ctx_t *ctx, double *lat, double *lon)
{
    int fix = 0;
    ml307_gps_out_t out = { lat, lon, &fix };

    if (!ctx || !lat || !lon) {
        return false;
    }

    *lat = 0.0;
    *lon = 0.0;

    if (!at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
        .cmd = "AT+MGNSSLOC",
        .expect_urc = "MGNSSLOC",
        .parse = ml307_endpoint_parse_mgnssloc,
        .timeout_ms = 2000U,
    }, &out)) {
        return false;
    }

    return fix == 2 || fix == 3;
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
    if (!connected && ctx) {
        ctx->status.last_error = lisa_modem_cme_may_indicate_traffic_exceeded(endpoint->last_error)
                               ? LISA_MODEM_ERR_TRAFFIC_EXCEEDED
                               : LISA_MODEM_ERR_AT_COMMAND_FAILED;
    }
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
    int ret;

    if (!endpoint || !endpoint->in_use || !data || length == 0) {
        return -1;
    }

    if (endpoint->protocol == IPPROTO_UDP && host) {
        endpoint->send_timeout_ms = timeout_ms;
        ret = ml307_at_cmd_sendto(endpoint, host, port, (const char *)data, length);
        if (ret < 0 && ctx) {
            ctx->status.last_error = lisa_modem_cme_may_indicate_traffic_exceeded(endpoint->last_error)
                                   ? LISA_MODEM_ERR_TRAFFIC_EXCEEDED
                                   : LISA_MODEM_ERR_AT_COMMAND_FAILED;
        }
        return ret;
    }

    if (endpoint->protocol == IPPROTO_UDP) {
        if (!endpoint->udp_peer.valid) {
            errno = ENOTCONN;
            return -1;
        }

        if (!endpoint->connected &&
            !ml307_at_cmd_connect(endpoint, endpoint->udp_peer.addr.host, (int)endpoint->udp_peer.addr.port)) {
            ctx->status.last_error = lisa_modem_cme_may_indicate_traffic_exceeded(endpoint->last_error)
                                   ? LISA_MODEM_ERR_TRAFFIC_EXCEEDED
                                   : LISA_MODEM_ERR_AT_COMMAND_FAILED;
            return -1;
        }
    }

    endpoint->send_timeout_ms = timeout_ms;
    ret = ml307_at_cmd_send(endpoint, (const char *)data, length);
    if (ret < 0 && ctx) {
        ctx->status.last_error = lisa_modem_cme_may_indicate_traffic_exceeded(endpoint->last_error)
                               ? LISA_MODEM_ERR_TRAFFIC_EXCEEDED
                               : LISA_MODEM_ERR_AT_COMMAND_FAILED;
    }
    return ret;
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
    uint32_t ring_before;
    size_t hint_before;

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
    ring_before = ring_buf_size_get(&endpoint->ring_buf);
    hint_before = endpoint->rx_hint.available_data_len;

    if (endpoint->protocol == IPPROTO_TCP &&
        ring_before == 0U &&
        hint_before > 0U) {
        if (endpoint->prefetching && endpoint->data_sem) {
            ml307_endpoint_wait_prefetch_data(endpoint);
        }
        if (ring_buf_size_get(&endpoint->ring_buf) == 0U &&
            !endpoint->prefetching &&
            endpoint->rx_hint.available_data_len > 0U) {
            (void)ml307_at_cmd_prefetch(endpoint);
        }
    }

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
        } else if (from && endpoint->udp_peer.valid) {
            *from = endpoint->udp_peer.addr;
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

static bool ml307_endpoint_driver_get_imei(void *driver_ctx, char *imei, size_t size)
{
    return driver_ctx ? ml307_endpoint_get_imei((ml307_endpoint_ctx_t *)driver_ctx, imei, size) : false;
}

static bool ml307_endpoint_driver_get_iccid(void *driver_ctx, char *iccid, size_t size)
{
    return driver_ctx ? ml307_endpoint_get_iccid((ml307_endpoint_ctx_t *)driver_ctx, iccid, size) : false;
}

static bool ml307_endpoint_driver_get_signal_quality(void *driver_ctx, int *rssi, int *ber)
{
    return driver_ctx
         ? ml307_endpoint_get_signal_quality((ml307_endpoint_ctx_t *)driver_ctx, rssi, ber)
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

static bool ml307_endpoint_driver_start_gnss(void *driver_ctx)
{
    return driver_ctx
         ? ml307_endpoint_start_gnss((ml307_endpoint_ctx_t *)driver_ctx)
         : false;
}

static bool ml307_endpoint_driver_get_gps_location(void *driver_ctx, double *lat, double *lon)
{
    return driver_ctx
         ? ml307_endpoint_get_gps_location((ml307_endpoint_ctx_t *)driver_ctx, lat, lon)
         : false;
}

static void ml307_endpoint_driver_get_status(void *driver_ctx, lisa_modem_status_t *status)
{
    ml307_endpoint_ctx_t *ctx = (ml307_endpoint_ctx_t *)driver_ctx;

    if (!ctx || !status) {
        return;
    }

    *status = ctx->status;
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
    .get_status = ml307_endpoint_driver_get_status,
    .dns_resolve = ml307_endpoint_driver_dns_resolve,
    .get_imei = ml307_endpoint_driver_get_imei,
    .get_iccid = ml307_endpoint_driver_get_iccid,
    .get_signal_quality = ml307_endpoint_driver_get_signal_quality,
    .start_gnss = ml307_endpoint_driver_start_gnss,
    .get_gps_location = ml307_endpoint_driver_get_gps_location,
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
