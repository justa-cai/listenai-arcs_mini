/**
 * @file at_uart.c
 * @brief AT Command UART Implementation
 * @details UART communication for 4G module AT commands
 *          Reference: c_version/at_uart.c (command/response/URC mechanism)
 *          Using lisa_uart driver with FreeRTOS synchronization
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "at_uart.h"
#include "IOMuxManager.h"
#include "lisa_device.h"
#include "lisa_uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#define TAG "at_uart"
#include "lisa_log.h"

/* Bit position macros for event groups */
#ifndef BIT0
#define BIT0    (1 << 0)
#define BIT1    (1 << 1)
#define BIT2    (1 << 2)
#define BIT3    (1 << 3)
#define BIT4    (1 << 4)
#define BIT5    (1 << 5)
#define BIT6    (1 << 6)
#define BIT7    (1 << 7)
#define BIT8    (1 << 8)
#define BIT9    (1 << 9)
#define BIT10   (1 << 10)
#define BIT11   (1 << 11)
#define BIT12   (1 << 12)
#define BIT13   (1 << 13)
#define BIT14   (1 << 14)
#define BIT15   (1 << 15)
#endif

/* AT UART Configuration */
#define AT_UART_DEVICE              "uart2"        /* UART device name */
#define AT_UART_DEFAULT_BAUDRATE    115200        /* Default baudrate */
#define AT_UART_USED_BAUDRATE       921600        /* used baudrate */
#define AT_UART_RX_BUF_SIZE         512            /* Receive buffer size per circular buffer */
#define AT_UART_RX_BUF_COUNT        60              /* Circular buffer count - increased for high load scenarios */
#define AT_UART_TASK_STACK_SIZE     2048           /* Task stack size */
#define AT_UART_TASK_PRIORITY       5              /* Task priority */

#define RX_BUFFER_INITIAL_SIZE      512            /* Initial dynamic buffer size */
#define RESPONSE_BUFFER_SIZE        512            /* Response buffer size */

/* AT Response Constants */
#define AT_RESPONSE_OK_LEN          4              /* Length of "OK\r\n" */
#define AT_RESPONSE_ERROR_LEN       7              /* Length of "ERROR\r\n" */
#define AT_RESPONSE_PROMPT_LEN      1              /* Length of '>' prompt */
#define AT_RESPONSE_CRLF_LEN        2              /* Length of "\r\n" */

/* Event bits */
#define AT_EVENT_COMMAND_DONE       BIT0           /* Command completed successfully */
#define AT_EVENT_COMMAND_ERROR      BIT1           /* Command error */
#define AT_EVENT_DATA_PROMPT        BIT2           /* '>' prompt received */

/* UART1 引脚: PB2=TX, PB3=RX */
#define UART2_TX_PAD   CSK_IOMUX_PAD_A
#define UART2_TX_PIN   15
#define UART2_RX_PAD   CSK_IOMUX_PAD_A
#define UART2_RX_PIN   16
#define UART2_FUNC     CSK_IOMUX_FUNC_ALTER4

/* AT UART Module Context */
static struct {
    lisa_device_t *uart_dev;                    /* UART device pointer */
    uint8_t rx_hw_buffer[1024];  /* HW receive buffer */

    /* Dynamic receive buffer (auto-grow) */
    char *rx_buffer;
    size_t rx_buffer_size;
    size_t rx_buffer_capacity;

    /* Response buffer */
    char response[RESPONSE_BUFFER_SIZE];
    int cme_error_code;

    /* Synchronization */
    TaskHandle_t process_task;                  /* Processing task handle */
    SemaphoreHandle_t tx_mutex;                 /* TX mutex for thread safety */
    SemaphoreHandle_t cmd_mutex;                /* Command mutex */
    SemaphoreHandle_t buffer_mutex;             /* Buffer mutex */
    EventGroupHandle_t event_group;             /* Event group for command sync */

    /* URC callback list */
    at_urc_callback_node_t *urc_callbacks_head;

    /* Flags */
    uint8_t initialized;
    uint8_t uart_config_status;

    bool wait_for_response;
} at_uart_ctx = {0};

void lisa_uart2_pinmux(void)
{
    IOMuxManager_PinConfigure(UART2_TX_PAD, UART2_TX_PIN, UART2_FUNC);
    IOMuxManager_PinConfigure(UART2_RX_PAD, UART2_RX_PIN, UART2_FUNC);
}

/**
 * @brief Handle URC (Unsolicited Result Code)
 */
static void handle_urc(const char *command, at_arg_value_t *arguments, size_t arg_count)
{
    if (!command) return;

    /* Special handling for CME ERROR */
    if (strcmp(command, "CME ERROR") == 0 && arg_count > 0) {
        if (arguments[0].type == AT_ARG_TYPE_INT) {
            at_uart_ctx.cme_error_code = arguments[0].data.int_val;
            LISA_LOGE(TAG, "CME ERROR received, error_code=%d", at_uart_ctx.cme_error_code);
            xEventGroupSetBits(at_uart_ctx.event_group, AT_EVENT_COMMAND_ERROR);
        }
        return;
    }

    /* Call all registered URC callbacks */
    at_urc_callback_node_t *node = at_uart_ctx.urc_callbacks_head;
    while (node) {
        if (node->callback) {
            node->callback(command, arguments, arg_count, node->user_data);
        }
        node = node->next;
    }
}

/**
 * @brief Check if string is a number
 */
