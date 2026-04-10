/*
 * SPDX-License-Identifier: Apache-2.0
 */
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

#define ALIGN_SIZE(len) ((len + IPC_ALIGN_SIZE - 1) / IPC_ALIGN_SIZE * IPC_ALIGN_SIZE)

/* CV组件回调事件定义 */
#define CV_CB_EVENT_OCR_RESULT       BIT(0) /* OCR识别结果返回 */
#define CV_CB_EVENT_STITCH_FRAME     BIT(1) /* 拼接帧数据返回 */
#define CV_CB_EVENT_STATUS           BIT(2) /* 状态变更通知 */
#define CV_CB_EVENT_STREAM_UPDATE    BIT(3) /* 数据流更新 */
#define CV_CB_EVENT_FRAME_DONE       BIT(4) /* AP处理完帧通知 */
#define CV_CB_EVENT_IMG_SAVE         BIT(5) /* AP回传CV处理后的图像 */

/* Image types for CV_CB_EVENT_IMG_SAVE */
#define CV_IMG_TYPE_RAW      0
#define CV_IMG_TYPE_STITCH   1
#define CV_IMG_TYPE_CUTLINE  2

/* Public event data for CV_CB_EVENT_IMG_SAVE */
typedef struct {
    uint32_t img_addr;
    uint16_t width;
    uint16_t height;
    uint8_t  img_type;
} cv_img_save_info_t;

typedef void (*cv_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

/**
 * @brief 初始化CV组件
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_FOUND : 设备未找到
 */
extern int acomp_cv_init(void);

/**
 * @brief 就绪CV组件
 *
 * @note 该函数会将调用方提供的prepare数据发送给AP端，初始化算法资源。
 *       在调用acomp_cv_start之前，必须调用该函数让组件进入就绪状态。
 *
 * @param prepare[in] 调用方提供的prepare数据结构指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cv_prepare(acomp_ipc_prepare_t *prepare);

/**
 * @brief 复位CV组件
 *
 * @note 该函数会释放内存块及算法资源。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cv_cleanup(void);

/**
 * @brief 启动CV组件
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cv_start(void);

/**
 * @brief 停止CV组件
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cv_stop(void);

/**
 * @brief 设置扫描模式
 *
 * @param scan_mode[in] 扫描模式
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 */
extern int acomp_cv_set_scan_mode(uint8_t scan_mode);

/**
 * @brief 设置左右模式
 *
 * @param lr_mode[in] 左右模式
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 */
extern int acomp_cv_set_lr_mode(uint8_t lr_mode);

/**
 * @brief 设置屏幕高度
 *
 * @param screen_height[in] 屏幕高度
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 */
extern int acomp_cv_set_screen_height(uint32_t screen_height);

/**
 * @brief 设置启动类型
 *
 * @param boot_type[in] 启动类型 (0=正常启动, 1=快扫启动)
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 */
extern int acomp_cv_set_boot_type(uint8_t boot_type);

/**
 * @brief 给CV组件增加事件回调函数
 *
 * @param events[in] 待增加的事件位，可以同步注册多个事件位
 * @param     cb[in] 回调的函数指针
 * @param   priv[in] 回调函数的私有数据指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cv_add_callback(uint32_t events, cv_event_cb_t cb, void *priv);

/**
 * @brief 移除CV组件的回调函数
 *
 * @param cb[in] 待移除的回调函数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cv_remove_callback(cv_event_cb_t cb);

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
extern int acomp_cv_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);

/**
 * @brief 禁用流通道
 *
 * @param chn[in] 通道索引
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 */
extern int acomp_cv_stream_ch_disable(int chn);

/**
 * @brief 分配TX流缓冲区用于向remote发送图像数据
 *
 * @param chn[in] 通道索引
 * @param len[out] 可用缓冲区长度指针
 * @param desc_idx[out] 描述符索引指针
 *
 * @return 缓冲区指针，如果失败返回NULL
 */
extern void* acomp_cv_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx);

/**
 * @brief 提交TX流缓冲区发送图像数据到remote
 *
 * @param chn[in] 通道索引
 * @param buffer[in] 缓冲区指针
 * @param len[in] 数据长度
 * @param desc_idx[in] 描述符索引
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 */
extern int acomp_cv_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx);

#ifdef __cplusplus
}
#endif
