/**
 * @file ml307_tcp.c
 * @brief ML307 TCP Connection Implementation
 * @details TCP/SSL connection management for ML307 4G module
 *          Reference: c_version/ml307_tcp.c
 */

#include "ml307_tcp.h"
#include "ml307_modem.h"
#include "at_uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#define TAG "ml307_tcp"
#include "lisa_log.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"
#include "task.h"

/* Include ring buffer for efficient TCP receive buffering */
#include "ring_buffer.h"

/* Maximum packet size for TCP send (half of 1460 for hex encoding) */
#define MAX_PACKET_SIZE  (1024)

/* Maximum number of TCP connections (shared with UDP, managed by modem) */
#define MAX_TCP_CONNECTIONS  5

/* Receive buffer size */
#define TCP_RECV_BUFFER_SIZE  1024 * 6

/* Convert connection ID (1-5) to array index (0-4) */
#define TCP_ID_TO_INDEX(tcp_id) ((tcp_id) - 1)

/* TCP prefetch task configuration */
#define TCP_PREFETCH_TASK_PRIORITY    5
#define TCP_PREFETCH_TASK_STACK_SIZE  2048
#define TCP_PREFETCH_INTERVAL_MS      3  /**< Prefetch interval in milliseconds */

/**
 * @brief ML307 TCP instance structure
 */
struct ml307_tcp {
    int tcp_id;                             /**< TCP connection ID */
    bool is_ssl;                            /**< SSL enabled flag */
    bool connected;                         /**< Connection status */
    bool instance_active;                   /**< Instance active flag */
    bool initialized;                       /**< Instance initialized flag */
    int last_error;                         /**< Last error code */
    int last_sent_bytes;                    /**< Last sent bytes count from MIPSEND */

    EventGroupHandle_t event_group;         /**< Event group for synchronization */
    at_urc_callback_node_t *urc_node;       /**< URC callback node */

    /* User callbacks */
    tcp_stream_callback_t stream_callback;  /**< Data stream callback */
    void *stream_user_data;                 /**< Stream callback user data */
    tcp_disconnect_callback_t disc_callback;/**< Disconnect callback */
    void *disc_user_data;                   /**< Disconnect callback user data */

    /* Receive buffer for ml307_tcp_recv() - using ring buffer */
    struct ring_buf ring_buf;                       /**< Ring buffer instance */
    uint8_t recv_buffer_data[TCP_RECV_BUFFER_SIZE]; /**< Ring buffer backing data */
    size_t available_data_len;                      /**< Available data length from URC notification */
    EventGroupHandle_t recv_event;                  /**< Event for data available */
};

/* Global TCP connections array */
static struct ml307_tcp tcp_connections[MAX_TCP_CONNECTIONS] __attribute__((section(".psram.data"))) = {0};

/* TCP prefetch task handle */
static TaskHandle_t tcp_prefetch_task_handle = NULL;
static volatile bool tcp_prefetch_task_running = false;

/* Forward declarations */
static char *hex_encode(const char *data, size_t length, size_t *out_len);
static char *hex_decode(const char *hex_str, size_t hex_len, size_t *out_len);
static void ml307_tcp_internal_recv_handler(const char *data, size_t len, void *user_data);
static void ml307_tcp_prefetch_task(void *pvParameters);

/**
 * @brief URC callback handler
 */