static bool is_number(const char *s)
{
    if (!s || *s == '\0') return false;

    size_t len = strlen(s);
    if (len >= 10) return false; /* Prevent overflow */

    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)s[i])) return false;
    }
    return true;
}


/* Legacy API compatibility (deprecated) */
int at_uart_send(const uint8_t *data, uint16_t size)
{
    LISA_LOGW(TAG, "at_uart_send() is deprecated. Use at_uart_send_command() instead.");

    if (!at_uart_ctx.initialized || !data || size == 0) {
        return -1;
    }

    xSemaphoreTake(at_uart_ctx.tx_mutex, portMAX_DELAY);
    int ret = lisa_uart_write_sync(at_uart_ctx.uart_dev, (const uint8_t *)data, size, 1000);
    xSemaphoreGive(at_uart_ctx.tx_mutex);

    return ret;
}

/**
 * @brief Send AT command，todo:返回发送的字节数
 */
bool at_uart_send_command(const char *command, size_t timeout_ms, bool add_crlf)
{
    // LISA_LOGI(TAG, "at_uart_send_command: %s", command);
    return at_uart_send_command_with_data(command, timeout_ms, add_crlf, NULL, 0);
}

/**
 * @brief Send AT command with data
 */
bool at_uart_send_command_with_data(const char *command, size_t timeout_ms,
                                    bool add_crlf, const uint8_t *data, size_t data_len)
{
    if (!at_uart_ctx.initialized || !command) {
        LISA_LOGE(TAG, "Invalid parameters");
        return false;
    }
    if (at_uart_ctx.uart_config_status){
        return false;
    }
    xSemaphoreTake(at_uart_ctx.cmd_mutex, portMAX_DELAY);
    /* Clear event bits */
    xEventGroupClearBits(at_uart_ctx.event_group,
                        AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR | AT_EVENT_DATA_PROMPT);
    at_uart_ctx.wait_for_response = true;
    at_uart_ctx.cme_error_code = 0;

    /* Clear response buffer */
    xSemaphoreTake(at_uart_ctx.buffer_mutex, portMAX_DELAY);
    at_uart_ctx.response[0] = '\0';
    xSemaphoreGive(at_uart_ctx.buffer_mutex);

    /* Send command */
    xSemaphoreTake(at_uart_ctx.tx_mutex, portMAX_DELAY);

    int ret;
    if (add_crlf) {
        size_t cmd_len = strlen(command);

        /* 防止超长命令 */
        if (cmd_len > 1024) {
            LISA_LOGE(TAG, "Command too long: %zu bytes", cmd_len);
            xSemaphoreGive(at_uart_ctx.tx_mutex);
            at_uart_ctx.wait_for_response = false;
            xSemaphoreGive(at_uart_ctx.cmd_mutex);
            return false;
        }

        /* 分配缓冲区: 命令长度 + "\r\n" + '\0' */
        char *cmd_with_crlf = (char *)malloc(cmd_len + 3);
        if (!cmd_with_crlf) {
            LISA_LOGE(TAG, "Failed to allocate memory for command");
            xSemaphoreGive(at_uart_ctx.tx_mutex);
            at_uart_ctx.wait_for_response = false;
            xSemaphoreGive(at_uart_ctx.cmd_mutex);
            return false;
        }

        /* 安全地构造带 CRLF 的命令 */
        memcpy(cmd_with_crlf, command, cmd_len);
        cmd_with_crlf[cmd_len] = '\r';
        cmd_with_crlf[cmd_len + 1] = '\n';
        cmd_with_crlf[cmd_len + 2] = '\0';

        ret = lisa_uart_write_sync(at_uart_ctx.uart_dev, (const uint8_t *)cmd_with_crlf, cmd_len + 2, 1000);
        free(cmd_with_crlf);
    } else {
        ret = lisa_uart_write_sync(at_uart_ctx.uart_dev, (const uint8_t *)command, strlen(command), 1000);
    }

    xSemaphoreGive(at_uart_ctx.tx_mutex);

    if (ret < 0) {
        LISA_LOGE(TAG, "Failed to send command");
        at_uart_ctx.wait_for_response = false;
        xSemaphoreGive(at_uart_ctx.cmd_mutex);
        return false;
    }
    /* Wait for response */
    bool result = false;
    if (timeout_ms > 0) {
        EventBits_t bits = xEventGroupWaitBits(at_uart_ctx.event_group,
                                              AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR | AT_EVENT_DATA_PROMPT,
                                              pdTRUE, pdFALSE,
                                              pdMS_TO_TICKS(timeout_ms));
        at_uart_ctx.wait_for_response = false;

        if (bits & AT_EVENT_COMMAND_DONE) {
            result = true;
        } else if (bits & AT_EVENT_DATA_PROMPT) {
            /* '>' prompt received, ready to send data */
            result = true;
        }
    } else {
        at_uart_ctx.wait_for_response = false;
        result = true;
    }

    /* Send data payload if provided */
    if (result && data && data_len > 0) {
        at_uart_ctx.wait_for_response = true;
        xEventGroupClearBits(at_uart_ctx.event_group,
                            AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR);

        xSemaphoreTake(at_uart_ctx.tx_mutex, portMAX_DELAY);
        ret = lisa_uart_write_sync(at_uart_ctx.uart_dev, (const uint8_t *)data, data_len, 1000);
        xSemaphoreGive(at_uart_ctx.tx_mutex);

        if (ret < 0) {
            LISA_LOGE(TAG, "Failed to send data");
            result = false;
        } else {
            EventBits_t bits = xEventGroupWaitBits(at_uart_ctx.event_group,
                                                  AT_EVENT_COMMAND_DONE | AT_EVENT_COMMAND_ERROR,
                                                  pdTRUE, pdFALSE,
                                                  pdMS_TO_TICKS(timeout_ms));
            result = (bits & AT_EVENT_COMMAND_DONE) != 0;
        }

        at_uart_ctx.wait_for_response = false;
    }
    xSemaphoreGive(at_uart_ctx.cmd_mutex);

    return result;
}


