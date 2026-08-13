/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file console.h
 * @brief Console 子系统 - 统一的控制台输入输出抽象
 *
 * Console 是标准输出（stdout/printk）和 shell 的底层后端。
 * 具体实现由后端提供（UART、RTT、USB CDC 等）。
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Console 后端操作接口
 */
typedef struct {
    int (*write)(const char *data, int len);
    int (*read)(char *data, int len);
    void (*flush)(void);
} console_backend_t;

/**
 * @brief 初始化 console 子系统（创建互斥锁等）
 *
 * 需在 heap 可用后调用。调用前 console 仍可使用，但无线程安全保护。
 */
void console_init(void);

/**
 * @brief 注册 console 后端
 *
 * @param backend 后端操作接口（必须为静态生命周期）
 */
void console_backend_register(const console_backend_t *backend);

/**
 * @brief 写数据到 console
 *
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 实际写入的字节数，负数表示错误
 */
int console_write(const char *data, int len);

/**
 * @brief 等待 console 输出缓冲区中的数据全部发送完成
 */
void console_flush(void);

/**
 * @brief 从 console 读数据
 *
 * @param data 数据缓冲区
 * @param len 缓冲区大小
 * @return 实际读取的字节数，负数表示错误
 */
int console_read(char *data, int len);

#ifdef __cplusplus
}
#endif