static void ml307_tcp_urc_handler(const char *command, at_arg_value_t *arguments,
                                  size_t arg_count, void *user_data)
{
    // LISA_LOGI(TAG, "%s -----", __func__);

    ml307_tcp_t *tcp = (ml307_tcp_t *)user_data;
    if (!tcp || !tcp->initialized || !command) return;

    // LISA_LOGI(TAG, "ml307_tcp_urc_handler---arg_count: %d, %s", arg_count, command);
    /* +MIPOPEN: <link_num>,<result> */
    if (strcmp(command, "MIPOPEN") == 0 && arg_count >= 2) {
        if (arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[0].data.int_val == tcp->tcp_id) {

            int result = (arguments[1].type == AT_ARG_TYPE_INT) ?
                         arguments[1].data.int_val : -1;

            tcp->connected = (result == 0);
            if (tcp->connected) {
                tcp->instance_active = true;
                xEventGroupClearBits(tcp->event_group,
                                    ML307_TCP_DISCONNECTED | ML307_TCP_ERROR);
                xEventGroupSetBits(tcp->event_group, ML307_TCP_CONNECTED);
                LISA_LOGI(TAG, "TCP %d connected", tcp->tcp_id);
            } else {
                tcp->last_error = result;
                xEventGroupSetBits(tcp->event_group, ML307_TCP_ERROR);
                LISA_LOGE(TAG, "TCP %d connection failed: %d", tcp->tcp_id, result);
            }
        }
    } else if (strcmp(command, "MIPCLOSE") == 0 && arg_count >= 1) {
        if (arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[0].data.int_val == tcp->tcp_id) {
            tcp->instance_active = false;
            xEventGroupSetBits(tcp->event_group, ML307_TCP_DISCONNECTED);
            LISA_LOGI(TAG, "TCP %d closed", tcp->tcp_id);
        }
    } else if (strcmp(command, "MIPSEND") == 0 && arg_count >= 2) {
        if (arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[0].data.int_val == tcp->tcp_id) {
            /* Save the number of bytes sent */
            if (arguments[1].type == AT_ARG_TYPE_INT) {
                tcp->last_sent_bytes = arguments[1].data.int_val;
                // LISA_LOGI(TAG, "TCP %d sent %d bytes", tcp->tcp_id, tcp->last_sent_bytes);
            }
            xEventGroupSetBits(tcp->event_group, ML307_TCP_SEND_COMPLETE);
        }
    } else if (strcmp(command, "MIPURC") == 0 && arg_count >= 3) {
        // LISA_LOGI(TAG, "MIPURC: %d , %d, %s-----", arguments[1].type, arguments[1].data.int_val, arguments[0].data.string_val.value);
        if (arguments[1].type == AT_ARG_TYPE_INT &&
            arguments[1].data.int_val == tcp->tcp_id) {

            if (arguments[0].type == AT_ARG_TYPE_STRING) {
                const char *urc_type = arguments[0].data.string_val.value;
                /* Received TCP data */
                if (strcmp(urc_type, "rtcp") == 0) {
                    if (tcp->connected) {
                        if (arg_count >= 4) {
                            /* Check if this is cache mode (arguments[3] is int) or normal mode (arguments[3] is string) */
                            if (arguments[3].type == AT_ARG_TYPE_STRING) {
                                /* Normal mode: arg[3] contains hex data */
                                size_t decoded_len = 0;
                                char *decoded = hex_decode(
                                    arguments[3].data.string_val.value,
                                    arguments[3].data.string_val.len,
                                    &decoded_len
                                );

                                if (decoded) {
                                    ml307_tcp_internal_recv_handler(decoded, decoded_len, tcp);
                                    free(decoded);
                                }
                            } else if (arguments[3].type == AT_ARG_TYPE_INT) {
                                /* Cache mode: arg[2]=data_length, arg[3]=total_buffer_length */
                                int data_len = (arguments[2].type == AT_ARG_TYPE_INT) ?
                                              arguments[2].data.int_val : 0;
                                int total_buf = arguments[3].data.int_val;

                                // LISA_LOGI(TAG, "TCP %d in CACHE mode - data notification: %d bytes (total buffer: %d bytes)",
                                //          tcp->tcp_id, data_len, total_buf);

                                /* 保存可用数据长度，供 ml307_tcp_recv() 使用 */
                                tcp->available_data_len = total_buf;
                            }
                        }
                    }
                } else if (strcmp(urc_type, "disconn") == 0) {
                    if (tcp->connected) {
                        tcp->connected = false;
                        LISA_LOGW(TAG, "TCP %d disconnected by remote", tcp->tcp_id);

                        if (tcp->disc_callback) {
                            tcp->disc_callback(tcp->disc_user_data);
                        }
                    }
                    tcp->instance_active = false;
                    xEventGroupSetBits(tcp->event_group, ML307_TCP_DISCONNECTED);
                }
            }
        }
    } else if (strcmp(command, "MIPSTATE") == 0) {
        // LISA_LOGI(TAG, "MIPSTATE: %d , %d, %d-----", arg_count, arguments[0].type, arguments[4].type);
        // LISA_LOGI(TAG, "TCP %d state: %s", tcp->tcp_id, arguments[4].data.string_val.value);
        /* 检查参数数量，防止越界访问 */
        if (arg_count >= 5 &&
            arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[0].data.int_val == tcp->tcp_id &&
            arguments[4].type == AT_ARG_TYPE_STRING) {

            const char *state = arguments[4].data.string_val.value;
            if (state) {
                /* 根据不同状态设置标志 */
                if (strcmp(state, "CONNECTED") == 0) {
                    tcp->connected = true;
                    tcp->instance_active = true;
                    xEventGroupClearBits(tcp->event_group, ML307_TCP_DISCONNECTED | ML307_TCP_ERROR);
                    xEventGroupSetBits(tcp->event_group, ML307_TCP_CONNECTED);
                    LISA_LOGI(TAG, "TCP %d state: CONNECTED", tcp->tcp_id);
                } else if (strcmp(state, "INITIAL") == 0) {
                    tcp->connected = false;
                    tcp->instance_active = true;  /* INITIAL状态表示实例已分配但未连接 */
                    xEventGroupSetBits(tcp->event_group, ML307_TCP_INITIALIZED);
                    LISA_LOGI(TAG, "TCP %d state: INITIAL", tcp->tcp_id);
                } else {
                    /* 其他状态（如CLOSING, CLOSED等）*/
                    tcp->connected = false;
                    tcp->instance_active = false;
                    LISA_LOGI(TAG, "TCP %d state: %s", tcp->tcp_id, state);
                }
            }
        }
    } else if (strcmp(command, "MIPRD") == 0 && arg_count >= 3) {
        // LISA_LOGI(TAG, "MIPRD received with %zu arguments", arg_count);
        if (arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[0].data.int_val == tcp->tcp_id) {

            /* 根据参数数量判断格式 */
            int hex_data_index = 2;  /* 默认：arg[2] 是 HEX 数据 */

            /* 如果有 4 个参数，可能是：<link_num>,<data_len>,<encoding>,<hex_data> */
            if (arg_count >= 4 && arguments[2].type == AT_ARG_TYPE_INT) {
                hex_data_index = 3;  /* arg[3] 是 HEX 数据 */
            }

            if (arguments[hex_data_index].type == AT_ARG_TYPE_STRING) {
                /* Decode hex data from AT+MIPRD response */
                size_t decoded_len = 0;
                char *decoded = hex_decode(
                    arguments[hex_data_index].data.string_val.value,
                    arguments[hex_data_index].data.string_val.len,
                    &decoded_len
                );

                if (decoded) {
                    /* Store in receive buffer */
                    ml307_tcp_internal_recv_handler(decoded, decoded_len, tcp);
                    free(decoded);

                    // LISA_LOGI(TAG, "TCP %d read %zu bytes via AT+MIPRD, remaining: %zu",
                    //          tcp->tcp_id, decoded_len, tcp->available_data_len);
                }
            }
        }
    } else if (strcmp(command, "FIFO_OVERFLOW") == 0) {
        LISA_LOGE(TAG, "FIFO overflow, disconnecting TCP %d", tcp->tcp_id);
        xEventGroupSetBits(tcp->event_group, ML307_TCP_ERROR);
        ml307_tcp_disconnect(tcp->tcp_id);
    }
}