/**
 * @brief Get CME error code
 */
int at_uart_get_cme_error_code(void)
{
    return at_uart_ctx.cme_error_code;
}

/**
 * @brief Parse AT command arguments
 */
static void parse_arguments(const char *values, at_arg_value_t **out_args, size_t *out_count)
{
    if (!values || !out_args || !out_count) {
        *out_args = NULL;
        *out_count = 0;
        return;
    }

    /* Allocate argument array */
    size_t capacity = 4;
    at_arg_value_t *args = (at_arg_value_t *)calloc(capacity, sizeof(at_arg_value_t));
    size_t count = 0;

    const char *p = values;
    const char *start = p;

    /* 动态分配item缓冲区,支持大数据 (如TCP hex数据可达2KB+) */
    #define MAX_ARG_ITEM_SIZE 10240
    char *item = (char *)malloc(MAX_ARG_ITEM_SIZE);
    if (!item) {
        free(args);
        *out_args = NULL;
        *out_count = 0;
        return;
    }

    while (*p) {
        /* Find delimiter or end */
        if (*p == ',' || *p == '\r' || *p == '\n' || *p == '\0') {
            size_t item_len = p - start;

            /* 检查缓冲区大小，确保有空间存储null终止符 */
            if (item_len >= MAX_ARG_ITEM_SIZE) {
                LISA_LOGE(TAG, "Argument too long (%zu bytes), truncating to %d", item_len, MAX_ARG_ITEM_SIZE - 1);
                item_len = MAX_ARG_ITEM_SIZE - 1;
            }

            /* Expand capacity if needed */
            if (count >= capacity) {
                capacity *= 2;
                at_arg_value_t *new_args = (at_arg_value_t *)realloc(args, capacity * sizeof(at_arg_value_t));
                if (new_args) {
                    args = new_args;
                } else {
                    /* Memory allocation failed */
                    at_arg_array_destroy(args, count);
                    free(item);
                    *out_args = NULL;
                    *out_count = 0;
                    return;
                }
            }

            /* Copy argument (包括空参数,如 1,,,,"INITIAL" 中的空项) */
            memcpy(item, start, item_len);
            item[item_len] = '\0';

            /* Trim whitespace */
            char *trimmed = item;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            char *end = trimmed + strlen(trimmed) - 1;
            while (end > trimmed && (*end == ' ' || *end == '\t')) *end-- = '\0';

            /* Identify type */
            if (strlen(trimmed) == 0) {
                /* 空参数: 保留为空字符串 */
                args[count].type = AT_ARG_TYPE_STRING;
                args[count].data.string_val.value = strdup("");
                if (!args[count].data.string_val.value) {
                    LISA_LOGE(TAG, "Failed to allocate memory for empty string");
                    at_arg_array_destroy(args, count);
                    free(item);
                    *out_args = NULL;
                    *out_count = 0;
                    return;
                }
                args[count].data.string_val.len = 0;
            } else if (trimmed[0] == '"' && strlen(trimmed) > 1) {
                /* String type */
                args[count].type = AT_ARG_TYPE_STRING;
                size_t str_len = strlen(trimmed) - 2;
                args[count].data.string_val.value = (char *)malloc(str_len + 1);
                if (!args[count].data.string_val.value) {
                    LISA_LOGE(TAG, "Failed to allocate memory for string argument");
                    at_arg_array_destroy(args, count);
                    free(item);
                    *out_args = NULL;
                    *out_count = 0;
                    return;
                }
                memcpy(args[count].data.string_val.value, trimmed + 1, str_len);
                args[count].data.string_val.value[str_len] = '\0';
                args[count].data.string_val.len = str_len;
            } else if (is_number(trimmed)) {
                /* Integer type */
                args[count].type = AT_ARG_TYPE_INT;
                args[count].data.int_val = atoi(trimmed);
            } else {
                /* Other as string */
                args[count].type = AT_ARG_TYPE_STRING;
                args[count].data.string_val.value = strdup(trimmed);
                if (!args[count].data.string_val.value) {
                    LISA_LOGE(TAG, "Failed to allocate memory for string argument");
                    at_arg_array_destroy(args, count);
                    free(item);
                    *out_args = NULL;
                    *out_count = 0;
                    return;
                }
                args[count].data.string_val.len = strlen(trimmed);
            }

            count++;

            /* 只在遇到行结束符时才退出循环 */
            if (*p == '\r' || *p == '\n' || *p == '\0') break;

            start = p + 1;
        }
        p++;
    }

    /* 处理最后一个参数(如果字符串不以逗号结尾) */
    if (p > start) {
        size_t item_len = p - start;

        /* 检查缓冲区大小，确保有空间存储null终止符 */
        if (item_len >= MAX_ARG_ITEM_SIZE) {
            LISA_LOGE(TAG, "Last argument too long (%zu bytes), truncating to %d", item_len, MAX_ARG_ITEM_SIZE - 1);
            item_len = MAX_ARG_ITEM_SIZE - 1;
        }

        /* Expand capacity if needed */
        if (count >= capacity) {
            capacity *= 2;
            at_arg_value_t *new_args = (at_arg_value_t *)realloc(args, capacity * sizeof(at_arg_value_t));
            if (new_args) {
                args = new_args;
            } else {
                /* Memory allocation failed */
                at_arg_array_destroy(args, count);
                free(item);
                *out_args = NULL;
                *out_count = 0;
                return;
            }
        }

        /* Copy last argument */
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
            args[count].data.string_val.value = strdup("");
            if (!args[count].data.string_val.value) {
                LISA_LOGE(TAG, "Failed to allocate memory for empty string");
                at_arg_array_destroy(args, count);
                free(item);
                *out_args = NULL;
                *out_count = 0;
                return;
            }
            args[count].data.string_val.len = 0;
        } else if (trimmed[0] == '"' && strlen(trimmed) > 1) {
            args[count].type = AT_ARG_TYPE_STRING;
            size_t str_len = strlen(trimmed) - 2;
            args[count].data.string_val.value = (char *)malloc(str_len + 1);
            if (!args[count].data.string_val.value) {
                LISA_LOGE(TAG, "Failed to allocate memory for string argument");
                at_arg_array_destroy(args, count);
                free(item);
                *out_args = NULL;
                *out_count = 0;
                return;
            }
            memcpy(args[count].data.string_val.value, trimmed + 1, str_len);
            args[count].data.string_val.value[str_len] = '\0';
            args[count].data.string_val.len = str_len;
        } else if (is_number(trimmed)) {
            args[count].type = AT_ARG_TYPE_INT;
            args[count].data.int_val = atoi(trimmed);
        } else {
            args[count].type = AT_ARG_TYPE_STRING;
            args[count].data.string_val.value = strdup(trimmed);
            if (!args[count].data.string_val.value) {
                LISA_LOGE(TAG, "Failed to allocate memory for string argument");
                at_arg_array_destroy(args, count);
                free(item);
                *out_args = NULL;
                *out_count = 0;
                return;
            }
            args[count].data.string_val.len = strlen(trimmed);
        }

        count++;
    }

    /* 释放临时缓冲区 */
    free(item);

    *out_args = args;
    *out_count = count;
}

