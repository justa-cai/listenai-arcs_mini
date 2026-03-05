/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_camera.h
 * @brief LISA Camera 摄像头设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Camera 类型定义
 * ======================================================================== */
 
/**
 * @brief Camera 像素格式
 */
typedef enum {
    LISA_CAMERA_PIXFMT_RGB565 = 0,   /* RGB565 格式 (2 字节/像素) */
    LISA_CAMERA_PIXFMT_RGB888 = 1,   /* RGB888 格式 (3 字节/像素) */
    LISA_CAMERA_PIXFMT_YUV422 = 2,   /* YUV422 格式 (2 字节/像素) */
    LISA_CAMERA_PIXFMT_YUV420 = 3,   /* YUV420 格式 (1.5 字节/像素) */
    LISA_CAMERA_PIXFMT_GRAY = 4,     /* 灰度格式 (1 字节/像素) */
    LISA_CAMERA_PIXFMT_JPEG = 5,     /* JPEG 压缩格式 */
    LISA_CAMERA_PIXFMT_RAW = 6,      /* RAW 格式 (Bayer) */
} lisa_camera_pixel_format_t;

/**
 * @brief Camera 分辨率
 */
typedef enum {
    LISA_CAMERA_FRAMESIZE_QQVGA = 0, /* 160x120 */
    LISA_CAMERA_FRAMESIZE_QCIF = 1,  /* 176x144 */
    LISA_CAMERA_FRAMESIZE_QVGA = 2,  /* 320x240 */
    LISA_CAMERA_FRAMESIZE_CIF = 3,   /* 400x296 */
    LISA_CAMERA_FRAMESIZE_VGA = 4,   /* 640x480 */
    LISA_CAMERA_FRAMESIZE_SVGA = 5,  /* 800x600 */
    LISA_CAMERA_FRAMESIZE_XGA = 6,   /* 1024x768 */
    LISA_CAMERA_FRAMESIZE_HD = 7,    /* 1280x720 */
    LISA_CAMERA_FRAMESIZE_SXGA = 8,  /* 1280x1024 */
    LISA_CAMERA_FRAMESIZE_UXGA = 9,  /* 1600x1200 */
    LISA_CAMERA_FRAMESIZE_FHD = 10,  /* 1920x1080 */
} lisa_camera_framesize_t;

/**
 * @brief Camera 总线接口类型
 */
typedef enum {
    LISA_CAMERA_BUS_DVP = 0,         /* DVP (Digital Video Port) 并行接口 */
    LISA_CAMERA_BUS_SPI = 1,         /* SPI 串行接口 */
} lisa_camera_bus_type_e;

/**
 * @brief Camera DVP 总线配置
 */
typedef struct {
    lisa_device_t *dvp_dev;                   /* DVP设备指针 */
    uint32_t dvp_freq;                        /* DVP频率 */
    uint16_t pixel_offset;                    /* 像素偏移 */
    uint16_t line_offset;                     /* 行偏移 */
    uint8_t pclk_polarity;                    /* 像素时钟极性 (0: 下降沿, 1: 上升沿) */
    uint8_t vsync_polarity;                   /* 垂直同步极性 (0: 低电平, 1: 高电平) */
    uint8_t hsync_polarity;                   /* 水平同步极性 (0: 低电平, 1: 高电平) */
    uint8_t data_align;                       /* 数据对齐方式 (0: right aligned[bit7~0], 1: left aligned[bit11~4])*/
    lisa_device_t *dma_dev;                    /* DMA设备指针（可选）*/
} lisa_camera_bus_dvp_config_t;

/**
 * @brief Camera SPI 总线配置
 */
typedef struct {
    lisa_device_t *spi_dev;          /* SPI设备指针 */
    uint32_t spi_freq;               /* SPI频率 */
    uint8_t spi_mode;                /* SPI模式 (0-3) */
    uint8_t spi_bit_order;           /* SPI位序 (0: MSB, 1: LSB) */
    lisa_device_t *cs_gpio;          /* 片选GPIO设备（可选）*/
    uint32_t cs_pin;                 /* 片选引脚号 */
    lisa_device_t *dma_dev;          /* DMA设备指针（可选）*/
} lisa_camera_bus_spi_config_t;