/**
 * @brief Hex encode helper
 */
static char *hex_encode(const char *data, size_t length, size_t *out_len)
{
    if (!data || length == 0 || !out_len) return NULL;

    char *hex = (char *)malloc(length * 2 + 1);
    if (!hex) {
        LISA_LOGE(TAG, "Failed to allocate hex buffer");
        return NULL;
    }

    for (size_t i = 0; i < length; i++) {
        sprintf(hex + i * 2, "%02X", (uint8_t)data[i]);
    }

    hex[length * 2] = '\0';
    *out_len = length * 2;
    return hex;
}

/**
 * @brief Hex decode helper
 */
static char *hex_decode(const char *hex_str, size_t hex_len, size_t *out_len)
{
    if (!hex_str || hex_len == 0 || !out_len) return NULL;

    if (hex_len % 2 != 0) {
        LISA_LOGE(TAG, "Invalid hex string length: %zu", hex_len);
        return NULL;
    }

    size_t data_len = hex_len / 2;
    char *data = (char *)malloc(data_len + 1);
    if (!data) {
        LISA_LOGE(TAG, "Failed to allocate decode buffer");
        return NULL;
    }

    for (size_t i = 0; i < data_len; i++) {
        char byte_str[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
        char *endptr;
        long val = strtol(byte_str, &endptr, 16);

        if (endptr != byte_str + 2) {
            LISA_LOGE(TAG, "Invalid hex character at position %zu", i * 2);
            free(data);
            return NULL;
        }

        /* 检查值是否在有效字节范围内 */
        if (val < 0 || val > 255) {
            LISA_LOGE(TAG, "Hex value out of range at position %zu: 0x%lx", i * 2, val);
            free(data);
            return NULL;
        }

        data[i] = (char)val;
    }

    data[data_len] = '\0';
    *out_len = data_len;
    return data;
}

/**
 * @brief Internal receive handler to store data in recv buffer
 */
static void ml307_tcp_internal_recv_handler(const char *data, size_t len, void *user_data)
{
    ml307_tcp_t *tcp = (ml307_tcp_t *)user_data;
    if (!tcp || !data || len == 0) return;

    /* Update modem buffer tracking */
    if (tcp->available_data_len >= len) {
        tcp->available_data_len -= len;
    } else {
        tcp->available_data_len = 0;
    }
    // LISA_LOGI(TAG, "ring_buf_put-----: %d", tcp->tcp_id, len);
    /* Write data to ring buffer */
    uint32_t written = ring_buf_put(&tcp->ring_buf, (const uint8_t *)data, (uint32_t)len);
    if (written < len) {
        LISA_LOGI(TAG, "TCP %d recv buffer overflow: dropped %zu bytes",
                 tcp->tcp_id, len - written);
    }

    /* Always set PREFETCH_AVAILABLE to unblock prefetch task, even if buffer is full */
    if (tcp->recv_event) {
        xEventGroupSetBits(tcp->recv_event, ML307_TCP_PREFETCH_AVAILABLE);
    }
}

/**
 * @brief Initialize ML307 TCP connection
 */
int ml307_tcp_init(bool is_ssl)
{
    /* Allocate a free connection ID (returns 1-5) */
    int tcp_id = ml307_modem_alloc_connect_id();
    if (tcp_id < 0) {
        LISA_LOGE(TAG, "No free connection ID available");
        return -1;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)];

    if (tcp->initialized) {
        LISA_LOGW(TAG, "TCP %d already initialized", tcp_id);
        return -1;
    }

    memset(tcp, 0, sizeof(ml307_tcp_t));
    tcp->tcp_id = tcp_id;
    tcp->is_ssl = is_ssl;

    /* Initialize ring buffer for receiving data */
    ring_buf_init(&tcp->ring_buf, TCP_RECV_BUFFER_SIZE, tcp->recv_buffer_data);

    tcp->event_group = xEventGroupCreate();
    if (!tcp->event_group) {
        LISA_LOGE(TAG, "Failed to create event group for TCP %d", tcp_id);
        return -1;
    }

    tcp->recv_event = xEventGroupCreate();
    if (!tcp->recv_event) {
        LISA_LOGE(TAG, "Failed to create recv event group for TCP %d", tcp_id);
        vEventGroupDelete(tcp->event_group);
        tcp->event_group = NULL;
        return -1;
    }

    tcp->urc_node = at_uart_register_urc_callback(ml307_tcp_urc_handler, tcp);
    if (!tcp->urc_node) {
        LISA_LOGE(TAG, "Failed to register URC callback for TCP %d", tcp_id);
        vEventGroupDelete(tcp->recv_event);
        vEventGroupDelete(tcp->event_group);
        tcp->event_group = NULL;
        tcp->recv_event = NULL;
        return -1;
    }
    
    tcp->initialized = true;
    LISA_LOGI(TAG, "Initialized TCP %d (SSL: %s)", tcp_id, is_ssl ? "enabled" : "disabled");
    return tcp_id;
}

