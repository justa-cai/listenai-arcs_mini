/**
 * @file at_client.c
 * @brief AT Command Client Implementation
 * @details Multi-instance AT protocol handler.
 *          Extracted from at_uart.c - pure AT protocol logic,
 *          no hardware dependency.
 */

#include "at_client.h"
#include "at_mem.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#define TAG "at_client"
#include "lisa_log.h"
#include "lisa_modem_perf_log.h"

/* ===== AT 指令发送暂停锁 ===== */
static SemaphoreHandle_t g_at_tx_pause_mutex = NULL;

void at_cmd_tx_pause(void)
{
    if (!g_at_tx_pause_mutex) {
        g_at_tx_pause_mutex = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(g_at_tx_pause_mutex, portMAX_DELAY);
}

void at_cmd_tx_resume(void)
{
    if (g_at_tx_pause_mutex) {
        xSemaphoreGive(g_at_tx_pause_mutex);
    }
}

void at_cmd_tx_wait(void)
{
    if (g_at_tx_pause_mutex) {
        xSemaphoreTake(g_at_tx_pause_mutex, portMAX_DELAY);
        xSemaphoreGive(g_at_tx_pause_mutex);
    }
}

/* Bit position macros */
#ifndef BIT0
#define BIT0    (1 << 0)
#define BIT1    (1 << 1)
#define BIT2    (1 << 2)
#endif

/* AT Response Constants */
#define AT_RESPONSE_OK_LEN      4   /* "OK\r\n" */
#define AT_RESPONSE_ERROR_LEN   7   /* "ERROR\r\n" */
#define AT_RESPONSE_CRLF_LEN    2   /* "\r\n" */
#define AT_IO_LOG_PREVIEW_LEN   192
#define AT_PERF_TASK_NAME_LEN   16
#define AT_PERF_CMD_PREVIEW_LEN 48
#define AT_CLIENT_RX_TASK_TIMEOUT_MS  100U
#define AT_CLIENT_RX_CHUNK_SIZE       1024U
#define AT_CLIENT_STREAM_PROBE_SIZE   128U

/* Event bits */
#define AT_EVENT_COMMAND_DONE   BIT0
#define AT_EVENT_COMMAND_ERROR  BIT1
#define AT_EVENT_DATA_PROMPT    BIT2
#define AT_EVENT_URC_MATCHED    (1 << 3)

/* Max argument item size for parsing */
#define MAX_ARG_ITEM_SIZE       10240

/**
 * @brief AT client instance structure
 */
struct at_client {
    at_transport_t *transport;              /**< Bound transport */

    /* Dynamic receive buffer (auto-grow, sliding window) */
    char *rx_buffer;
    size_t rx_buffer_head;
    size_t rx_buffer_size;
    size_t rx_buffer_capacity;
    size_t rx_scan_offset;
    char *line_buffer;
    size_t line_buffer_capacity;
    char stream_probe_buffer[AT_CLIENT_STREAM_PROBE_SIZE];
    size_t stream_probe_len;

    /* Response buffer */
    char *response;
    size_t response_buf_size;
    int cme_error_code;

    /* Synchronization */
    SemaphoreHandle_t tx_mutex;             /**< TX mutex for thread safety */
    SemaphoreHandle_t cmd_mutex;            /**< Command mutex */
    SemaphoreHandle_t buffer_mutex;         /**< Buffer mutex */
    SemaphoreHandle_t urc_mutex;            /**< URC callback list mutex */
    EventGroupHandle_t event_group;         /**< Event group for command sync */
    TaskHandle_t rx_task;                   /**< RX pump task */

    /* URC callback list */
    at_urc_callback_node_t *urc_head;

    /* URC wait state */
    const char *expect_urc;             /**< Expected URC command name (NULL = not waiting) */
    at_arg_value_t *matched_args;       /**< Matched URC arguments (output) */
    size_t matched_arg_count;           /**< Matched URC argument count */

    /* Flags */
    bool initialized;
    bool wait_for_response;
    bool capture_prefixed_response;
    const char *binary_response_urc;
    uint8_t *binary_response;
    size_t binary_response_size;
    size_t binary_response_len;
    size_t binary_response_expected;
    size_t binary_response_received;
    bool binary_response_active;
    bool binary_response_ready;
    bool binary_response_overflow;
    bool debug;
    volatile bool rx_task_running;
    uint8_t task_priority;
    uint16_t task_stack_size;
    char perf_cmd_owner_task[AT_PERF_TASK_NAME_LEN];
    char perf_cmd_preview[AT_PERF_CMD_PREVIEW_LEN];
    uint32_t perf_last_cmd_wait_ms;
    TickType_t perf_last_ok_tick;
    uint32_t perf_last_ok_wait_resume_ms;

    at_line_stream_handler_t line_stream_handler;
    void *line_stream_user_data;
    void *line_stream_claim_ctx;
    bool line_stream_active;
    bool line_stream_drop_until_eol;
    bool rx_pending_cr;
};

typedef enum {
    AT_CLIENT_FRAME_NONE = 0,
    AT_CLIENT_FRAME_PROMPT,
    AT_CLIENT_FRAME_LINE,
} at_client_frame_kind_t;

/* ===== Internal Helper Functions ===== */

static bool is_number(const char *s)
{
    if (!s || *s == '\0') return false;
    size_t len = strlen(s);
    if (len >= 10) return false;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)s[i])) return false;
    }
    return true;
}

static char *at_client_strdup(const char *src)
{
    size_t len;
    char *dst;

    if (!src) {
        return NULL;
    }

    len = strlen(src) + 1;
    dst = (char *)at_mem_alloc(len);
    if (dst) {
        memcpy(dst, src, len);
    }

    return dst;
}

