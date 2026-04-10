/**
 * @file ml307_udp.c
 * @brief ML307 UDP Connection Implementation
 * @details UDP connection management for ML307 4G module
 *          Reference: c_version/ml307_udp.cc
 */

#include "ml307_udp.h"
#include "ml307_modem.h"
#include "at_uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#define TAG "ml307_udp"
#include "lisa_log.h"

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

/* Include ring buffer for efficient UDP receive buffering */
#include "ring_buffer.h"

/* Maximum number of UDP connections (shared with TCP, managed by modem) */
#define MAX_UDP_CONNECTIONS  5

/* Receive buffer size - larger for better performance */
#define UDP_RECV_BUFFER_SIZE (1024 * 4)

/* Convert connection ID (1-5) to array index (0-4) */
#define UDP_ID_TO_INDEX(udp_id) ((udp_id) - 1)

/* UDP prefetch task configuration */
#define UDP_PREFETCH_TASK_PRIORITY    5
#define UDP_PREFETCH_TASK_STACK_SIZE  2048
#define UDP_PREFETCH_INTERVAL_MS      3  /**< Prefetch interval in milliseconds */

/**
 * @brief ML307 UDP structure
 */
struct ml307_udp {
    int udp_id;                                 /**< UDP connection ID (0-5) */
    bool connected;                             /**< Connection status */
    bool instance_active;                       /**< Instance active flag */
    bool initialized;                           /**< Initialized flag */
    int last_error;                             /**< Last error code */
    int last_sent_bytes;                        /**< Actual sent bytes from MIPSEND URC */

    EventGroupHandle_t event_group;             /**< Event group for synchronization */
    at_urc_callback_node_t *urc_node;           /**< URC callback node */

    udp_message_callback_t message_callback;    /**< Message callback */
    void *message_user_data;                    /**< Message callback user data */

    udp_disconnect_callback_t disconnect_callback;  /**< Disconnect callback */
    void *disconnect_user_data;                 /**< Disconnect callback user data */

    /* Timeout settings */
    uint32_t send_timeout_ms;                   /**< Send timeout in milliseconds */
    uint32_t recv_timeout_ms;                   /**< Receive timeout in milliseconds */
    bool blocking;                              /**< Blocking mode flag */

    /* Receive buffer for ml307_udp_recv() - using ring buffer */
    struct ring_buf ring_buf;                       /**< Ring buffer instance */
    uint8_t recv_buffer_data[UDP_RECV_BUFFER_SIZE]; /**< Ring buffer backing data */
    size_t available_data_len;                      /**< Available data length from URC notification */
    EventGroupHandle_t recv_event;                  /**< Event for data available */
};

static struct ml307_udp udp_connections[MAX_UDP_CONNECTIONS] __attribute__((section(".psram.data"))) = {0};

/* UDP prefetch task handle */
static TaskHandle_t udp_prefetch_task_handle = NULL;
static volatile bool udp_prefetch_task_running = false;

/* Forward declarations */
static void ml307_udp_urc_handler(const char *command, at_arg_value_t *arguments,
                                  size_t arg_count, void *user_data);
static void ml307_udp_internal_recv_handler(const char *data, size_t len, void *user_data);
static void ml307_udp_prefetch_task(void *pvParameters);

/**
 * @brief Internal receive handler for ml307_udp_recv()
 *
 * Stores received data in ring buffer and signals data availability
 */
static void ml307_udp_internal_recv_handler(const char *data, size_t len, void *user_data)
{
    ml307_udp_t *udp = (ml307_udp_t *)user_data;
    if (!udp || !data || len == 0) return;

    /* Update modem buffer tracking */
    if (udp->available_data_len >= len) {
        udp->available_data_len -= len;
    } else {
        udp->available_data_len = 0;
    }

    /* Write data to ring buffer */
    uint32_t written = ring_buf_put(&udp->ring_buf, (const uint8_t *)data, (uint32_t)len);
    if (written < len) {
        LISA_LOGW(TAG, "UDP %d ring buffer overflow: dropped %zu bytes",
                 udp->udp_id, len - written);
    }

    /* Always set PREFETCH_AVAILABLE to unblock prefetch task, even if buffer is full */
    if (udp->recv_event) {
        xEventGroupSetBits(udp->recv_event, ML307_UDP_PREFETCH_AVAILABLE);
    }
}