/**
 * @brief Deinitialize TCP connection
 */
void ml307_tcp_deinit(int tcp_id)
{
    if (tcp_id < 1 || tcp_id > MAX_TCP_CONNECTIONS) {
        LISA_LOGE(TAG, "Invalid TCP ID: %d (valid range: 1-%d)", tcp_id, MAX_TCP_CONNECTIONS);
        return;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)];

    if (!tcp->initialized) {
        return;
    }

    LISA_LOGI(TAG, "Deinitializing TCP %d", tcp_id);

    if (tcp->urc_node) {
        at_uart_unregister_urc_callback(tcp->urc_node);
        tcp->urc_node = NULL;
    }

    if (tcp->recv_event) {
        vEventGroupDelete(tcp->recv_event);
        tcp->recv_event = NULL;
    }

    if (tcp->event_group) {
        vEventGroupDelete(tcp->event_group);
        tcp->event_group = NULL;
    }

    /* Free the connection ID */
    ml307_modem_free_connect_id(tcp_id);

    memset(tcp, 0, sizeof(ml307_tcp_t));
}

/**
 * @brief Connect to remote server
 */
bool ml307_tcp_connect(int tcp_id, const char *host, int port, bool is_ssl)
{
    if (tcp_id < 1 || tcp_id > MAX_TCP_CONNECTIONS) {
        LISA_LOGE(TAG, "Invalid TCP ID: %d (valid range: 1-%d)", tcp_id, MAX_TCP_CONNECTIONS);
        return false;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)]; /* Refresh pointer after init */

    if (tcp->connected) 
        return true;

    if (!host) {
        LISA_LOGE(TAG, "Invalid parameters: host is NULL");
        return false;
    }

    LISA_LOGI(TAG, "Connecting TCP %d to %s:%d", tcp_id, host, port);

    xEventGroupClearBits(tcp->event_group,
                        ML307_TCP_CONNECTED | ML307_TCP_DISCONNECTED | ML307_TCP_ERROR);

    char command[128];
    EventBits_t bits;

    snprintf(command, sizeof(command), "AT+MIPSTATE=%d", tcp_id);
    if (!at_uart_send_command(command, 1000, true)) {
        LISA_LOGE(TAG, "Failed to query TCP state");
        ml307_tcp_deinit(tcp_id);
        return -1;
    }

    bits = xEventGroupWaitBits(tcp->event_group,
                              ML307_TCP_INITIALIZED | ML307_TCP_CONNECTED,
                              pdTRUE, pdFALSE,
                              pdMS_TO_TICKS(TCP_CONNECT_TIMEOUT_MS));
    if (!(bits & ML307_TCP_INITIALIZED) && !(bits & ML307_TCP_CONNECTED)) {
        LISA_LOGE(TAG, "Failed to initialize TCP connection");
        ml307_tcp_deinit(tcp_id);
        return -1;
    }

    if (tcp->instance_active || tcp->connected) {
        LISA_LOGI(TAG, "TCP %d already active, closing...", tcp_id);
        snprintf(command, sizeof(command), "AT+MIPCLOSE=%d", tcp_id);
        if (at_uart_send_command(command, 1000, true)) {
            xEventGroupWaitBits(tcp->event_group,
                              ML307_TCP_DISCONNECTED,
                              pdTRUE, pdFALSE,
                              pdMS_TO_TICKS(TCP_CONNECT_TIMEOUT_MS));
        }
    }

    // 配置 SSL
    if (tcp->is_ssl) {
        snprintf(command, sizeof(command), "AT+MIPCFG=\"ssl\",%d,1,0", tcp->tcp_id);
    } else {
        snprintf(command, sizeof(command), "AT+MIPCFG=\"ssl\",%d,0,0", tcp->tcp_id);
    }

    if (!at_uart_send_command(command, 1000, true)) {
        LISA_LOGE(TAG, "Failed to configure SSL");
        ml307_tcp_deinit(tcp_id);
        return false;
    }

    snprintf(command, sizeof(command), "AT+MIPCFG=\"encoding\",%d,1,1", tcp_id);
    if (!at_uart_send_command(command, 1000, true)) {
        LISA_LOGE(TAG, "Failed to set HEX encoding");
        ml307_tcp_deinit(tcp_id);
        return false;
    }

    snprintf(command, sizeof(command), "AT+MIPOPEN=%d,\"TCP\",\"%s\",%d,60,2,0",
            tcp_id, host, port);
    if (!at_uart_send_command(command, TCP_CONNECT_TIMEOUT_MS, true)) {
        tcp->last_error = at_uart_get_cme_error_code();
        LISA_LOGE(TAG, "Failed to open TCP connection, error=%d", tcp->last_error);
        ml307_tcp_deinit(tcp_id);
        return false;
    }

    bits = xEventGroupWaitBits(tcp->event_group,
                              ML307_TCP_CONNECTED | ML307_TCP_ERROR,
                              pdTRUE, pdFALSE,
                              pdMS_TO_TICKS(TCP_CONNECT_TIMEOUT_MS));

    if (bits & ML307_TCP_ERROR) {
        LISA_LOGE(TAG, "Failed to connect to %s:%d, error=%d", host, port, tcp->last_error);
        ml307_tcp_deinit(tcp_id);
        return false;
    }

    if (!(bits & ML307_TCP_CONNECTED)) {
        LISA_LOGE(TAG, "Connection timeout to %s:%d", host, port);
        ml307_tcp_deinit(tcp_id);
        return false;
    }

    LISA_LOGI(TAG, "TCP %d connected successfully", tcp_id);
    return true;
}

