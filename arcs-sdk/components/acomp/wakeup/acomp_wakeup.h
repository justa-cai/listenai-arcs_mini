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

#define ACOMP_WAKEUP_RES_NUMBER  (2)
#define WAKEUP_INDEX_CAE_ESR_MLP (1)
#define WAKEUP_INDEX_AI_WRAP (2)

#define ALIGN_SIZE(len) ((len + IPC_ALIGN_SIZE - 1) / IPC_ALIGN_SIZE * IPC_ALIGN_SIZE)

/*语音唤醒组件回调事件定义*/
#define WAKEUP_CB_EVENT_ENGINE_RLT       BIT(0) /*语音唤醒引擎结果返回*/
#define WAKEUP_CB_EVENT_STREAM_UPDATE    BIT(1) /*音频数据流更新*/
#define WAKEUP_CB_EVENT_ENGINE_TIMEOUT   BIT(2) /*唤醒超时*/
#define WAKEUP_CB_EVENT_ENGINE_ANGLE     BIT(3) /*角度信息*/
#define WAKEUP_CB_EVENT_ENGINE_SWITCH_MODE BIT(4) /*模式切换*/

/* 语音唤醒组件输入音频数据帧信息 */
#define ACOMP_WAKEUP_AUDIO_INPUT_LEN_ONE_SAMPLE     (sizeof(acomp_wakeup_audio_in_t))   /* 单个输入音频采样点的字节数 */
#define ACOMP_WAKEUP_AUDIO_INPUT_SAMPLE_CNT         (256)                               /* 每帧输入音频的采样点数量 */
#define ACOMP_WAKEUP_AUDIO_INPUT_LEN_ONCE_FRAME     (ACOMP_WAKEUP_AUDIO_INPUT_LEN_ONE_SAMPLE * ACOMP_WAKEUP_AUDIO_INPUT_SAMPLE_CNT)  /* 单帧输入音频数据的总字节数 */
#define ACOMP_WAKEUP_ESR_NEED_FRAME_CNT             (10)                                /* 一次完整识别需要输入的音频帧数量 */

/* 语音唤醒组件输出音频数据信息 */
#define ACOMP_WAKEUP_AUDIO_OUTPUT_CHANNELS_PER_SAMPLE       (5)                         /* 输出音频每个采样点的通道数 */
#define ACOMP_WAKEUP_AUDIO_OUTPUT_LEN_ONE_SAMPLE            (sizeof(acomp_wakeup_audio_out_t))  /* 单个输出音频采样点的字节数 */
#define ACOMP_WAKEUP_AUDIO_OUTPUT_MAX_LEN_ONCE_FRAME        (ACOMP_WAKEUP_AUDIO_OUTPUT_LEN_ONE_SAMPLE * ACOMP_WAKEUP_AUDIO_INPUT_SAMPLE_CNT * ACOMP_WAKEUP_ESR_NEED_FRAME_CNT)  /* 单次输出音频数据的最大字节数 */

