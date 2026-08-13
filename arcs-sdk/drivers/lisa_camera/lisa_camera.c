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
#if defined(CONFIG_SOC_VENUSA)
#include "Driver_DVP.h"
#else
#include "arcs_ap.h"
#endif
#include "lisa_mem.h"
#include <lisa_queue.h>
#include <lisa_time.h>
#include <lisa_typedef.h>

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif


#if CONFIG_LISA_CAMERA_SENSOR_GC032A
#include "gc032a.h"
#endif
#if CONFIG_LISA_CAMERA_SENSOR_GC0328
#include "gc0328.h"
#endif
#if CONFIG_LISA_CAMERA_SENSOR_BF3901
#include "bf3901.h"
#endif
#if CONFIG_LISA_CAMERA_SENSOR_TC6036
#include "tc6036.h"
#endif
#if CONFIG_LISA_CAMERA_SENSOR_SC030IOT
#include "sc030iot.h"
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
#if CONFIG_LISA_CAMERA_SENSOR_TC6036
    {CAMERA_TC6036, tc6036_detect, tc6036_init},
#endif
};

// ============================================================================
// Camera 驱动实现
// ============================================================================

typedef struct {
    sensor_t sensor;
    lisa_camera_capabilities_t capabilities;
    lisa_camera_pixel_format_t pixel_format;
    uint16_t frame_width;
    uint16_t frame_height;
    bool valid;
} camera_sensor_priv_t;

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
    lisa_camera_sensor_index_t current_sensor;
    camera_sensor_priv_t sensor_priv[LISA_CAMERA_SENSOR_MAX];
    lisa_camera_bus_config_t last_bus_config;
    bool has_bus_config;
    bool dual_camera_mode;
    uint32_t fb_slot_size;
} camera_priv_t;

static camera_priv_t camera_priv;

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
    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_fb_reinit(camera_priv_t *priv);