/**
 * @brief URC callback handler
 */
static void ml307_udp_urc_handler(const char *command, at_arg_value_t *arguments,
                                  size_t arg_count, void *user_data)
{
    ml307_udp_t *udp = (ml307_udp_t *)user_data;
    if (!udp || !udp->initialized || !command) return;

    /* +MIPOPEN: <connect_id>,<result> - Connection result */
    if (strcmp(command, "MIPOPEN") == 0 && arg_count >= 2) {
        if (arguments[0].type == AT_ARG_TYPE_INT && arguments[0].data.int_val == udp->udp_id) {
            if (arguments[1].type == AT_ARG_TYPE_INT) {
                int result = arguments[1].data.int_val;
                if (result == 0) {
                    udp->connected = true;
                    udp->instance_active = true;
                    xEventGroupClearBits(udp->event_group, ML307_UDP_DISCONNECTED | ML307_UDP_ERROR);
                    xEventGroupSetBits(udp->event_group, ML307_UDP_CONNECTED);
                    LISA_LOGI(TAG, "UDP[%d] connected", udp->udp_id);
                } else {
                    udp->last_error = result;
                    xEventGroupSetBits(udp->event_group, ML307_UDP_ERROR);
                    LISA_LOGE(TAG, "UDP[%d] connection failed: %d", udp->udp_id, result);
                }
            }
        }
    } else if (strcmp(command, "MIPCLOSE") == 0 && arg_count >= 1) {
        if (arguments[0].type == AT_ARG_TYPE_INT && arguments[0].data.int_val == udp->udp_id) {
            udp->instance_active = false;
            xEventGroupSetBits(udp->event_group, ML307_UDP_DISCONNECTED);
            LISA_LOGI(TAG, "UDP[%d] closed", udp->udp_id);
        }
    } else if (strcmp(command, "MIPSEND") == 0 && arg_count >= 2) {
        if (arguments[0].type == AT_ARG_TYPE_INT && arguments[0].data.int_val == udp->udp_id) {
            if (arguments[1].type == AT_ARG_TYPE_INT) {
                udp->last_sent_bytes = arguments[1].data.int_val;
                LISA_LOGD(TAG, "UDP %d sent %d bytes", udp->udp_id, udp->last_sent_bytes);
            }
            xEventGroupSetBits(udp->event_group, ML307_UDP_SEND_COMPLETE);
        }
    } else if (strcmp(command, "MIPRD") == 0 && arg_count >= 3) {
        if (arguments[0].type == AT_ARG_TYPE_INT && arguments[0].data.int_val == udp->udp_id) {
            /* Determine hex data index based on argument count */
            int hex_data_index = 2;  /* Default: arg[2] is hex data */

            /* If 4 arguments: <link_num>,<data_len>,<encoding>,<hex_data> */
            if (arg_count >= 4 && arguments[2].type == AT_ARG_TYPE_INT) {
                hex_data_index = 3;  /* arg[3] is hex data */
            }

            if (arguments[hex_data_index].type == AT_ARG_TYPE_STRING) {
                const char *hex_data = arguments[hex_data_index].data.string_val.value;
                size_t hex_len = arguments[hex_data_index].data.string_val.len;

                /* Decode hex data */
                size_t data_len;
                char *data = at_uart_decode_hex(hex_data, hex_len, &data_len);
                if (data) {
                    /* Store in receive buffer for ml307_udp_recv() */
                    ml307_udp_internal_recv_handler(data, data_len, udp);
                    free(data);

                    /* Signal prefetch complete */
                    if (udp->recv_event) {
                        xEventGroupSetBits(udp->recv_event, ML307_UDP_PREFETCH_AVAILABLE);
                    }
                }
            }
        }
    } else if (strcmp(command, "MIPURC") == 0 && arg_count >= 4) {
        if (arguments[1].type == AT_ARG_TYPE_INT && arguments[1].data.int_val == udp->udp_id) {
            if (arguments[0].type == AT_ARG_TYPE_STRING) {
                const char *urc_type = arguments[0].data.string_val.value;

                if (strcmp(urc_type, "rudp") == 0) {
                    /* Received UDP data */
                    if (udp->connected) {
                        if (arguments[3].type == AT_ARG_TYPE_STRING) {
                            /* Direct mode: arg[3] contains hex data */
                            const char *hex_data = arguments[3].data.string_val.value;
                            size_t hex_len = arguments[3].data.string_val.len;

                            /* Decode hex data */
                            size_t data_len;
                            char *data = at_uart_decode_hex(hex_data, hex_len, &data_len);
                            if (data) {
                                /* Store in receive buffer for ml307_udp_recv() */
                                ml307_udp_internal_recv_handler(data, data_len, udp);
                                free(data);
                            }
                        } else if (arguments[3].type == AT_ARG_TYPE_INT) {
                            /* Cache mode: arg[2]=data_length, arg[3]=total_buffer_length */
                            int data_len = (arguments[2].type == AT_ARG_TYPE_INT) ?
                                          arguments[2].data.int_val : 0;
                            int total_buf = arguments[3].data.int_val;

                            /* Save available data length for prefetch task */
                            udp->available_data_len = total_buf;

                            /* Signal that data is ready for reading via AT+MIPRD */
                            if (udp->recv_event) {
                                xEventGroupSetBits(udp->recv_event, ML307_UDP_DATA_READY);
                            }
                        }
                    }
                } else if (strcmp(urc_type, "disconn") == 0) {
                    /* UDP disconnected */
                    udp->connected = false;
                    udp->instance_active = false;
                    xEventGroupSetBits(udp->event_group, ML307_UDP_DISCONNECTED);
                    LISA_LOGW(TAG, "UDP[%d] disconnected by remote", udp->udp_id);

                    if (udp->disconnect_callback) {
                        udp->disconnect_callback(udp->disconnect_user_data);
                    }
                } else {
                    LISA_LOGE(TAG, "Unknown MIPURC type: %s", urc_type);
                }
            }
        }
    } else if (strcmp(command, "MIPSTATE") == 0) {
        if (arg_count >= 5 &&  arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[0].data.int_val == udp->udp_id && arguments[1].type == AT_ARG_TYPE_STRING &&
            arguments[4].type == AT_ARG_TYPE_STRING) {

            const char *conn_type = arguments[1].data.string_val.value;
            const char *state = arguments[4].data.string_val.value;
            if (state) {
                /* 根据不同状态设置标志 */
                if (strcmp(state, "CONNECTED") == 0) {
                    udp->connected = true;
                    udp->instance_active = true;
                    xEventGroupClearBits(udp->event_group, ML307_UDP_DISCONNECTED | ML307_UDP_ERROR);
                    xEventGroupSetBits(udp->event_group, ML307_UDP_CONNECTED);
                    LISA_LOGI(TAG, "UDP[%d] state: CONNECTED", udp->udp_id);
                } else if (strcmp(state, "INITIAL") == 0) {
                    udp->connected = false;
                    udp->instance_active = true;  /* INITIAL状态表示实例已分配但未连接 */
                    xEventGroupSetBits(udp->event_group, ML307_UDP_INITIALIZED);
                    LISA_LOGI(TAG, "UDP[%d] state: INITIAL", udp->udp_id);
                } else {
                    /* 其他状态（如CLOSING, CLOSED等）*/
                    udp->connected = false;
                    udp->instance_active = false;
                    LISA_LOGI(TAG, "UDP[%d] state: %s", udp->udp_id, state);
                }
            }
        }
    } else if (strcmp(command, "FIFO_OVERFLOW") == 0) {
        xEventGroupSetBits(udp->event_group, ML307_UDP_ERROR);
        LISA_LOGE(TAG, "UDP[%d] FIFO overflow", udp->udp_id);
        ml307_udp_disconnect(udp->udp_id);
    }
}

