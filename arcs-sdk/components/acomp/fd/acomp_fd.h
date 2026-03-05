/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include "../utils/acomp_err.h"
#include "acomp_stream_ipc.h"
#include "acomp_fd_params.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

/* 人脸识别组件参数设置 */

/**
 * @brief 人脸识别参数键值对
 */
typedef struct{
	uint32_t key;   /* 参数键（参见 acomp_fd_params.h的 acomp_fd_params_e枚举） */
	float value;    /* 参数值 */
}acomp_fd_param_t;

/**
 * @brief 人脸识别的输入图片帧结构体
 */
typedef struct{
    acomp_fd_pixel_format format;    /* 像素格式（如 PIX_FMT_BGR888, PIX_FMT_YUV422 等） */
    uint32_t index;         /* 帧索引号 */
	uint16_t width;         /* 图像宽度（像素） */
	uint16_t height;        /* 图像高度（像素） */
	uint32_t length;        /* 图像数据长度（字节） */
    uint32_t resv[4];       /* reserved */
	uint8_t data[0];          /* 图像数据指针 */
} __attribute__((packed)) acomp_fd_input_frame_t;

/**
 * @brief 活体检测模式配置
 */
typedef struct{
    int     enable;         /* 是否启用活体检测（0:禁用 1:启用） */
	float   score_threshold[2];      /* 活体检测阈值 [0]:假人阈值 [1]:真人阈值， 在真人得分 > 真人阈值的情况下才会进行人脸特征提取和人脸识别 */
}acomp_fd_live_detect_mode_t;

/* 人脸识别组件结果 */

/**
 * @brief 人脸检测矩形框
 */
typedef struct{
	int x;      /* 矩形框左上角x坐标 */
	int y;      /* 矩形框左上角y坐标 */
	int w;      /* 矩形框宽度 */
	int h;      /* 矩形框高度 */
}acomp_fd_rect_t;

/**
 * @brief 人脸头部姿态角度
 */
typedef struct{
	float yaw;      /* 偏航角（左右转头），范围 -90° ~ +90° */
	float pitch;    /* 俯仰角（上下点头），范围 -90° ~ +90° */
	float roll;     /* 翻滚角（左右歪头），范围 -180° ~ +180° */
}acomp_fd_head_pose_t;

/**
 * @brief 人脸关键点（特征点）
 */
typedef struct{
	int x;              /* 关键点x坐标 */
	int y;              /* 关键点y坐标 */
	float score;        /* 关键点置信度得分 */
	float visable;      /* 关键点可见性（0-1，1表示完全可见） */
}acomp_fd_align_point_t;

/**
 * @brief 人脸活体检测结果
 */
typedef struct {
	float   scores[2];  /* 活体检测得分 [0]:假人得分 [1]:真人得分 */
	int	    status;     /* 活体检测状态（0:假人 1:真人） */
}acomp_fd_live_detect_result_t;

typedef struct {
    float   features[ACOMP_FD_MAX_FEATURE_CNT];     /* 人脸特征点 */
    int     feature_cnt;                            /* 人脸特征点个数 */
}acomp_fd_feature_result_t;

/**
 * @brief 人脸检测完整结果
 */
typedef struct {
	acomp_fd_rect_t 		        face_rect;                              /* 人脸检测矩形框 */
	float 			                face_score;                             /* 人脸检测得分 */
	acomp_fd_align_point_t          align_points[ACOMP_FD_MAX_ALIGN_CNT];    /* 人脸标定点 */
	int                             n_align_point;                          /* 人脸标定点个数 */
	acomp_fd_head_pose_t            pose;                                   /* 人脸头部姿势 */
	acomp_fd_live_detect_result_t   live_result;                            /* 活体检测结果 */
	int			                    face_id;                                /* 人脸id */
    float                           features[ACOMP_FD_MAX_FEATURE_CNT];     /* 人脸特征点 */
    int                             feature_cnt;                            /* 人脸特征点个数 */
    float                           compare_scores[ACOMP_FD_MAX_RESULT_CNT];/* 输入图片和已注册人脸特征(最大10个人脸注册)的比较得分 */
    int                             compare_cnt;                            /* 已经注册人脸特征个数 */    
}acomp_fd_result_t;