static int lisa_camera_fb_init(camera_priv_t *priv, uint32_t frame_size)
{
    uint8_t fb_count = priv->config.fb_count;
    uint32_t slot_size = (frame_size + 31U) & ~31U;
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
        /* 如果配置了外部 mem_pool，从外部内存地址分配 */
        if (priv->config.mem_pool != NULL) {
            /* 计算对齐后的偏移量 */
            uint32_t offset = i * slot_size;  /* 32字节对齐 */

            /* 检查是否超出内存池大小 */
            if (offset + frame_size > priv->config.mem_pool_size) {
                LOGE("Frame buffer %d exceeds mem_pool size (offset=%lu, size=%lu, pool_size=%lu)",
                     i, offset, frame_size, priv->config.mem_pool_size);
                return LISA_DEVICE_ERR_NO_MEM;
            }

            priv->fb_buf[i] = (uint8_t *)priv->config.mem_pool + offset;
            LOGD("Frame buffer %d allocated from external mem_pool: %p (offset=%lu)", i, priv->fb_buf[i], offset);
        } else {
            /* 使用系统默认内存分配 */
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
    priv->fb_slot_size = slot_size;
    if (priv->config.mem_pool != NULL) {
        LOGI("Frame buffers initialized: count=%d, size=%lu, slot=%lu, external mem_pool=%p",
             fb_count, frame_size, priv->fb_slot_size, priv->config.mem_pool);
    } else {
        LOGI("Frame buffers initialized: count=%d, size=%lu (system memory)", fb_count, frame_size);
    }

    return LISA_DEVICE_OK;
}

static void lisa_camera_reset_frame_queues(camera_priv_t *priv)
{
    if (!priv || !priv->queue_free || !priv->queue_filled) {
        return;
    }

    lisa_camera_fb_t *fb;
    while (lisa_queue_pop(priv->queue_filled, (void *)&fb, sizeof(fb), LISA_NO_WAIT) == LISA_OK) {
    }
    while (lisa_queue_pop(priv->queue_free, (void *)&fb, sizeof(fb), LISA_NO_WAIT) == LISA_OK) {
    }

    for (uint8_t i = 0; i < priv->fb_count; i++) {
        lisa_camera_fb_t *fb_ptr = &priv->fb_list[i];
        lisa_queue_push(priv->queue_free, (void *)&fb_ptr, sizeof(fb_ptr), LISA_NO_WAIT);
    }

    LOGI("Frame buffer queues reset: %d buffers available", priv->fb_count);
}

static int lisa_camera_refresh_fb_metadata(camera_priv_t *priv, uint32_t frame_size)
{
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (priv->fb_count == 0) {
        return lisa_camera_fb_init(priv, frame_size);
    }

    uint32_t slot_size = (frame_size + 31U) & ~31U;
    if (slot_size > priv->fb_slot_size) {
        if (priv->config.mem_pool != NULL) {
            uint32_t required = slot_size * priv->fb_count;
            if (required > priv->config.mem_pool_size) {
                LOGE("Frame buffers need %lu bytes, mem_pool only %lu", required, priv->config.mem_pool_size);
                return LISA_DEVICE_ERR_NO_MEM;
            }
        } else {
            return lisa_camera_fb_reinit(priv);
        }
        priv->fb_slot_size = slot_size;
    }

    for (uint8_t i = 0; i < priv->fb_count; i++) {
        if (priv->config.mem_pool != NULL) {
            priv->fb_buf[i] = (uint8_t *)priv->config.mem_pool + (i * priv->fb_slot_size);
        }
        priv->fb_list[i].buf = priv->fb_buf[i];
        priv->fb_list[i].len = frame_size;
        priv->fb_list[i].width = priv->frame_width;
        priv->fb_list[i].height = priv->frame_height;
        priv->fb_list[i].format = priv->pixel_format;
        priv->fb_list[i].timestamp = 0;
    }
    lisa_camera_reset_frame_queues(priv);
    LOGI("Frame buffers refreshed: count=%d, frame=%lu, slot=%lu", priv->fb_count, frame_size, priv->fb_slot_size);
    return LISA_DEVICE_OK;
}

/**
 * @brief 重新初始化帧缓冲区
 */
static int lisa_camera_fb_reinit(camera_priv_t *priv)
{
    uint32_t frame_size = priv->frame_width * priv->frame_height * 2;  /* 默认 RGB565/YUV422 */
    uint32_t slot_size;
    if (priv->pixel_format == LISA_CAMERA_PIXFMT_GRAY) {
        frame_size = priv->frame_width * priv->frame_height;
    }
    slot_size = (frame_size + 31U) & ~31U;

    /* 仅重新分配缓冲区内存，不销毁队列 */
    for (uint8_t i = 0; i < priv->fb_count; i++) {
        if (priv->fb_buf[i]) {
            /* 如果使用了外部 mem_pool，不需要释放旧内存（由外部管理） */
            if (priv->config.mem_pool == NULL) {
                lisa_mem_free(priv->fb_buf[i]);
            }
        }

        /* 分配新内存 */
        if (priv->config.mem_pool != NULL) {
            /* 从外部内存地址分配 */
            uint32_t offset = i * slot_size;  /* 32字节对齐 */

            /* 检查是否超出内存池大小 */
            if (offset + frame_size > priv->config.mem_pool_size) {
                LOGE("Frame buffer %d exceeds mem_pool size during reinit", i);
                return LISA_DEVICE_ERR_NO_MEM;
            }

            priv->fb_buf[i] = (uint8_t *)priv->config.mem_pool + offset;
        } else {
            /* 使用系统内存分配 */
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
        }

        priv->fb_list[i].buf = priv->fb_buf[i];
        priv->fb_list[i].len = frame_size;
        priv->fb_list[i].width = priv->frame_width;
        priv->fb_list[i].height = priv->frame_height;
    }

    priv->fb_slot_size = slot_size;
    lisa_camera_reset_frame_queues(priv);
    LOGI("Frame buffers re-initialized: count=%d, size=%lu", priv->fb_count, frame_size);

    return LISA_DEVICE_OK;
}

static void lisa_camera_fb_deinit(camera_priv_t *priv)
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
            /* 如果使用了外部 mem_pool，不需要释放（由外部管理） */
            if (priv->config.mem_pool == NULL) {
                lisa_mem_free(priv->fb_buf[i]);
            }
            priv->fb_buf[i] = NULL;
        }
    }
    priv->fb_count = 0;
}

