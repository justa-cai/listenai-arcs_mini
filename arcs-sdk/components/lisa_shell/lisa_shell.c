/**
 * @file lisa_shell.c
 * @brief LISA Shell 组件实现
 * @copyright Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "lisa_shell"
#include <lisa_log.h>

#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_device.h"
#include "lisa_uart.h"
#include "lisa_mem.h"

#define TAG "lisa_shell"
#include "lisa_log.h"

/* Shell实例和UART设备句柄 */
static Shell *g_shell;
static lisa_device_t *shell_dev;

/* UART接收缓冲区 */
static uint8_t rx_buffer[CONFIG_LISA_SHELL_RX_BUF_SIZE];


static signed short shell_write(char *data, unsigned short size)
{
    for (size_t i = 0; i < size; i++) {
        lisa_uart_poll_out(shell_dev, data[i]);
    }
    return size;
}




static signed short shell_read(char *data, unsigned short len)
{
    int ret = lisa_uart_read_sync(shell_dev, (uint8_t *)data, len);
    if(ret == LISA_DEVICE_ERR_OVERFLOW){
        lisa_uart_rx_disable(shell_dev);
        if (lisa_uart_rx_enable(shell_dev) == 0) {
            LISA_LOGI(TAG, "Reception restarted");
        } else {
            LISA_LOGE(TAG, "Failed to restart reception");
        }     
    }
    return (ret > 0) ? ret : 0;
}

void log_shell_backend_output(const uint8_t *log, uint32_t len, void *data)
{
    if (g_shell) {
        shellWriteEndLine(g_shell, (uint8_t *)log, len);
    }
}

int lisa_shell_init(void)
{
    #if defined(CONFIG_SYSLOG_UART_DEVICE_UART0)
        shell_dev = lisa_device_get("uart0");
    #elif defined(CONFIG_SYSLOG_UART_DEVICE_UART1)
        shell_dev = lisa_device_get("uart1");
    #elif defined(CONFIG_SYSLOG_UART_DEVICE_UART2)
        shell_dev = lisa_device_get("uart2");
    #endif
    if (!lisa_device_ready(shell_dev)) {
        LISA_LOGE(LOG_TAG, "UART device not ready");
        return -1;
    }

    lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
    config.baudrate = CONFIG_SYSLOG_UART_BAUDRATE;
    config.rx_buf_config.buffer_count = 2;
    config.rx_buf_config.buffer_size = CONFIG_LISA_SHELL_RX_BUF_SIZE;
    if (lisa_uart_configure(shell_dev, &config) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to configure UART");
        return -1;
    }

    if (lisa_uart_rx_enable(shell_dev) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to enable UART RX");
        return -1;
    }

    Shell *sh = lisa_mem_alloc(sizeof(Shell));
    if (sh == NULL) {
        LISA_LOGE(LOG_TAG, "Failed to allocate memory for Shell");
        return -1;
    }
    memset(sh, 0, sizeof(Shell));

    uint8_t *shell_buf = lisa_mem_alloc(CONFIG_LISA_SHELL_BUFFER_SIZE);
    if (shell_buf == NULL) {
        LISA_LOGE(LOG_TAG, "Failed to allocate shell buffer");
        lisa_mem_free(sh);
        return -1;
    }

    sh->write = shell_write;
    sh->read = shell_read;
    g_shell = sh;

    shellInit(sh, (char *)shell_buf, CONFIG_LISA_SHELL_BUFFER_SIZE);

    lisa_log_backend_pause("sys.log");
    lisa_log_backend_add("user.shell", log_shell_backend_output, NULL);

    xTaskCreate(shellTask, "shell",
                CONFIG_LISA_SHELL_TASK_STACK_SIZE,
                sh,
                CONFIG_LISA_SHELL_TASK_PRIORITY,
                NULL);

    return 0;
}