static void at_client_format_log_data(const uint8_t *data, size_t len,
                                      char *out, size_t out_size)
{
    size_t src_index = 0;
    size_t dst_index = 0;
    size_t preview_len;

    if (!out || out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (!data || len == 0) {
        return;
    }

    preview_len = len > AT_IO_LOG_PREVIEW_LEN ? AT_IO_LOG_PREVIEW_LEN : len;

    while (src_index < preview_len && dst_index + 1 < out_size) {
        unsigned char ch = data[src_index++];

        if (ch == '\r') {
            if (dst_index + 2 >= out_size) break;
            out[dst_index++] = '\\';
            out[dst_index++] = 'r';
            continue;
        }
        if (ch == '\n') {
            if (dst_index + 2 >= out_size) break;
            out[dst_index++] = '\\';
            out[dst_index++] = 'n';
            continue;
        }
        if (ch == '\t') {
            if (dst_index + 2 >= out_size) break;
            out[dst_index++] = '\\';
            out[dst_index++] = 't';
            continue;
        }
        if (!isprint(ch)) {
            int written;
            if (dst_index + 4 >= out_size) break;
            written = snprintf(out + dst_index, out_size - dst_index, "\\x%02X", ch);
            if (written <= 0 || (size_t)written >= out_size - dst_index) break;
            dst_index += (size_t)written;
            continue;
        }

        out[dst_index++] = (char)ch;
    }

    if (len > preview_len && dst_index + 3 < out_size) {
        out[dst_index++] = '.';
        out[dst_index++] = '.';
        out[dst_index++] = '.';
    }

    out[dst_index] = '\0';
}

static void at_client_log_io(const at_client_t *client, const char *direction,
                             const uint8_t *data, size_t len)
{
    char preview[AT_IO_LOG_PREVIEW_LEN * 4] = {0};

    if (!client || !client->debug || !direction || !data || len == 0) {
        return;
    }

    at_client_format_log_data(data, len, preview, sizeof(preview));
    LISA_LOGI(TAG, "%s[%u]: %s", direction, (unsigned int)len, preview);
}

static uint32_t at_client_tick_elapsed_ms(TickType_t start, TickType_t end)
{
    return (uint32_t)((end - start) * portTICK_PERIOD_MS);
}

static void at_client_perf_copy_preview(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (!dst || dst_size == 0U) {
        return;
    }

    dst[0] = '\0';
    if (!src) {
        return;
    }

    len = strcspn(src, "\r\n");
    if (len >= dst_size) {
        len = dst_size - 1U;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
}

static const char *at_client_perf_task_name(void)
{
    const char *task_name = pcTaskGetName(NULL);

    return task_name ? task_name : "?";
}

static void at_client_perf_set_cmd_owner(at_client_t *client, const char *command)
{
    if (!client) {
        return;
    }

    at_client_perf_copy_preview(client->perf_cmd_owner_task,
                                sizeof(client->perf_cmd_owner_task),
                                at_client_perf_task_name());
    at_client_perf_copy_preview(client->perf_cmd_preview,
                                sizeof(client->perf_cmd_preview),
                                command);
}

static void at_client_perf_clear_cmd_owner(at_client_t *client)
{
    if (!client) {
        return;
    }

    client->perf_cmd_owner_task[0] = '\0';
    client->perf_cmd_preview[0] = '\0';
}

static void at_client_perf_take_cmd_mutex(at_client_t *client, const char *command)
{
    TickType_t t_mutex_wait;
    TickType_t t_mutex_got;
    char owner_task[AT_PERF_TASK_NAME_LEN];
    char owner_cmd[AT_PERF_CMD_PREVIEW_LEN];
    const char *waiter_task;
    uint32_t wait_ms;

    if (!client) {
        return;
    }

    waiter_task = at_client_perf_task_name();
    memcpy(owner_task, client->perf_cmd_owner_task, sizeof(owner_task));
    memcpy(owner_cmd, client->perf_cmd_preview, sizeof(owner_cmd));

    t_mutex_wait = xTaskGetTickCount();
    xSemaphoreTake(client->cmd_mutex, portMAX_DELAY);
    t_mutex_got = xTaskGetTickCount();

    at_client_perf_set_cmd_owner(client, command);

    wait_ms = at_client_tick_elapsed_ms(t_mutex_wait, t_mutex_got);
    client->perf_last_cmd_wait_ms = wait_ms;
    if (wait_ms > 10U) {
        LISA_MODEM_PERF_LOGW(TAG, "cmd_mutex contention: waited=%ums waiter=%s owner=%s cmd=%s",
                             wait_ms,
                             waiter_task,
                             owner_task[0] != '\0' ? owner_task : "?",
                             owner_cmd[0] != '\0' ? owner_cmd : "?");
    }
}

static void at_client_perf_give_cmd_mutex(at_client_t *client)
{
    if (!client) {
        return;
    }

    at_client_perf_clear_cmd_owner(client);
    xSemaphoreGive(client->cmd_mutex);
}

static int at_client_send_locked(at_client_t *client, const char *direction,
                                 const uint8_t *data, size_t len)
{
    int ret;

    if (!client || !client->transport || !data || len == 0U) {
        return -1;
    }

    at_client_log_io(client, direction, data, len);
    ret = at_transport_send(client->transport, data, len);
    return ret < 0 ? ret : 0;
}

static int at_client_send_command_locked(at_client_t *client, const char *command, bool add_crlf)
{
    static const uint8_t crlf[] = {'\r', '\n'};
    size_t cmd_len;
    int ret;

    if (!client || !command) {
        return -1;
    }

    cmd_len = strlen(command);
    if (cmd_len == 0U) {
        return 0;
    }

    ret = at_client_send_locked(client, "TX", (const uint8_t *)command, cmd_len);
    if (ret < 0 || !add_crlf) {
        return ret;
    }

    return at_client_send_locked(client, "TX", crlf, sizeof(crlf));
}

static bool at_client_ensure_line_buffer(at_client_t *client, size_t required_len)
{
    size_t required_capacity;
    char *new_buffer;

    if (!client) {
        return false;
    }

    required_capacity = required_len + 1U;
    if (required_capacity <= client->line_buffer_capacity) {
        return true;
    }

    new_buffer = (char *)at_mem_realloc(client->line_buffer, required_capacity + 128U);
    if (!new_buffer) {
        LISA_LOGE(TAG, "Failed to grow line buffer");
        return false;
    }

    client->line_buffer = new_buffer;
    client->line_buffer_capacity = required_capacity + 128U;
    return true;
}

static bool at_client_has_line_stream_handler(const at_client_t *client)
{
    return client && client->line_stream_handler.claim && client->line_stream_handler.consume;
}

static void at_client_reset_line_stream_probe(at_client_t *client)
{
    if (!client) {
        return;
    }

    client->stream_probe_len = 0U;
}

static void at_client_finish_line_stream(at_client_t *client, bool success)
{
    void *claim_ctx;

    if (!client) {
        return;
    }

    claim_ctx = client->line_stream_claim_ctx;
    if (client->line_stream_active && client->line_stream_handler.finish) {
        client->line_stream_handler.finish(claim_ctx, success, client->line_stream_user_data);
    }

    client->line_stream_claim_ctx = NULL;
    client->line_stream_active = false;
    if (success) {
        client->line_stream_drop_until_eol = false;
    }
}

static bool at_client_line_stream_is_complete(at_client_t *client)
{
    if (!client || !client->line_stream_active) {
        return true;
    }

    if (!client->line_stream_handler.is_complete) {
        return true;
    }

    return client->line_stream_handler.is_complete(client->line_stream_claim_ctx,
                                                   client->line_stream_user_data);
}

static bool at_client_line_stream_raw_mode(at_client_t *client)
{
    if (!client || !client->line_stream_active ||
        !client->line_stream_handler.raw_mode) {
        return false;
    }

    return client->line_stream_handler.raw_mode(client->line_stream_claim_ctx,
                                                client->line_stream_user_data);
}

static bool at_client_line_stream_feed_crlf_if_needed(at_client_t *client)
{
    static const uint8_t crlf[] = {'\r', '\n'};

    if (at_client_line_stream_is_complete(client)) {
        return true;
    }

    return client->line_stream_handler.consume(client->line_stream_claim_ctx,
                                               crlf, sizeof(crlf), false,
                                               client->line_stream_user_data) >= 0;
}

static bool at_client_line_stream_consume(at_client_t *client, const uint8_t *data,
                                          size_t len, bool line_end,
                                          size_t *consumed_out)
{
    int consumed;

    if (consumed_out) {
        *consumed_out = 0U;
    }
    if (!client || !client->line_stream_active || (!data && len > 0U)) {
        return false;
    }
    if (len == 0U) {
        return true;
    }

    consumed = client->line_stream_handler.consume(client->line_stream_claim_ctx,
                                                   data, len, line_end,
                                                   client->line_stream_user_data);
    if (consumed < 0 || (size_t)consumed > len) {
        return false;
    }
    if (consumed_out) {
        *consumed_out = (size_t)consumed;
    }
    return true;
}

static void at_client_reset_rx_window_locked(at_client_t *client)
{
    if (!client) {
        return;
    }

    client->rx_buffer_head = 0U;
    client->rx_buffer_size = 0U;
    client->rx_scan_offset = 0U;
    if (client->rx_buffer && client->rx_buffer_capacity > 0U) {
        client->rx_buffer[0] = '\0';
    }
}

static void at_client_compact_rx_window_locked(at_client_t *client)
{
    if (!client || !client->rx_buffer) {
        return;
    }

    if (client->rx_buffer_size == 0U) {
        at_client_reset_rx_window_locked(client);
        return;
    }

    if (client->rx_buffer_head == 0U) {
        return;
    }

    memmove(client->rx_buffer,
            client->rx_buffer + client->rx_buffer_head,
            client->rx_buffer_size);
    client->rx_buffer_head = 0U;
    client->rx_buffer[client->rx_buffer_size] = '\0';
}

static bool at_client_ensure_rx_window_locked(at_client_t *client, size_t append_len)
{
    char *new_buffer;
    size_t required_capacity;

    if (!client) {
        return false;
    }

    required_capacity = client->rx_buffer_size + append_len + 1U;
    if (client->rx_buffer_head + required_capacity <= client->rx_buffer_capacity) {
        return true;
    }

    at_client_compact_rx_window_locked(client);
    if (required_capacity <= client->rx_buffer_capacity) {
        return true;
    }

    client->rx_buffer_capacity = required_capacity + 512U;
    new_buffer = (char *)at_mem_realloc(client->rx_buffer, client->rx_buffer_capacity);
    if (!new_buffer) {
        LISA_LOGE(TAG, "Failed to reallocate rx_buffer");
        return false;
    }

    client->rx_buffer = new_buffer;
    return true;
}

static bool at_client_append_rx_locked(at_client_t *client, const uint8_t *data, int len)
{
    if (!client || !data || len <= 0) {
        return false;
    }

    if (!at_client_ensure_rx_window_locked(client, (size_t)len)) {
        return false;
    }

    memcpy(client->rx_buffer + client->rx_buffer_head + client->rx_buffer_size,
           data, (size_t)len);
    client->rx_buffer_size += (size_t)len;
    client->rx_buffer[client->rx_buffer_head + client->rx_buffer_size] = '\0';
    return true;
}

static bool at_client_append_rx_pair_locked(at_client_t *client,
                                            const uint8_t *first, size_t first_len,
                                            const uint8_t *second, size_t second_len)
{
    size_t total_len;

    if (!client) {
        return false;
    }

    total_len = first_len + second_len;
    if (total_len == 0U) {
        return true;
    }

    if (!at_client_ensure_rx_window_locked(client, total_len)) {
        return false;
    }

    if (first && first_len > 0U) {
        memcpy(client->rx_buffer + client->rx_buffer_head + client->rx_buffer_size,
               first, first_len);
        client->rx_buffer_size += first_len;
    }
    if (second && second_len > 0U) {
        memcpy(client->rx_buffer + client->rx_buffer_head + client->rx_buffer_size,
               second, second_len);
        client->rx_buffer_size += second_len;
    }

    client->rx_buffer[client->rx_buffer_head + client->rx_buffer_size] = '\0';
    return true;
}

static bool at_client_append_rx_line_fragment_locked(at_client_t *client,
                                                     const uint8_t *data, size_t len,
                                                     bool line_end)
{
    static const uint8_t crlf[] = {'\r', '\n'};

    if (!client) {
        return false;
    }

    if (line_end) {
        return at_client_append_rx_pair_locked(client, data, len, crlf, sizeof(crlf));
    }

    if (len == 0U) {
        return true;
    }

    return at_client_append_rx_locked(client, data, (int)len);
}

static at_client_frame_kind_t at_client_extract_frame_locked(at_client_t *client, size_t *line_len)
{
    size_t current_line_len;
    size_t bytes_to_remove;
    size_t search_offset;
    char *window;

    if (line_len) {
        *line_len = 0U;
    }
    if (!client || !client->rx_buffer || client->rx_buffer_size == 0U) {
        return AT_CLIENT_FRAME_NONE;
    }

    while (client->rx_buffer_size > 0U) {
        window = client->rx_buffer + client->rx_buffer_head;

        if (client->wait_for_response && window[0] == '>') {
            client->rx_buffer_head++;
            client->rx_buffer_size--;
            if (client->rx_buffer_size == 0U) {
                at_client_reset_rx_window_locked(client);
            } else {
                if (client->rx_scan_offset > 0U) {
                    client->rx_scan_offset--;
                }
                client->rx_buffer[client->rx_buffer_head + client->rx_buffer_size] = '\0';
            }
            return AT_CLIENT_FRAME_PROMPT;
        }

        search_offset = client->rx_scan_offset;
        while (search_offset + 1U < client->rx_buffer_size) {
            if (window[search_offset] == '\r' && window[search_offset + 1U] == '\n') {
                break;
            }
            search_offset++;
        }

        if (search_offset + 1U >= client->rx_buffer_size) {
            client->rx_scan_offset = (client->rx_buffer_size > 0U && window[client->rx_buffer_size - 1U] == '\r')
                                   ? (client->rx_buffer_size - 1U)
                                   : client->rx_buffer_size;
            return AT_CLIENT_FRAME_NONE;
        }

        current_line_len = search_offset;
        bytes_to_remove = current_line_len + AT_RESPONSE_CRLF_LEN;

        if (current_line_len == 0U) {
            client->rx_buffer_head += bytes_to_remove;
            client->rx_buffer_size -= bytes_to_remove;
            client->rx_scan_offset = 0U;
            if (client->rx_buffer_size == 0U) {
                at_client_reset_rx_window_locked(client);
            } else {
                client->rx_buffer[client->rx_buffer_head + client->rx_buffer_size] = '\0';
            }
            continue;
        }

        if (!at_client_ensure_line_buffer(client, current_line_len)) {
            return AT_CLIENT_FRAME_NONE;
        }

        memcpy(client->line_buffer, window, current_line_len);
        client->line_buffer[current_line_len] = '\0';

        client->rx_buffer_head += bytes_to_remove;
        client->rx_buffer_size -= bytes_to_remove;
        client->rx_scan_offset = 0U;
        if (client->rx_buffer_size == 0U) {
            at_client_reset_rx_window_locked(client);
        } else {
            client->rx_buffer[client->rx_buffer_head + client->rx_buffer_size] = '\0';
        }

        if (line_len) {
            *line_len = current_line_len;
        }
        return AT_CLIENT_FRAME_LINE;
    }

    return AT_CLIENT_FRAME_NONE;
}

static void handle_urc(at_client_t *client, const char *command,
                        at_arg_value_t *arguments, size_t arg_count)
{
    if (!command) return;

    /* Special handling for CME ERROR */
    if (strcmp(command, "CME ERROR") == 0 && arg_count > 0) {
        if (arguments[0].type == AT_ARG_TYPE_INT) {
            client->cme_error_code = arguments[0].data.int_val;
            LISA_LOGE(TAG, "CME ERROR received, error_code=%d", client->cme_error_code);
            xEventGroupSetBits(client->event_group, AT_EVENT_COMMAND_ERROR);
        }
        return;
    }

    if (client->binary_response_urc && strcmp(command, client->binary_response_urc) == 0 &&
        arg_count > 0U && arguments && arguments[0].type == AT_ARG_TYPE_INT) {
        int expected = arguments[0].data.int_val;

        if (expected < 0) {
            client->binary_response_overflow = true;
        } else {
            client->binary_response_expected = (size_t)expected;
            client->binary_response_received = 0U;
            client->binary_response_len = 0U;
            client->binary_response_ready = (expected == 0);
            client->binary_response_active = (expected > 0);
        }
    }

    /* Check if this URC matches the expected one */
    if (client->expect_urc && strcmp(command, client->expect_urc) == 0) {
        /* Clone arguments for the caller */
        if (arguments && arg_count > 0) {
            at_arg_value_t *cloned = (at_arg_value_t *)at_mem_calloc(arg_count, sizeof(at_arg_value_t));
            if (cloned) {
                for (size_t i = 0; i < arg_count; i++) {
                    cloned[i].type = arguments[i].type;
                    if (arguments[i].type == AT_ARG_TYPE_STRING && arguments[i].data.string_val.value) {
                        cloned[i].data.string_val.value = at_client_strdup(arguments[i].data.string_val.value);
                        cloned[i].data.string_val.len = arguments[i].data.string_val.len;
                    } else {
                        cloned[i].data = arguments[i].data;
                    }
                }
                client->matched_args = cloned;
                client->matched_arg_count = arg_count;
            }
        } else {
            client->matched_args = NULL;
            client->matched_arg_count = 0;
        }
        xEventGroupSetBits(client->event_group, AT_EVENT_URC_MATCHED);
    }

    /* Call all registered URC callbacks without blocking rx_buffer access. */
    xSemaphoreTake(client->urc_mutex, portMAX_DELAY);
    at_urc_callback_node_t *node = client->urc_head;
    while (node) {
        if (node->callback) {
            node->callback(command, arguments, arg_count, node->user_data);
        }
        node = node->next;
    }
    xSemaphoreGive(client->urc_mutex);
}

static void parse_arguments(const char *values, at_arg_value_t **out_args, size_t *out_count)
{
    if (!values || !out_args || !out_count) {
        if (out_args) *out_args = NULL;
        if (out_count) *out_count = 0;
        return;
    }

    size_t capacity = 4;
    at_arg_value_t *args = (at_arg_value_t *)at_mem_calloc(capacity, sizeof(at_arg_value_t));
    size_t count = 0;

    const char *p = values;
    const char *start = p;

    char *item = (char *)at_mem_alloc(MAX_ARG_ITEM_SIZE);
    if (!item) {
        at_mem_free(args);
        *out_args = NULL;
        *out_count = 0;
        return;
    }

    while (*p) {
        if (*p == ',' || *p == '\r' || *p == '\n' || *p == '\0') {
            size_t item_len = p - start;

            if (item_len >= MAX_ARG_ITEM_SIZE) {
                item_len = MAX_ARG_ITEM_SIZE - 1;
            }

            if (count >= capacity) {
                capacity *= 2;
                at_arg_value_t *new_args = (at_arg_value_t *)at_mem_realloc(args, capacity * sizeof(at_arg_value_t));
                if (new_args) {
                    args = new_args;
                } else {
                    at_arg_array_destroy(args, count);
                    at_mem_free(item);
                    *out_args = NULL;
                    *out_count = 0;
                    return;
                }
            }

            memcpy(item, start, item_len);
            item[item_len] = '\0';

            /* Trim whitespace */
            char *trimmed = item;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            char *end = trimmed + strlen(trimmed) - 1;
            while (end > trimmed && (*end == ' ' || *end == '\t')) *end-- = '\0';

            /* Identify type */
            if (strlen(trimmed) == 0) {
                args[count].type = AT_ARG_TYPE_STRING;
                args[count].data.string_val.value = at_client_strdup("");
                args[count].data.string_val.len = 0;
            } else if (trimmed[0] == '"' && strlen(trimmed) > 1) {
                args[count].type = AT_ARG_TYPE_STRING;
                size_t str_len = strlen(trimmed) - 2;
                args[count].data.string_val.value = (char *)at_mem_alloc(str_len + 1);
                if (args[count].data.string_val.value) {
                    memcpy(args[count].data.string_val.value, trimmed + 1, str_len);
                    args[count].data.string_val.value[str_len] = '\0';
                    args[count].data.string_val.len = str_len;
                }
            } else if (is_number(trimmed)) {
                args[count].type = AT_ARG_TYPE_INT;
                args[count].data.int_val = atoi(trimmed);
            } else {
                args[count].type = AT_ARG_TYPE_STRING;
                args[count].data.string_val.value = at_client_strdup(trimmed);
                args[count].data.string_val.len = strlen(trimmed);
            }

            count++;

            if (*p == '\r' || *p == '\n' || *p == '\0') break;
            start = p + 1;
        }
        p++;
    }

    /* Handle last argument if string doesn't end with delimiter */
    if (p > start) {
        size_t item_len = p - start;
        if (item_len >= MAX_ARG_ITEM_SIZE) {
            item_len = MAX_ARG_ITEM_SIZE - 1;
        }

        if (count >= capacity) {
            capacity *= 2;
            at_arg_value_t *new_args = (at_arg_value_t *)at_mem_realloc(args, capacity * sizeof(at_arg_value_t));
            if (new_args) {
                args = new_args;
            } else {
                at_arg_array_destroy(args, count);
                at_mem_free(item);
                *out_args = NULL;
                *out_count = 0;
                return;
            }
        }

        memcpy(item, start, item_len);
        item[item_len] = '\0';

        char *trimmed = item;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        char *end = trimmed + strlen(trimmed) - 1;
        while (end > trimmed && (*end == ' ' || *end == '\t')) *end-- = '\0';

        if (strlen(trimmed) == 0) {
            args[count].type = AT_ARG_TYPE_STRING;
            args[count].data.string_val.value = at_client_strdup("");
            args[count].data.string_val.len = 0;
        } else if (trimmed[0] == '"' && strlen(trimmed) > 1) {
            args[count].type = AT_ARG_TYPE_STRING;
            size_t str_len = strlen(trimmed) - 2;
            args[count].data.string_val.value = (char *)at_mem_alloc(str_len + 1);
            if (args[count].data.string_val.value) {
                memcpy(args[count].data.string_val.value, trimmed + 1, str_len);
                args[count].data.string_val.value[str_len] = '\0';
                args[count].data.string_val.len = str_len;
            }
        } else if (is_number(trimmed)) {
            args[count].type = AT_ARG_TYPE_INT;
            args[count].data.int_val = atoi(trimmed);
        } else {
            args[count].type = AT_ARG_TYPE_STRING;
            args[count].data.string_val.value = at_client_strdup(trimmed);
            args[count].data.string_val.len = strlen(trimmed);
        }

        count++;
    }

    at_mem_free(item);
    *out_args = args;
    *out_count = count;
}

static void at_client_process_line(at_client_t *client, const char *line, size_t line_len)
{
    bool is_echo_line;

    if (!client || !line || line_len == 0U) {
        return;
    }

    is_echo_line = (line[line_len - 1U] == '\r');

    if (line[0] == '+') {
        char command[64] = {0};
        const char *colon = strchr(line, ':');

        if (colon && (size_t)(colon - line) < line_len) {
            size_t cmd_len = (size_t)(colon - line) - 1U;

            if (cmd_len < sizeof(command)) {
                const char *values_start = NULL;
                size_t colon_offset;
                size_t values_len = 0U;
                char *values = NULL;
                at_arg_value_t *args = NULL;
                size_t arg_count = 0U;

                strncpy(command, line + 1, cmd_len);
                command[cmd_len] = '\0';

                colon_offset = (size_t)(colon - line);
                if (colon_offset + 2U < line_len) {
                    values_start = colon + 2;
                    values_len = line_len - (colon_offset + 2U);
                } else if (colon_offset + 1U < line_len) {
                    values_start = colon + 1;
                    values_len = line_len - (colon_offset + 1U);
                } else {
                    values_start = "";
                }

                if (values_len > 0U) {
                    values = (char *)at_mem_alloc(values_len + 1U);
                    if (values) {
                        memcpy(values, values_start, values_len);
                        values[values_len] = '\0';
                    } else {
                        values = at_client_strdup("");
                    }
                } else {
                    values = at_client_strdup("");
                }

                if (!values) {
                    return;
                }

                parse_arguments(values, &args, &arg_count);
                handle_urc(client, command, args, arg_count);
                at_arg_array_destroy(args, arg_count);
                at_mem_free(values);
            }
        } else if (line_len > 1U) {
            size_t cmd_len = line_len - 1U;

            if (cmd_len < sizeof(command)) {
                strncpy(command, line + 1, cmd_len);
                command[cmd_len] = '\0';
                handle_urc(client, command, NULL, 0);
            }
        }

        at_client_log_io(client, "RX-URC", (const uint8_t *)line, line_len);
    } else if (line_len == 2U && line[0] == 'O' && line[1] == 'K') {
        client->perf_last_ok_tick = xTaskGetTickCount();
        at_client_log_io(client, "RX-OK", (const uint8_t *)line, line_len);
        xEventGroupSetBits(client->event_group, AT_EVENT_COMMAND_DONE);
        return;
    } else if (line_len == 5U && memcmp(line, "ERROR", 5U) == 0) {
        at_client_log_io(client, "RX-ERR", (const uint8_t *)line, line_len);
        xEventGroupSetBits(client->event_group, AT_EVENT_COMMAND_ERROR);
        return;
    }

    if (!is_echo_line &&
        (line[0] != '+' || client->capture_prefixed_response)) {
        size_t copy_len = (line_len < client->response_buf_size - 1U)
                        ? line_len
                        : (client->response_buf_size - 1U);

        xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
        memcpy(client->response, line, copy_len);
        client->response[copy_len] = '\0';
        xSemaphoreGive(client->buffer_mutex);
    }

    at_client_log_io(client, "RX-LINE", (const uint8_t *)line, line_len);
}

static void at_client_consume_prompt(at_client_t *client)
{
    if (!client) {
        return;
    }

    xEventGroupSetBits(client->event_group, AT_EVENT_DATA_PROMPT);
}

static size_t at_client_capture_binary_response(at_client_t *client,
                                                const uint8_t *data, size_t len)
{
    size_t remaining;
    size_t consume_len;
    size_t copy_len;

    if (!client || !client->binary_response_active || !data || len == 0U) {
        return 0U;
    }

    if (client->binary_response_received >= client->binary_response_expected) {
        client->binary_response_active = false;
        client->binary_response_ready = true;
        return 0U;
    }

    remaining = client->binary_response_expected - client->binary_response_received;
    consume_len = len < remaining ? len : remaining;
    copy_len = consume_len;
    if (client->binary_response_len + copy_len > client->binary_response_size) {
        copy_len = client->binary_response_size > client->binary_response_len
                 ? client->binary_response_size - client->binary_response_len
                 : 0U;
        client->binary_response_overflow = true;
    }

    if (copy_len > 0U && client->binary_response) {
        memcpy(client->binary_response + client->binary_response_len, data, copy_len);
        client->binary_response_len += copy_len;
    }

    client->binary_response_received += consume_len;
    if (client->binary_response_received >= client->binary_response_expected) {
        client->binary_response_active = false;
        client->binary_response_ready = true;
    }

    return consume_len;
}

static void at_client_drain_buffered_frames(at_client_t *client)
{
    at_client_frame_kind_t frame_kind;
    size_t line_len = 0U;

    if (!client) {
        return;
    }

    for (;;) {
        xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
        frame_kind = at_client_extract_frame_locked(client, &line_len);
        xSemaphoreGive(client->buffer_mutex);

        if (frame_kind == AT_CLIENT_FRAME_NONE) {
            break;
        }

        if (frame_kind == AT_CLIENT_FRAME_PROMPT) {
            at_client_consume_prompt(client);
            continue;
        }

        at_client_process_line(client, client->line_buffer, line_len);
    }
}

static bool at_client_process_combined_line(at_client_t *client,
                                            const uint8_t *first, size_t first_len,
                                            const uint8_t *second, size_t second_len)
{
    size_t total_len;

    if (!client) {
        return false;
    }

    total_len = first_len + second_len;
    if (total_len == 0U) {
        return true;
    }

    if (!at_client_ensure_line_buffer(client, total_len)) {
        return false;
    }

    if (first && first_len > 0U) {
        memcpy(client->line_buffer, first, first_len);
    }
    if (second && second_len > 0U) {
        memcpy(client->line_buffer + first_len, second, second_len);
    }
    client->line_buffer[total_len] = '\0';
    at_client_process_line(client, client->line_buffer, total_len);
    return true;
}

static bool at_client_process_line_fragment(at_client_t *client, const uint8_t *data,
                                            size_t len, bool line_end)
{
    bool plain_buffered = false;
    size_t copied = 0U;
    size_t inspect_len;
    at_line_stream_claim_t claim = AT_LINE_STREAM_PASS;
    void *claim_ctx = NULL;

    if (!client) {
        return false;
    }

    if (client->line_stream_drop_until_eol) {
        if (line_end) {
            client->line_stream_drop_until_eol = false;
        }
        return true;
    }

    if (client->line_stream_active) {
        size_t consumed = 0U;

        if (!at_client_line_stream_consume(client, data, len, line_end, &consumed)) {
            at_client_finish_line_stream(client, false);
            client->line_stream_drop_until_eol = !line_end;
            return false;
        }
        if (consumed < len) {
            if (!at_client_line_stream_is_complete(client)) {
                at_client_finish_line_stream(client, false);
                client->line_stream_drop_until_eol = !line_end;
                return false;
            }
            at_client_finish_line_stream(client, true);
            at_client_reset_line_stream_probe(client);
            return at_client_process_line_fragment(client, data + consumed,
                                                   len - consumed, line_end);
        }

        if (line_end) {
            if (!at_client_line_stream_feed_crlf_if_needed(client)) {
                at_client_finish_line_stream(client, false);
                return false;
            }
            if (at_client_line_stream_is_complete(client)) {
                at_client_finish_line_stream(client, true);
                at_client_reset_line_stream_probe(client);
            }
        }
        return true;
    }

    xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
    plain_buffered = client->rx_buffer_size > 0U;
    xSemaphoreGive(client->buffer_mutex);
    if (plain_buffered) {
        xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
        if (!at_client_append_rx_line_fragment_locked(client, data, len, line_end)) {
            xSemaphoreGive(client->buffer_mutex);
            return false;
        }
        xSemaphoreGive(client->buffer_mutex);
        if (line_end) {
            at_client_drain_buffered_frames(client);
        }
        return true;
    }

    if (!at_client_has_line_stream_handler(client)) {
        if (line_end) {
            return at_client_process_combined_line(client, NULL, 0U, data, len);
        }

        xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
        plain_buffered = at_client_append_rx_locked(client, data, (int)len);
        xSemaphoreGive(client->buffer_mutex);
        return plain_buffered;
    }

    inspect_len = client->stream_probe_len;
    if (inspect_len < AT_CLIENT_STREAM_PROBE_SIZE && len > 0U) {
        copied = len;
        if (copied > AT_CLIENT_STREAM_PROBE_SIZE - inspect_len) {
            copied = AT_CLIENT_STREAM_PROBE_SIZE - inspect_len;
        }
        memcpy(client->stream_probe_buffer + inspect_len, data, copied);
        inspect_len += copied;
    }

    if (inspect_len > 0U) {
        claim = client->line_stream_handler.claim((const uint8_t *)client->stream_probe_buffer,
                                                  inspect_len, line_end,
                                                  client->line_stream_user_data,
                                                  &claim_ctx);
    } else if (!line_end) {
        claim = AT_LINE_STREAM_NEED_MORE;
    }

    if (claim == AT_LINE_STREAM_NEED_MORE && !line_end &&
        inspect_len < AT_CLIENT_STREAM_PROBE_SIZE) {
        client->stream_probe_len = inspect_len;
        return true;
    }

    if (claim == AT_LINE_STREAM_CLAIM && inspect_len > 0U) {
        client->line_stream_claim_ctx = claim_ctx;
        client->line_stream_active = true;
        client->stream_probe_len = 0U;

        size_t consumed = 0U;
        if (!at_client_line_stream_consume(client,
                                           (const uint8_t *)client->stream_probe_buffer,
                                           inspect_len,
                                           line_end && copied == len,
                                           &consumed)) {
            at_client_finish_line_stream(client, false);
            client->line_stream_drop_until_eol = !line_end;
            return false;
        }
        if (consumed < inspect_len) {
            bool has_more = copied < len;

            if (!at_client_line_stream_is_complete(client)) {
                at_client_finish_line_stream(client, false);
                client->line_stream_drop_until_eol = !line_end;
                return false;
            }
            at_client_finish_line_stream(client, true);
            at_client_reset_line_stream_probe(client);
            if (!at_client_process_line_fragment(client,
                                                 (const uint8_t *)client->stream_probe_buffer + consumed,
                                                 inspect_len - consumed,
                                                 line_end && !has_more)) {
                return false;
            }
            if (has_more) {
                return at_client_process_line_fragment(client, data + copied,
                                                       len - copied, line_end);
            }
            return true;
        }

        if (copied < len) {
            size_t tail_consumed = 0U;

            if (!at_client_line_stream_consume(client, data + copied, len - copied,
                                               line_end, &tail_consumed)) {
                at_client_finish_line_stream(client, false);
                client->line_stream_drop_until_eol = !line_end;
                return false;
            }
            if (tail_consumed < len - copied) {
                if (!at_client_line_stream_is_complete(client)) {
                    at_client_finish_line_stream(client, false);
                    client->line_stream_drop_until_eol = !line_end;
                    return false;
                }
                at_client_finish_line_stream(client, true);
                at_client_reset_line_stream_probe(client);
                return at_client_process_line_fragment(client,
                                                       data + copied + tail_consumed,
                                                       len - copied - tail_consumed,
                                                       line_end);
            }
        }

        if (line_end) {
            if (!at_client_line_stream_feed_crlf_if_needed(client)) {
                at_client_finish_line_stream(client, false);
                return false;
            }
            if (at_client_line_stream_is_complete(client)) {
                at_client_finish_line_stream(client, true);
            }
        }
        return true;
    }

    if (line_end) {
        bool ok = at_client_process_combined_line(client,
                                                  (const uint8_t *)client->stream_probe_buffer,
                                                  inspect_len,
                                                  data + copied,
                                                  len - copied);
        client->stream_probe_len = 0U;
        return ok;
    }

    xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
    plain_buffered = at_client_append_rx_pair_locked(client,
                                                     (const uint8_t *)client->stream_probe_buffer,
                                                     inspect_len,
                                                     data + copied, len - copied);
    xSemaphoreGive(client->buffer_mutex);
    client->stream_probe_len = 0U;
    return plain_buffered;
}

/**
 * @brief Process received data from transport
 */
static void at_client_process_data(at_client_t *client, const uint8_t *data, int len)
{
    size_t frame_start = 0U;
    size_t offset = 0U;
    size_t probe_frame_start = 0U;
    bool line_in_progress = false;
    bool stream_probe_rejected = false;

    if (len <= 0) return;

    at_client_log_io(client, "RX", data, (size_t)len);

    if (client->rx_pending_cr) {
        client->rx_pending_cr = false;
        if (at_client_line_stream_raw_mode(client)) {
            static const uint8_t cr = '\r';

            if (!at_client_process_line_fragment(client, &cr, 1U, false)) {
                return;
            }
            if (at_client_line_stream_is_complete(client)) {
                at_client_finish_line_stream(client, true);
                at_client_reset_line_stream_probe(client);
            }
        } else if (data[0] == '\n') {
            if (!at_client_process_line_fragment(client, NULL, 0U, true)) {
                return;
            }
            offset = 1U;
            frame_start = 1U;
            probe_frame_start = frame_start;
            stream_probe_rejected = false;
        } else {
            static const uint8_t cr = '\r';

            if (!at_client_process_line_fragment(client, &cr, 1U, false)) {
                return;
            }
        }
    }

    while (offset < (size_t)len) {
        if (client->binary_response_active) {
            size_t captured = at_client_capture_binary_response(client,
                                                                data + offset,
                                                                (size_t)len - offset);
            if (captured > 0U) {
                offset += captured;
                frame_start = offset;
                continue;
            }
        }

        xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
        line_in_progress = (client->rx_buffer_size > 0U);
        xSemaphoreGive(client->buffer_mutex);
        line_in_progress = line_in_progress ||
                         client->line_stream_active ||
                         client->stream_probe_len > 0U ||
                         client->line_stream_drop_until_eol;

        if (!line_in_progress && at_client_has_line_stream_handler(client)) {
            at_line_stream_claim_t claim = AT_LINE_STREAM_PASS;
            void *claim_ctx = NULL;
            size_t probe_len;

            if (probe_frame_start != frame_start) {
                probe_frame_start = frame_start;
                stream_probe_rejected = false;
            }

            probe_len = offset - frame_start + 1U;
            if (!stream_probe_rejected && probe_len <= AT_CLIENT_STREAM_PROBE_SIZE) {
                claim = client->line_stream_handler.claim(data + frame_start,
                                                          probe_len, false,
                                                          client->line_stream_user_data,
                                                          &claim_ctx);
                if (claim == AT_LINE_STREAM_CLAIM) {
                    if (!at_client_process_line_fragment(client, data + frame_start,
                                                         probe_len, false)) {
                        return;
                    }
                    offset = frame_start + probe_len;
                    frame_start = offset;
                    probe_frame_start = frame_start;
                    stream_probe_rejected = false;
                    continue;
                }
                if (claim == AT_LINE_STREAM_PASS) {
                    stream_probe_rejected = true;
                }
            }
        }

        if (at_client_line_stream_raw_mode(client)) {
            size_t consumed = 0U;

            /* Declared-length streams can carry binary TLS bytes, including CRLF. */
            if (!at_client_line_stream_consume(client,
                                               data + offset,
                                               (size_t)len - offset,
                                               false,
                                               &consumed)) {
                at_client_finish_line_stream(client, false);
                return;
            }
            if (consumed == 0U && !at_client_line_stream_is_complete(client)) {
                at_client_finish_line_stream(client, false);
                return;
            }

            offset += consumed;
            frame_start = offset;
            if (at_client_line_stream_is_complete(client)) {
                at_client_finish_line_stream(client, true);
                at_client_reset_line_stream_probe(client);
                continue;
            }
            return;
        }

        if (!line_in_progress && client->wait_for_response && data[offset] == '>') {
            if (offset > frame_start) {
                if (!at_client_process_line_fragment(client,
                                                     data + frame_start,
                                                     offset - frame_start,
                                                     false)) {
                    return;
                }
            }

            at_client_consume_prompt(client);
            offset++;
            frame_start = offset;
            continue;
        }

        if (offset + 1U < (size_t)len &&
            data[offset] == '\r' &&
            data[offset + 1U] == '\n') {
            if (!at_client_process_line_fragment(client,
                                                 data + frame_start,
                                                 offset - frame_start,
                                                 true)) {
                return;
            }

            offset += AT_RESPONSE_CRLF_LEN;
            frame_start = offset;
            probe_frame_start = frame_start;
            stream_probe_rejected = false;
            continue;
        }

        if (data[offset] == '\r' && offset + 1U == (size_t)len) {
            if (offset > frame_start) {
                if (!at_client_process_line_fragment(client,
                                                     data + frame_start,
                                                     offset - frame_start,
                                                     false)) {
                    return;
                }
            }
            client->rx_pending_cr = true;
            return;
        }

        offset++;
    }

    if (frame_start < (size_t)len) {
        if (!at_client_process_line_fragment(client,
                                             data + frame_start,
                                             (size_t)len - frame_start,
                                             false)) {
            return;
        }
    }
}

/**
 * @brief Transport receive callback
 */
static void at_client_rx_callback(const uint8_t *data, size_t len, void *ctx)
{
    at_client_t *client = (at_client_t *)ctx;
    if (!client || !client->initialized) return;
    at_client_process_data(client, data, (int)len);
}

static void at_client_rx_task(void *pvParameters)
{
    at_client_t *client = (at_client_t *)pvParameters;
    uint8_t rx_buf[AT_CLIENT_RX_CHUNK_SIZE];

    while (client && client->rx_task_running) {
        int ret;

        if (!client->transport) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        vTaskDelay(3);
        ret = at_transport_read(client->transport, rx_buf, sizeof(rx_buf), AT_CLIENT_RX_TASK_TIMEOUT_MS);
        if (!client->rx_task_running) {
            break;
        }

        if (ret > 0) {
            at_client_process_data(client, rx_buf, ret);
            continue;
        }

        if (ret < 0) {
            LISA_LOGW(TAG, "Transport read failed: %d", ret);
        }
    }

    if (client) {
        client->rx_task = NULL;
    }
    vTaskDelete(NULL);
}

/* ===== Public API ===== */

at_client_t *at_client_create(const at_client_config_t *config)
{
    at_client_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = (at_client_config_t)AT_CLIENT_CONFIG_DEFAULT();
    }

    at_client_t *client = (at_client_t *)at_mem_calloc(1, sizeof(at_client_t));
    if (!client) return NULL;

    /* Allocate RX buffer */
    client->rx_buffer_capacity = cfg.rx_buf_initial_size;
    client->rx_buffer = (char *)at_mem_alloc(client->rx_buffer_capacity);
    if (!client->rx_buffer) {
        at_mem_free(client);
        return NULL;
    }
    at_client_reset_rx_window_locked(client);
    client->line_buffer_capacity = cfg.resp_buf_size;
    client->line_buffer = (char *)at_mem_alloc(client->line_buffer_capacity);
    if (!client->line_buffer) {
        at_mem_free(client->rx_buffer);
        at_mem_free(client);
        return NULL;
    }
    client->line_buffer[0] = '\0';

    /* Allocate response buffer */
    client->response_buf_size = cfg.resp_buf_size;
    client->response = (char *)at_mem_alloc(client->response_buf_size);
    if (!client->response) {
        at_mem_free(client->line_buffer);
        at_mem_free(client->rx_buffer);
        at_mem_free(client);
        return NULL;
    }
    client->response[0] = '\0';

    /* Create synchronization objects */
    client->tx_mutex = xSemaphoreCreateMutex();
    client->cmd_mutex = xSemaphoreCreateMutex();
    client->buffer_mutex = xSemaphoreCreateMutex();
    client->urc_mutex = xSemaphoreCreateMutex();
    client->event_group = xEventGroupCreate();

    if (!client->tx_mutex || !client->cmd_mutex ||
        !client->buffer_mutex || !client->urc_mutex || !client->event_group) {
        LISA_LOGE(TAG, "Failed to create synchronization objects");
        if (client->tx_mutex) vSemaphoreDelete(client->tx_mutex);
        if (client->cmd_mutex) vSemaphoreDelete(client->cmd_mutex);
        if (client->buffer_mutex) vSemaphoreDelete(client->buffer_mutex);
        if (client->urc_mutex) vSemaphoreDelete(client->urc_mutex);
        if (client->event_group) vEventGroupDelete(client->event_group);
        at_mem_free(client->response);
        at_mem_free(client->line_buffer);
        at_mem_free(client->rx_buffer);
        at_mem_free(client);
        return NULL;
    }

    client->initialized = true;
    client->debug = false;
    client->task_priority = cfg.task_priority;
    client->task_stack_size = cfg.task_stack_size;
    LISA_LOGI(TAG, "AT client created");
    return client;
}

void at_client_destroy(at_client_t *client)
{
    if (!client) return;

    client->rx_task_running = false;
    if (client->rx_task) {
        for (int i = 0; i < 20 && client->rx_task; ++i) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    client->initialized = false;
    at_client_finish_line_stream(client, false);

    /* Unregister from transport */
    if (client->transport) {
        at_transport_set_rx_callback(client->transport, NULL, NULL);
        at_transport_close(client->transport);
        client->transport = NULL;
    }

    /* Free URC callback list */
    at_urc_callback_node_t *node = client->urc_head;
    while (node) {
        at_urc_callback_node_t *next = node->next;
        at_mem_free(node);
        node = next;
    }

    /* Delete synchronization objects */
    if (client->tx_mutex) vSemaphoreDelete(client->tx_mutex);
    if (client->cmd_mutex) vSemaphoreDelete(client->cmd_mutex);
    if (client->buffer_mutex) vSemaphoreDelete(client->buffer_mutex);
    if (client->urc_mutex) vSemaphoreDelete(client->urc_mutex);
    if (client->event_group) vEventGroupDelete(client->event_group);

    at_mem_free(client->response);
    at_mem_free(client->line_buffer);
    at_mem_free(client->rx_buffer);
    at_mem_free(client);

    LISA_LOGI(TAG, "AT client destroyed");
}

int at_client_bind(at_client_t *client, at_transport_t *transport)
{
    BaseType_t xret;

    if (!client || !transport) return -1;

    client->transport = transport;
    if (at_transport_open(transport) != 0) {
        client->transport = NULL;
        return -1;
    }

    if (transport->ops && transport->ops->read_fn) {
        client->rx_task_running = true;
        xret = xTaskCreate(at_client_rx_task, "at_client_rx",
                           client->task_stack_size,
                           client,
                           client->task_priority,
                           &client->rx_task);
        if (xret != pdPASS) {
            client->rx_task_running = false;
            at_transport_close(transport);
            client->transport = NULL;
            return -1;
        }
    } else {
        if (at_transport_set_rx_callback(transport, at_client_rx_callback, client) != 0) {
            at_transport_close(transport);
            client->transport = NULL;
            return -1;
        }
    }

    return 0;
}

bool at_client_send_cmd(at_client_t *client, const char *command,
                        size_t timeout_ms, bool add_crlf)
{
    return at_client_send_cmd_with_data(client, command, timeout_ms, add_crlf, NULL, 0);
}

bool at_client_send_cmd_with_data(at_client_t *client, const char *command,
                                  size_t timeout_ms, bool add_crlf,
                                  const uint8_t *data, size_t data_len)
{
    if (!client || !client->initialized || !command || !client->transport) {
        return false;
    }

    at_client_perf_take_cmd_mutex(client, command);

    /* 等待 flash 操作完成再发送 AT 指令 */
    at_cmd_tx_wait();

    /* Clear event bits */
    xEventGroupClearBits(client->event_group,
                        AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR | AT_EVENT_DATA_PROMPT);
    client->wait_for_response = true;
    client->cme_error_code = 0;
    client->perf_last_ok_wait_resume_ms = 0U;

    /* Clear response buffer */
    xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
    client->response[0] = '\0';
    xSemaphoreGive(client->buffer_mutex);

    /* Send command */
    xSemaphoreTake(client->tx_mutex, portMAX_DELAY);

    int ret = at_client_send_command_locked(client, command, add_crlf);

    xSemaphoreGive(client->tx_mutex);

    if (ret < 0) {
        LISA_LOGE(TAG, "Failed to send command");
        client->wait_for_response = false;
        at_client_perf_give_cmd_mutex(client);
        return false;
    }

    /* Wait for response */
    bool result = false;
    if (timeout_ms > 0) {
        EventBits_t bits = xEventGroupWaitBits(client->event_group,
                                              AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR | AT_EVENT_DATA_PROMPT,
                                              pdTRUE, pdFALSE,
                                              pdMS_TO_TICKS(timeout_ms));
        client->wait_for_response = false;

        if (bits & AT_EVENT_COMMAND_DONE) {
            if (client->perf_last_ok_tick != 0) {
                client->perf_last_ok_wait_resume_ms =
                    at_client_tick_elapsed_ms(client->perf_last_ok_tick, xTaskGetTickCount());
            }
            result = true;
        } else if (bits & AT_EVENT_DATA_PROMPT) {
            result = true;
        }
    } else {
        client->wait_for_response = false;
        result = true;
    }

    /* Send data payload if provided */
    if (result && data && data_len > 0) {
        client->wait_for_response = true;
        xEventGroupClearBits(client->event_group,
                            AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR);

        xSemaphoreTake(client->tx_mutex, portMAX_DELAY);
        ret = at_client_send_locked(client, "TX-DATA", data, data_len);
        xSemaphoreGive(client->tx_mutex);

        if (ret < 0) {
            LISA_LOGE(TAG, "Failed to send data");
            result = false;
        } else {
            EventBits_t bits = xEventGroupWaitBits(client->event_group,
                                                  AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR,
                                                  pdTRUE, pdFALSE,
                                                  pdMS_TO_TICKS(timeout_ms));
            result = (bits & AT_EVENT_COMMAND_DONE) != 0;
        }

        client->wait_for_response = false;
    }

    at_client_perf_give_cmd_mutex(client);
    return result;
}

uint32_t at_client_get_last_cmd_wait_ms(at_client_t *client)
{
    return client ? client->perf_last_cmd_wait_ms : 0U;
}

uint32_t at_client_get_last_ok_wait_resume_ms(at_client_t *client)
{
    return client ? client->perf_last_ok_wait_resume_ms : 0U;
}

int at_client_get_cme_error(at_client_t *client)
{
    if (!client) return 0;
    return client->cme_error_code;
}

at_urc_callback_node_t *at_client_register_urc(at_client_t *client,
                                                at_urc_callback_t callback,
                                                void *user_data)
{
    if (!client || !client->initialized || !callback) return NULL;

    at_urc_callback_node_t *node = (at_urc_callback_node_t *)at_mem_alloc(sizeof(at_urc_callback_node_t));
    if (!node) return NULL;

    node->callback = callback;
    node->user_data = user_data;

    xSemaphoreTake(client->urc_mutex, portMAX_DELAY);
    node->next = client->urc_head;
    client->urc_head = node;
    xSemaphoreGive(client->urc_mutex);

    LISA_LOGI(TAG, "URC callback registered");
    return node;
}

void at_client_unregister_urc(at_client_t *client, at_urc_callback_node_t *node)
{
    if (!client || !client->initialized || !node) return;

    xSemaphoreTake(client->urc_mutex, portMAX_DELAY);

    at_urc_callback_node_t **current = &client->urc_head;
    while (*current) {
        if (*current == node) {
            *current = node->next;
            at_mem_free(node);
            LISA_LOGI(TAG, "URC callback unregistered");
            break;
        }
        current = &(*current)->next;
    }

    xSemaphoreGive(client->urc_mutex);
}

int at_client_set_line_stream_handler(at_client_t *client,
                                      const at_line_stream_handler_t *handler,
                                      void *user_data)
{
    if (!client || !client->initialized) {
        return -1;
    }

    at_client_finish_line_stream(client, false);
    at_client_reset_line_stream_probe(client);
    client->line_stream_drop_until_eol = false;

    memset(&client->line_stream_handler, 0, sizeof(client->line_stream_handler));
    client->line_stream_user_data = NULL;

    if (handler) {
        if (!handler->claim || !handler->consume) {
            return -1;
        }
        client->line_stream_handler = *handler;
        client->line_stream_user_data = user_data;
    }

    return 0;
}

void at_client_set_debug(at_client_t *client, bool enable)
{
    if (client) client->debug = enable;
}

at_transport_t *at_client_get_transport(at_client_t *client)
{
    if (!client) return NULL;
    return client->transport;
}

/* ===== Utility Functions (stateless) ===== */

void at_arg_array_destroy(at_arg_value_t *args, size_t count)
{
    if (!args) return;
    for (size_t i = 0; i < count; i++) {
        if (args[i].type == AT_ARG_TYPE_STRING && args[i].data.string_val.value) {
            at_mem_free(args[i].data.string_val.value);
        }
    }
    at_mem_free(args);
}

static const char s_hex_table[] = "0123456789ABCDEF";

static inline int hex_char_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

char *at_encode_hex(const char *data, size_t length, size_t *out_len)
{
    if (!data || length == 0 || !out_len) return NULL;

    char *hex = (char *)at_mem_alloc(length * 2 + 1);
    if (!hex) return NULL;

    for (size_t i = 0; i < length; i++) {
        uint8_t byte = (uint8_t)data[i];
        hex[i * 2]     = s_hex_table[byte >> 4];
        hex[i * 2 + 1] = s_hex_table[byte & 0x0F];
    }

    hex[length * 2] = '\0';
    *out_len = length * 2;
    return hex;
}

char *at_decode_hex(const char *hex_str, size_t hex_len, size_t *out_len)
{
    if (!hex_str || hex_len == 0 || !out_len) return NULL;
    if (hex_len % 2 != 0) return NULL;

    size_t data_len = hex_len / 2;
    char *data = (char *)at_mem_alloc(data_len + 1);
    if (!data) return NULL;

    for (size_t i = 0; i < data_len; i++) {
        int hi = hex_char_val(hex_str[i * 2]);
        int lo = hex_char_val(hex_str[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            at_mem_free(data);
            return NULL;
        }
        data[i] = (char)((hi << 4) | lo);
    }

    data[data_len] = '\0';
    *out_len = data_len;
    return data;
}

/* ===== Execute Command with Descriptor ===== */

bool at_client_exec_cmd(at_client_t *client, const at_cmd_desc_t *desc, void *user_data)
{
    if (!client || !desc || !desc->cmd) return false;

    /* No URC expected: simple command, just wait for OK/ERROR */
    if (!desc->expect_urc) {
        return at_client_send_cmd(client, desc->cmd, desc->timeout_ms, true);
    }

    /* Send command and wait for URC */
    at_arg_value_t *args = NULL;
    size_t count = 0;

    if (!at_client_send_cmd_wait_urc(client, desc->cmd, desc->expect_urc,
                                      &args, &count, desc->timeout_ms, true)) {
        return false;
    }

    /* Invoke parse callback */
    bool result = true;
    if (desc->parse) {
        result = desc->parse(args, count, user_data);
    }

    at_arg_array_destroy(args, count);
    return result;
}

bool at_client_exec_text_cmd(at_client_t *client, const char *command,
                             char *response, size_t size, uint32_t timeout_ms)
{
    bool ok;

    if (!client || !command || !response || size == 0) {
        return false;
    }

    response[0] = '\0';
    client->capture_prefixed_response = true;
    ok = at_client_send_cmd(client, command, timeout_ms, true);
    client->capture_prefixed_response = false;
    if (!ok) {
        return false;
    }

    xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
    strncpy(response, client->response, size - 1);
    response[size - 1] = '\0';
    xSemaphoreGive(client->buffer_mutex);

    return response[0] != '\0';
}

bool at_client_exec_binary_cmd(at_client_t *client, const char *command,
                               const char *data_urc, uint8_t *response,
                               size_t size, size_t *out_len,
                               uint32_t timeout_ms)
{
    bool result = false;
    int ret;

    if (!client || !client->initialized || !command || !data_urc ||
        !response || size == 0U || !out_len || !client->transport) {
        return false;
    }

    *out_len = 0U;
    at_client_perf_take_cmd_mutex(client, command);

    xEventGroupClearBits(client->event_group,
                         AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR);
    client->wait_for_response = true;
    client->cme_error_code = 0;
    client->binary_response_urc = data_urc;
    client->binary_response = response;
    client->binary_response_size = size;
    client->binary_response_len = 0U;
    client->binary_response_expected = 0U;
    client->binary_response_received = 0U;
    client->binary_response_active = false;
    client->binary_response_ready = false;
    client->binary_response_overflow = false;

    xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
    client->response[0] = '\0';
    xSemaphoreGive(client->buffer_mutex);

    xSemaphoreTake(client->tx_mutex, portMAX_DELAY);
    ret = at_client_send_command_locked(client, command, true);
    xSemaphoreGive(client->tx_mutex);

    if (ret >= 0 && timeout_ms > 0U) {
        EventBits_t bits = xEventGroupWaitBits(client->event_group,
                                               AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR,
                                               pdTRUE, pdFALSE,
                                               pdMS_TO_TICKS(timeout_ms));
        result = (bits & AT_EVENT_COMMAND_DONE) != 0 &&
                 (bits & AT_EVENT_COMMAND_ERROR) == 0 &&
                 client->binary_response_ready &&
                 !client->binary_response_overflow;
    } else if (ret >= 0) {
        result = true;
    }

    client->wait_for_response = false;
    if (result) {
        *out_len = client->binary_response_len;
    }

    client->binary_response_urc = NULL;
    client->binary_response = NULL;
    client->binary_response_size = 0U;
    client->binary_response_len = 0U;
    client->binary_response_expected = 0U;
    client->binary_response_received = 0U;
    client->binary_response_active = false;
    client->binary_response_ready = false;
    client->binary_response_overflow = false;

    at_client_perf_give_cmd_mutex(client);
    return result;
}

bool at_client_exec_cmdf(at_client_t *client, const at_cmd_desc_t *desc,
                          void *user_data, ...)
{
    if (!client || !desc || !desc->cmd) return false;

    va_list ap;

    /* Calculate required buffer size */
    va_start(ap, user_data);
    int needed = vsnprintf(NULL, 0, desc->cmd, ap);
    va_end(ap);

    if (needed < 0) return false;

    char *cmd_buf = (char *)at_mem_alloc(needed + 1);
    if (!cmd_buf) return false;

    va_start(ap, user_data);
    vsnprintf(cmd_buf, needed + 1, desc->cmd, ap);
    va_end(ap);

    at_cmd_desc_t local_desc = *desc;
    local_desc.cmd = cmd_buf;

    bool result = at_client_exec_cmd(client, &local_desc, user_data);

    at_mem_free(cmd_buf);
    return result;
}

/* ===== Send Command and Wait URC ===== */

bool at_client_send_cmd_wait_urc(at_client_t *client, const char *command,
                                  const char *expect_urc,
                                  at_arg_value_t **out_args, size_t *out_count,
                                  size_t timeout_ms, bool add_crlf)
{
    if (!client || !client->initialized || !command || !expect_urc || !client->transport) {
        return false;
    }
    if (out_args) *out_args = NULL;
    if (out_count) *out_count = 0;

    at_client_perf_take_cmd_mutex(client, command);

    /* Setup URC wait state */
    client->expect_urc = expect_urc;
    client->matched_args = NULL;
    client->matched_arg_count = 0;

    /* Clear event bits */
    xEventGroupClearBits(client->event_group,
                        AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR |
                        AT_EVENT_DATA_PROMPT | AT_EVENT_URC_MATCHED);
    client->wait_for_response = true;
    client->cme_error_code = 0;

    /* Clear response buffer */
    xSemaphoreTake(client->buffer_mutex, portMAX_DELAY);
    client->response[0] = '\0';
    xSemaphoreGive(client->buffer_mutex);

    /* Send command */
    xSemaphoreTake(client->tx_mutex, portMAX_DELAY);

    int ret = at_client_send_command_locked(client, command, add_crlf);

    xSemaphoreGive(client->tx_mutex);

    if (ret < 0) {
        LISA_LOGE(TAG, "Failed to send command");
        client->wait_for_response = false;
        client->expect_urc = NULL;
        at_client_perf_give_cmd_mutex(client);
        return false;
    }

    /* Wait for both AT success (OK) and business success (expected URC). */
    EventBits_t wait_bits = AT_EVENT_URC_MATCHED | AT_EVENT_COMMAND_DONE |
                            AT_EVENT_COMMAND_ERROR;
    bool urc_matched = false;
    bool command_done = false;
    bool command_error = false;

    TickType_t remaining = pdMS_TO_TICKS(timeout_ms);
    TickType_t start_tick = xTaskGetTickCount();

    while (remaining > 0) {
        EventBits_t bits = xEventGroupWaitBits(client->event_group, wait_bits,
                                               pdTRUE, pdFALSE, remaining);

        if (bits == 0) {
            break;
        }

        if (bits & AT_EVENT_COMMAND_ERROR) {
            command_error = true;
            break;
        }

        if (bits & AT_EVENT_URC_MATCHED) {
            urc_matched = true;
        }

        if (bits & AT_EVENT_COMMAND_DONE) {
            command_done = true;
        }

        if (command_done && urc_matched) {
            break;
        }

        TickType_t elapsed = xTaskGetTickCount() - start_tick;
        remaining = (elapsed < pdMS_TO_TICKS(timeout_ms)) ?
                    pdMS_TO_TICKS(timeout_ms) - elapsed : 0;
    }

    client->wait_for_response = false;
    client->expect_urc = NULL;

    /* Return matched arguments */
    if (urc_matched && client->matched_args) {
        if (out_args) *out_args = client->matched_args;
        else at_arg_array_destroy(client->matched_args, client->matched_arg_count);

        if (out_count) *out_count = client->matched_arg_count;
        client->matched_args = NULL;
        client->matched_arg_count = 0;
    } else {
        /* Clean up if no match */
        if (client->matched_args) {
            at_arg_array_destroy(client->matched_args, client->matched_arg_count);
            client->matched_args = NULL;
            client->matched_arg_count = 0;
        }
    }

    at_client_perf_give_cmd_mutex(client);
    return command_done && urc_matched && !command_error;
}

/* ===== Baudrate Adaptation ===== */

static bool at_client_uart_find_ready_baud(at_client_t *client,
                                           uint32_t init_baud, uint32_t fallback_baud,
                                           uint32_t *ready_baud)
{
    if (!client || !client->transport) {
        return false;
    }

    at_transport_t *tp = client->transport;
    const int max_retries_per_baudrate = 1;
    const int max_switch_times = 5;
    uint32_t current_baud = init_baud;

    for (int switch_count = 0; switch_count < max_switch_times; switch_count++) {
        if (at_transport_ioctl(tp, AT_TRANSPORT_IOCTL_SET_BAUDRATE, &current_baud) != 0) {
            LISA_LOGE(TAG, "Failed to set baudrate");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));

        for (int retry = 0; retry < max_retries_per_baudrate; retry++) {
            if (at_client_send_cmd(client, "AT", 1000, true)) {
                if (ready_baud) {
                    *ready_baud = current_baud;
                }
                LISA_LOGI(TAG, "AT ready at baudrate=%u", current_baud);
                return true;
            }
            LISA_LOGI(TAG, "AT retry %d/%d at baudrate %u",
                     retry + 1, max_retries_per_baudrate, current_baud);
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        current_baud = (current_baud == init_baud) ? fallback_baud : init_baud;
        LISA_LOGI(TAG, "Switching baudrate to %u", current_baud);
    }

    LISA_LOGE(TAG, "AT not ready after %d baudrate switches", max_switch_times);
    return false;
}

bool at_client_uart_baudrate_sync(at_client_t *client,
                                  uint32_t init_baud, uint32_t fallback_baud)
{
    return at_client_uart_find_ready_baud(client, init_baud, fallback_baud, NULL);
}

bool at_client_uart_get_baudrate(at_client_t *client, uint32_t *baudrate)
{
    if (!client || !client->transport || !baudrate) {
        return false;
    }

    return at_transport_ioctl(client->transport, AT_TRANSPORT_IOCTL_GET_BAUDRATE, baudrate) == 0;
}

bool at_client_uart_baudrate_adapt(at_client_t *client,
                                   uint32_t init_baud, uint32_t target_baud)
{
    if (!client || !client->transport) return false;

    at_transport_t *tp = client->transport;
    int retry;
    uint32_t current_baud = init_baud;

    if (!at_client_uart_find_ready_baud(client, init_baud, target_baud, &current_baud)) {
        return false;
    }

    /* Switch module to target baudrate if needed */
    if (current_baud != target_baud) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "AT+IPR=%u", target_baud);

        for (retry = 0; retry < 5; retry++) {
            if (at_client_send_cmd(client, cmd, 1000, true)) {
                LISA_LOGI(TAG, "Module baudrate set to %u", target_baud);

                if (at_transport_ioctl(tp, AT_TRANSPORT_IOCTL_SET_BAUDRATE, &target_baud) != 0) {
                    LISA_LOGE(TAG, "Failed to set transport baudrate");
                    return false;
                }
                /* Some modules require a short settle window after AT+IPR. */
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (retry >= 5) {
            LISA_LOGE(TAG, "Failed to set module baudrate to %u", target_baud);
            return false;
        }

        /* Verify AT at new baudrate */
        for (retry = 0; retry < 15; retry++) {
            if (at_client_send_cmd(client, "AT", 1000, true)) {
                LISA_LOGI(TAG, "AT ready at %u", target_baud);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (retry >= 15) {
            LISA_LOGE(TAG, "AT not ready at %u after adaptation", target_baud);
            return false;
        }
    }

    return true;
}