/**
 * @brief Initialize ML307 UDP connection
 */
int ml307_udp_init(void)
{
    /* Allocate a free connection ID (returns 1-5) */
    int udp_id = ml307_modem_alloc_connect_id();
    if (udp_id < 0) {
        LISA_LOGE(TAG, "No free connection ID available");
        return -1;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    LISA_LOGI(TAG, "Initializing UDP connection %d...", udp_id);

    memset(udp, 0, sizeof(ml307_udp_t));

    /* Initialize non-zero fields only */
    udp->udp_id = udp_id;

    /* Initialize ring buffer for receiving data */
    ring_buf_init(&udp->ring_buf, UDP_RECV_BUFFER_SIZE, udp->recv_buffer_data);

    /* Create event group */
    udp->event_group = xEventGroupCreate();
    if (!udp->event_group) {
        LISA_LOGE(TAG, "Failed to create event group");
        return -1;
    }

    /* Create receive event group */
    udp->recv_event = xEventGroupCreate();
    if (!udp->recv_event) {
        LISA_LOGE(TAG, "Failed to create recv event group");
        vEventGroupDelete(udp->event_group);
        return -1;
    }

    /* Register URC callback */
    udp->urc_node = at_uart_register_urc_callback(ml307_udp_urc_handler, udp);
    if (!udp->urc_node) {
        LISA_LOGE(TAG, "Failed to register URC callback");
        vEventGroupDelete(udp->recv_event);
        vEventGroupDelete(udp->event_group);
        return -1;
    }

    udp->initialized = true;
    LISA_LOGI(TAG, "UDP %d initialized successfully", udp_id);
    return udp_id;
}

/**
 * @brief Deinitialize UDP connection
 */
void ml307_udp_deinit(int udp_id)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        LISA_LOGE(TAG, "Invalid UDP ID: %d (valid range: 1-%d)", udp_id, MAX_UDP_CONNECTIONS);
        return;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        return;
    }

    LISA_LOGI(TAG, "Deinitializing UDP %d...", udp_id);

    /* Unregister URC callback */
    if (udp->urc_node) {
        at_uart_unregister_urc_callback(udp->urc_node);
        udp->urc_node = NULL;
    }

    /* Delete receive event group */
    if (udp->recv_event) {
        vEventGroupDelete(udp->recv_event);
        udp->recv_event = NULL;
    }

    /* Delete event group */
    if (udp->event_group) {
        vEventGroupDelete(udp->event_group);
        udp->event_group = NULL;
    }

    /* Free the connection ID */
    ml307_modem_free_connect_id(udp_id);

    /* Clear state */
    memset(udp, 0, sizeof(ml307_udp_t));
}

