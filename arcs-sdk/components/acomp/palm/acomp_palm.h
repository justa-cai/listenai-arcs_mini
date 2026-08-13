/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "../utils/acomp_err.h"
#include "acomp_stream_ipc.h"
#include "ipc/acomp_ipc.h"
#include "acomp_palm_params.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

/**
 * @brief 掌静脉算法资源存储位置
 */
typedef enum {
    ACOMP_PALM_RES_STORAGE_FLASH = ACOMP_RES_STORAGE_FLASH, /* 资源存储在 Flash 中 */
    ACOMP_PALM_RES_STORAGE_SD = ACOMP_RES_STORAGE_SD,       /* 资源存储在 SD/eMMC 中 */
    ACOMP_PALM_RES_STORAGE_PSRAM = ACOMP_RES_STORAGE_PSRAM, /* 资源已加载到 PSRAM 中 */
} acomp_palm_res_storage_t;

/**
 * @brief 掌静脉算法单个资源配置项
 */
typedef struct {
    acomp_palm_res_storage_t storage; /* 资源存储位置 */
    uintptr_t addr;                   /* 资源地址，含义由 storage 决定 */
    uint32_t size;                    /* 资源大小（字节） */
} acomp_palm_res_item_t;

/**
 * @brief 掌静脉算法资源配置
 */
typedef struct {
    acomp_palm_res_item_t detect; /* 掌静脉检测模型资源 */
    acomp_palm_res_item_t verify; /* 掌静脉特征提取模型资源 */
} acomp_palm_resource_config_t;

/* 掌静脉识别组件参数设置 */

/**
 * @brief 掌静脉识别参数键值对
 */
typedef struct {
    uint32_t key; /* 参数键（参见 acomp_palm_params.h 的 acomp_palm_params_e 枚举） */
    float value;  /* 参数值 */
} acomp_palm_param_t;

/**
 * @brief 掌静脉识别的输入图片帧结构体
 */
typedef struct {
    acomp_palm_pixel_format_t format; /* 像素格式，如 ACOMP_PALM_PIX_FMT_BGR888/YUV422/RGB565 */
    uint32_t index;                   /* 帧索引号 */
    uint16_t width;                   /* 图像宽度（像素） */
    uint16_t height;                  /* 图像高度（像素） */
    uint32_t length;                  /* 图像数据长度（字节） */
    uint32_t resv[4];                 /* 保留字段，发送前应置 0 */
    uint8_t data[0];                  /* 图像数据 */
} __attribute__((packed)) acomp_palm_input_frame_t;

/* 掌静脉识别组件结果 */

/**
 * @brief 掌静脉检测矩形框
 */
typedef struct {
    int x; /* 矩形框左上角 x 坐标 */
    int y; /* 矩形框左上角 y 坐标 */
    int w; /* 矩形框宽度 */
    int h; /* 矩形框高度 */
} acomp_palm_rect_t;

/**
 * @brief 掌静脉关键点
 */
typedef struct {
    int x;         /* 关键点 x 坐标 */
    int y;         /* 关键点 y 坐标 */
    float score;   /* 关键点置信度得分 */
    float visable; /* 关键点可见性（0-1，1 表示完全可见） */
} acomp_palm_align_point_t;

/**
 * @brief 掌静脉特征结果
 */
typedef struct {
    float features[ACOMP_PALM_MAX_FEATURE_CNT]; /* 掌静脉特征向量 */
    int feature_cnt;                            /* 掌静脉特征维度 */
} acomp_palm_feature_result_t;

/**
 * @brief 掌静脉识别单个结果
 */
typedef struct {
    acomp_palm_rect_t palm_rect;                                 /* 掌静脉检测矩形框 */
    float palm_score;                                            /* 掌静脉检测得分 */
    acomp_palm_align_point_t align_points[ACOMP_PALM_MAX_ALIGN_CNT]; /* 掌静脉关键点 */
    int n_align_point;                                           /* 掌静脉关键点个数 */
    int palm_id;                                                 /* 掌静脉 id */
    float features[ACOMP_PALM_MAX_FEATURE_CNT];                  /* 掌静脉特征向量 */
    int feature_cnt;                                             /* 掌静脉特征维度 */
    float compare_scores[ACOMP_PALM_MAX_RESULT_CNT];             /* 输入图片和已注册掌静脉特征的比较得分 */
    int compare_cnt;                                             /* 已注册掌静脉特征个数 */
} acomp_palm_result_t;

/**
 * @brief 掌静脉识别完整结果
 */
typedef struct {
    uint32_t results_cnt;          /* 掌静脉检测结果个数 */
    uint32_t max_area_results_index; /* 最大面积掌静脉结果索引 */
    acomp_palm_result_t results[0]; /* 掌静脉检测结果数组 */
} acomp_palm_result_info_t;

