/**
 * @file lisa_shell.h
 * @brief LISA Shell 组件对外接口
 * @copyright Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LISA_SHELL_H__
#define __LISA_SHELL_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化LISA Shell
 *
 * 该函数初始化shell组件，配置UART通信，并创建shell任务。
 * shell任务的堆栈大小和优先级可以通过Kconfig配置。
 *
 * @return int 初始化结果
 *         @retval  0 成功
 *         @retval -1 失败
 */
int lisa_shell_init(void);


#ifdef __cplusplus
}
#endif

#endif /* __LISA_SHELL_H__ */