/**
 * @brief Connect to remote server
 */
bool ml307_udp_connect(int udp_id, const char *host, int port)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        LISA_LOGE(TAG, "Invalid UDP ID: %d (valid range: 1-%d)", udp_id, MAX_UDP_CONNECTIONS);
        return false;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (udp->connected)
        return true;

    if (!host) {
        LISA_LOGE(TAG, "Invalid parameters: host is NULL");
        return false;
    }

    LISA_LOGI(TAG, "Connecting UDP %d to %s:%d", udp_id, host, port);

    xEventGroupClearBits(udp->event_group,
                        ML307_UDP_CONNECTED | ML307_UDP_DISCONNECTED | ML307_UDP_ERROR);

    char command[128];
    EventBits_t bits;

    /* Query current state */
    snprintf(command, sizeof(command), "AT+MIPSTATE=%d", udp_id);
    if (!at_uart_send_command(command, 1000, true)) {
        LISA_LOGE(TAG, "Failed to query UDP state");
        ml307_udp_deinit(udp_id);
        return false;
    }

    bits = xEventGroupWaitBits(udp->event_group,
                              ML307_UDP_INITIALIZED | ML307_UDP_CONNECTED,
                              pdTRUE, pdFALSE,
                              pdMS_TO_TICKS(UDP_CONNECT_TIMEOUT_MS));
    if (!(bits & ML307_UDP_INITIALIZED) && !(bits & ML307_UDP_CONNECTED)) {
        LISA_LOGE(TAG, "Failed to initialize UDP connection");
        ml307_udp_deinit(udp_id);
        return false;
    }

    /* Close previous connection if active */
    if (udp->connected) {
        LISA_LOGI(TAG, "UDP %d already active, closing...", udp_id);
        snprintf(command, sizeof(command), "AT+MIPCLOSE=%d", udp_id);
        if (at_uart_send_command(command, 1000, true)) {
            xEventGroupWaitBits(udp->event_group,
                              ML307_UDP_DISCONNECTED,
                              pdTRUE, pdFALSE,
                              pdMS_TO_TICKS(UDP_CONNECT_TIMEOUT_MS));
        }
    }

    /* Configure SSL (disabled for UDP) */
    snprintf(command, sizeof(command), "AT+MIPCFG=\"ssl\",%d,0,0", udp_id);
    if (!at_uart_send_command(command, 1000, true)) {
        LISA_LOGE(TAG, "Failed to configure SSL");
        ml307_udp_deinit(udp_id);
        return false;
    }

    /* Configure HEX encoding */
    snprintf(command, sizeof(command), "AT+MIPCFG=\"encoding\",%d,1,1", udp_id);
    if (!at_uart_send_command(command, 1000, true)) {
        LISA_LOGE(TAG, "Failed to set HEX encoding");
        ml307_udp_deinit(udp_id);
        return false;
    }

    /* Open UDP connection */
    snprintf(command, sizeof(command), "AT+MIPOPEN=%d,\"UDP\",\"%s\",%d,60,2,0",
            udp_id, host, port);
    if (!at_uart_send_command(command, UDP_CONNECT_TIMEOUT_MS, true)) {
        udp->last_error = at_uart_get_cme_error_code();
        LISA_LOGE(TAG, "Failed to open UDP connection, error=%d", udp->last_error);
        ml307_udp_deinit(udp_id);
        return false;
    }

    /* Wait for connection result */
    bits = xEventGroupWaitBits(udp->event_group,
                              ML307_UDP_CONNECTED | ML307_UDP_ERROR,
                              pdTRUE, pdFALSE,
                              pdMS_TO_TICKS(UDP_CONNECT_TIMEOUT_MS));

    if (bits & ML307_UDP_ERROR) {
        LISA_LOGE(TAG, "Failed to connect to %s:%d, error=%d", host, port, udp->last_error);
        ml307_udp_deinit(udp_id);
        return false;
    }

    if (!(bits & ML307_UDP_CONNECTED)) {
        LISA_LOGE(TAG, "Connection timeout to %s:%d", host, port);
        ml307_udp_deinit(udp_id);
        return false;
    }

    LISA_LOGI(TAG, "UDP %d connected successfully", udp_id);
    return true;
}

