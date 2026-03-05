#include "cache.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define TAG "lisa_camera"
#include "lisa_log.h"

#include "bus/lisa_camera_bus.h"
#include "IOMuxManager.h"
#include "lisa_camera.h"
#include "lisa_device.h"
#include "lisa_gpio.h"
#include "sensor.h"

#include "systick.h"
#include "arcs_ap.h"
#include "lisa_mem.h"
#include <lisa_queue.h>
#include <lisa_time.h>
#include <lisa_typedef.h>


#if CONFIG_LISA_CAMERA_SENSOR_GC032A
#include "gc032a.h"
#endif
#if CONFIG_LISA_CAMERA_SENSOR_GC0328
#include "gc0328.h"
#endif
#if CONFIG_LISA_CAMERA_SENSOR_BF3901
#include "bf3901.h"
#endif

#define MAX_BUF_NUM         5

typedef struct {
    camera_model_t model;
    int (*detect)(int slv_addr, sensor_id_t *id);
    int (*init)(sensor_t *sensor);
} sensor_func_t;

static const sensor_func_t camera_sensors[] = {
#if CONFIG_LISA_CAMERA_SENSOR_OV7725
    {CAMERA_OV7725, ov7725_detect, ov7725_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_OV7670
    {CAMERA_OV7670, ov7670_detect, ov7670_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_OV2640
    {CAMERA_OV2640, ov2640_detect, ov2640_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_OV3660
    {CAMERA_OV3660, ov3660_detect, ov3660_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_OV5640
    {CAMERA_OV5640, ov5640_detect, ov5640_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_NT99141
    {CAMERA_NT99141, nt99141_detect, nt99141_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_GC2145
    {CAMERA_GC2145, gc2145_detect, gc2145_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_GC032A
    {CAMERA_GC032A, gc032a_detect, gc032a_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_GC0328
    {CAMERA_GC0328, gc0328_detect, gc0328_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_GC0308
    {CAMERA_GC0308, gc0308_detect, gc0308_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_GC0310
    {CAMERA_GC0310, gc0310_detect, gc0310_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_BF3005
    {CAMERA_BF3005, bf3005_detect, bf3005_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_BF20A6
    {CAMERA_BF20A6, bf20a6_detect, bf20a6_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_BF3901
    {CAMERA_BF3901, bf3901_detect, bf3901_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_SC101IOT
    {CAMERA_SC101IOT, sc101iot_detect, sc101iot_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_SC030IOT
    {CAMERA_SC030IOT, sc030iot_detect, sc030iot_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_SC031GS
    {CAMERA_SC031GS, sc031gs_detect, sc031gs_init},
#endif
#if CONFIG_LISA_CAMERA_SENSOR_OV9655
    {CAMERA_OV9655, ov9655_detect, ov9655_init},
#endif
};

// ============================================================================
// Camera ARCS 驱动实现
// ============================================================================

typedef struct {
    lisa_camera_config_t config;
    lisa_device_t              *bus_dev;     /* 数据线设备 */
    lisa_camera_pixel_format_t pixel_format; /* 像素格式 */
    uint16_t frame_width;                    /* 图像宽度 */
    uint16_t frame_height;                   /* 图像高度 */
    lisa_camera_fb_t fb_list[MAX_BUF_NUM];  /* 帧缓冲区列表 */
    uint8_t *fb_buf[MAX_BUF_NUM];           /* 帧缓冲区内存 */
    sensor_t sensor;                        /* sensor */
    lisa_camera_capabilities_t capabilities;/* 能力 */
    lisa_queue_t *queue_free;               /* 空闲帧队列 */
    lisa_queue_t *queue_filled;             /* 已填充帧队列 */
    uint8_t fb_count;
    lisa_camera_frame_callback_t callback;
    void *callback_user_data;
    bool is_started;
    bool is_initialized;
} arcs_camera_priv_t;

static arcs_camera_priv_t arcs_camera_priv;

/**
 * @brief 将 sensor pixformat 掩码转换为 lisa_camera pixformat 掩码
 */
static uint32_t convert_sensor_pixfmt_mask_to_lisa(uint16_t sensor_mask)
{
    uint32_t lisa_mask = 0;
    if (sensor_mask & PIXFORMAT_MASK_RGB565) {
        lisa_mask |= LISA_CAMERA_PIXFMT_MASK_RGB565;
    }
    if (sensor_mask & PIXFORMAT_MASK_YUV422) {
        lisa_mask |= LISA_CAMERA_PIXFMT_MASK_YUV422;
    }
    if (sensor_mask & PIXFORMAT_MASK_YUV420) {
        lisa_mask |= LISA_CAMERA_PIXFMT_MASK_YUV420;
    }
    if (sensor_mask & PIXFORMAT_MASK_GRAYSCALE) {
        lisa_mask |= LISA_CAMERA_PIXFMT_MASK_GRAY;
    }
    if (sensor_mask & PIXFORMAT_MASK_JPEG) {
        lisa_mask |= LISA_CAMERA_PIXFMT_MASK_JPEG;
    }
    if (sensor_mask & PIXFORMAT_MASK_RGB888) {
        lisa_mask |= LISA_CAMERA_PIXFMT_MASK_RGB888;
    }
    if (sensor_mask & PIXFORMAT_MASK_RAW) {
        lisa_mask |= LISA_CAMERA_PIXFMT_MASK_RAW;
    }
    return lisa_mask;
}

/**
 * @brief 帧完成回调函数 (由总线驱动调用)
 * 
 * 当 SPI/DVP 接收完一帧数据后，将帧放入已填充队列，并从空闲队列获取下一个帧缓冲区
 */
static void lisa_camera_frame_done_callback(const lisa_camera_fb_t *fb_in, void *user_data)
{
    lisa_device_t *dev = (lisa_device_t *)user_data;
    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    lisa_camera_fb_t *fb = (lisa_camera_fb_t *)fb_in;  /* 去除 const 以便修改时间戳 */

    if (!priv || !fb) {
        return;
    }

    /* 设置时间戳 */
    fb->timestamp = lisa_os_get_tick_ms();
    fb->format = priv->pixel_format;

    /* 将已填充的帧放入 filled 队列 */
    if (lisa_queue_push(priv->queue_filled, (void *)&fb, sizeof(fb), LISA_NO_WAIT) != LISA_OK) {
        /* 队列满，丢弃当前帧 */
        lisa_queue_push(priv->queue_free, (void *)&fb, sizeof(fb), LISA_NO_WAIT);
    }

    /* 通知用户回调 */
    if (priv->callback) {
        priv->callback(fb, priv->callback_user_data);
    }
}

/**
 * @brief 获取空闲帧缓冲区 (供总线驱动使用，非 ISR 上下文)
 */
static lisa_camera_fb_t *lisa_camera_get_free_fb(lisa_device_t *dev)
{
    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    lisa_camera_fb_t *fb = NULL;

    if (!priv || !priv->queue_free) {
        return NULL;
    }

    /* 从空闲队列获取帧缓冲区，不等待 */
    if (lisa_queue_pop(priv->queue_free, (void *)&fb, sizeof(fb), LISA_NO_WAIT) != LISA_OK) {
        return NULL;
    }

    return fb;
}

/**
 * @brief 获取空闲帧缓冲区 (ISR 上下文)
 */
static lisa_camera_fb_t *lisa_camera_get_free_fb_from_isr(lisa_device_t *dev)
{
    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    lisa_camera_fb_t *fb = NULL;

    if (!priv || !priv->queue_free) {
        return NULL;
    }

    /* 从空闲队列获取帧缓冲区 (lisa_queue_pop 内置ISR检测) */
    if (lisa_queue_pop(priv->queue_free, (void *)&fb, sizeof(fb), LISA_NO_WAIT) != LISA_OK) {
        return NULL;
    }

    return fb;
}

/**
 * @brief 获取空闲帧缓冲区回调包装 (普通上下文)
 */
static lisa_camera_fb_t *lisa_camera_get_free_fb_wrapper(void *ctx)
{
    return lisa_camera_get_free_fb((lisa_device_t *)ctx);
}

/**
 * @brief 获取空闲帧缓冲区回调包装 (ISR 上下文)
 */
static lisa_camera_fb_t *lisa_camera_get_free_fb_from_isr_wrapper(void *ctx)
{
    return lisa_camera_get_free_fb_from_isr((lisa_device_t *)ctx);
}

/**
 * @brief 初始化帧缓冲区
 */
static int lisa_camera_fb_init(arcs_camera_priv_t *priv, uint32_t frame_size)
{
    uint8_t fb_count = priv->config.fb_count;
    if (fb_count == 0 || fb_count > MAX_BUF_NUM) {
        fb_count = 2;  /* 默认双缓冲 */
    }

    /* 创建队列 */
    priv->queue_free = lisa_queue_create(fb_count, NULL, sizeof(lisa_camera_fb_t *));
    priv->queue_filled = lisa_queue_create(fb_count, NULL, sizeof(lisa_camera_fb_t *));

    if (!priv->queue_free || !priv->queue_filled) {
        LOGE("Failed to create frame queues");
        return LISA_DEVICE_ERR_NO_MEM;
    }

    /* 分配帧缓冲区内存并初始化 */
    for (uint8_t i = 0; i < fb_count; i++) {
        priv->fb_buf[i] = (uint8_t *)lisa_mem_align_alloc(32, frame_size);
        if (!priv->fb_buf[i]) {
            LOGE("Failed to allocate frame buffer %d", i);
            /* 释放已分配的内存 */
            for (uint8_t j = 0; j < i; j++) {
                lisa_mem_free(priv->fb_buf[j]);
                priv->fb_buf[j] = NULL;
            }
            return LISA_DEVICE_ERR_NO_MEM;
        }

        /* 初始化帧缓冲区结构 */
        priv->fb_list[i].buf = priv->fb_buf[i];
        priv->fb_list[i].len = frame_size;
        priv->fb_list[i].width = priv->frame_width;
        priv->fb_list[i].height = priv->frame_height;
        priv->fb_list[i].format = priv->pixel_format;
        priv->fb_list[i].timestamp = 0;

        /* 将帧缓冲区指针放入空闲队列 */
        lisa_camera_fb_t *fb_ptr = &priv->fb_list[i];
        lisa_queue_push(priv->queue_free, (void *)&fb_ptr, sizeof(fb_ptr), LISA_NO_WAIT);
    }

    priv->fb_count = fb_count;
    LOGI("Frame buffers initialized: count=%d, size=%lu", fb_count, frame_size);

    return LISA_DEVICE_OK;
}

/**
 * @brief 重新初始化帧缓冲区
 */
static int lisa_camera_fb_reinit(arcs_camera_priv_t *priv)
{
    uint32_t frame_size = priv->frame_width * priv->frame_height * 2;  /* 默认 RGB565/YUV422 */
    if (priv->pixel_format == LISA_CAMERA_PIXFMT_GRAY) {
        frame_size = priv->frame_width * priv->frame_height;
    }

    /* 仅重新分配缓冲区内存，不销毁队列 */
    for (uint8_t i = 0; i < priv->fb_count; i++) {
        if (priv->fb_buf[i]) {
            lisa_mem_free(priv->fb_buf[i]);
        }
        priv->fb_buf[i] = (uint8_t *)lisa_mem_align_alloc(32, frame_size);
        if (!priv->fb_buf[i]) {
            LOGE("Failed to re-allocate frame buffer %d", i);
            /* 释放已分配的内存 */
            for (uint8_t j = 0; j < i; j++) {
                lisa_mem_free(priv->fb_buf[j]);
                priv->fb_buf[j] = NULL;
            }
            return LISA_DEVICE_ERR_NO_MEM;
        }
        priv->fb_list[i].buf = priv->fb_buf[i];
        priv->fb_list[i].len = frame_size;
        priv->fb_list[i].width = priv->frame_width;
        priv->fb_list[i].height = priv->frame_height;
    }

    LOGI("Frame buffers re-initialized: count=%d, size=%lu", priv->fb_count, frame_size);

    return LISA_DEVICE_OK;
}

static void lisa_camera_fb_deinit(arcs_camera_priv_t *priv)
{
    /* 删除队列 */
    if (priv->queue_free) {
        lisa_queue_delete(priv->queue_free);
        priv->queue_free = NULL;
    }
    if (priv->queue_filled) {
        lisa_queue_delete(priv->queue_filled);
        priv->queue_filled = NULL;
    }

    /* 释放帧缓冲区内存 */
    for (uint8_t i = 0; i < priv->fb_count; i++) {
        if (priv->fb_buf[i]) {
            lisa_mem_free(priv->fb_buf[i]);
            priv->fb_buf[i] = NULL;
        }
    }
    priv->fb_count = 0;
}

/**
 * @brief 附加总线配置
 */
static int lisa_camera_attach_bus_arcs(lisa_device_t *dev, const lisa_camera_bus_config_t *bus_config)
{
    if (!dev || !bus_config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    int ret = LISA_DEVICE_OK;

    lisa_device_t *bus_dev = lisa_device_get("camera_bus");
    priv->bus_dev = bus_dev;
    if (!bus_dev || !lisa_device_ready(bus_dev)) {
        LOGE("camera_bus not ready");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    lisa_camera_bus_if_t *api = (lisa_camera_bus_if_t *)bus_dev->api;
    if (api->init) {
        ret = api->init(bus_dev, bus_config);
    }

    return ret;
}

/**
 * @brief 设置像素格式
 */
static int lisa_camera_set_pixformat_arcs(lisa_device_t *dev, lisa_camera_pixel_format_t format)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    priv->pixel_format = format;

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (priv->sensor.set_pixformat) {
        pixformat_t pixformat;
        switch (format) {
        case LISA_CAMERA_PIXFMT_RGB565:
            pixformat = PIXFORMAT_RGB565;
            break;
        case LISA_CAMERA_PIXFMT_RGB888:
            pixformat = PIXFORMAT_RGB888;
            break;
        case LISA_CAMERA_PIXFMT_YUV422:
            pixformat = PIXFORMAT_YUV422;
            break;
        case LISA_CAMERA_PIXFMT_YUV420:
            pixformat = PIXFORMAT_YUV420;
            break;
        case LISA_CAMERA_PIXFMT_GRAY:
            pixformat = PIXFORMAT_GRAYSCALE;
            break;
        case LISA_CAMERA_PIXFMT_JPEG:
            pixformat = PIXFORMAT_JPEG;
            break;
        case LISA_CAMERA_PIXFMT_RAW:
            pixformat = PIXFORMAT_RAW;
            break;
        default:
            return LISA_DEVICE_ERR_INVALID;
        }
        priv->sensor.set_pixformat(&priv->sensor, pixformat);
        lisa_camera_fb_reinit(priv);
    }

    return LISA_DEVICE_OK;
}


static lisa_camera_pixel_format_t lisa_camera_get_pixformat_arcs(lisa_device_t *dev)
{
    if (!dev) {
        return LISA_CAMERA_PIXFMT_RAW;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_CAMERA_PIXFMT_RAW;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_CAMERA_PIXFMT_RAW;
    }

    return priv->pixel_format;
}

/**
 * @brief 设置水平镜像
 */
static int lisa_camera_set_hmirror_arcs(lisa_device_t *dev, bool enable)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    priv->config.enable_hmirror = enable;

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 如果 sensor 已初始化，直接设置 */
    if (priv->sensor.set_hmirror) {
        priv->sensor.set_hmirror(&priv->sensor, enable);
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置垂直翻转
 */
static int lisa_camera_set_vflip_arcs(lisa_device_t *dev, bool enable)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    priv->config.enable_vflip = enable;

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 如果 sensor 已初始化，直接设置 */
    if (priv->sensor.set_vflip) {
        priv->sensor.set_vflip(&priv->sensor, enable);
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置曝光值
 */
static int lisa_camera_set_exposure(const lisa_device_t *dev, int exposure)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    // TODO: 实现曝光设置
    return LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置增益
 */
static int lisa_camera_set_gain(lisa_device_t *dev, int gain)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }
    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (priv->sensor.set_gainceiling) {
        priv->sensor.set_gainceiling(&priv->sensor, gain);
    }
    else {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取帧大小尺寸
 */
static int lisa_camera_get_framesize_arcs(lisa_device_t *dev, uint16_t *width, uint16_t *height)
{
    if (!dev || !width || !height) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    *width = priv->frame_width;
    *height = priv->frame_height;

    return LISA_DEVICE_OK;
}

/**
 * @brief 获取设备能力
 */
static int lisa_camera_get_capabilities_arcs(lisa_device_t *dev, lisa_camera_capabilities_t *caps)
{
    if (!dev || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    // 根据硬件能力填充能力结构体
    caps->max_width = priv->capabilities.max_width;
    caps->max_height = priv->capabilities.max_height;
    caps->supported_formats = priv->capabilities.supported_formats;

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置裁剪区域
 */
static int lisa_camera_set_crop_arcs(lisa_device_t *dev, const lisa_camera_crop_t *crop)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (crop && priv->sensor.set_window) {
        priv->sensor.set_window(&priv->sensor, crop->x, crop->y, 
                                crop->width, crop->height);
        priv->frame_width = crop->width;
        priv->frame_height = crop->height;
        lisa_camera_fb_reinit(priv);
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置寄存器
 */
static int lisa_camera_set_reg_arcs(lisa_device_t *dev, int reg, int mask, int value)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (priv->sensor.set_reg) {
        return priv->sensor.set_reg(&priv->sensor, reg, mask, value);
    }

    return LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 获取寄存器
 */
static int lisa_camera_get_reg_arcs(lisa_device_t *dev, int reg, int mask)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (priv->sensor.get_reg) {
        return priv->sensor.get_reg(&priv->sensor, reg, mask);
    }

    return LISA_DEVICE_ERR_NOT_SUPPORT;
}

/**
 * @brief 设置帧回调函数
 */
static int lisa_camera_set_callback_arcs(lisa_device_t *dev, lisa_camera_frame_callback_t callback, void *user_data)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    priv->callback = callback;
    priv->callback_user_data = user_data;

    return LISA_DEVICE_OK;
}

/**
 * @brief 初始化 PWDN 引脚
 */
static int lisa_camera_pwdn_init(arcs_camera_priv_t *priv)
{
    lisa_camera_hw_config_t *hw = &priv->config.hw_config;

    if (hw->pwdn_gpio_dev == NULL) {
        LOGW("PWDN GPIO device not configured, skipping");
        return LISA_DEVICE_OK;
    }

    /* 设置 PWDN 引脚为输出模式 */
    lisa_gpio_configure(hw->pwdn_gpio_dev, hw->pwdn_pin, LISA_GPIO_CONFIG_OUTPUT_LOW);
    /* 延时等待 sensor 稳定 */
    SysTick_Delay_Us(hw->pwdn_delay_us);

    LOGI("PWDN pin initialized (pin=%d)", hw->pwdn_pin);
    return LISA_DEVICE_OK;
}

/**
 * @brief 初始化时钟输出
 */
static int lisa_camera_xclk_init(arcs_camera_priv_t *priv)
{
    uint32_t xclk_freq = priv->config.xclk_freq_hz;

    if (xclk_freq > 0) {
        LOGI("Enabling XCLK output: %d Hz", xclk_freq);
        IOMuxManager_PinConfigure(priv->config.hw_config.mclk_pad, priv->config.hw_config.mclk_pin, CSK_IOMUX_FUNC_ALTER16);
        /* 使能视频时钟 */
        IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;
        IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;
        /* 配置 DVP 时钟输出 */
        DVP_EnableClockout(DVP0(), xclk_freq);
    }

    /* 时钟输出后延时 */
    SysTick_Delay_Us(priv->config.hw_config.xclk_delay_us);

    return LISA_DEVICE_OK;
}

/**
 * @brief Probe 并初始化 sensor
 */
static int lisa_camera_sensor_probe(arcs_camera_priv_t *priv)
{
    int ret = -1;
    uint32_t i;
    camera_sensor_info_t *sensor_info;
    uint32_t sensor_count = sizeof(camera_sensors) / sizeof(camera_sensors[0]);

    /* 初始化 I2C 用于 sensor 通信 */
    ret = sensor_twi_init(priv->config.hw_config.i2c_dev);
    if (ret != 0) {
        LOGE("Failed to init sensor I2C");
        return LISA_DEVICE_ERR_IO;
    }

    /* 遍历所有支持的 sensor，尝试检测 */
    for (i = 0; i < sensor_count; i++) {
        sensor_info = camera_sensor_get_info(camera_sensors[i].model);
        if (sensor_info != NULL) {
            if (camera_sensors[i].detect(sensor_info->sccb_addr, &priv->sensor.id)) {
                priv->sensor.slv_addr = sensor_info->sccb_addr;
                priv->sensor.xclk_freq_hz = priv->config.xclk_freq_hz;
                /* 初始化 sensor 函数指针 */
                camera_sensors[i].init(&priv->sensor);
                LOGI("Sensor detected: PID=0x%04X, addr=0x%02X", 
                     priv->sensor.id.PID, priv->sensor.slv_addr);
                priv->capabilities.max_height = sensor_info->max_height;
                priv->capabilities.max_width = sensor_info->max_width;
                priv->capabilities.supported_formats = convert_sensor_pixfmt_mask_to_lisa(sensor_info->supported_formats);
                break;
            }
        }
    }

    if (i == sensor_count) {
        LOGE("No camera sensor detected");
        return LISA_DEVICE_ERR_NOT_FOUND;
    }

    /* 复位 sensor */
    if (priv->sensor.reset) {
        priv->sensor.reset(&priv->sensor);
    }

    assert(priv->sensor.get_window);
    priv->sensor.get_window(&priv->sensor, &priv->frame_width, &priv->frame_height);
    LOGI("Detected sensor frame size: %ux%u", priv->frame_width, priv->frame_height);

    assert(priv->sensor.get_pixformat);
    pixformat_t pixformat = priv->sensor.get_pixformat(&priv->sensor);
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        priv->pixel_format = LISA_CAMERA_PIXFMT_RGB565;
        break;
    case PIXFORMAT_RGB888:
        priv->pixel_format = LISA_CAMERA_PIXFMT_RGB888;
        break;
    case PIXFORMAT_YUV422:
        priv->pixel_format = LISA_CAMERA_PIXFMT_YUV422;
        break;
    case PIXFORMAT_YUV420:
        priv->pixel_format = LISA_CAMERA_PIXFMT_YUV420;
        break;
    case PIXFORMAT_GRAYSCALE:
        priv->pixel_format = LISA_CAMERA_PIXFMT_GRAY;
        break;
    case PIXFORMAT_JPEG:
        priv->pixel_format = LISA_CAMERA_PIXFMT_JPEG;
        break;
    default:
        priv->pixel_format = LISA_CAMERA_PIXFMT_RAW;
        break;
    }
    LOGI("Detected sensor pixel format: %d", pixformat);

    return LISA_DEVICE_OK;
}

/**
 * @brief 配置摄像头设备
 */
static int lisa_camera_setup_arcs(lisa_device_t *dev, const lisa_camera_config_t *config)
{
    int ret;

    if (!dev || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 复制配置 */
    memcpy(&priv->config, config, sizeof(lisa_camera_config_t));

    /* 设置默认硬件配置（如果用户未配置）*/
    if (priv->config.hw_config.pwdn_delay_us == 0) {
        priv->config.hw_config.pwdn_delay_us = 1000;  /* 默认 1ms */
    }
    if (priv->config.hw_config.xclk_delay_us == 0) {
        priv->config.hw_config.xclk_delay_us = 1000;  /* 默认 1ms */
    }
    if (priv->config.hw_config.i2c_dev== NULL) {
        LOGE("i2c_dev is NULL");
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 1. 初始化 PWDN 引脚 */
    ret = lisa_camera_pwdn_init(priv);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to init PWDN pin");
        return ret;
    }

    /* 2. 初始化时钟输出 */
    ret = lisa_camera_xclk_init(priv);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to init XCLK");
        return ret;
    }

    /* 3. Probe 并初始化 sensor (包含配置) */
    ret = lisa_camera_sensor_probe(priv);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to probe sensor");
        return ret;
    }

    /* 4. 初始化帧缓冲区 */
    uint32_t frame_size = priv->frame_width * priv->frame_height * 2;  /* 默认 RGB565/YUV422 */
    if (priv->pixel_format == LISA_CAMERA_PIXFMT_GRAY) {
        frame_size = priv->frame_width * priv->frame_height;
    }

    ret = lisa_camera_fb_init(priv, frame_size);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to init frame buffers");
        return ret;
    }

    priv->is_initialized = true;
    LOGI("Camera setup completed");

    return LISA_DEVICE_OK;
}

/**
 * @brief 启动摄像头
 */
static int lisa_camera_start_arcs(lisa_device_t *dev)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 启动 sensor */
    if (priv->sensor.start) {
        priv->sensor.start(&priv->sensor);
    }

    /* 启动总线捕获，使用内部回调管理帧缓冲区 */
    if (priv->bus_dev) {
        lisa_camera_bus_if_t *api = (lisa_camera_bus_if_t *)priv->bus_dev->api;
        api->start_capture(priv->bus_dev, lisa_camera_frame_done_callback,
                           lisa_camera_get_free_fb_wrapper,
                           lisa_camera_get_free_fb_from_isr_wrapper, dev);
    }

    priv->is_started = true;
    LOGI("Camera started");
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止摄像头
 */
static int lisa_camera_stop_arcs(lisa_device_t *dev)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 停止总线捕获 */
    if (priv->bus_dev) {
        lisa_camera_bus_if_t *api = (lisa_camera_bus_if_t *)priv->bus_dev->api;
        api->stop_capture(priv->bus_dev);
    }

    /* 停止 sensor */
    if (priv->sensor.stop) {
        priv->sensor.stop(&priv->sensor);
    }

    /* 重置帧缓冲区队列：清空 filled 队列，将所有帧放回 free 队列 */
    if (priv->queue_free && priv->queue_filled) {
        lisa_camera_fb_t *fb;

        /* 清空 filled 队列 */
        while (lisa_queue_pop(priv->queue_filled, (void *)&fb, sizeof(fb), LISA_NO_WAIT) == LISA_OK) {
            /* 帧已取出，稍后统一放入 free 队列 */
        }

        /* 清空 free 队列（可能有重复或残留）*/
        while (lisa_queue_pop(priv->queue_free, (void *)&fb, sizeof(fb), LISA_NO_WAIT) == LISA_OK) {
            /* 清空 */
        }

        /* 将所有帧缓冲区重新放入 free 队列 */
        for (uint8_t i = 0; i < priv->fb_count; i++) {
            lisa_camera_fb_t *fb_ptr = &priv->fb_list[i];
            lisa_queue_push(priv->queue_free, (void *)&fb_ptr, sizeof(fb_ptr), LISA_NO_WAIT);
        }

        LOGI("Frame buffer queues reset: %d buffers available", priv->fb_count);
    }

    priv->is_started = false;
    LOGI("Camera stopped");
    return LISA_DEVICE_OK;
}

/**
 * @brief 捕获一帧图像
 */
static int lisa_camera_capture_arcs(lisa_device_t *dev, lisa_camera_fb_t **fb)
{
    if (!dev || !fb) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!priv->is_started) {
        LOGE("Camera not started");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!priv->queue_filled) {
        LOGE("Frame queue not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 从已填充队列获取帧，等待最多 1000ms */
    lisa_camera_fb_t *frame = NULL;
    if (lisa_queue_pop(priv->queue_filled, (void *)&frame, sizeof(frame), 1000) != LISA_OK) {
        return LISA_DEVICE_ERR_TIMEOUT;
    }

    HAL_InvalidateDCache_by_Addr((uint32_t *)frame->buf, frame->len);
    *fb = frame;

    return LISA_DEVICE_OK;
}

/**
 * @brief 释放帧缓冲区
 */
static int lisa_camera_release_fb_arcs(lisa_device_t *dev, lisa_camera_fb_t *fb)
{
    if (!dev || !fb) {
        return LISA_DEVICE_ERR_INVALID;
    }

    arcs_camera_priv_t *priv = (arcs_camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!priv->queue_free) {
        LOGE("Frame queue not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 如果 filled 队列已满，则丢弃一个旧帧 */
    if (lisa_queue_full(priv->queue_filled)) {
        lisa_camera_fb_t *old_fb;
        if (lisa_queue_pop(priv->queue_filled, (void *)&old_fb, sizeof(old_fb), LISA_NO_WAIT) == LISA_OK) {
            lisa_queue_push(priv->queue_free, (void *)&old_fb, sizeof(old_fb), LISA_NO_WAIT);
        }
    }

    /* 将当前帧放回空闲队列 */
    if (lisa_queue_push(priv->queue_free, (void *)&fb, sizeof(fb), 10) != LISA_OK) {
        LOGE("Failed to return frame to free queue");
        return LISA_DEVICE_ERR_BUSY;
    }

    return LISA_DEVICE_OK;
}

static const lisa_camera_api_t arcs_camera_api = {
    .setup            = lisa_camera_setup_arcs,
    .start            = lisa_camera_start_arcs,
    .stop             = lisa_camera_stop_arcs,
    .capture          = lisa_camera_capture_arcs,
    .release_fb       = lisa_camera_release_fb_arcs,
    .get_capabilities = lisa_camera_get_capabilities_arcs,
    .attach_bus       = lisa_camera_attach_bus_arcs,
    .set_hmirror      = lisa_camera_set_hmirror_arcs,
    .set_vflip        = lisa_camera_set_vflip_arcs,
    .set_crop         = lisa_camera_set_crop_arcs,
    .get_framesize    = lisa_camera_get_framesize_arcs,
    .set_pixformat    = lisa_camera_set_pixformat_arcs,
    .set_reg          = lisa_camera_set_reg_arcs,
    .get_reg          = lisa_camera_get_reg_arcs,
    .set_callback     = lisa_camera_set_callback_arcs,
    .get_pixformat    = lisa_camera_get_pixformat_arcs,
};


static int lisa_camera_device_init(void)
{
    LOGD("camera device init");
    return LISA_DEVICE_OK;
}

LISA_DEVICE_REGISTER(camera, &arcs_camera_api, &arcs_camera_priv, NULL, lisa_camera_device_init, LISA_DEVICE_PRIORITY_NORMAL);