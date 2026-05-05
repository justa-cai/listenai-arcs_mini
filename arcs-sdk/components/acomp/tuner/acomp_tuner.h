#pragma once

#include <stdint.h>
#include "../utils/acomp_err.h"
#include "acomp_stream_ipc.h"
#include "ipc/acomp_ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define TUNER_ALIGN_SIZE(len) ((len + IPC_ALIGN_SIZE - 1) / IPC_ALIGN_SIZE * IPC_ALIGN_SIZE)

/* 调音组件回调事件定义 */
#define TUNER_CB_EVENT_STATUS        BIT(0) /* 状态变更通知，event_data 指向 uint32_t 状态值 */
#define TUNER_CB_EVENT_STREAM_UPDATE BIT(1) /* 数据流更新通知，event_data 指向 acomp_stream_channel_t */

typedef void (*tuner_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

/**
 * @brief 初始化调音组件
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 组件已初始化
 * @retval ACOMP_ERR_NOT_FOUND : 设备未找到
 * @retval ACOMP_ERR_CREATE_STREAM_FAILED : 创建流失败
 */
extern int acomp_tuner_init(void);

/**
 * @brief 启动调音组件
 *
 * @note 启动后才能继续使能调音处理或进行流数据交互。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_start(void);

/**
 * @brief 停止调音组件
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_stop(void);

/**
 * @brief 清理调音组件资源
 *
 * @note 该函数会注销IPC回调并释放流对象及回调链表资源。
 *       若组件尚未初始化，调用该函数也会直接返回成功。
 *
 * @return ACOMP_ERR_OK : 成功
 */
extern int acomp_tuner_cleanup(void);

/**
 * @brief 使能或关闭调音处理
 *
 * @param en[in] 使能开关，0表示关闭，非0表示开启
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_enable(uint8_t en);

/**
 * @brief 设置调音组件采样率
 *
 * @param sr[in] 采样率，单位Hz
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_set_samplerate(float sr);

/**
 * @brief 设置调音参数
 *
 * @param param[in] 参数数据缓冲区指针
 * @param len[in] 参数数据长度，单位字节
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_set_param(void *param, uint32_t len);

/**
 * @brief 设置限幅器参数
 *
 * @param param[in] 限幅器参数数据缓冲区指针
 * @param len[in] 限幅器参数数据长度，单位字节
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_set_limiter(void *param, uint32_t len);

/**
 * @brief 设置调音输出音量
 *
 * @param vol[in] 音量系数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_set_volume(float vol);

/**
 * @brief 给调音组件增加事件回调函数
 *
 * @param events[in] 待订阅的事件位，可以同时注册多个事件
 * @param cb[in] 回调函数指针
 * @param priv[in] 回调函数的私有数据指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_add_callback(uint32_t events, tuner_event_cb_t cb, void *priv);

/**
 * @brief 移除调音组件的回调函数
 *
 * @param cb[in] 待移除的回调函数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_remove_callback(tuner_event_cb_t cb);

/**
 * @brief 使能流通道
 *
 * @param chn[in] 通道索引
 * @param desc[in] 通道描述符指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_CREATE_STREAM_FAILED : 创建流失败
 */
extern int acomp_tuner_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);

/**
 * @brief 禁用流通道
 *
 * @param chn[in] 通道索引
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 */
extern int acomp_tuner_stream_ch_disable(int chn);

/**
 * @brief 分配TX流缓冲区用于向remote发送输入数据
 *
 * @param chn[in] 通道索引
 * @param len[out] 可用缓冲区长度指针
 * @param desc_idx[out] 描述符索引指针
 *
 * @return 缓冲区指针，如果失败返回NULL
 */
extern void *acomp_tuner_stream_tx_buffer_alloc(int chn, uint32_t *len, uint16_t *desc_idx);

/**
 * @brief 提交TX流缓冲区发送输入数据到remote
 *
 * @param chn[in] 通道索引
 * @param buffer[in] 缓冲区指针
 * @param len[in] 数据长度
 * @param desc_idx[in] 描述符索引
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_stream_tx_buffer_submit(int chn, void *buffer, uint32_t len, uint16_t desc_idx);

/**
 * @brief 主动触发流通道kick操作
 *
 * @param chn[in] 通道索引
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_stream_kick(int chn);

/**
 * @brief 获取RX流缓冲区
 *
 * @param chn[in] 通道索引
 * @param len[out] 数据长度指针
 * @param desc_idx[out] 描述符索引指针
 *
 * @return 缓冲区指针，如果失败返回NULL
 */
extern void *acomp_tuner_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx);

/**
 * @brief 释放RX流缓冲区
 *
 * @param chn[in] 通道索引
 * @param desc_idx[in] 描述符索引
 * @param len[in] 数据长度
 * @param buffer[in] 缓冲区指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_tuner_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer);

#ifdef __cplusplus
}
#endif