/**
 * @brief Disconnect from remote server
 */
int ml307_udp_disconnect(int udp_id)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        return -1;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        return -1;
    }

    if (udp->instance_active) {
        LISA_LOGI(TAG, "Disconnecting UDP %d", udp_id);

        char command[128];
        snprintf(command, sizeof(command), "AT+MIPCLOSE=%d", udp_id);
        if (at_uart_send_command(command, 1000, true)) {
            xEventGroupWaitBits(udp->event_group,
                              ML307_UDP_DISCONNECTED,
                              pdTRUE, pdFALSE,
                              pdMS_TO_TICKS(UDP_CONNECT_TIMEOUT_MS));
        }

        if (udp->connected) {
            udp->connected = false;
            if (udp->disconnect_callback) {
                udp->disconnect_callback(udp->disconnect_user_data);
            }
        }
    }

    /* Auto-deinitialize after disconnect */
    ml307_udp_deinit(udp_id);

    return 0;
}

/**
 * @brief Send data to remote server
 */
int ml307_udp_send(int udp_id, const char *data, size_t length)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS || !data || length == 0) {
        LISA_LOGE(TAG, "Invalid parameters--udp_id: %d, length: %zu, data: %p", udp_id, length, data);
        return -1;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        LISA_LOGE(TAG, "UDP %d not initialized", udp_id);
        return -1;
    }

    if (!udp->connected) {
        LISA_LOGE(TAG, "UDP[%d] not connected", udp->udp_id);
        return -1;
    }

    size_t total_sent = 0;

    while (total_sent < length) {
        size_t chunk_size = (length - total_sent > UDP_MAX_PACKET_SIZE) ?
                           UDP_MAX_PACKET_SIZE : (length - total_sent);

        size_t hex_len = 0;
        char *hex_data = at_uart_encode_hex(data + total_sent, chunk_size, &hex_len);
        if (!hex_data) {
            LISA_LOGE(TAG, "Failed to encode hex data");
            return -1;
        }

        /* 计算命令头实际长度: "AT+MIPSEND=XX,XXXX," 最多64字节 */
        char cmd_header[64];
        int cmd_len = snprintf(cmd_header, sizeof(cmd_header), "AT+MIPSEND=%d,%zu,", udp_id, chunk_size);

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

        xEventGroupClearBits(udp->event_group, ML307_UDP_SEND_COMPLETE);
        udp->last_sent_bytes = 0;

        if (!at_uart_send_command(command, UDP_SEND_TIMEOUT_MS, false)) {
            LISA_LOGE(TAG, "Failed to send data chunk");
            free(command);
            ml307_udp_disconnect(udp_id);
            return -1;
        }
        free(command);

        /* Wait for send confirmation */
        EventBits_t bits = xEventGroupWaitBits(udp->event_group,
                                              ML307_UDP_SEND_COMPLETE,
                                              pdTRUE, pdFALSE,
                                              pdMS_TO_TICKS(UDP_SEND_TIMEOUT_MS));
        if (!(bits & ML307_UDP_SEND_COMPLETE)) {
            LISA_LOGE(TAG, "No send confirmation received");
            return -1;
        }

        if (udp->last_sent_bytes > 0 && udp->last_sent_bytes != (int)chunk_size) {
            LISA_LOGW(TAG, "UDP %d partial send: requested %zu, sent %d",
                     udp_id, chunk_size, udp->last_sent_bytes);
            total_sent += udp->last_sent_bytes;
        } else {
            total_sent += chunk_size;
        }
    }

    return (int)total_sent;
}