static void lisa_camera_release_runtime(camera_priv_t *priv)
{
    if (priv == NULL) {
        return;
    }

    if (priv->is_started) {
        if (priv->bus_dev) {
            lisa_camera_bus_if_t *api = (lisa_camera_bus_if_t *)priv->bus_dev->api;
            if ((api != NULL) && (api->stop_capture != NULL)) {
                api->stop_capture(priv->bus_dev);
            }
        }

        if (priv->sensor.stop) {
            priv->sensor.stop(&priv->sensor);
        }
    }

    lisa_camera_fb_deinit(priv);
    priv->bus_dev = NULL;
    priv->callback = NULL;
    priv->callback_user_data = NULL;
    memset(&priv->sensor, 0, sizeof(priv->sensor));
    memset(&priv->capabilities, 0, sizeof(priv->capabilities));
    priv->pixel_format = LISA_CAMERA_PIXFMT_RAW;
    priv->frame_width = 0;
    priv->frame_height = 0;
    priv->current_sensor = LISA_CAMERA_SENSOR_0;
    memset(priv->sensor_priv, 0, sizeof(priv->sensor_priv));
    memset(&priv->last_bus_config, 0, sizeof(priv->last_bus_config));
    priv->has_bus_config = false;
    priv->dual_camera_mode = false;
    priv->fb_slot_size = 0;
    priv->is_started = false;
    priv->is_initialized = false;
}

/**
 * @brief 附加总线配置
 */
static int lisa_camera_attach_bus_ops(lisa_device_t *dev, const lisa_camera_bus_config_t *bus_config)
{
    if (!dev || !bus_config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
    if (ret == LISA_DEVICE_OK) {
        memcpy(&priv->last_bus_config, bus_config, sizeof(priv->last_bus_config));
        priv->has_bus_config = true;
    }

    return ret;
}

static void lisa_camera_sync_active_sensor_cache(camera_priv_t *priv)
{
    if (!priv || priv->current_sensor >= LISA_CAMERA_SENSOR_MAX) {
        return;
    }

    camera_sensor_priv_t *cache = &priv->sensor_priv[priv->current_sensor];
    cache->sensor = priv->sensor;
    cache->capabilities = priv->capabilities;
    cache->pixel_format = priv->pixel_format;
    cache->frame_width = priv->frame_width;
    cache->frame_height = priv->frame_height;
    cache->valid = true;
}

/**
 * @brief 设置像素格式
 */
static int lisa_camera_set_pixformat_ops(lisa_device_t *dev, lisa_camera_pixel_format_t format)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

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
        int ret = priv->sensor.set_pixformat(&priv->sensor, pixformat);
        if (ret != 0) {
            return ret;
        }

        /* sensor 已成功切换格式后再提交 pixel_format，失败则回滚 */
        lisa_camera_pixel_format_t old_format = priv->pixel_format;
        priv->pixel_format = format;
        lisa_camera_sync_active_sensor_cache(priv);

        uint32_t frame_size = priv->frame_width * priv->frame_height * 2U;
        if (format == LISA_CAMERA_PIXFMT_GRAY) {
            frame_size = priv->frame_width * priv->frame_height;
        }
        ret = lisa_camera_refresh_fb_metadata(priv, frame_size);
        if (ret != LISA_DEVICE_OK) {
            priv->pixel_format = old_format;
            lisa_camera_sync_active_sensor_cache(priv);
            return ret;
        }
        return LISA_DEVICE_OK;
    }

    /* 无 sensor 下发接口时（纯软件路径）直接提交 */
    priv->pixel_format = format;
    return LISA_DEVICE_OK;
}