/* 掌静脉识别组件回调事件定义 */
#define PALM_CB_EVENT_ENGINE_RLT    BIT(0) /* 掌静脉识别引擎结果返回 */
#define PALM_CB_EVENT_ENGINE_WR_ERR BIT(1) /* 算法引擎数据写入出错 */

typedef void (*palm_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

/**
 * @brief 初始化掌静脉识别组件（PALM）
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_FOUND : 设备未找到
 *
 */
extern int acomp_palm_init(void);

/**
 * @brief 逆初始化掌静脉识别组件（PALM）
 *
 * @note 该函数会关闭已使能的流通道，并释放组件实例、事件回调和 stream 资源。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_palm_deinit(void);

/**
 * @brief 就绪掌静脉识别组件（PALM）
 *
 * @note 该函数会使用 Kconfig 中配置的模型资源初始化 AP 侧算法资源，在调用 acomp_palm_start() 之前必须先调用。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_palm_prepare(void);

/**
 * @brief 使用指定资源配置就绪掌静脉识别组件（PALM）
 *
 * @param config[in] 自定义资源配置，支持 Flash / eMMC / PSRAM
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_palm_prepare_with_resources(const acomp_palm_resource_config_t *config);

/**
 * @brief 复位掌静脉识别组件（PALM）
 *
 * @note 该函数会释放 AP 侧算法运行资源；如需再次处理图像，需要重新调用 acomp_palm_prepare() 和 acomp_palm_start()。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_palm_cleanup(void);

/**
 * @brief 启动掌静脉识别组件（PALM）
 *
 * @note 该函数会启动 AP 侧掌静脉算法，之后用户可以向组件输入图像流。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_palm_start(void);

/**
 * @brief 停止掌静脉识别组件（PALM）
 *
 * @note 该函数会停止 AP 侧掌静脉算法，之后不会处理图像流数据。
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_palm_stop(void);

/**
 * @brief 设置组件参数
 *
 * @note 设置组件参数信息，参数键见 acomp_palm_params.h 中的 acomp_palm_params_e。
 *
 * @param params[in] 存储 acomp_palm_param_t 参数的结构指针
 * @param params_cnt[in] acomp_palm_param_t 的个数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_palm_params_set(const acomp_palm_param_t *params, uint32_t params_cnt);

/**
 * @brief 获取组件参数
 *
 * @note 当前接口预留，暂未支持。
 *
 * @param params[out] 存储 acomp_palm_param_t 参数的结构指针
 * @param params_cnt[out] acomp_palm_param_t 的个数
 *
 * @return ACOMP_ERR_NOT_SUPPORTED : 暂不支持
 *
 */
extern int acomp_palm_params_get(const acomp_palm_param_t *params, uint32_t *params_cnt);

/**
 * @brief 从外部导入掌静脉特征到注册库
 *
 * @note 导入外部特征到 AP 侧注册库，用于后续掌静脉比对；最多支持 ACOMP_PALM_MAX_RESULT_CNT 个特征。
 *
 * @param features[in] 掌静脉特征数组指针
 * @param count[in] 掌静脉特征个数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_NO_MEM : 没有足够内存
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 *
 */
extern int acomp_palm_features_load(const acomp_palm_feature_result_t *features, uint32_t count);

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
 *
 */
extern int acomp_palm_add_callback(uint32_t events, palm_event_cb_t cb, void *priv);

/**
 * @brief 移除组件的回调函数
 *
 * @param cb[in] 待移除的回调函数
 *
 * @return ACOMP_ERR_OK : 成功
 * @retval ACOMP_ERR_INVALID_ARG : 错误参数
 * @retval ACOMP_ERR_INVALID_STATE : 无效状态
 * @retval ACOMP_ERR_NOT_SUPPORTED : 无效操作
 *
 */
extern int acomp_palm_remove_callback(palm_event_cb_t cb);

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
extern int acomp_palm_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);

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
extern int acomp_palm_stream_ch_disable(int chn);

/**
 * @brief 获取 RX 流缓冲区
 *
 * @param chn[in] 通道索引
 * @param len[out] 数据长度指针
 * @param desc_idx[out] 描述符索引指针
 *
 * @return 缓冲区指针，如果失败返回 NULL
 *
 */
extern void *acomp_palm_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx);

/**
 * @brief 释放 RX 流缓冲区
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
extern int acomp_palm_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer);

/**
 * @brief 分配 TX 流缓冲区用于向 remote 发送图像数据
 *
 * @param chn[in] 通道索引
 * @param len[out] 可用缓冲区长度指针
 * @param desc_idx[out] 描述符索引指针
 *
 * @return 缓冲区指针，如果失败返回 NULL
 *
 */
extern void *acomp_palm_stream_tx_buffer_alloc(int chn, uint32_t *len, uint16_t *desc_idx);

/**
 * @brief 提交 TX 流缓冲区发送图像数据到 remote
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
extern int acomp_palm_stream_tx_buffer_submit(int chn, void *buffer, uint32_t len, uint16_t desc_idx);

#ifdef __cplusplus
}
#endif