/**
 * @brief Receive data from remote server
 */
int ml307_udp_recv(int udp_id, char *buffer, size_t length, uint32_t timeout_ms)
{
    if (!length) {
        LISA_LOGE(TAG, "recv length is 0");
        errno = EAGAIN;
        return -1;
    }

    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS || !buffer) {
        LISA_LOGE(TAG, "Invalid parameters: udp_id=%d, length=%zu", udp_id, length);
        return -1;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        LISA_LOGE(TAG, "UDP %d not initialized", udp_id);
        return -1;
    }

    if (!udp->connected) {
        LISA_LOGE(TAG, "UDP %d not connected", udp_id);
        return -1;
    }

    /* Check if we have enough data in ring buffer */
    uint32_t available = ring_buf_size_get(&udp->ring_buf);
    if (available < length) {
        /* Polling-based approach: check buffer length directly without event waiting */
        TickType_t start_tick = xTaskGetTickCount();

        /* Use configured recv_timeout_ms if available, otherwise use parameter or default */
        TickType_t timeout_ticks = pdMS_TO_TICKS(udp->recv_timeout_ms > 0 ? udp->recv_timeout_ms : 3);

        do {
            /* Check timeout condition first */
            TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
            if (elapsed_ticks >= timeout_ticks) {
                LISA_LOGD(TAG, "UDP %d recv timeout after %u ms", udp_id, (unsigned int)(elapsed_ticks * portTICK_PERIOD_MS));
                break;
            }

            /* Check if we have enough data now */
            uint32_t current_available = ring_buf_size_get(&udp->ring_buf);
            if (current_available >= length) {
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(3));
        } while (1);
    }

    /* Check if ring buffer has data */
    uint32_t buf_available = ring_buf_size_get(&udp->ring_buf);
    if (buf_available == 0) {
        /* No data available */
        errno = EAGAIN;  /* Set errno to indicate temporarily no data */
        return -1;
    }

    /* Read data from ring buffer */
    uint32_t to_read = (length > buf_available) ? buf_available : length;
    uint32_t bytes_read = ring_buf_get(&udp->ring_buf, (uint8_t *)buffer, to_read);

    if (bytes_read > 0) {
        return (int)bytes_read;
    }

    return 0;
}