/**
 * @brief Register URC callback
 */
at_urc_callback_node_t *at_uart_register_urc_callback(at_urc_callback_t callback, void *user_data)
{
    if (!at_uart_ctx.initialized || !callback) {
        return NULL;
    }

    at_urc_callback_node_t *node = (at_urc_callback_node_t *)malloc(sizeof(at_urc_callback_node_t));
    if (!node) {
        return NULL;
    }

    node->callback = callback;
    node->user_data = user_data;

    xSemaphoreTake(at_uart_ctx.buffer_mutex, portMAX_DELAY);
    node->next = at_uart_ctx.urc_callbacks_head;
    at_uart_ctx.urc_callbacks_head = node;
    xSemaphoreGive(at_uart_ctx.buffer_mutex);

    LISA_LOGI(TAG, "URC callback registered");

    return node;
}

/**
 * @brief Unregister URC callback
 */
void at_uart_unregister_urc_callback(at_urc_callback_node_t *node)
{
    if (!at_uart_ctx.initialized || !node) {
        return;
    }

    xSemaphoreTake(at_uart_ctx.buffer_mutex, portMAX_DELAY);

    at_urc_callback_node_t **current = &at_uart_ctx.urc_callbacks_head;
    while (*current) {
        if (*current == node) {
            *current = node->next;
            free(node);
            LISA_LOGI(TAG, "URC callback unregistered");
            break;
        }
        current = &(*current)->next;
    }

    xSemaphoreGive(at_uart_ctx.buffer_mutex);
}

/**
 * @brief Destroy argument array
 */
void at_arg_array_destroy(at_arg_value_t *args, size_t count)
{
    if (!args) return;

    for (size_t i = 0; i < count; i++) {
        if (args[i].type == AT_ARG_TYPE_STRING && args[i].data.string_val.value) {
            free(args[i].data.string_val.value);
        }
    }
    free(args);
}

/**
 * @brief Parse response from RX buffer
 */
