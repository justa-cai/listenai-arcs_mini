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

#define ACOMP_CAE_RES_NUMBER        (2)
#define ACOMP_CAE_ANGLE_MAX_CNT     (10)

#define ALIGN_SIZE(len) ((len + IPC_ALIGN_SIZE - 1) / IPC_ALIGN_SIZE * IPC_ALIGN_SIZE)

typedef enum {
    ACOMP_CAE_RES_STORAGE_FLASH = ACOMP_RES_STORAGE_FLASH,
    ACOMP_CAE_RES_STORAGE_SD = ACOMP_RES_STORAGE_SD,
    ACOMP_CAE_RES_STORAGE_PSRAM = ACOMP_RES_STORAGE_PSRAM,
} acomp_cae_res_storage_t;

typedef struct {
    acomp_cae_res_storage_t storage;
    uintptr_t addr;
    uint32_t size;
} acomp_cae_res_item_t;

typedef struct {
    acomp_cae_res_item_t aes;
    acomp_cae_res_item_t json;
} acomp_cae_resource_config_t;

/* CAE 组件回调事件定义 - 简化版，无唤醒事件 */
#define CAE_CB_EVENT_STREAM_UPDATE    BIT(0)  /* 音频数据流更新 */
#define CAE_CB_EVENT_ENGINE_ANGLE     BIT(1)  /* 声源角度信息 */
#define CAE_CB_EVENT_ENGINE_ERROR     BIT(2)  /* 错误事件 */

/* CAE 组件输入音频数据帧信息 */
#define ACOMP_CAE_AUDIO_INPUT_LEN_ONE_SAMPLE     (sizeof(acomp_cae_audio_in_t))
#define ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT         (256)
#define ACOMP_CAE_AUDIO_INPUT_LEN_ONCE_FRAME     (ACOMP_CAE_AUDIO_INPUT_LEN_ONE_SAMPLE * ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT)

/* CAE 组件输出音频数据信息 */
#ifdef CONFIG_ACOMP_CAE_ALGORITHM_TYPE_DUAL_MIC
#define ACOMP_CAE_AUDIO_OUTPUT_CHANNELS_PER_SAMPLE       (5)
#else
#define ACOMP_CAE_AUDIO_OUTPUT_CHANNELS_PER_SAMPLE       (1)
#endif
#define ACOMP_CAE_AUDIO_OUTPUT_LEN_ONE_SAMPLE            (sizeof(acomp_cae_audio_out_t))
#define ACOMP_CAE_AUDIO_OUTPUT_MAX_LEN_ONCE_FRAME        (ACOMP_CAE_AUDIO_OUTPUT_LEN_ONE_SAMPLE * ACOMP_CAE_AUDIO_INPUT_SAMPLE_CNT * 10)

/* 波束选择定义 */
typedef enum {
    ACOMP_CAE_BEAM_0_60    = 0,   /* 波束 0: 0-60° */
    ACOMP_CAE_BEAM_60_120  = 1,   /* 波束 1: 60-120° */
    ACOMP_CAE_BEAM_120_180 = 2,   /* 波束 2: 120-180° */
} acomp_cae_beam_e;

typedef void (*cae_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

#ifdef CONFIG_ACOMP_CAE_ALGORITHM_TYPE_DUAL_MIC
typedef struct {
    short mic0;
    short mic1;
    short ref0;
    short ref1;
} acomp_cae_audio_in_t;
#else
typedef struct {
    short mic0;
    short ref0;
} acomp_cae_audio_in_t;
#endif

#ifdef CONFIG_ACOMP_CAE_ALGORITHM_TYPE_DUAL_MIC
typedef struct {
    short mic0;
    short mic1;
    short ref0;
    short out1;
    short out2;
} acomp_cae_audio_out_t;
#else
typedef struct {
    short out;
} acomp_cae_audio_out_t;
#endif

/**
 * @brief 初始化 CAE 组件
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_FOUND : 设备未找到
 */
extern int acomp_cae_init(void);

/**
 * @brief 逆初始化 CAE 组件
 */
extern int acomp_cae_deinit(void);

/**
 * @brief 就绪 CAE 组件
 *
 * @note 该函数会初始化内存块及算法资源，在调用acomp_cae_start之前必须调用
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cae_prepare(void);

/**
 * @brief 使用指定资源配置就绪 CAE 组件
 *
 * @param config[in] 自定义资源配置，支持 Flash / eMMC / PSRAM
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cae_prepare_with_resources(const acomp_cae_resource_config_t *config);

/**
 * @brief 复位 CAE 组件
 *
 * @note 该函数会释放内存块及算法资源
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cae_cleanup(void);

/**
 * @brief 启动 CAE 组件
 *
 * @note 该函数会启动 CAE 组件，同步会启动音频流传输
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cae_start(void);

/**
 * @brief 停止 CAE 组件
 *
 * @note 该函数会停止 CAE 组件，同步会停止音频流传输
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cae_stop(void);

/**
 * @brief 设置波束选择
 *
 * @note 该函数必须在调用 acomp_cae_start() 之后设置才能生效
 *
 * @param beam[in] 波束索引,参考 acomp_cae_beam_e
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cae_set_beam(acomp_cae_beam_e beam);

/**
 * @brief 获取声源角度
 *
 * @param angle_data[out] 角度数据数组指针
 * @param angle_cnt[in/out] 输入数组容量，输出实际写入数量
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cae_get_angle(int *angle_data, uint32_t *angle_cnt);

/**
 * @brief 给组件增加事件回调函数
 *
 * @param events[in] 待增加的事件位，可以同步注册多个事件位
 * @param cb[in] 回调的函数指针
 * @param priv[in] 回调函数的私有数据指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 */
extern int acomp_cae_add_callback(uint32_t events, cae_event_cb_t cb, void *priv);

/**
 * @brief 移除组件的回调函数
 *
 * @param cb[in] 待移除的回调函数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_SUPPORTED : 无效操作
 */
extern int acomp_cae_remove_callback(cae_event_cb_t cb);

/**
 * @brief 使能流通道
 */
extern int acomp_cae_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);

/**
 * @brief 禁用流通道
 */
extern int acomp_cae_stream_ch_disable(int chn);

/**
 * @brief 获取RX流缓冲区
 */
extern void* acomp_cae_stream_rx_buffer_get(int chn, uint32_t* len, uint16_t* desc_idx);

/**
 * @brief 释放RX流缓冲区
 */
extern int acomp_cae_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void* buffer);

/**
 * @brief 分配TX流缓冲区
 */
extern void* acomp_cae_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx);

/**
 * @brief 提交TX流缓冲区
 */
extern int acomp_cae_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx);

#ifdef __cplusplus
}
#endif