typedef void (*wakeup_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

/*算法模式定义*/
typedef enum {
    ACOMP_WAKEUP_ALGO_MODE_WAKEUP = 0,  /*唤醒模式*/
    ACOMP_WAKEUP_ALGO_MODE_ESR = 1,     /*ESR模式*/
} acomp_wakeup_algo_mode_e;

/*门限等级定义*/
typedef enum {
    ACOMP_WAKEUP_THRESHOLD_LEVEL_1 = 1,  /*门限等级1（最低，极易唤醒）*/
    ACOMP_WAKEUP_THRESHOLD_LEVEL_2 = 2,  /*门限等级2（易唤醒）*/
    ACOMP_WAKEUP_THRESHOLD_LEVEL_3 = 3,  /*门限等级3（默认档位）*/
    ACOMP_WAKEUP_THRESHOLD_LEVEL_4 = 4,  /*门限等级4（难唤醒）*/
    ACOMP_WAKEUP_THRESHOLD_LEVEL_5 = 5,  /*门限等级5（极难唤醒）*/
    ACOMP_WAKEUP_THRESHOLD_LEVEL_6 = 6,  /*门限等级6（最高，禁用唤醒）*/
} acomp_wakeup_threshold_level_e;

#ifdef CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC
typedef struct {
    short mic0;         /* mic0的音频 */
    short mic1;         /* mic1的音频 */
    short ref0;         /* 回采的音频 */
    short ref1;         /* 回采的音频，如只有一路回采，则可复制ref0的数据 */
} acomp_wakeup_audio_in_t;
#else
typedef struct {
    short mic0;         /* mic0的音频 */
    short ref0;         /* 回采的音频 */
} acomp_wakeup_audio_in_t;
#endif

typedef struct {
    short mic0;         /* mic0的音频 */
    short mic1;         /* mic1的音频，在单麦算法的情况下，mic1的音频实际为mic0的音频 */
    short ref0;         /* 回采的音频 */
    short out1;         /* 回声消除之后的音频 */
    short out2;         /* 算法输出的其他音频 */
} acomp_wakeup_audio_out_t;

/**
 * @brief 初始化语音唤醒组件（WAKEUP）
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_FOUND : 设备未找到
 *
 */
extern int acomp_wakeup_init(void);

// /**
//  * @brief 逆初始化语音唤醒组件（WAKEUP）
//  *
//  * @return GCL_OK : 成功
//  *
//  */
// extern int acomp_wakeup_deinit(void);

/**
 * @brief 就绪语音唤醒组件（WAKEUP）
 *
 * @note 该函数会初始化内存块及算法资源，在调用acomp_wakeup_start之前，必须调用该函数让组件进入就绪状态。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wakeup_prepare(void);

/**
 * @brief 复位语音唤醒组件（WAKEUP）
 *
 * @note 该函数会释放内存块及算法资源。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wakeup_cleanup(void);

/**
 * @brief 启动语音唤醒组件（WAKEUP）
 *
 * @note 该函数会启动语音唤醒组件，同步会启动音频流传输
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wakeup_start(void);

/**
 * @brief 停止语音唤醒组件（WAKEUP）
 *
 * @note 该函数会停止语音唤醒组件，同步会停止音频流传输
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wakeup_stop(void);

/**
 * @brief 设置组件参数
 *
 * @note 设置组件参数信息，该参数项将在组件进入就绪态时生效（调用wakeup_gcl_prepare（））。
 *
 * @param params[in] 存储参数的结构指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
// extern int acomp_wakeup_params_set(acomp_wakeup_params_t *params);

/**
 * @brief 设置是否使能调试模式
 *
 * @param enable[in] 0，不使能；1，使能
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
// extern int acomp_wakeup_set_debug_mode(uint8_t enable);

/**
 * @brief 设置算法模式
 *
 * @note 该函数必须在调用 acomp_wakeup_start() 之后设置才能生效
 *
 * @param mode[in] 算法模式,参考 acomp_wakeup_algo_mode_e
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wakeup_set_algo_mode(acomp_wakeup_algo_mode_e mode);

/**
 * @brief 设置唤醒门限等级
 *
 * @note 该函数必须在调用 acomp_wakeup_start() 之后设置才能生效
 *
 * @param level[in] 门限等级,参考 acomp_wakeup_threshold_level_e (1-6)
 *                  等级1最容易唤醒,等级6最难唤醒
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wakeup_set_threshold(acomp_wakeup_threshold_level_e level);

/**
 * @brief 给组件增加事件回调函数
 *
 * @param events[in] 待增加的事件位，可以同步注册多个事件位;
 * @param     cb[in] 回调的函数指针;
 * @param   priv[in] 回调函数的私有数据指针;
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wakeup_add_callback(uint32_t events, wakeup_event_cb_t cb, void *priv);

/**
 * @brief 移除组件的毁掉函数
 *
 * @param cb[in] 待移除的回调函数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_SUPPORTED : 无效操作
 *
 *
 */
extern int acomp_wakeup_remove_callback(wakeup_event_cb_t cb);

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
 *
 */
extern int acomp_wakeup_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);

/**
 * @brief 禁用流通道
 *
 * @param chn[in] 通道索引
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 *
 */
extern int acomp_wakeup_stream_ch_disable(int chn);

/**
 * @brief 获取RX流缓冲区
 *
 * @param chn[in] 通道索引
 * @param len[out] 数据长度指针
 * @param desc_idx[out] 描述符索引指针
 *
 * @return 缓冲区指针，如果失败返回NULL
 *
 */
extern void* acomp_wakeup_stream_rx_buffer_get(int chn, uint32_t* len, uint16_t* desc_idx);

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
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 *
 */
extern int acomp_wakeup_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void* buffer);

/**
 * @brief 分配TX流缓冲区用于向remote发送音频数据
 *
 * @param chn[in] 通道索引
 * @param len[out] 可用缓冲区长度指针
 * @param desc_idx[out] 描述符索引指针
 *
 * @return 缓冲区指针，如果失败返回NULL
 *
 */
extern void* acomp_wakeup_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx);

/**
 * @brief 提交TX流缓冲区发送音频数据到remote
 *
 * @param chn[in] 通道索引
 * @param buffer[in] 缓冲区指针
 * @param len[in] 数据长度
 * @param desc_idx[in] 描述符索引
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 *
 */
extern int acomp_wakeup_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx);

/**
 *
 * @note 索引0必须是WAKEUP_INDEX_CAE_ESR_MLP资源
 * @note 索引1必须是WAKEUP_INDEX_AI_WRAP资源
 */
int acomp_wakeup_prepare_with_config(acomp_ipc_prepare_t *prepare);

#ifdef __cplusplus
}
#endif