static bool parse_response(void)
{
    if (!at_uart_ctx.rx_buffer || at_uart_ctx.rx_buffer_size == 0) {
        return false;
    }

    /* Check for '>' prompt (data input mode) */
    if (at_uart_ctx.wait_for_response && at_uart_ctx.rx_buffer[0] == '>') {
        size_t remaining = at_uart_ctx.rx_buffer_size - AT_RESPONSE_PROMPT_LEN;
        if (remaining > 0) {
            memmove(at_uart_ctx.rx_buffer, at_uart_ctx.rx_buffer + AT_RESPONSE_PROMPT_LEN, remaining);
        }
        at_uart_ctx.rx_buffer_size = remaining;
        at_uart_ctx.rx_buffer[at_uart_ctx.rx_buffer_size] = '\0';

        xEventGroupSetBits(at_uart_ctx.event_group, AT_EVENT_DATA_PROMPT);
        return true;
    }

    /* Find line ending */
    char *end_pos = strstr(at_uart_ctx.rx_buffer, "\r\n");
    if (!end_pos) {
        return false; /* Wait for more data */
    }

    size_t line_len = end_pos - at_uart_ctx.rx_buffer;

    /* Ignore empty lines */
    if (line_len == 0) {
        size_t remaining = at_uart_ctx.rx_buffer_size - AT_RESPONSE_CRLF_LEN;
        if (remaining > 0) {
            memmove(at_uart_ctx.rx_buffer, at_uart_ctx.rx_buffer + AT_RESPONSE_CRLF_LEN, remaining);
        }
        at_uart_ctx.rx_buffer_size = remaining;
        at_uart_ctx.rx_buffer[at_uart_ctx.rx_buffer_size] = '\0';
        return true;
    }

    bool handled = false;
    /* 安全日志: 限制打印长度,防止缓冲区溢出 */
    // if (line_len <= 100) {
    //     LISA_LOGE(TAG, "parse_response recv: %.*s", (int)line_len, at_uart_ctx.rx_buffer);
    // } else {
    //     LISA_LOGE(TAG, "parse_response recv (len=%zu): %.*s...", line_len, 150, at_uart_ctx.rx_buffer);
    // }
    /* URC message (starts with '+') */
    if (at_uart_ctx.rx_buffer[0] == '+') {
        char command[64] = {0};
        const char *colon = strchr(at_uart_ctx.rx_buffer, ':');
        if (colon && (size_t)(colon - at_uart_ctx.rx_buffer) < line_len) {
            /* URC with arguments: +COMMAND: value1,value2 */
            size_t cmd_len = colon - at_uart_ctx.rx_buffer - 1; /* Skip '+' */
            if (cmd_len < sizeof(command)) {
                strncpy(command, at_uart_ctx.rx_buffer + 1, cmd_len);
                command[cmd_len] = '\0';

                /* Parse arguments - 安全地跳过 ": " */
                size_t colon_offset = colon - at_uart_ctx.rx_buffer;

                /* 计算参数字符串的长度: 从冒号后到行尾 */
                size_t values_len = 0;
                const char *values_start = NULL;

                /* 检查冒号后是否还有至少2个字符 (空格和数据) */
                if (colon_offset + 2 < line_len) {
                    values_start = colon + 2; /* Skip ": " */
                    values_len = line_len - (colon_offset + 2);
                } else if (colon_offset + 1 < line_len) {
                    values_start = colon + 1; /* 只跳过冒号,没有空格 */
                    values_len = line_len - (colon_offset + 1);
                } else {
                    values_start = ""; /* 冒号后没有数据 */
                    values_len = 0;
                }

                /* 复制参数字符串到临时缓冲区,确保只包含当前行 */
                char *values = NULL;
                if (values_len > 0) {
                    values = (char *)malloc(values_len + 1);
                    if (values) {
                        memcpy(values, values_start, values_len);
                        values[values_len] = '\0';
                    } else {
                        LISA_LOGE(TAG, "Failed to allocate memory for URC values");
                        values = strdup("");  /* 尝试使用空字符串 */
                        if (!values) {
                            return false;  /* 内存严重不足，跳过此URC */
                        }
                    }
                } else {
                    values = strdup("");
                    if (!values) {
                        LISA_LOGE(TAG, "Failed to allocate memory for empty URC values");
                        return false;  /* 内存严重不足，跳过此URC */
                    }
                }

                at_arg_value_t *args = NULL;
                size_t arg_count = 0;

                parse_arguments(values, &args, &arg_count);
                handle_urc(command, args, arg_count);

                /* Free arguments */
                at_arg_array_destroy(args, arg_count);

                /* Free values buffer */
                free(values);
            }
        } else {
            /* URC without arguments: +COMMAND */
            size_t cmd_len = line_len - 1; /* Skip '+' */
            if (cmd_len < sizeof(command)) {
                strncpy(command, at_uart_ctx.rx_buffer + 1, cmd_len);
                command[cmd_len] = '\0';
                handle_urc(command, NULL, 0);
            }
        }
        handled = true;
    } else if (at_uart_ctx.rx_buffer_size >= AT_RESPONSE_OK_LEN &&
             at_uart_ctx.rx_buffer[0] == 'O' &&
             at_uart_ctx.rx_buffer[1] == 'K' &&
             at_uart_ctx.rx_buffer[2] == '\r' &&
             at_uart_ctx.rx_buffer[3] == '\n') {
        size_t remaining = at_uart_ctx.rx_buffer_size - AT_RESPONSE_OK_LEN;
        if (remaining > 0) {
            memmove(at_uart_ctx.rx_buffer, at_uart_ctx.rx_buffer + AT_RESPONSE_OK_LEN, remaining);
        }
        at_uart_ctx.rx_buffer_size = remaining;
        at_uart_ctx.rx_buffer[at_uart_ctx.rx_buffer_size] = '\0';

        xEventGroupSetBits(at_uart_ctx.event_group, AT_EVENT_COMMAND_DONE);
        return true;
    } else if (at_uart_ctx.rx_buffer_size >= AT_RESPONSE_ERROR_LEN &&
             memcmp(at_uart_ctx.rx_buffer, "ERROR\r\n", AT_RESPONSE_ERROR_LEN) == 0) {
        size_t remaining = at_uart_ctx.rx_buffer_size - AT_RESPONSE_ERROR_LEN;
        if (remaining > 0) {
            memmove(at_uart_ctx.rx_buffer, at_uart_ctx.rx_buffer + AT_RESPONSE_ERROR_LEN, remaining);
        }
        at_uart_ctx.rx_buffer_size = remaining;
        at_uart_ctx.rx_buffer[at_uart_ctx.rx_buffer_size] = '\0';

        xEventGroupSetBits(at_uart_ctx.event_group, AT_EVENT_COMMAND_ERROR);
        return true;
    } else {
        size_t copy_len = (line_len < RESPONSE_BUFFER_SIZE - 1) ?
                         line_len : (RESPONSE_BUFFER_SIZE - 1);
        memcpy(at_uart_ctx.response, at_uart_ctx.rx_buffer, copy_len);
        at_uart_ctx.response[copy_len] = '\0';
        LISA_LOGE(TAG, "response: %s--------", at_uart_ctx.response);
        handled = true;
    }

    /* Remove processed line */
    if (handled) {
        size_t bytes_to_remove = line_len + AT_RESPONSE_CRLF_LEN; /* line + "\r\n" */

        /* 防止下溢: 确保缓冲区足够大 */
        if (bytes_to_remove > at_uart_ctx.rx_buffer_size) {
            LISA_LOGE(TAG, "BUG: bytes_to_remove(%zu) > buffer_size(%zu), corrupted state!",
                      bytes_to_remove, at_uart_ctx.rx_buffer_size);
            /* 清空缓冲区避免进一步错误 */
            at_uart_ctx.rx_buffer_size = 0;
            at_uart_ctx.rx_buffer[0] = '\0';
            return false;
        }

        /* 防止 memmove 源指针越界: 确保 end_pos + AT_RESPONSE_CRLF_LEN 在缓冲区内 */
        size_t end_offset = end_pos - at_uart_ctx.rx_buffer + AT_RESPONSE_CRLF_LEN;
        if (end_offset > at_uart_ctx.rx_buffer_size) {
            LISA_LOGE(TAG, "BUG: end_offset(%zu) > buffer_size(%zu), corrupted!",
                      end_offset, at_uart_ctx.rx_buffer_size);
            at_uart_ctx.rx_buffer_size = 0;
            at_uart_ctx.rx_buffer[0] = '\0';
            return false;
        }

        /* 计算剩余数据的实际长度 */
        size_t remaining_bytes = at_uart_ctx.rx_buffer_size - bytes_to_remove;
        at_uart_ctx.rx_buffer_size = remaining_bytes;

        /* 只移动剩余数据，避免越界读取 */
        if (remaining_bytes > 0) {
            memmove(at_uart_ctx.rx_buffer, end_pos + AT_RESPONSE_CRLF_LEN, remaining_bytes);
        }
        at_uart_ctx.rx_buffer[at_uart_ctx.rx_buffer_size] = '\0';
    }

    return true;
}