/**
 * @brief Disconnect from remote server
 */
int ml307_tcp_disconnect(int tcp_id)
{
    if (tcp_id < 1 || tcp_id > MAX_TCP_CONNECTIONS) {
        return -1;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)];

    if (!tcp->initialized) {
        return -1;
    }

    if (tcp->instance_active) {
        LISA_LOGI(TAG, "Disconnecting TCP %d", tcp_id);

        char command[128];
        snprintf(command, sizeof(command), "AT+MIPCLOSE=%d", tcp_id);
        if (at_uart_send_command(command, 1000, true)) {
            xEventGroupWaitBits(tcp->event_group,
                              ML307_TCP_DISCONNECTED,
                              pdTRUE, pdFALSE,
                              pdMS_TO_TICKS(TCP_CONNECT_TIMEOUT_MS));
        }

        if (tcp->connected) {
            tcp->connected = false;
            if (tcp->disc_callback) {
                tcp->disc_callback(tcp->disc_user_data);
            }
        }
    }

    /* Auto-deinitialize after disconnect */
    ml307_tcp_deinit(tcp_id);

    return 0;
}

/**
 * @brief Send data to remote server
 */
int ml307_tcp_send(int tcp_id, const char *data, size_t length)
{
    if (tcp_id < 1 || tcp_id > MAX_TCP_CONNECTIONS || !data || length == 0) {
        LISA_LOGE(TAG, "Invalid parameters--tcp_id: %d, length: %zu, data: %p", tcp_id, length, data);
        return -1;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)];

    if (!tcp->initialized) {
        LISA_LOGE(TAG, "TCP %d not initialized", tcp_id);
        return -1;
    }

    if (!tcp->connected) {
        LISA_LOGE(TAG, "TCP %d not connected", tcp_id);
        return -1;
    }

    size_t total_sent = 0;

    while (total_sent < length) {
        size_t chunk_size = (length - total_sent > MAX_PACKET_SIZE) ?
                           MAX_PACKET_SIZE : (length - total_sent);

        size_t hex_len = 0;
        char *hex_data = hex_encode(data + total_sent, chunk_size, &hex_len);
        if (!hex_data) {
            LISA_LOGE(TAG, "Failed to encode hex data");
            return -1;
        }

        /* 计算命令头实际长度: "AT+MIPSEND=XX,XXXX," 最多64字节 */
        char cmd_header[64];
        int cmd_len = snprintf(cmd_header, sizeof(cmd_header), "AT+MIPSEND=%d,%zu,", tcp_id, chunk_size);

        /* 检查命令头是否被截断 */
        if (cmd_len < 0 || cmd_len >= (int)sizeof(cmd_header)) {
            LISA_LOGE(TAG, "Command header too long");
            free(hex_data);
            return -1;
        }

        /* 检查整数溢出：防止 hex_len 过大导致 total_cmd_len 溢出 */
        if (hex_len > SIZE_MAX - (size_t)cmd_len - 3) {
            LISA_LOGE(TAG, "Command size overflow: hex_len=%zu, cmd_len=%d", hex_len, cmd_len);
            free(hex_data);
            return -1;
        }

        /* 分配完整命令缓冲区: 命令头 + hex数据 + "\r\n" + '\0' */
        size_t total_cmd_len = cmd_len + hex_len + 3;
        char *command = (char *)malloc(total_cmd_len);
        if (!command) {
            LISA_LOGE(TAG, "Failed to allocate command buffer");
            free(hex_data);
            return -1;
        }

        /* 安全地构造完整命令 */
        memcpy(command, cmd_header, cmd_len);
        memcpy(command + cmd_len, hex_data, hex_len);
        command[cmd_len + hex_len] = '\r';
        command[cmd_len + hex_len + 1] = '\n';
        command[cmd_len + hex_len + 2] = '\0';
        free(hex_data);

        // LISA_LOGI(TAG, "Sending %zu bytes (chunk %zu)", chunk_size, total_sent / MAX_PACKET_SIZE + 1);
        xEventGroupClearBits(tcp->event_group, ML307_TCP_SEND_COMPLETE);
        tcp->last_sent_bytes = 0;
        if (!at_uart_send_command(command, 1500, false)) {
            LISA_LOGE(TAG, "Failed to send data chunk");
            free(command);
            ml307_tcp_disconnect(tcp_id);
            return -1;
        }
        free(command);

        EventBits_t bits = xEventGroupWaitBits(tcp->event_group,
                                              ML307_TCP_SEND_COMPLETE,
                                              pdTRUE, pdFALSE,
                                              pdMS_TO_TICKS(TCP_SEND_TIMEOUT_MS));
        if (!(bits & ML307_TCP_SEND_COMPLETE)) {
            LISA_LOGE(TAG, "No send confirmation received");
            return -1;
        }

        if (tcp->last_sent_bytes > 0 && tcp->last_sent_bytes != (int)chunk_size) {
            LISA_LOGW(TAG, "TCP %d partial send: requested %zu, sent %d",
                     tcp_id, chunk_size, tcp->last_sent_bytes);
            total_sent += tcp->last_sent_bytes;
        } else {
            total_sent += chunk_size;
        }
    }

    // LISA_LOGI(TAG, "TCP %d sent %zu bytes", tcp_id, total_sent);
    return (int)total_sent;
}

