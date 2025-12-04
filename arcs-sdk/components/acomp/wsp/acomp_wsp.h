/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include "comm/stream/acomp_stream_ipc.h"
#include "utils/acomp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(x) (1 << (x))
#endif
/*WSP组件音频帧大小 BYTES*/
#define WSP_FRAME_BYTES (320)

typedef struct {

    int32_t alg_wsp_en;          /* 0-CSP OFF; 1-CSP ON */
    int32_t alg_vad_en;          /* 0-VAD OFF; 1-VAD ON */
    int32_t alg_vad_threshold;   /* Vad门限 */
    int32_t alg_vad_gap;         /* Vad的末端点GAP */
    int32_t adc_a_gain;          /* ADC 模块的模拟增益*/
    int32_t adc_d_gain;          /* ADC 模块的数字增益*/
    int32_t stream_cut_front_ms; /* 裁剪启动后音频数据的长度，单位为ms*/
    int32_t stream_cut_last_ms;  /* 裁剪停止前音频数据的长度，单位为ms*/
    int32_t stream_direction; /* 音频流方向，0：Capture使用MIC采集音频，1：playback外部输入音频*/
    int32_t stream_channel_index; /* 录音通道号，0:左声道有效，1：右声道有效*/
} __attribute__((packed)) acomp_wsp_params_t;


typedef enum{
    COMP_WSP_RESULT_TYPE_UNKNOWN = 0,
    COMP_WSP_RESULT_TYPE_STREAM,
    COMP_WSP_RESULT_TYPE_FINISH,
}comp_wsp_result_type_e;

typedef struct{
    comp_wsp_result_type_e type;
    uint32_t len;
    uint8_t data[];
}__attribute__((packed)) comp_wsp_result_t;


typedef struct {
    int32_t mlp_res_type; /* 引擎声学模型类型*/
    char version[64];     /* 引擎版本信息*/
} __attribute__((packed)) wsp_gcl_info_t;

/*发音拼读组件回调事件定义*/
#define WSP_CB_EVENT_STREAM_UPDATE    BIT(0) /*音频数据流更新*/
#define WSP_CB_EVENT_ENGINE_RLT       BIT(4) /*发音拼读引擎结果返回*/
#define WSP_CB_EVENT_ENGINE_VAD_BEGIN BIT(5) /*VAD检测起始*/
#define WSP_CB_EVENT_ENGINE_VAD_END   BIT(6) /*VAD检测结束*/
#define WSP_CB_EVENT_ENGINE_WR_ERR    BIT(7) /*算法引擎数据写入出错*/

typedef void (*wsp_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

/**
 * @brief 初始化发音拼读组件（WSP）
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_FOUND : 设备未找到
 *
 */
extern int acomp_wsp_init(void);

// /**
//  * @brief 逆初始化发音拼读组件（WSP）
//  *
//  * @return GCL_OK : 成功
//  * @retval -GCL_ERR_COMM_FAIL(209) : 通讯失败
//  *
//  */
// extern int acomp_wsp_deinit(void);

/**
 * @brief 就绪发音拼读组件（WSP）
 *
 * @note 该函数会初始化内存块及算法资源，在调用wsp_gcl_start之前，必须调用该函数让组件进入就绪状态。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wsp_prepare(void);

/**
 * @brief 复位发音拼读组件（WSP）
 *
 * @note 该函数会释放内存块及算法资源。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wsp_cleanup(void);

/**
 * @brief 启动发音拼读组件（WSP）
 *
 * @note 该函数会启动发音拼读组件，同步会启动音频流传输
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wsp_start(void);

/**
 * @brief 停止发音拼读组件（WSP）
 *
 * @note 该函数会停止发音拼读组件，同步会停止音频流传输
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wsp_stop(void);

/**
 * @brief 设置组件参数
 *
 * @note 设置组件参数信息，该参数项将在组件进入就绪态时生效（调用wsp_gcl_prepare（））。
 *
 * @param params[in] 存储参数的结构指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_wsp_params_set(acomp_wsp_params_t *params);

/**
 * @brief 写入音频数据
 *
 * @note 只有当参数（stream_direction）设置为1（playback）时，可以使用该接口向engine输入音频数据
 *
 * @param data[in] 音频数据指针
 * @param len[in] 数据长度，固定为WSP_FRAME_BYTES（320 bytes）
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
// extern int acomp_wsp_stream_write(uint8_t* data,uint32_t len);

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
extern int acomp_wsp_add_callback(uint32_t events, wsp_event_cb_t cb, void *priv);

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
extern int acomp_wsp_remove_callback(wsp_event_cb_t cb);

#ifdef __cplusplus
}
#endif