/**
 * @brief Process received data from AT UART
 */
static void at_uart_process_data(uint8_t *data, int len)
{
    if (len <= 0) {
        return;
    }

    xSemaphoreTake(at_uart_ctx.buffer_mutex, portMAX_DELAY);

    /* Ensure buffer has enough space (需要额外1字节存储'\0') */
    if (at_uart_ctx.rx_buffer_size + len + 1 > at_uart_ctx.rx_buffer_capacity) {
        /* 扩容: 当前大小 + 新数据长度 + 1(终止符) + 512(额外空间) */
        at_uart_ctx.rx_buffer_capacity = at_uart_ctx.rx_buffer_size + len + 1 + 512;
        char *new_buffer = (char *)realloc(at_uart_ctx.rx_buffer, at_uart_ctx.rx_buffer_capacity);
        if (new_buffer) {
            at_uart_ctx.rx_buffer = new_buffer;
        } else {
            LISA_LOGE(TAG, "Failed to reallocate rx_buffer");
            xSemaphoreGive(at_uart_ctx.buffer_mutex);
            return;
        }
    }

    /* Append data to buffer */
    memcpy(at_uart_ctx.rx_buffer + at_uart_ctx.rx_buffer_size, data, len);
    at_uart_ctx.rx_buffer_size += len;
    at_uart_ctx.rx_buffer[at_uart_ctx.rx_buffer_size] = '\0';

    /* Parse all available responses */
    while (parse_response()) {}

    xSemaphoreGive(at_uart_ctx.buffer_mutex);
}

/**
 * @brief AT UART data processing task
 */
