/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_display.h
 * @brief LISA Display 设备驱动接口
 */

#pragma once

#include "lisa_device.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Display 类型定义
 * ======================================================================== */

/**
 * @brief Display 总线接口类型
 */
typedef enum {
    LISA_DISPLAY_BUS_SPI_3WIRE = 0,  /* 3线SPI接口（SCLK, MOSI, CS）*/
    LISA_DISPLAY_BUS_SPI_4WIRE = 1,  /* 4线SPI接口（SCLK, MOSI, CS, D/C）*/
    LISA_DISPLAY_BUS_I8080 = 2,      /* Intel 8080 并行接口 */
    LISA_DISPLAY_BUS_QSPI = 3,       /* QSPI 接口 */
    LISA_DISPLAY_BUS_RGB = 4,        /* RGB并行接口 */
} lisa_display_bus_type_t;

/**
 * @brief Display 命令总线类型（用于 RGB 等需要独立配置通道的场景）
 */
typedef enum {
    LISA_DISPLAY_CMD_BUS_NONE = 0,     /* 命令和数据共用总线 */
    LISA_DISPLAY_CMD_BUS_SW_SPI = 1,   /* 软件 SPI 命令总线 */
} lisa_display_cmd_bus_type_t;

/* ========================================================================
 * RGB LCD 配置数据结构
 * ======================================================================== */
/**
 * @brief RGB 输入格式
 */
typedef enum {
    LISA_RGB_INPUT_FORMAT_RGB888 = 0,
    LISA_RGB_INPUT_FORMAT_XRGB8888 = 1,
    LISA_RGB_INPUT_FORMAT_RGB565 = 2,
} lisa_rgb_input_format_t;

/**
 * @brief RGB 信号极性
 */
typedef enum {
    LISA_RGB_POLARITY_POSITIVE = 0,  /**< 正极性（上升沿有效） */
    LISA_RGB_POLARITY_NEGATIVE = 1,  /**< 负极性（下降沿有效） */
} lisa_rgb_polarity_t;

/**
 * @brief RGB 输出格式
 */
typedef enum {
    LISA_RGB_OUTPUT_FORMAT_RGB888 = 0,
    LISA_RGB_OUTPUT_FORMAT_RGB666 = 1,
    LISA_RGB_OUTPUT_FORMAT_RGB565 = 2,
    LISA_RGB_OUTPUT_FORMAT_BGR888 = 3,
    LISA_RGB_OUTPUT_FORMAT_BGR666 = 4,
    LISA_RGB_OUTPUT_FORMAT_BGR565 = 5,
} lisa_rgb_output_format_t;

/**
 * @brief Display 总线配置（4线SPI）
 */
typedef struct {
    lisa_device_t *spi_dev;    /* SPI设备指针 */
    lisa_device_t *dc_gpio;    /* D/C信号的GPIO设备 */
    lisa_device_t *cs_gpio;    /* CS信号的GPIO设备 */
    uint32_t cs_pin;           /* CS引脚号 */
    uint32_t dc_pin;           /* D/C引脚号 */
    uint32_t spi_freq;         /* SPI频率 */
} lisa_display_bus_spi_4wire_config_t;

/**
 * @brief Display 总线配置（3线SPI）
 */
typedef struct {
    lisa_device_t *spi_dev;    /* SPI设备指针 */
    uint32_t spi_freq;         /* SPI频率 */
} lisa_display_bus_spi_3wire_config_t;

/**
 * @brief Display 总线配置（QSPI）
 */
typedef struct {
    lisa_device_t *qspi_dev;    /* QSPI LCD 设备指针 */
    uint32_t qspi_freq;         /* QSPI LCD 时钟频率 */
    lisa_device_t *cs_gpio;     /* QSPI LCD CS GPIO 设备 */
    uint32_t cs_pin;            /* QSPI LCD CS 引脚 */
} lisa_display_bus_qspi_config_t;

/**
 * @brief Display 总线配置（RGB并行）
 */