/**
 * @brief Camera 总线配置联合体
 */
typedef union {
    lisa_camera_bus_dvp_config_t dvp;
    lisa_camera_bus_spi_config_t spi;
} lisa_camera_bus_config_u;

/**
 * @brief Camera 总线配置结构体
 */
typedef struct {
    lisa_camera_bus_type_e bus_type;          /* 总线类型 */
    lisa_camera_bus_config_u config;          /* 总线配置 */
    lisa_camera_pixel_format_t pixel_format;  /* 像素格式 */
    uint8_t dma_channel;                      /* DMA通道号 */
    uint16_t width;                           /* 图像宽度 */
    uint16_t height;                          /* 图像高度 */
} lisa_camera_bus_config_t;

/**
 * @brief Camera 硬件配置结构体
 */
typedef struct {
    uint8_t mclk_pad;
    uint8_t mclk_pin;
    lisa_device_t *pwdn_gpio_dev;    /* PWDN GPIO 设备指针 */
    uint8_t pwdn_pin;                /* PWDN 引脚号 */
    uint32_t pwdn_delay_us;          /* PWDN 延时 (微秒) */
    uint32_t xclk_delay_us;          /* 时钟输出后延时 (微秒) */
    lisa_device_t *i2c_dev;          /* I2C 设备 */
} lisa_camera_hw_config_t;

/**
 * @brief Camera 配置结构体
 */
typedef struct {
    lisa_camera_hw_config_t hw_config;       /* 硬件配置 */
    uint32_t xclk_freq_hz;                   /* 外部时钟频率 (Hz) */
    uint8_t jpeg_quality;                    /* JPEG质量 (0-100, 仅JPEG格式有效) */
    uint8_t fb_count;                        /* 帧缓冲区数量 */
    bool enable_hmirror;                     /* 水平镜像 */
    bool enable_vflip;                       /* 垂直翻转 */
    bool enable_colorbar;                    /* 测试模式 (colorbar) */
} lisa_camera_config_t;

/**
 * @brief Camera 帧缓冲区结构体
 */
typedef struct {
    uint8_t *buf;                    /* 缓冲区指针 */
    uint32_t len;                    /* 数据长度（字节）*/
    uint16_t width;                  /* 图像宽度 */
    uint16_t height;                 /* 图像高度 */
    lisa_camera_pixel_format_t format; /* 像素格式 */
    uint32_t timestamp;              /* 时间戳（毫秒）*/
} lisa_camera_fb_t;

/**
 * @brief Camera 裁剪区域
 */
typedef struct {
    int16_t x;                       /* 起始 X 坐标 */
    int16_t y;                       /* 起始 Y 坐标 */
    uint16_t width;                  /* 裁剪宽度 */
    uint16_t height;                 /* 裁剪高度 */
} lisa_camera_crop_t;

/**
 * @brief Camera 能力结构体
 */
typedef struct {
    uint16_t max_width;              /* 最大宽度 */
    uint16_t max_height;             /* 最大高度 */
    uint32_t supported_formats;      /* 支持的像素格式位掩码 */
} lisa_camera_capabilities_t;

/**
 * @brief Camera 帧回调函数类型
 *
 * @param fb 帧缓冲区指针
 * @param user_data 用户数据指针
 *
 * @note 如果需要访问设备，可以通过 user_data 传入设备指针
 */
typedef void (*lisa_camera_frame_callback_t)(const lisa_camera_fb_t *fb, void *user_data);

