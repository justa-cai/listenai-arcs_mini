/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include "utils/acomp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 日志输出回调函数类型
 *
 * 应用层可以通过 acomp_logger_set_output_callback() 注册此回调函数，
 * 自定义 AP 日志的输出方式。
 *
 * @param log 日志数据缓冲区指针
 * @param len 日志数据长度（字节数）
 *
 * @return 0 表示成功，非 0 表示失败
 *
 * @note 此回调函数在日志接收线程中调用，应避免阻塞操作
 * @note 日志数据已经是格式化好的字符串，可以直接输出
 */
typedef int (*acomp_logger_output_cb_t)(const uint8_t *log, uint32_t len);

/**
 * @brief 初始化日志传输组件（Logger）
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_FOUND : 设备未找到
 */
extern int acomp_logger_init(void);

/**
 * @brief 设置日志输出回调函数
 *
 * 允许应用层自定义 AP 日志的输出方式。如果不设置回调函数，
 * 日志将使用默认方式通过 LISA_LOG_RAW 输出。
 *
 * @param cb 日志输出回调函数指针，传入 NULL 则恢复使用默认输出方式
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 组件未初始化
 *
 * @note 必须在 acomp_logger_init() 之后调用
 * @note 可以在运行时动态更改回调函数
 * @note 回调函数在日志接收线程中执行，应避免长时间阻塞操作
 *
 * @example
 * ```c
 * // 自定义输出函数
 * int my_log_output(const uint8_t *log, uint32_t len)
 * {
 *     printf("[AP] %.*s", len, log);
 *     return 0;
 * }
 *
 * // 注册回调
 * acomp_logger_set_output_callback(my_log_output);
 *
 * // 恢复默认输出
 * acomp_logger_set_output_callback(NULL);
 * ```
 */
extern int acomp_logger_set_output_callback(acomp_logger_output_cb_t cb);

/**
 * @brief 启动日志传输组件（Logger）
 *
 * @note 该函数会自动完成以下操作：
 *       1. 配置 R2M 流通道（缓冲区大小和数量由 Kconfig 配置决定）
 *          - CONFIG_ACOMP_LOGGER_STREAM_BUFFER_SIZE: 每个缓冲区大小
 *          - CONFIG_ACOMP_LOGGER_STREAM_BUFFER_COUNT: 缓冲区数量（必须是2的指数倍）
 *       2. 启动 Remote 端的日志捕获
 *       3. 创建日志接收线程，根据 Kconfig 配置的触发策略工作：
 *          - 自动触发模式：等待流更新信号量，收到指定数量的 buffer 后自动处理
 *          - 手动触发模式：按配置的轮询间隔定期检查并处理 buffer
 *       无需手动调用流管理或缓冲区操作 API
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_logger_start(void);

/**
 * @brief 停止日志传输组件（Logger）
 *
 * @note 该函数会自动完成以下操作：
 *       1. 停止日志接收线程
 *       2. 停止 Remote 端的日志捕获
 *       3. 禁用 R2M 流通道
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_logger_stop(void);

#ifdef __cplusplus
}
#endif