typedef struct {
    lisa_device_t *rgb_dev;    /* RGB控制器句柄 */
    uint32_t pclk_hz;          /* PCLK时钟频率 */
    lisa_rgb_input_format_t input_format;
    lisa_rgb_output_format_t output_format;
    bool output_lsb_first;
    lisa_rgb_polarity_t vsync_polarity;
    lisa_rgb_polarity_t hsync_polarity;
    lisa_rgb_polarity_t de_polarity;
    lisa_rgb_polarity_t pclk_polarity;
    uint32_t bounce_buffer_size;
    struct {
        uint16_t h_res;
        uint16_t v_res;
        uint8_t h_pulse_width;
        uint8_t v_pulse_width;
        uint8_t h_front_blanking;
        uint8_t h_back_blanking;
        uint8_t v_front_blanking;
        uint8_t v_back_blanking;
    } timings;
} lisa_display_bus_rgb_config_t;

/**
 * @brief Display 命令通道配置（软件模拟 3 线 SPI）
 */
typedef struct {
    lisa_device_t *cs_gpio;     /* CS 信号的 GPIO 设备 */
    uint32_t cs_pin;            /* CS 引脚号 */
    lisa_device_t *scl_gpio;    /* SCL 信号的 GPIO 设备 */
    uint32_t scl_pin;           /* SCL 引脚号 */
    lisa_device_t *sda_gpio;    /* SDA 信号的 GPIO 设备 */
    uint32_t sda_pin;           /* SDA 引脚号 */
    uint32_t spi_freq;          /* SPI 频率（Hz）*/

    /* 时序和选项配置 */
    uint8_t lsb_first;          /* 1=LSB先传输, 0=MSB先传输 */
    uint8_t cs_high_active;     /* 1=CS高电平有效, 0=CS低电平有效 */
    uint8_t spi_mode;           /* SPI模式 0-3 (CPOL/CPHA) */

    /* 3线DC位编码配置 */
    uint8_t use_dc_bit;         /* 1=使用DC位（9位协议）, 0=标准8位 */
    uint8_t dc_zero_on_data;    /* 1=数据时DC=0,命令时DC=1; 0=相反 */
} lisa_display_cmd_sw_spi_config_t;

/**
 * @brief Display 命令通道配置联合体
 */
typedef union {
    lisa_display_cmd_sw_spi_config_t sw_spi;
} lisa_display_cmd_bus_config_u;


/* --- 背光控制 --- */
/**
 * @brief 背光控制类型
 */
typedef enum {
    LISA_DISPLAY_BACKLIGHT_TYPE_PWM,        /**< PWM 控制 */
    LISA_DISPLAY_BACKLIGHT_TYPE_SINGLE_WIRE,/**< 单线控制 */
} lisa_display_backlight_type_t;

typedef enum {
    LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,  /* 低电平有效 */
    LISA_DISPLAY_BLACKLIGHT_POLARITY_HIGH, /* 高电平有效 */
} lisa_display_backlight_polarity_t;

/**
 * @brief PWM 背光配置
 */
typedef struct {
    lisa_device_t *dev;         /**< PWM 设备句柄 */
    uint8_t channel;            /**< PWM 通道 */
    uint32_t freq;              /**< PWM 频率 */
} lisa_display_backlight_pwm_config_t;

/**
 * @brief 单线调光背光配置
 */
typedef struct {
    lisa_device_t *dev;         /**< 单线调光设备句柄 */
    uint8_t pin;                /**< 单线调光引脚 */
    uint8_t steps;              /**< 亮度步进数 */
    uint8_t current_level;      /**< 当前亮度级别 */
} lisa_display_backlight_single_wire_config_t;

/**
 * @brief 背光控制句柄
 */
typedef struct {
    lisa_display_backlight_type_t type; /**< 背光类型 */
    union {
        lisa_display_backlight_pwm_config_t pwm; /**< PWM 配置 */
        lisa_display_backlight_single_wire_config_t sw; /**< 单线调光配置 */
    } config;
    lisa_display_backlight_polarity_t blacklight_polarity;
    void *priv_data; /**< 背光驱动私有数据 */
} lisa_display_backlight_t;