/**
 * @brief Receive data from remote server using AT+MIPRD command
 */
int ml307_tcp_recv(int tcp_id, char *buffer, size_t length, uint32_t timeout_ms)
{
    // LISA_LOGI(TAG, "%s-----------", __func__);
    if (!length) {
        LISA_LOGE(TAG, "recv length is 0");
        errno = EAGAIN; 
        return -1;
    }

    if (tcp_id < 1 || tcp_id > MAX_TCP_CONNECTIONS || !buffer) {
        LISA_LOGE(TAG, "Invalid parameters-----: tcp_id: %d, %d", tcp_id, length);
        return -1;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)];

    if (!tcp->initialized) {
        LISA_LOGE(TAG, "TCP %d not initialized", tcp_id);
        return -1;
    }

    if (!tcp->connected) {
        LISA_LOGE(TAG, "TCP %d not connected", tcp_id);
        return -1;
    }

    /* Check if we have enough data in ring buffer */
    uint32_t available = ring_buf_size_get(&tcp->ring_buf);
    if (available < length) {
        // if (length != 2)
        //     LISA_LOGI(TAG, "length----: %d, ring_buf_available: %u, modem_available: %zu\r\n", length, available, tcp->available_data_len);

        /* Polling-based approach: check buffer length directly without event waiting */
        TickType_t start_tick = xTaskGetTickCount();
        TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms > 0 ? timeout_ms : 3);

        do {
            /* Check timeout condition first */
            TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
            if (elapsed_ticks >= timeout_ticks) {
                LISA_LOGD(TAG, "TCP %d recv timeout after %u ms", tcp_id, (unsigned int)(elapsed_ticks * portTICK_PERIOD_MS));
                break;
            }

            /* Check if we have enough data now */
            uint32_t current_available = ring_buf_size_get(&tcp->ring_buf);
            if (current_available >= length) {
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(3));
        } while (1);
    }

    /* Check if ring buffer has data */
    uint32_t buf_available = ring_buf_size_get(&tcp->ring_buf);
    if (buf_available == 0) {
        /* No data available */
        errno = EAGAIN;  /* 设置errno表示暂时没有数据 */
        return -1;
    }

    /* Read data from ring buffer */
    uint32_t to_read = (length > buf_available) ? buf_available : length;
    uint32_t bytes_read = ring_buf_get(&tcp->ring_buf, (uint8_t *)buffer, to_read);

    if (bytes_read > 0) {
        // LISA_LOGI(TAG, "TCP %d received %u bytes via ring buffer", tcp_id, bytes_read);
        return (int)bytes_read;
    }

    return 0;
}