/* ========================================================================
 * Camera 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*setup)(lisa_device_t *dev, const lisa_camera_config_t *config);
    int (*start)(lisa_device_t *dev);
    int (*stop)(lisa_device_t *dev);
    int (*capture)(lisa_device_t *dev, lisa_camera_fb_t **fb);
    int (*release_fb)(lisa_device_t *dev, lisa_camera_fb_t *fb);
    int (*get_capabilities)(lisa_device_t *dev, lisa_camera_capabilities_t *caps);
    int (*attach_bus)(lisa_device_t *dev, const lisa_camera_bus_config_t *bus_config);
    int (*set_hmirror)(lisa_device_t *dev, bool enable);
    int (*set_vflip)(lisa_device_t *dev, bool enable);
    int (*set_crop)(lisa_device_t *dev, const lisa_camera_crop_t *crop);
    int (*get_framesize)(lisa_device_t *dev, uint16_t *width, uint16_t *height);
    int (*set_pixformat)(lisa_device_t *dev, lisa_camera_pixel_format_t format);
    int (*set_reg)(lisa_device_t *dev, int reg, int mask, int value);
    int (*get_reg)(lisa_device_t *dev, int reg, int mask);
    int (*set_callback)(lisa_device_t *dev, lisa_camera_frame_callback_t callback, void *user_data);
    lisa_camera_pixel_format_t (*get_pixformat)(lisa_device_t *dev);
} lisa_camera_api_t;

/* ========================================================================
 * Camera 对外接口函数
 * ======================================================================== */

/**
 * @brief 配置摄像头设备
 *
 * @param dev Camera设备指针
 * @param config 配置参数
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_setup(lisa_device_t *dev, const lisa_camera_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->setup ? api->setup(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 启动摄像头
 *
 * @param dev Camera设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_start(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->start ? api->start(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 停止摄像头
 *
 * @param dev Camera设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_stop(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->stop ? api->stop(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 捕获一帧图像
 *
 * @param dev Camera设备指针
 * @param fb 输出参数，帧缓冲区指针的指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 捕获后需要调用 lisa_camera_release_fb() 释放帧缓冲区
 */