/**
 * @brief Display 总线配置联合体
 */
typedef union {
    lisa_display_bus_spi_3wire_config_t spi_3wire;
    lisa_display_bus_spi_4wire_config_t spi_4wire;
    lisa_display_bus_qspi_config_t qspi;
    lisa_display_bus_rgb_config_t rgb;
} lisa_display_bus_config_u;

/**
 * @brief Display 总线配置结构体
 */
typedef struct {
    lisa_display_bus_type_t bus_type;               /* 总线类型 */
    lisa_display_bus_config_u bus_config;           /* 总线配置 */

    /* 命令总线配置 */
    lisa_display_cmd_bus_type_t cmd_bus_type;      /* 命令总线类型 */
    lisa_display_cmd_bus_config_u cmd_bus_config;  /* 命令总线配置 */

    lisa_device_t *rst_gpio;                        /* 复位信号的GPIO设备（可选）*/
    uint32_t rst_pin;                               /* 复位引脚号 */
    lisa_device_t *te_gpio;                         /* TE信号的GPIO设备（可选）*/
    uint32_t te_pin;                                /* TE引脚号 */
    lisa_display_backlight_t backlight;             /* 背光控制配置，type=NONE 表示无背光 */
} lisa_display_config_t;

/**
 * @brief Display 像素格式
 */
typedef enum {
    LISA_DISPLAY_PIXEL_FORMAT_RGB_888 = 0,   /* RGB 888 格式 */
    LISA_DISPLAY_PIXEL_FORMAT_RGB_565 = 1,   /* RGB 565 格式 */
    LISA_DISPLAY_PIXEL_FORMAT_BGR_565 = 2,   /* BGR 565 格式 */
    LISA_DISPLAY_PIXEL_FORMAT_ARGB_8888 = 3, /* ARGB 8888 格式 */
    LISA_DISPLAY_PIXEL_FORMAT_MONO_1 = 4,    /* 单色 1 位格式 */
} lisa_display_pixel_format_t;

/**
 * @brief Display 方向
 */
typedef enum {
    LISA_DISPLAY_ORIENTATION_0 = 0,     /* 0 度 */
    LISA_DISPLAY_ORIENTATION_90 = 1,    /* 90 度 */
    LISA_DISPLAY_ORIENTATION_180 = 2,   /* 180 度 */
    LISA_DISPLAY_ORIENTATION_270 = 3,   /* 270 度 */
} lisa_display_orientation_t;

/**
 * @brief Display 能力结构体
 */
typedef struct {
    uint16_t width;                              /* 显示宽度（像素） */
    uint16_t height;                             /* 显示高度（像素） */
    lisa_display_pixel_format_t pixel_format;    /* 当前像素格式 */
    lisa_display_orientation_t orientation;      /* 当前方向 */
    uint32_t supported_pixel_formats;            /* 支持的像素格式位掩码 */
} lisa_display_capabilities_t;

/**
 * @brief Display 缓冲区描述符
 */
typedef struct {
    uint16_t width;      /* 缓冲区宽度（像素） */
    uint16_t height;     /* 缓冲区高度（像素） */
    uint16_t pitch;      /* 行间距（像素） */
    uint32_t buf_size;   /* 缓冲区大小（字节） */
} lisa_display_buffer_desc_t;

/**
 * @brief Display 区域结构体
 */
typedef struct {
    uint16_t x;      /* 起始 X 坐标 */
    uint16_t y;      /* 起始 Y 坐标 */
    uint16_t width;  /* 区域宽度 */
    uint16_t height; /* 区域高度 */
} lisa_display_rect_t;

/* ========================================================================
 * Display 设备 API 结构体
 * ======================================================================== */