typedef struct {
    uint32_t results_cnt;
    uint32_t max_area_results_index;
    acomp_fd_result_t results[0];
}acomp_fd_result_info_t;

/*人脸识别组件回调事件定义*/
#define FD_CB_EVENT_ENGINE_RLT       BIT(0) /*人脸识别引擎结果返回*/
#define FD_CB_EVENT_ENGINE_WR_ERR    BIT(1) /*算法引擎数据写入出错*/

typedef void (*fd_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

/**
 * @brief 初始化人脸识别组件（FD）
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_FOUND : 设备未找到
 *
 */
extern int acomp_fd_init(void);

// /**
//  * @brief 逆初始化人脸识别组件（FD）
//  *
//  * @return GCL_OK : 成功
//  * @retval -GCL_ERR_COMM_FAIL(209) : 通讯失败
//  *
//  */
// extern int acomp_fd_deinit(void);

/**
 * @brief 就绪人脸识别组件（FD）
 *
 * @note 该函数会初始化内存块及算法资源，在调用fd_gcl_start之前，必须调用该函数让组件进入就绪状态。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_prepare(void);

/**
 * @brief 复位人脸识别组件（FD）
 *
 * @note 该函数会释放内存块及算法资源。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_cleanup(void);

/**
 * @brief 启动人脸识别组件（FD）
 *
 * @note 该函数会启动人脸识别组件，之后用户可以向组件输入视频流
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_start(void);

/**
 * @brief 停止人脸识别组件（FD）
 *
 * @note 该函数会停止人脸识别组件，之后不会处理视频流数据
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_stop(void);

/**
 * @brief 设置组件参数
 *
 * @note 设置组件参数信息，该参数项将在组件进入就绪态时生效（调用fd_gcl_prepare（））。
 *
 * @param params[in] 存储acomp_fd_param_t参数的结构指针
 * @param params_cnt[in] acomp_fd_param_t的个数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_params_set(const acomp_fd_param_t *params, uint32_t params_cnt);

/**
 * @brief 获取组件参数
 *
 * @note 获取组件参数信息，该参数项将在组件进入就绪态时生效（调用fd_gcl_prepare（））。
 *
 * @param params[out] 存储acomp_fd_param_t参数的结构指针
 * @param params_cnt[out] acomp_fd_param_t的个数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_params_get(const acomp_fd_param_t *params, uint32_t *params_cnt);

/**
 * @brief 设置人脸标定的头部姿势的阈值
 *
 * @note 如果超过这些阈值，就不会进行活体检测和人脸特征获取
 *
 * @param params[in] 存储acomp_fd_head_pose_t参数的结构指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_align_threshold_set(const acomp_fd_head_pose_t *threshold);

/**
 * @brief 设置人脸活体检测的模式
 *
 * @note 这个函数用来设置是否使能活体检测，以及设置活体检测的得分阈值
 *
 * @param params[in] 存储acomp_fd_live_detect_mode_t参数的结构指针
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_live_detect_mode_set(const acomp_fd_live_detect_mode_t *mode);

/**
 * @brief 从外部导入人脸特征到注册库
 *
 * @note 导入外部特征到注册库，用于后续人脸识别
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_fd_features_load(const acomp_fd_feature_result_t *features, uint32_t count);

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
extern int acomp_fd_add_callback(uint32_t events, fd_event_cb_t cb, void *priv);

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
extern int acomp_fd_remove_callback(fd_event_cb_t cb);

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
extern int acomp_fd_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);

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
extern int acomp_fd_stream_ch_disable(int chn);

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
extern void* acomp_fd_stream_rx_buffer_get(int chn, uint32_t* len, uint16_t* desc_idx);

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
extern int acomp_fd_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void* buffer);

/**
 * @brief 分配TX流缓冲区用于向remote发送视频数据
 *
 * @param chn[in] 通道索引
 * @param len[out] 可用缓冲区长度指针
 * @param desc_idx[out] 描述符索引指针
 *
 * @return 缓冲区指针，如果失败返回NULL
 *
 */
extern void* acomp_fd_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx);

/**
 * @brief 提交TX流缓冲区发送视频数据到remote
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
extern int acomp_fd_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx);

#ifdef __cplusplus
}
#endif