/**
 * @brief Register message callback
 */
void ml307_udp_on_message(int udp_id, udp_message_callback_t callback, void *user_data)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        LISA_LOGE(TAG, "Invalid UDP ID: %d (valid range: 1-%d)", udp_id, MAX_UDP_CONNECTIONS);
        return;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        LISA_LOGE(TAG, "UDP %d not initialized", udp_id);
        return;
    }

    udp->message_callback = callback;
    udp->message_user_data = user_data;
}

/**
 * @brief Register disconnect callback
 */
void ml307_udp_on_disconnected(int udp_id, udp_disconnect_callback_t callback, void *user_data)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        LISA_LOGE(TAG, "Invalid UDP ID: %d (valid range: 1-%d)", udp_id, MAX_UDP_CONNECTIONS);
        return;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        LISA_LOGE(TAG, "UDP %d not initialized", udp_id);
        return;
    }

    udp->disconnect_callback = callback;
    udp->disconnect_user_data = user_data;
}

/**
 * @brief Check if UDP is connected
 */
bool ml307_udp_connected(int udp_id)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        return false;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];
    return udp->initialized && udp->connected;
}

/**
 * @brief Get last error code
 */
int ml307_udp_get_last_error(int udp_id)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        return -1;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];
    if (!udp->initialized) {
        return -1;
    }

    return udp->last_error;
}

/**
 * @brief Set send timeout
 */
int ml307_udp_set_send_timeout(int udp_id, uint32_t timeout_ms)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        return -1;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        return -1;
    }

    udp->send_timeout_ms = timeout_ms;
    return 0;
}

/**
 * @brief Set receive timeout
 */
int ml307_udp_set_recv_timeout(int udp_id, uint32_t timeout_ms)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        return -1;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        return -1;
    }

    udp->recv_timeout_ms = timeout_ms;
    return 0;
}

/**
 * @brief Set blocking mode
 */
int ml307_udp_set_blocking(int udp_id, bool blocking)
{
    if (udp_id < 1 || udp_id > MAX_UDP_CONNECTIONS) {
        return -1;
    }

    ml307_udp_t *udp = &udp_connections[UDP_ID_TO_INDEX(udp_id)];

    if (!udp->initialized) {
        return -1;
    }

    udp->blocking = blocking;

    /* If non-blocking, set timeout to 0 */
    if (blocking){
        udp->recv_timeout_ms = 1500;
    } else {
        udp->recv_timeout_ms = 0;
    }

    return 0;
}

/**
 * @brief Try to prefetch UDP data from modem to recv buffer for all active connections
 *
 * @return Total number of bytes prefetched across all connections, 0 if no prefetch needed
 */