typedef struct {
    int (*get_capabilities)(lisa_device_t *dev, lisa_display_capabilities_t *caps);
    int (*write)(lisa_device_t *dev, uint16_t x, uint16_t y, const lisa_display_buffer_desc_t *desc, const void *buf);
    int (*blanking_on)(lisa_device_t *dev);
    int (*blanking_off)(lisa_device_t *dev);
    int (*set_brightness)(lisa_device_t *dev, uint8_t brightness);
    int (*set_orientation)(lisa_device_t *dev, lisa_display_orientation_t orientation);
    int (*attach_bus)(const lisa_device_t *dev, const lisa_display_config_t *config);
} lisa_display_api_t;

/* ========================================================================
 * Display 对外接口函数
 * ======================================================================== */

/**
 * @brief 获取显示设备能力
 *
 * @param dev Display设备指针
 * @param caps 输出参数，用于接收设备能力信息
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_display_get_capabilities(lisa_device_t *dev, lisa_display_capabilities_t *caps)
{
    if (!dev || !dev->api || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_display_api_t *api = (lisa_display_api_t *)dev->api;
    return api->get_capabilities ? api->get_capabilities(dev, caps) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 写入显示数据
 *
 * @param dev Display设备指针
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param desc 缓冲区描述符
 * @param buf 数据缓冲区指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_display_write(lisa_device_t *dev, uint16_t x, uint16_t y,
                                     const lisa_display_buffer_desc_t *desc, const void *buf)
{
    if (!dev || !dev->api || !desc || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_display_api_t *api = (lisa_display_api_t *)dev->api;
    return api->write ? api->write(dev, x, y, desc, buf) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 开启消隐（关闭显示）
 *
 * @param dev Display设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_display_blanking_on(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_display_api_t *api = (lisa_display_api_t *)dev->api;
    return api->blanking_on ? api->blanking_on(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 关闭消隐（开启显示）
 *
 * @param dev Display设备指针
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_display_blanking_off(lisa_device_t *dev)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_display_api_t *api = (lisa_display_api_t *)dev->api;
    return api->blanking_off ? api->blanking_off(dev) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置亮度
 *
 * @param dev Display设备指针
 * @param brightness 亮度值 (0-100)
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_display_set_brightness(lisa_device_t *dev, uint8_t brightness)
{
    if (!dev || !dev->api || brightness > 100) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_display_api_t *api = (lisa_display_api_t *)dev->api;
    return api->set_brightness ? api->set_brightness(dev, brightness) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置显示方向
 *
 * @param dev Display设备指针
 * @param orientation 显示方向
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_display_set_orientation(lisa_device_t *dev, lisa_display_orientation_t orientation)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_display_api_t *api = (lisa_display_api_t *)dev->api;
    return api->set_orientation ? api->set_orientation(dev, orientation) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 附加显示总线接口
 *
 * 用于配置显示总线接口（SPI/RGB/MIPI DSI等）及相关引脚。
 *
 * @param dev Display设备指针
 * @param config 显示配置结构体
 *
 * @return 0 成功
 * @return LISA_DEVICE_ERR_INVALID 参数无效
 * @return LISA_DEVICE_ERR_NOT_SUPPORT 不支持该操作
 * @return <0 其他错误
 */
static inline int lisa_display_attach_bus(lisa_device_t *dev, const lisa_display_config_t *config)
{
    if (!dev || !dev->api || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_display_api_t *api = (lisa_display_api_t *)dev->api;
    return api->attach_bus ? api->attach_bus(dev, config) : LISA_DEVICE_ERR_NOT_SUPPORT;
}

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief RGB565 颜色宏
 */
#define LISA_DISPLAY_RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

/**
 * @brief 常用颜色定义 (RGB565)
 */
#define LISA_DISPLAY_COLOR_BLACK   0x0000
#define LISA_DISPLAY_COLOR_WHITE   0xFFFF
#define LISA_DISPLAY_COLOR_RED     0xF800
#define LISA_DISPLAY_COLOR_GREEN   0x07E0
#define LISA_DISPLAY_COLOR_BLUE    0x001F
#define LISA_DISPLAY_COLOR_YELLOW  0xFFE0
#define LISA_DISPLAY_COLOR_CYAN    0x07FF
#define LISA_DISPLAY_COLOR_MAGENTA 0xF81F

#ifdef __cplusplus
}
#endif