static void at_uart_process_task(void *pvParameters)
{
    int ret;

    LISA_LOGI(TAG, "AT UART process task started");

    while (1) {
        if (at_uart_ctx.uart_config_status){
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        /* Synchronous receive data (blocking wait) */
        ret = lisa_uart_read_sync(at_uart_ctx.uart_dev, at_uart_ctx.rx_hw_buffer,
                                  sizeof(at_uart_ctx.rx_hw_buffer));
        // LISA_LOGI(TAG, "UART read data : %d, %s", ret, at_uart_ctx.rx_hw_buffer);
        if (ret < 0) {
            if (ret == LISA_DEVICE_ERR_OVERFLOW) {
                LISA_LOGE(TAG, "Buffer overflow! Recovering...");

                /* 禁用接收，等待硬件FIFO清空 */
                lisa_uart_rx_disable(at_uart_ctx.uart_dev);
                vTaskDelay(pdMS_TO_TICKS(10));

                /*缓冲区状态 */
                xSemaphoreTake(at_uart_ctx.buffer_mutex, portMAX_DELAY);
                at_uart_ctx.rx_buffer_size = 0;
                if (at_uart_ctx.rx_buffer) {
                    at_uart_ctx.rx_buffer[0] = '\0';
                }
                at_uart_ctx.response[0] = '\0';
                xSemaphoreGive(at_uart_ctx.buffer_mutex);

                /* 通知所有等待响应的命令：发生错误 */
                if (at_uart_ctx.wait_for_response) {
                    xEventGroupSetBits(at_uart_ctx.event_group, AT_EVENT_COMMAND_ERROR);
                    at_uart_ctx.wait_for_response = false;
                }

                /* 重新启用接收 */
                lisa_uart_rx_enable(at_uart_ctx.uart_dev);

                LISA_LOGI(TAG, "Buffer overflow recovery completed, AT state cleared");
            } else {
                LISA_LOGE(TAG, "Failed to read data (ret=%d)", ret);
            }
            // vTaskDelay(pdMS_TO_TICKS(1));
        } else if (ret > 0) {
            /* Process received data */
            at_uart_process_data(at_uart_ctx.rx_hw_buffer, ret);
            memset(at_uart_ctx.rx_hw_buffer, 0, sizeof(at_uart_ctx.rx_hw_buffer));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

bool uart_baudrate_adapt(void)
{
    /* Configure UART */
    lisa_uart_config_t uart_config = LISA_UART_CONFIG_HIGH_SPEED();
    uart_config.rx_buf_config.buffer_count = AT_UART_RX_BUF_COUNT;
    uart_config.rx_buf_config.buffer_size = AT_UART_RX_BUF_SIZE;

    /* Test AT communication */
    int retry, switch_count = 0;
    const int max_retries_per_baudrate = 5;
    const int max_switch_times = 5;  /* 最多切换5次波特率 */
    bool at_ready = false;

    for (switch_count = 0; switch_count < max_switch_times && !at_ready; switch_count++) {
        /* 每个波特率重试5次 */
        for (retry = 0; retry < max_retries_per_baudrate; retry++) {
            if (at_uart_send_command("AT", 1000, true)) {
                LISA_LOGI(TAG, "4G AT is ready: %d, baudrate=%d.", __LINE__, uart_config.baudrate);
                at_ready = true;
                break;
            }
            LISA_LOGI(TAG, "4G AT retry %d/%d at baudrate %d.", retry + 1, max_retries_per_baudrate, uart_config.baudrate);
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if (at_ready) {
            break;
        }

        /* 5次重试都失败，切换波特率 */
        if (uart_config.baudrate == AT_UART_DEFAULT_BAUDRATE) {
            uart_config.baudrate = AT_UART_USED_BAUDRATE;
        } else {
            uart_config.baudrate = AT_UART_DEFAULT_BAUDRATE;
        }

        LISA_LOGI(TAG, "Switching baudrate to %d.", uart_config.baudrate);

        at_uart_ctx.uart_config_status = 1;
        lisa_uart_rx_disable(at_uart_ctx.uart_dev);
        int ret = lisa_uart_configure(at_uart_ctx.uart_dev, &uart_config);
        lisa_uart_rx_enable(at_uart_ctx.uart_dev);
        at_uart_ctx.uart_config_status = 0;
        if (ret != 0) {
            LISA_LOGE(TAG, "Failed to configure UART");
            free(at_uart_ctx.rx_buffer);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!at_ready) {
        LISA_LOGE(TAG, "4G AT is not ready after %d baudrate switches!", max_switch_times);
        return false;
    }

    /*修改串口波特率为921600*/
    if (uart_config.baudrate != AT_UART_USED_BAUDRATE) {
        for (retry = 0; retry < 5; retry++) {
            if (at_uart_send_command("AT+IPR=921600", 1000, true)) {
                LISA_LOGI(TAG, "uart baudrate is 921600.");

                uart_config.baudrate = AT_UART_USED_BAUDRATE;
                at_uart_ctx.uart_config_status = 1;
                // vTaskDelay(pdMS_TO_TICKS(20));
                LISA_LOGI(TAG, "lisa_uart_configure: %d.", uart_config.baudrate);
                lisa_uart_rx_disable(at_uart_ctx.uart_dev);
                int ret = lisa_uart_configure(at_uart_ctx.uart_dev, &uart_config);
                lisa_uart_rx_enable(at_uart_ctx.uart_dev);
                // vTaskDelay(pdMS_TO_TICKS(20));
                at_uart_ctx.uart_config_status = 0;
                if (ret != 0) {
                    LISA_LOGE(TAG, "Failed to configure UART");
                    free(at_uart_ctx.rx_buffer);
                    return false;
                }
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (retry >= 5) {
            LISA_LOGE(TAG, "4G AT is not ready: %d, %d.", __LINE__, uart_config.baudrate);
            return false;
        }

        for (retry = 0; retry < 15; retry++) {
            if (at_uart_send_command("AT", 1000, true)) {
                LISA_LOGI(TAG, "4G AT is ready, 921600, %d.", __LINE__);
                break;
            }
            LISA_LOGI(TAG, "4G AT is not ready, 921600, %d.", __LINE__);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (retry >= 15) {
            LISA_LOGE(TAG, "4G AT baudrate adapt fail, 921600, %d.", __LINE__);
            return false;
        }
    }

    return true;
}

/**
 * @brief Initialize AT UART
 */
int at_uart_init(const char *uart_dev_name)
{
    int ret;

    if (at_uart_ctx.initialized) {
        LISA_LOGW(TAG, "AT UART already initialized");
        return 0;
    }

    LISA_LOGI(TAG, "Initializing AT UART module...");

    /* Allocate dynamic receive buffer */
    at_uart_ctx.rx_buffer_capacity = RX_BUFFER_INITIAL_SIZE;
    at_uart_ctx.rx_buffer = (char *)malloc(at_uart_ctx.rx_buffer_capacity);
    if (!at_uart_ctx.rx_buffer) {
        LISA_LOGE(TAG, "Failed to allocate rx_buffer");
        return -1;
    }
    at_uart_ctx.rx_buffer_size = 0;
    at_uart_ctx.rx_buffer[0] = '\0';

    /* Get UART device */
    at_uart_ctx.uart_dev = lisa_device_get(uart_dev_name);
    if (!lisa_device_ready(at_uart_ctx.uart_dev)) {
        LISA_LOGE(TAG, "Failed to get UART device or device not ready");
        free(at_uart_ctx.rx_buffer);
        return -1;
    }

    lisa_uart_config_t uart_config = LISA_UART_CONFIG_HIGH_SPEED();
    uart_config.rx_buf_config.buffer_count = AT_UART_RX_BUF_COUNT;
    uart_config.rx_buf_config.buffer_size = AT_UART_RX_BUF_SIZE;

    ret = lisa_uart_configure(at_uart_ctx.uart_dev, &uart_config);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to configure UART");
        free(at_uart_ctx.rx_buffer);
        return -1;
    }

    /* Create synchronization objects */
    at_uart_ctx.tx_mutex = xSemaphoreCreateMutex();
    at_uart_ctx.cmd_mutex = xSemaphoreCreateMutex();
    at_uart_ctx.buffer_mutex = xSemaphoreCreateMutex();
    at_uart_ctx.event_group = xEventGroupCreate();

    if (!at_uart_ctx.tx_mutex || !at_uart_ctx.cmd_mutex ||
        !at_uart_ctx.buffer_mutex || !at_uart_ctx.event_group) {
        LISA_LOGE(TAG, "Failed to create synchronization objects");
        if (at_uart_ctx.tx_mutex) vSemaphoreDelete(at_uart_ctx.tx_mutex);
        if (at_uart_ctx.cmd_mutex) vSemaphoreDelete(at_uart_ctx.cmd_mutex);
        if (at_uart_ctx.buffer_mutex) vSemaphoreDelete(at_uart_ctx.buffer_mutex);
        if (at_uart_ctx.event_group) vEventGroupDelete(at_uart_ctx.event_group);
        free(at_uart_ctx.rx_buffer);
        return -1;
    }

    /* Enable reception */
    ret = lisa_uart_rx_enable(at_uart_ctx.uart_dev);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to enable UART RX");
        vSemaphoreDelete(at_uart_ctx.tx_mutex);
        vSemaphoreDelete(at_uart_ctx.cmd_mutex);
        vSemaphoreDelete(at_uart_ctx.buffer_mutex);
        vEventGroupDelete(at_uart_ctx.event_group);
        free(at_uart_ctx.rx_buffer);
        return -1;
    }

    /* Create data processing task */
    ret = xTaskCreate(at_uart_process_task,
        "at_uart_proc",
        AT_UART_TASK_STACK_SIZE,
        NULL,
        AT_UART_TASK_PRIORITY,
        &at_uart_ctx.process_task);
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create process task");
        lisa_uart_rx_disable(at_uart_ctx.uart_dev);
        vSemaphoreDelete(at_uart_ctx.tx_mutex);
        vSemaphoreDelete(at_uart_ctx.cmd_mutex);
        vSemaphoreDelete(at_uart_ctx.buffer_mutex);
        vEventGroupDelete(at_uart_ctx.event_group);
        free(at_uart_ctx.rx_buffer);
        return -1;
    }

    /* Initialize state */
    at_uart_ctx.wait_for_response = false;
    at_uart_ctx.cme_error_code = 0;
    at_uart_ctx.response[0] = '\0';
    at_uart_ctx.urc_callbacks_head = NULL;
    at_uart_ctx.initialized = 1;

    LISA_LOGI(TAG, "AT UART module initialized successfully");
    LISA_LOGI(TAG, "  - Device: %s", AT_UART_DEVICE);
    LISA_LOGI(TAG, "  - Buffer: %zu bytes (dynamic)", at_uart_ctx.rx_buffer_capacity);

    return 0;
}

/**
 * @brief Deinitialize AT UART
 */
int at_uart_deinit(void)
{
    if (!at_uart_ctx.initialized) {
        LISA_LOGW(TAG, "AT UART not initialized");
        return -1;
    }

    LISA_LOGI(TAG, "Deinitializing AT UART...");

    /* Delete processing task */
    if (at_uart_ctx.process_task) {
        vTaskDelete(at_uart_ctx.process_task);
        at_uart_ctx.process_task = NULL;
    }

    /* Disable reception */
    lisa_uart_rx_disable(at_uart_ctx.uart_dev);

    /* Delete synchronization objects */
    if (at_uart_ctx.tx_mutex) vSemaphoreDelete(at_uart_ctx.tx_mutex);
    if (at_uart_ctx.cmd_mutex) vSemaphoreDelete(at_uart_ctx.cmd_mutex);
    if (at_uart_ctx.buffer_mutex) vSemaphoreDelete(at_uart_ctx.buffer_mutex);
    if (at_uart_ctx.event_group) vEventGroupDelete(at_uart_ctx.event_group);

    /* Free URC callback list */
    at_urc_callback_node_t *node = at_uart_ctx.urc_callbacks_head;
    while (node) {
        at_urc_callback_node_t *next = node->next;
        free(node);
        node = next;
    }

    /* Free dynamic buffer */
    if (at_uart_ctx.rx_buffer) {
        free(at_uart_ctx.rx_buffer);
        at_uart_ctx.rx_buffer = NULL;
    }

    at_uart_ctx.initialized = 0;

    LISA_LOGI(TAG, "AT UART deinitialized");

    return 0;
}