static int ml307_udp_data_prefetch(void)
{
    /* Iterate through all UDP connection slots */
    for (int i = 0; i < MAX_UDP_CONNECTIONS; i++) {
        ml307_udp_t *udp = &udp_connections[i];
        int udp_id = i + 1;  /* Connection IDs are 1-5 */

        /* Skip uninitialized or disconnected connections */
        if (!udp->initialized || !udp->connected) {
            continue;
        }

        /* Get available space in ring buffer and calculate read length */
        uint32_t max_space = ring_buf_space_get(&udp->ring_buf);
        size_t read_len = udp->available_data_len < max_space ? udp->available_data_len : max_space;

        /* Check if there's data to read and space available */
        if (read_len == 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        if (read_len > 512) {
            read_len = 512;
        }

        /* Clear the prefetch event before issuing new command */
        xEventGroupClearBits(udp->recv_event, ML307_UDP_PREFETCH_AVAILABLE);

        /* Issue AT+MIPRD command to read data from modem (with retry) */
        char command[128];
        snprintf(command, sizeof(command), "AT+MIPRD=%d,%zu", udp_id, read_len);

        int retry_count = 0;
        const int max_retries = 2;  /* 最多重试2次 */
        bool cmd_success = false;

        while (retry_count <= max_retries && !cmd_success) {
            if (!at_uart_send_command(command, 1000, true)) {
                retry_count++;
                LISA_LOGE(TAG, "UDP %d failed to send MIPRD command (attempt %d/%d)",
                         udp_id, retry_count, max_retries + 1);

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
            LISA_LOGE(TAG, "UDP %d MIPRD command failed after %d retries, skipping",
                     udp_id, max_retries + 1);
            continue;
        }

        /* Wait for data to be received into recv_buffer (via URC handler) */
        EventBits_t bits = xEventGroupWaitBits(udp->recv_event,
                                               ML307_UDP_PREFETCH_AVAILABLE,
                                               pdTRUE,  /* Clear bit after wait */
                                               pdFALSE,
                                               pdMS_TO_TICKS(1000));

        if ((bits & ML307_UDP_PREFETCH_AVAILABLE) == 0) {
            LISA_LOGW(TAG, "UDP %d prefetch timeout after MIPRD", udp_id);
            continue;  /* Try next connection */
        }
    }

    return 0;
}

/**
 * @brief UDP prefetch task
 *
 * This task runs periodically to prefetch UDP data from the modem
 * for all active connections, reducing latency.
 */
static void ml307_udp_prefetch_task(void *pvParameters)
{
    (void)pvParameters;

    LISA_LOGI(TAG, "UDP prefetch task started (interval: %d ms)", UDP_PREFETCH_INTERVAL_MS);

    while (udp_prefetch_task_running) {
        /* Call prefetch function for all active connections */
        ml307_udp_data_prefetch();

        /* Sleep for the configured interval */
        vTaskDelay(pdMS_TO_TICKS(UDP_PREFETCH_INTERVAL_MS));
    }

    LISA_LOGI(TAG, "UDP prefetch task stopped");
    udp_prefetch_task_handle = NULL;
    vTaskDelete(NULL);
}

int ml307_udp_start_prefetch_task(void)
{
    if (udp_prefetch_task_handle != NULL) {
        LISA_LOGW(TAG, "UDP prefetch task already running");
        return -1;
    }

    udp_prefetch_task_running = true;

    BaseType_t ret = xTaskCreate(
        ml307_udp_prefetch_task,
        "udp_prefetch",
        UDP_PREFETCH_TASK_STACK_SIZE,
        NULL,
        UDP_PREFETCH_TASK_PRIORITY,
        &udp_prefetch_task_handle
    );

    if (ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create UDP prefetch task");
        udp_prefetch_task_running = false;
        udp_prefetch_task_handle = NULL;
        return -1;
    }

    LISA_LOGI(TAG, "UDP prefetch task created successfully");
    return 0;
}

/**
 * @brief Stop UDP prefetch task
 *
 * Stops the background UDP prefetch task.
 */
void ml307_udp_stop_prefetch_task(void)
{
    if (udp_prefetch_task_handle == NULL) {
        LISA_LOGW(TAG, "UDP prefetch task not running");
        return;
    }

    LISA_LOGI(TAG, "Stopping UDP prefetch task...");
    udp_prefetch_task_running = false;

    /* Wait for task to finish (max 1 second) */
    for (int i = 0; i < 10 && udp_prefetch_task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (udp_prefetch_task_handle != NULL) {
        LISA_LOGW(TAG, "UDP prefetch task did not stop gracefully");
    }
}
