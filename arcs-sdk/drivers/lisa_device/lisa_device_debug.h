/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_device_debug.h
 * @brief LISA 设备框架 - 调试维测接口
 *
 * 提供设备信息打印、注册表验证等调试功能
 * 这些功能依赖 lisa_device.h 的核心接口实现
 */

#pragma once

#include "lisa_device.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_LISA_DEVICE_DEBUG

/* ===== 信息打印 ===== */

/**
 * @brief 打印设备注册表信息
 *
 * 显示所有通过 LISA_DEVICE_REGISTER 注册的设备条目
 */
void lisa_device_print_registry(void);

/**
 * @brief 打印单个设备信息
 *
 * @param dev 设备指针
 */
void lisa_device_print_info(const lisa_device_t *dev);

/**
 * @brief 打印所有已注册设备信息
 *
 * 使用 lisa_device_foreach 遍历并打印所有设备
 */
void lisa_device_print_all(void);

/* ===== 验证功能 ===== */

/**
 * @brief 验证设备注册表完整性
 *
 * @return LISA_DEVICE_OK 验证通过, 负数错误码
 */
int lisa_device_verify_registry(void);

/* ===== 辅助工具 ===== */

/**
 * @brief 获取设备状态名称字符串
 *
 * @param state 设备状态
 * @return 状态名称字符串
 */
const char *lisa_device_get_state_name(lisa_device_state_t state);

#endif /* CONFIG_LISA_DEVICE_DEBUG */

#ifdef __cplusplus
}
#endif