/**
 * @brief Register data stream callback
 */
void ml307_tcp_on_stream(int tcp_id, tcp_stream_callback_t callback, void *user_data)
{
    if (tcp_id < 1 || tcp_id > MAX_TCP_CONNECTIONS) {
        return;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)];

    if (!tcp->initialized) {
        return;
    }

    tcp->stream_callback = callback;
    tcp->stream_user_data = user_data;
    LISA_LOGI(TAG, "Stream callback registered for TCP %d", tcp_id);
}

/**
 * @brief Register disconnect callback
 */
void ml307_tcp_on_disconnected(int tcp_id, tcp_disconnect_callback_t callback, void *user_data)
{
    if (tcp_id < 1 || tcp_id > MAX_TCP_CONNECTIONS) {
        return;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)];

    if (!tcp->initialized) {
        return;
    }

    tcp->disc_callback = callback;
    tcp->disc_user_data = user_data;
    LISA_LOGI(TAG, "Disconnect callback registered for TCP %d", tcp_id);
}

/**
 * @brief Check if TCP is connected
 */
bool ml307_tcp_connected(int tcp_id)
{
    if (tcp_id < 1 || tcp_id > MAX_TCP_CONNECTIONS) {
        return false;
    }

    ml307_tcp_t *tcp = &tcp_connections[TCP_ID_TO_INDEX(tcp_id)];
    return tcp->initialized && tcp->connected;
}

/**
 * @brief Try to prefetch TCP data from modem to recv buffer for all active connections
 *
 * @return Total number of bytes prefetched across all connections, 0 if no prefetch needed
 */
