/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file console_uart.c
 * @brief Console UART 后端
 *
 * 通过 lisa_uart 设备实现 console 输出。
 * 在 PRE_SYSTEM_INIT 阶段自动注册（UART 设备已由 device early init 完成初始化）。
 */

#include "console.h"
#include "sys_init.h"
#include "lisa_device.h"
#include "lisa_uart.h"

#if CONFIG_CONSOLE_UART_BACKEND

static lisa_device_t *console_uart_dev = NULL;

static int console_uart_write(const char *data, int len)
{
    if (console_uart_dev == NULL) {
        return -1;
    }

    if (!lisa_device_ready(console_uart_dev)) {
        return -1;
    }

    for (int i = 0; i < len; i++) {
        lisa_uart_poll_out(console_uart_dev, data[i]);
    }
    return len;
}

static const console_backend_t console_uart_backend = {
    .write = console_uart_write,
    .read = NULL,
};

static int console_uart_init(void)
{
    console_uart_dev = lisa_device_get(CONFIG_CONSOLE_UART_NAME);

    if (!lisa_device_ready(console_uart_dev)) {
        return -1;
    }

    lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
    config.baudrate = CONFIG_CONSOLE_UART_BAUDRATE;
    lisa_uart_configure(console_uart_dev, &config);

    console_backend_register(&console_uart_backend);

    return 0;
}

/* 在 device early init 之后注册（同一 level，更高 sub_priority 数值 = 更晚执行） */
SYS_INIT(console_uart_init, SYS_INIT_LEVEL_PRE_SYSTEM_INIT, SYS_INIT_SUB_PRIORITY_LAST);

#endif /* CONFIG_CONSOLE_UART_BACKEND */