static inline int lisa_camera_capture(lisa_device_t *dev, lisa_camera_fb_t **fb)
{
    if (!dev || !dev->api || !fb) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->capture ? api->capture(dev, fb) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 释放帧缓冲区
 *
 * @param dev Camera设备指针
 * @param fb 帧缓冲区指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_release_fb(lisa_device_t *dev, lisa_camera_fb_t *fb)
{
    if (!dev || !dev->api || !fb) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->release_fb ? api->release_fb(dev, fb) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取摄像头设备能力
 *
 * @param dev Camera设备指针
 * @param caps 输出参数，用于接收设备能力信息
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_get_capabilities(lisa_device_t *dev, lisa_camera_capabilities_t *caps)
{
    if (!dev || !dev->api || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->get_capabilities ? api->get_capabilities(dev, caps) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 附加总线接口
 *
 * 用于配置摄像头总线接口（DVP/SPI等）及相关引脚。
 *
 * @param dev Camera设备指针
 * @param bus_config 总线配置结构体
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_attach_bus(lisa_device_t *dev, const lisa_camera_bus_config_t *bus_config)
{
    if (!dev || !dev->api || !bus_config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->attach_bus ? api->attach_bus(dev, bus_config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置水平镜像
 *
 * @param dev Camera设备指针
 * @param enable 是否启用水平镜像
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_set_hmirror(lisa_device_t *dev, bool enable)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->set_hmirror ? api->set_hmirror(dev, enable) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置垂直翻转
 *
 * @param dev Camera设备指针
 * @param enable 是否启用垂直翻转
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_set_vflip(lisa_device_t *dev, bool enable)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->set_vflip ? api->set_vflip(dev, enable) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置裁剪区域
 *
 * 设置摄像头的裁剪窗口，只输出指定区域的图像数据。
 * 可以节省内存和带宽，提高帧率。
 *
 * @param dev Camera设备指针
 * @param crop 裁剪区域配置，传入NULL则取消裁剪
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 *
 * @note 裁剪区域必须在当前分辨率范围内
 */
static inline int lisa_camera_set_crop(lisa_device_t *dev, const lisa_camera_crop_t *crop)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->set_crop ? api->set_crop(dev, crop) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取当前帧分辨率
 *
 * @param dev Camera设备指针
 * @param width 输出帧宽度
 * @param height 输出帧高度
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_get_framesize(lisa_device_t *dev, uint16_t *width, uint16_t *height)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->get_framesize ? api->get_framesize(dev, width, height) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

static inline lisa_camera_pixel_format_t lisa_camera_get_pixformat(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->get_pixformat ? api->get_pixformat(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置像素格式
 *
 * @param dev Camera设备指针
 * @param format 像素格式
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_set_pixformat(lisa_device_t *dev, lisa_camera_pixel_format_t format)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->set_pixformat ? api->set_pixformat(dev, format) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置camera寄存器
 *
 * @param dev Camera设备指针
 * @param reg 寄存器地址
 * @param mask 寄存器掩码
 * @param value 寄存器值
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_set_reg(lisa_device_t *dev, int reg, int mask, int value)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->set_reg ? api->set_reg(dev, reg, mask, value) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取camera寄存器的值
 *
 * @param dev Camera设备指针
 * @param reg 寄存器地址
 * @param mask 寄存器掩码
 *
 * @return 寄存器值
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_get_reg(lisa_device_t *dev, int reg, int mask)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->get_reg ? api->get_reg(dev, reg, mask) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置帧回调函数
 *
 * @param dev Camera设备指针
 * @param callback 回调函数指针
 * @param user_data 用户数据指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_camera_set_callback(lisa_device_t *dev, lisa_camera_frame_callback_t callback, void *user_data)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_camera_api_t *api = (lisa_camera_api_t *)dev->api;
    return api->set_callback ? api->set_callback(dev, callback, user_data) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 像素格式支持位掩码
 */
#define LISA_CAMERA_PIXFMT_MASK_RGB565   (1 << LISA_CAMERA_PIXFMT_RGB565)
#define LISA_CAMERA_PIXFMT_MASK_RGB888   (1 << LISA_CAMERA_PIXFMT_RGB888)
#define LISA_CAMERA_PIXFMT_MASK_YUV422   (1 << LISA_CAMERA_PIXFMT_YUV422)
#define LISA_CAMERA_PIXFMT_MASK_YUV420   (1 << LISA_CAMERA_PIXFMT_YUV420)
#define LISA_CAMERA_PIXFMT_MASK_GRAY     (1 << LISA_CAMERA_PIXFMT_GRAY)
#define LISA_CAMERA_PIXFMT_MASK_JPEG     (1 << LISA_CAMERA_PIXFMT_JPEG)
#define LISA_CAMERA_PIXFMT_MASK_RAW      (1 << LISA_CAMERA_PIXFMT_RAW)

/**
 * @brief 分辨率支持位掩码
 */
#define LISA_CAMERA_FRAMESIZE_MASK_QQVGA (1 << LISA_CAMERA_FRAMESIZE_QQVGA)
#define LISA_CAMERA_FRAMESIZE_MASK_QCIF  (1 << LISA_CAMERA_FRAMESIZE_QCIF)
#define LISA_CAMERA_FRAMESIZE_MASK_QVGA  (1 << LISA_CAMERA_FRAMESIZE_QVGA)
#define LISA_CAMERA_FRAMESIZE_MASK_CIF   (1 << LISA_CAMERA_FRAMESIZE_CIF)
#define LISA_CAMERA_FRAMESIZE_MASK_VGA   (1 << LISA_CAMERA_FRAMESIZE_MASK_VGA)
#define LISA_CAMERA_FRAMESIZE_MASK_SVGA  (1 << LISA_CAMERA_FRAMESIZE_SVGA)
#define LISA_CAMERA_FRAMESIZE_MASK_XGA   (1 << LISA_CAMERA_FRAMESIZE_XGA)
#define LISA_CAMERA_FRAMESIZE_MASK_HD    (1 << LISA_CAMERA_FRAMESIZE_HD)
#define LISA_CAMERA_FRAMESIZE_MASK_SXGA  (1 << LISA_CAMERA_FRAMESIZE_SXGA)
#define LISA_CAMERA_FRAMESIZE_MASK_UXGA  (1 << LISA_CAMERA_FRAMESIZE_UXGA)
#define LISA_CAMERA_FRAMESIZE_MASK_FHD   (1 << LISA_CAMERA_FRAMESIZE_FHD)

#ifdef __cplusplus
}
#endif