static int ml307_tcp_data_prefetch(void)
{
    /* Iterate through all TCP connection slots */
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        ml307_tcp_t *tcp = &tcp_connections[i];
        int tcp_id = i + 1;  /* Connection IDs are 1-5 */

        /* Skip uninitialized or disconnected connections */
        if (!tcp->initialized || !tcp->connected) {
            continue;
        }

        /* Get available space in ring buffer and calculate read length */
        uint32_t max_space = ring_buf_space_get(&tcp->ring_buf);
        size_t read_len = tcp->available_data_len < max_space ? tcp->available_data_len : max_space;

        /* Check if there's data to read and space available */
        if (read_len == 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        if (read_len > 512) {
            read_len = 512;
        }

        /* Clear the prefetch event before issuing new command */
        xEventGroupClearBits(tcp->recv_event, ML307_TCP_PREFETCH_AVAILABLE);

        /* Issue AT+MIPRD command to read data from modem (支持重试) */
        char command[128];
        snprintf(command, sizeof(command), "AT+MIPRD=%d,%zu", tcp_id, read_len);
        // LISA_LOGI(TAG, "command---: %s-----------", command);

        int retry_count = 0;
        const int max_retries = 2;  /* 最多重试2次 */
        bool cmd_success = false;

        while (retry_count <= max_retries && !cmd_success) {
            if (!at_uart_send_command(command, 1000, true)) {
                retry_count++;
                LISA_LOGE(TAG, "TCP %d failed to send MIPRD command (attempt %d/%d)",
                         tcp_id, retry_count, max_retries + 1);

                if (retry_count <= max_retries) {
                    vTaskDelay(pdMS_TO_TICKS(50));  /* 短暂延迟后重试 */
                    continue;
                } else {
                    break;
                }
            }
            cmd_success = true;
        }

        if (!cmd_success) {
            LISA_LOGE(TAG, "TCP %d MIPRD command failed after %d retries, skipping",
                     tcp_id, max_retries + 1);
            continue;
        }

        /* Wait for data to be received into recv_buffer (via URC handler) */
        EventBits_t bits = xEventGroupWaitBits(tcp->recv_event,
                                               ML307_TCP_PREFETCH_AVAILABLE,
                                               pdTRUE,  /* Clear bit after wait */
                                               pdFALSE,
                                               pdMS_TO_TICKS(1000));

        if ((bits & ML307_TCP_PREFETCH_AVAILABLE) == 0) {
            LISA_LOGW(TAG, "TCP %d prefetch timeout after MIPRD", tcp_id);
            continue;  /* Try next connection */
        }
    }

    return 0;
}

/**
 * @brief TCP prefetch task
 *
 * This task runs periodically to prefetch TCP data from the modem
 * for all active connections, reducing latency.
 */
static void ml307_tcp_prefetch_task(void *pvParameters)
{
    (void)pvParameters;

    LISA_LOGI(TAG, "TCP prefetch task started (interval: %d ms)", TCP_PREFETCH_INTERVAL_MS);

    while (tcp_prefetch_task_running) {
        /* Call prefetch function for all active connections */
        ml307_tcp_data_prefetch();

        /* Sleep for the configured interval */
        vTaskDelay(pdMS_TO_TICKS(TCP_PREFETCH_INTERVAL_MS));
    }

    LISA_LOGI(TAG, "TCP prefetch task stopped");
    tcp_prefetch_task_handle = NULL;
    vTaskDelete(NULL);
}

int ml307_tcp_start_prefetch_task(void)
{
    if (tcp_prefetch_task_handle != NULL) {
        LISA_LOGW(TAG, "TCP prefetch task already running");
        return -1;
    }

    tcp_prefetch_task_running = true;

    BaseType_t ret = xTaskCreate(
        ml307_tcp_prefetch_task,
        "tcp_prefetch",
        TCP_PREFETCH_TASK_STACK_SIZE,
        NULL,
        TCP_PREFETCH_TASK_PRIORITY,
        &tcp_prefetch_task_handle
    );

    if (ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create TCP prefetch task");
        tcp_prefetch_task_running = false;
        tcp_prefetch_task_handle = NULL;
        return -1;
    }

    LISA_LOGI(TAG, "TCP prefetch task created successfully");
    return 0;
}

/**
 * @brief Stop TCP prefetch task
 *
 * Stops the background TCP prefetch task.
 */
void ml307_tcp_stop_prefetch_task(void)
{
    if (tcp_prefetch_task_handle == NULL) {
        LISA_LOGW(TAG, "TCP prefetch task not running");
        return;
    }

    LISA_LOGI(TAG, "Stopping TCP prefetch task...");
    tcp_prefetch_task_running = false;

    /* Wait for task to finish (max 1 second) */
    for (int i = 0; i < 10 && tcp_prefetch_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (tcp_prefetch_task_handle != NULL) {
        LISA_LOGW(TAG, "TCP prefetch task did not stop gracefully");
    }
}
