/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LISTENAI_PA_MANAGER_H__
#define __LISTENAI_PA_MANAGER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief PA 控制回调函数类型
 * @param onoff  1: 打开PA, 0: 关闭PA
 * @return 0: 成功, 其他值: 失败
 */
typedef int (*pa_ctrl_callback_t)(int onoff);

/**
 * @brief PA 管理器配置结构体
 */
typedef struct {
    pa_ctrl_callback_t ctrl_callback;  /* PA控制回调函数 */
} pa_manager_config_t;

/**
 * @brief 初始化 PA 管理器
 * @param config  配置参数
 * @return 0: 成功, 其他值: 失败
 *
 * @note 必须在使用其他PA管理器功能前调用此函数
 * @note config中的ctrl_callback不能为NULL
 */
int pa_manager_init(const pa_manager_config_t *config);

/**
 * @brief 控制 PA 开关（带引用计数）
 * @param onoff     1: 打开PA, 0: 关闭PA
 * @param delay_ms  延时时间(毫秒)
 *                  0: 立即执行开/关动作
 *                  其他值: 延时指定毫秒数后执行开/关动作
 * @return 0: 成功, 其他值: 失败
 *
 * @note 引用计数机制：
 * - onoff=1 时增加引用计数，只在首次开启时真正打开PA
 * - onoff=0 时减少引用计数，只在计数为0时真正关闭PA
 * - 支持多个播放器实例同时使用PA，避免互相干扰
 * - 调用者需要确保每次ON调用都有对应的OFF调用
 *
 * @example
 * // 立即打开PA（引用计数+1）
 * pa_manager_control(1, 0);
 *
 * // 延时500毫秒后关闭PA（引用计数-1，只在计数为0时真正关闭）
 * pa_manager_control(0, 500);
 */
int pa_manager_control(int onoff, uint32_t delay_ms);

/**
 * @brief 获取当前 PA 状态
 * @return 1: PA已打开, 0: PA已关闭
 */
int pa_manager_get_state(void);

/**
 * @brief 获取当前 PA 引用计数
 * @return 引用计数值（有多少个实例正在使用PA）
 */
int pa_manager_get_ref_count(void);

/**
 * @brief 反初始化 PA 管理器
 * @note 释放PA管理器占用的资源
 */
void pa_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif