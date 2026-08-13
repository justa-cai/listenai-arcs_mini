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

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#if CONFIG_CONSOLE_UART_BACKEND

static lisa_device_t *console_uart_dev = NULL;
static lisa_uart_config_t s_console_uart_config;

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

static void console_uart_flush(void)
{
    if (console_uart_dev != NULL && lisa_device_ready(console_uart_dev)) {
        lisa_uart_flush(console_uart_dev);
    }
}

static const console_backend_t console_uart_backend = {
    .write = console_uart_write,
    .read = NULL,
    .flush = console_uart_flush,
};

static int console_uart_init(void)
{
    console_uart_dev = lisa_device_get(CONFIG_CONSOLE_UART_NAME);

    if (!lisa_device_ready(console_uart_dev)) {
        return -1;
    }

    s_console_uart_config = (lisa_uart_config_t)LISA_UART_CONFIG_DEFAULT();
    s_console_uart_config.baudrate = CONFIG_CONSOLE_UART_BAUDRATE;
    lisa_uart_configure(console_uart_dev, &s_console_uart_config);

    console_backend_register(&console_uart_backend);

    return 0;
}

/* 在 device early init 之后注册（同一 level，更高 sub_priority 数值 = 更晚执行） */
SYS_INIT(console_uart_init, SYS_INIT_LEVEL_PRE_SYSTEM_INIT, SYS_INIT_SUB_PRIORITY_LAST);

#if CONFIG_LISA_PM
/* ===== console UART 跨 AUTO_LIGHT_SLEEP 的 destroy / reinit =====
 *
 * lisa_uart 采用“睡前 destroy / 唤醒后 reinit”模型，且 UART 控制器在睡眠时会掉电、
 * HAL 寄存器状态丢失，唤醒后必须重新 init + configure 才能继续输出。console 后端在
 * 系统启动阶段通过 SYS_INIT 一次性 configure UART，自身没有“应用循环”来在睡眠前后
 * 管理设备，这里通过一个仅用于 PM 的合成 lisa_device 承担这个职责：
 *   - prepare_suspend：先 flush 待发日志，再 lisa_device_destroy(uart) 释放全部
 *     软硬件资源（HAL 下电 + OS 资源），让 UART 干净地进入掉电；
 *   - resume_restore：lisa_device_reinit(uart) 把 HAL 拉回 _init 出口形态
 *     （Initialize + PowerControl + pinmux），再 lisa_uart_configure 恢复 console
 *     的波特率等配置，使唤醒后的日志输出立即可用。
 *
 * 顺序保证：合成设备使用 LISA_DEVICE_PRIORITY_LOWEST，按 lisa_pm 分发顺序，其
 * prepare_suspend 在其它设备之后执行（待它们的睡前日志都 flush 完再 destroy），
 * resume_restore 在其它设备之后执行。destroy / reinit 都作用在 console 自己通过
 * lisa_device_get() 持有的 UART 设备上。
 */

static int console_uart_pm_init(void)
{
    return 0;
}

static int32_t console_uart_pm_check_idle(void *ctx)
{
    (void)ctx;
    return 1;
}

static int32_t console_uart_pm_prepare_suspend(void *ctx)
{
    (void)ctx;
    if (console_uart_dev == NULL) {
        return 0;
    }
    /* 进入掉电前把待发日志冲到串口，避免最后一句话丢失 */
    if (lisa_device_ready(console_uart_dev)) {
        lisa_uart_flush(console_uart_dev);
    }
    /* UART 睡眠掉电：销毁设备，释放全部软硬件资源 */
    lisa_device_destroy(console_uart_dev);
    return 0;
}

static int32_t console_uart_pm_resume_restore(void *ctx)
{
    (void)ctx;
    if (console_uart_dev == NULL) {
        return 0;
    }
    /* 唤醒后重建 UART 设备（HAL 重新上电）并恢复 console 配置 */
    int ret = lisa_device_reinit(console_uart_dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }
    return lisa_uart_configure(console_uart_dev, &s_console_uart_config);
}

static const lisa_pm_system_ops_t console_uart_pm_ops = {
    .check_idle = console_uart_pm_check_idle,
    .prepare_suspend = console_uart_pm_prepare_suspend,
    .resume_restore = console_uart_pm_resume_restore,
};

LISA_DEVICE_REGISTER_PM(console_uart_pm, NULL, NULL, NULL, console_uart_pm_init,
                        LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_LOWEST,
                        &console_uart_pm_ops, NULL);
#endif /* CONFIG_LISA_PM */

#endif /* CONFIG_CONSOLE_UART_BACKEND */