static lisa_camera_pixel_format_t lisa_camera_get_pixformat_ops(lisa_device_t *dev)
{
    if (!dev) {
        return LISA_CAMERA_PIXFMT_RAW;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_set_hmirror_ops(lisa_device_t *dev, bool enable)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
        lisa_camera_sync_active_sensor_cache(priv);
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置垂直翻转
 */
static int lisa_camera_set_vflip_ops(lisa_device_t *dev, bool enable)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
        lisa_camera_sync_active_sensor_cache(priv);
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
    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_get_framesize_ops(lisa_device_t *dev, uint16_t *width, uint16_t *height)
{
    if (!dev || !width || !height) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_get_capabilities_ops(lisa_device_t *dev, lisa_camera_capabilities_t *caps)
{
    if (!dev || !caps) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_set_crop_ops(lisa_device_t *dev, const lisa_camera_crop_t *crop)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (crop && priv->sensor.set_window) {
        int ret = priv->sensor.set_window(&priv->sensor, crop->x, crop->y,
                                          crop->width, crop->height);
        if (ret != 0) {
            return ret;
        }
        priv->frame_width = crop->width;
        priv->frame_height = crop->height;
        lisa_camera_sync_active_sensor_cache(priv);
        return lisa_camera_refresh_fb_metadata(priv, priv->frame_width * priv->frame_height * 2U);
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief 设置硬件跳采 (subsampling)
 */
static int lisa_camera_set_subsample_ops(lisa_device_t *dev, uint8_t row_ratio, uint8_t col_ratio)
{
    if (!dev || row_ratio == 0 || col_ratio == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!priv->sensor.set_subsample) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    int ret = priv->sensor.set_subsample(&priv->sensor, row_ratio, col_ratio);
    /* 跳采后传感器输出为 crop/ratio，同步缩小缓存的帧尺寸并按新尺寸重建帧
     * 缓冲（帧缓冲在 setup 时已按全画幅分配，不重建会错配/浪费内存）。*/
    if (ret == LISA_DEVICE_OK) {
        priv->frame_width /= col_ratio;
        priv->frame_height /= row_ratio;
        lisa_camera_sync_active_sensor_cache(priv);
        LOGI("Subsample %ux%u: frame size -> %ux%u", col_ratio, row_ratio,
             priv->frame_width, priv->frame_height);
        ret = lisa_camera_fb_reinit(priv);
    }
    return ret;
}

/**
 * @brief 设置寄存器
 */
static int lisa_camera_set_reg_ops(lisa_device_t *dev, int reg, int mask, int value)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_get_reg_ops(lisa_device_t *dev, int reg, int mask)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_set_callback_ops(lisa_device_t *dev, lisa_camera_frame_callback_t callback, void *user_data)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_pwdn_init(camera_priv_t *priv)
{
    lisa_camera_hw_config_t *hw = &priv->config.hw_config;
    int ret;

    if (hw->multiplex_camera) {
        for (uint8_t i = 0; i < LISA_CAMERA_SENSOR_MAX; i++) {
            lisa_camera_sensor_pwdn_t *pwdn = &hw->sensor_pwdn[i];
            if (pwdn->pwdn_gpio_dev == NULL) {
                continue;
            }
            lisa_gpio_configure(pwdn->pwdn_gpio_dev, pwdn->pwdn_pin,
                                pwdn->pwdn_active_level ? LISA_GPIO_CONFIG_OUTPUT_LOW : LISA_GPIO_CONFIG_OUTPUT_HIGH);
        }
        SysTick_Delay_Us(hw->pwdn_delay_us);
        LOGI("Multiplex camera PWDN pins initialized");
        return LISA_DEVICE_OK;
    }

    if (hw->pwdn_gpio_dev == NULL) {
        LOGW("PWDN GPIO device not configured, skipping");
        return LISA_DEVICE_OK;
    }

    ret = lisa_gpio_configure(hw->pwdn_gpio_dev, hw->pwdn_pin,
                              hw->pwdn_inactive_level ? LISA_GPIO_CONFIG_OUTPUT_HIGH
                                                      : LISA_GPIO_CONFIG_OUTPUT_LOW);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to configure PWDN pin %d level %d: %d",
             hw->pwdn_pin, hw->pwdn_inactive_level, ret);
        return ret;
    }

    /* 延时等待 sensor 稳定 */
    SysTick_Delay_Us(hw->pwdn_delay_us);

    LOGI("PWDN pin initialized (pin=%d, level=%d)",
         hw->pwdn_pin, hw->pwdn_inactive_level);
    return LISA_DEVICE_OK;
}

/**
 * @brief 初始化 RESET 引脚并执行硬件复位
 */
static int lisa_camera_reset_init(camera_priv_t *priv)
{
    lisa_camera_hw_config_t *hw = &priv->config.hw_config;

    if (hw->reset_gpio_dev == NULL) {
        LOGW("RESET GPIO device not configured, skipping");
        return LISA_DEVICE_OK;
    }

    uint32_t active_cfg = hw->reset_active_level ?
                          LISA_GPIO_CONFIG_OUTPUT_HIGH :
                          LISA_GPIO_CONFIG_OUTPUT_LOW;
    uint8_t inactive_level = hw->reset_active_level ? LISA_GPIO_LOW : LISA_GPIO_HIGH;

    lisa_gpio_configure(hw->reset_gpio_dev, hw->reset_pin, active_cfg);
    SysTick_Delay_Us(hw->reset_delay_us);

    lisa_gpio_write_pin(hw->reset_gpio_dev, hw->reset_pin, inactive_level);
    SysTick_Delay_Us(hw->reset_delay_us);

    LOGI("RESET pin initialized (pin=%d, active=%d)", hw->reset_pin, hw->reset_active_level);
    return LISA_DEVICE_OK;
}

static int lisa_camera_select_sensor_pwdn(camera_priv_t *priv, lisa_camera_sensor_index_t index)
{
    lisa_camera_hw_config_t *hw = &priv->config.hw_config;

    if (!hw->multiplex_camera) {
        return LISA_DEVICE_OK;
    }
    if (index >= LISA_CAMERA_SENSOR_MAX) {
        return LISA_DEVICE_ERR_INVALID;
    }

    for (uint8_t i = 0; i < LISA_CAMERA_SENSOR_MAX; i++) {
        lisa_camera_sensor_pwdn_t *pwdn = &hw->sensor_pwdn[i];
        if (pwdn->pwdn_gpio_dev == NULL) {
            continue;
        }

        uint8_t level = (i == (uint8_t)index) ? (pwdn->pwdn_active_level ? 0U : 1U) : pwdn->pwdn_active_level;
        lisa_gpio_write_pin(pwdn->pwdn_gpio_dev, pwdn->pwdn_pin, level);
    }

    SysTick_Delay_Us(hw->pwdn_delay_us);
    return LISA_DEVICE_OK;
}


/**
 * @brief 初始化时钟输出
 * TODO: 待优化, 考虑将XCLK相关的初始化，不依赖hal, 直接调用bus_api
 *
 */
static int lisa_camera_xclk_init(camera_priv_t *priv)
{
    uint32_t xclk_freq = priv->config.xclk_freq_hz;

#if defined(CONFIG_SOC_VENUSA)
    if (xclk_freq > 0) {
        LOGI("Enabling XCLK output: %d Hz", xclk_freq);
        /* Venusa routes DVP MCLK through the DVP alternate function. */
        IOMuxManager_PinConfigure(priv->config.hw_config.mclk_pad, priv->config.hw_config.mclk_pin, CSK_IOMUX_FUNC_ALTER13);

        /* 使能视频时钟 */
        int32_t ret = DVP_EnableClockout(xclk_freq);
        if (ret != CSK_DRIVER_OK) {
            LOGE("DVP_EnableClockout failed: %ld", (long)ret);
            return LISA_DEVICE_ERR_IO;
        }
    }
#else
    if (xclk_freq > 0) {
        LOGI("Enabling XCLK output: %d Hz", xclk_freq);
        IOMuxManager_PinConfigure(priv->config.hw_config.mclk_pad, priv->config.hw_config.mclk_pin, CSK_IOMUX_FUNC_ALTER16);
        /* 使能视频时钟 */
        IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;
        IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;
        /* 配置 DVP 时钟输出 */
        DVP_EnableClockout(DVP0(), xclk_freq);
    }
#endif
    /* 时钟输出后延时 */
    SysTick_Delay_Us(priv->config.hw_config.xclk_delay_us);

    return LISA_DEVICE_OK;
}

/**
 * @brief Probe 并初始化 sensor
 */
static int lisa_camera_sensor_probe(camera_priv_t *priv)
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

static int lisa_camera_probe_selected_sensor(camera_priv_t *priv, lisa_camera_sensor_index_t index)
{
    int ret = -1;
    uint32_t i;
    camera_sensor_info_t *sensor_info;
    uint32_t sensor_count = sizeof(camera_sensors) / sizeof(camera_sensors[0]);
    camera_sensor_priv_t *cache;

    if (!priv || index >= LISA_CAMERA_SENSOR_MAX) {
        return LISA_DEVICE_ERR_INVALID;
    }

    cache = &priv->sensor_priv[index];
    memset(cache, 0, sizeof(*cache));
    ret = lisa_camera_select_sensor_pwdn(priv, index);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    for (i = 0; i < sensor_count; i++) {
        sensor_info = camera_sensor_get_info(camera_sensors[i].model);
        if (sensor_info != NULL) {
            if (camera_sensors[i].detect(sensor_info->sccb_addr, &cache->sensor.id)) {
                cache->sensor.slv_addr = sensor_info->sccb_addr;
                cache->sensor.xclk_freq_hz = priv->config.xclk_freq_hz;
                camera_sensors[i].init(&cache->sensor);
                cache->capabilities.max_height = sensor_info->max_height;
                cache->capabilities.max_width = sensor_info->max_width;
                cache->capabilities.supported_formats =
                    convert_sensor_pixfmt_mask_to_lisa(sensor_info->supported_formats);
                cache->valid = true;
                LOGI("Sensor[%u] detected: PID=0x%04X, addr=0x%02X",
                     (unsigned int)index, cache->sensor.id.PID, cache->sensor.slv_addr);
                return LISA_DEVICE_OK;
            }
        }
    }

    LOGE("No camera sensor detected on index %u", (unsigned int)index);
    return LISA_DEVICE_ERR_NOT_FOUND;
}

static int lisa_camera_sensor_probe_all(camera_priv_t *priv)
{
    int ret;

    ret = sensor_twi_init(priv->config.hw_config.i2c_dev);
    if (ret != 0) {
        LOGE("Failed to init sensor I2C");
        return LISA_DEVICE_ERR_IO;
    }

    for (uint8_t i = 0; i < LISA_CAMERA_SENSOR_MAX; i++) {
        ret = lisa_camera_probe_selected_sensor(priv, (lisa_camera_sensor_index_t)i);
        if (ret != LISA_DEVICE_OK) {
            return ret;
        }
    }

    return LISA_DEVICE_OK;
}

static int lisa_camera_sensor_activate(camera_priv_t *priv, lisa_camera_sensor_index_t index)
{
    int ret;
    camera_sensor_priv_t *cache;

    if (!priv || index >= LISA_CAMERA_SENSOR_MAX) {
        return LISA_DEVICE_ERR_INVALID;
    }

    cache = &priv->sensor_priv[index];
    if (!cache->valid) {
        return LISA_DEVICE_ERR_NOT_FOUND;
    }

    ret = lisa_camera_select_sensor_pwdn(priv, index);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    priv->sensor = cache->sensor;
    priv->capabilities = cache->capabilities;
    priv->current_sensor = index;

    if (priv->sensor.reset) {
        priv->sensor.reset(&priv->sensor);
    }
    if (priv->sensor.init_status) {
        priv->sensor.init_status(&priv->sensor);
    }

    if (priv->sensor.get_window) {
        priv->sensor.get_window(&priv->sensor, &priv->frame_width, &priv->frame_height);
    } else {
        priv->frame_width = cache->frame_width;
        priv->frame_height = cache->frame_height;
    }

    if (priv->sensor.get_pixformat) {
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
    } else {
        priv->pixel_format = cache->pixel_format;
    }

    lisa_camera_sync_active_sensor_cache(priv);
    LOGI("Sensor[%u] activated: frame=%ux%u fmt=%d",
         (unsigned int)index, priv->frame_width, priv->frame_height, priv->pixel_format);
    return LISA_DEVICE_OK;
}

/**
 * @brief 配置摄像头设备
 */
static int lisa_camera_setup_ops(lisa_device_t *dev, const lisa_camera_config_t *config)
{
    int ret;

    if (!dev || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (priv->is_initialized) {
        LOGW("reconfigure existing camera instance");
        lisa_camera_release_runtime(priv);
    }

    /* 复制配置 */
    memcpy(&priv->config, config, sizeof(lisa_camera_config_t));
    priv->dual_camera_mode = config->hw_config.multiplex_camera;
    priv->current_sensor = LISA_CAMERA_SENSOR_0;

    /* 设置默认硬件配置（如果用户未配置）*/
    if (priv->config.hw_config.pwdn_delay_us == 0) {
        priv->config.hw_config.pwdn_delay_us = 1000;  /* 默认 1ms */
    }
    if (priv->config.hw_config.xclk_delay_us == 0) {
        priv->config.hw_config.xclk_delay_us = 1000;  /* 默认 1ms */
    }
    if (priv->config.hw_config.reset_delay_us == 0) {
        priv->config.hw_config.reset_delay_us = 10000;  /* 默认 10ms */
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

    /* 3. 硬件复位 sensor（如果配置了 RESET 引脚） */
    ret = lisa_camera_reset_init(priv);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to init RESET pin");
        return ret;
    }

    /* 4. Probe 并初始化 sensor (包含配置) */
    if (priv->dual_camera_mode) {
        ret = lisa_camera_sensor_probe_all(priv);
        if (ret != LISA_DEVICE_OK) {
            LOGE("Failed to probe sensors");
            return ret;
        }
        ret = lisa_camera_sensor_activate(priv, LISA_CAMERA_SENSOR_0);
        if (ret != LISA_DEVICE_OK) {
            LOGE("Failed to activate default sensor");
            return ret;
        }
    } else {
        ret = lisa_camera_sensor_probe(priv);
        if (ret != LISA_DEVICE_OK) {
            LOGE("Failed to probe sensor");
            return ret;
        }
        lisa_camera_sync_active_sensor_cache(priv);
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
static int lisa_camera_start_ops(lisa_device_t *dev)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
        if (api && api->start_capture) {
            int ret = api->start_capture(priv->bus_dev, lisa_camera_frame_done_callback,
                                         lisa_camera_get_free_fb_wrapper,
                                         lisa_camera_get_free_fb_from_isr_wrapper, dev);
            if (ret != 0) {
                LOGE("bus start_capture failed: %d", ret);
                /* 回滚已启动的 sensor，保持 is_started=false */
                if (priv->sensor.stop) {
                    priv->sensor.stop(&priv->sensor);
                }
                return ret;
            }
        }
    }

    priv->is_started = true;
    LOGI("Camera started");
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止摄像头
 */
static int lisa_camera_stop_ops(lisa_device_t *dev)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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

    lisa_camera_reset_frame_queues(priv);

    priv->is_started = false;
    LOGI("Camera stopped");
    return LISA_DEVICE_OK;
}

/**
 * @brief 捕获一帧图像
 */
static int lisa_camera_capture_ops(lisa_device_t *dev, lisa_camera_fb_t **fb)
{
    if (!dev || !fb) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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
static int lisa_camera_release_fb_ops(lisa_device_t *dev, lisa_camera_fb_t *fb)
{
    if (!dev || !fb) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
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

static int lisa_camera_switch_sensor_ops(lisa_device_t *dev, lisa_camera_sensor_index_t index)
{
    int ret;

    if (!dev || index >= LISA_CAMERA_SENSOR_MAX) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!priv->dual_camera_mode) {
        return (index == LISA_CAMERA_SENSOR_0) ? LISA_DEVICE_OK : LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    if (priv->current_sensor == index) {
        return LISA_DEVICE_OK;
    }

    if (priv->is_started) {
        if (priv->bus_dev) {
            lisa_camera_bus_if_t *api = (lisa_camera_bus_if_t *)priv->bus_dev->api;
            if (api && api->stop_capture) {
                api->stop_capture(priv->bus_dev);
            }
        }
        if (priv->sensor.stop) {
            priv->sensor.stop(&priv->sensor);
        }
        priv->is_started = false;
    }

    lisa_camera_reset_frame_queues(priv);
    lisa_camera_sync_active_sensor_cache(priv);

    ret = lisa_camera_sensor_activate(priv, index);
    if (ret != LISA_DEVICE_OK) {
        LOGE("activate sensor failed: %d", ret);
        return ret;
    }

    uint32_t frame_size = priv->frame_width * priv->frame_height * 2U;
    if (priv->pixel_format == LISA_CAMERA_PIXFMT_GRAY) {
        frame_size = priv->frame_width * priv->frame_height;
    }
    ret = lisa_camera_refresh_fb_metadata(priv, frame_size);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    LOGI("Camera sensor switched to %u", (unsigned int)index);
    return LISA_DEVICE_OK;
}

static int lisa_camera_get_current_sensor_ops(lisa_device_t *dev, lisa_camera_sensor_index_t *index)
{
    if (!dev || !index) {
        return LISA_DEVICE_ERR_INVALID;
    }

    camera_priv_t *priv = (camera_priv_t *)dev->priv_data;
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!priv->is_initialized) {
        LOGE("Camera not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    *index = priv->current_sensor;
    return LISA_DEVICE_OK;
}

static const lisa_camera_api_t camera_api = {
    .setup              = lisa_camera_setup_ops,
    .start              = lisa_camera_start_ops,
    .stop               = lisa_camera_stop_ops,
    .capture            = lisa_camera_capture_ops,
    .release_fb         = lisa_camera_release_fb_ops,
    .get_capabilities   = lisa_camera_get_capabilities_ops,
    .attach_bus         = lisa_camera_attach_bus_ops,
    .set_hmirror        = lisa_camera_set_hmirror_ops,
    .set_vflip          = lisa_camera_set_vflip_ops,
    .set_crop           = lisa_camera_set_crop_ops,
    .set_subsample      = lisa_camera_set_subsample_ops,
    .get_framesize      = lisa_camera_get_framesize_ops,
    .set_pixformat      = lisa_camera_set_pixformat_ops,
    .set_reg            = lisa_camera_set_reg_ops,
    .get_reg            = lisa_camera_get_reg_ops,
    .set_callback       = lisa_camera_set_callback_ops,
    .get_pixformat      = lisa_camera_get_pixformat_ops,
    .switch_sensor      = lisa_camera_switch_sensor_ops,
    .get_current_sensor = lisa_camera_get_current_sensor_ops,
};


static int lisa_camera_device_init(void)
{
    LOGD("camera device init");
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止并释放 camera 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。camera 的运行期资源（sensor / bus / 帧缓冲 /
 * 队列）都在应用层调用 setup_ops 时按需申请，因此这里直接复用
 * lisa_camera_release_runtime() 完成停采集、停 sensor、释放帧缓冲与队列；随后
 * 整体清零 camera_priv，回到 lisa_camera_device_init 之前的上电初值。
 *
 * 约定：调用方需保证此时采集已停止、无并发业务在使用本设备。
 */
static int lisa_camera_device_deinit(void)
{
    lisa_camera_release_runtime(&camera_priv);
    memset(&camera_priv, 0, sizeof(camera_priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(camera) 释放全部软硬件资源，唤醒后在 PM after_wake 回调中
 * 调 lisa_device_reinit(camera) 并重新 setup。因此 prepare_suspend / resume_restore
 * 不需要（二者运行于 HAL 临界区无法做重活，能力已被 destroy/reinit 覆盖）。
 *
 * 仅保留 check_idle：在 AUTO_LIGHT_SLEEP 策略下，采集运行中（DVP/SPI DMA 持续搬运
 * 帧数据）阻止系统自动进入轻睡眠。只读 priv 运行标记，不取锁 / 不读 HAL。
 */
static int32_t lisa_camera_pm_check_idle(void *ctx)
{
    camera_priv_t *priv = (camera_priv_t *)ctx;
    if (priv == NULL) {
        return 1; /* 上下文异常时允许睡眠，不阻塞整机 */
    }
    if (priv->is_started) {
        return 0;
    }
    return 1;
}

static const lisa_pm_system_ops_t lisa_camera_pm_ops = {
    .check_idle      = lisa_camera_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif /* CONFIG_LISA_PM */

LISA_DEVICE_REGISTER_DEINIT(camera, &camera_api, &camera_priv, NULL, lisa_camera_device_init,
                            lisa_camera_device_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(camera, &lisa_camera_pm_ops, NULL, &camera_priv);
#endif
