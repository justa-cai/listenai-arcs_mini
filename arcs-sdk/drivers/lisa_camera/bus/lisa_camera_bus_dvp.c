#include <stdint.h>
#include <string.h>

#include "lisa_camera.h"

#include "lisa_camera_bus.h"
#include "lisa_dvp.h"
#include "board.h"

#define TAG "camera_bus_dvp"
#include "lisa_log.h"

/* 定义是否使用 PingPong DMA 模式，默认使用普通模式 */
// #define DVP_USE_PINGPONG_DMA

/**
 * @brief 将 lisa_camera 像素格式转换为 DVP 输入格式
 */
static inline lisa_dvp_input_format_t convert_to_dvp_pixel_format(lisa_camera_pixel_format_t format)
{
    switch (format) {
    case LISA_CAMERA_PIXFMT_GRAY:
        return LISA_DVP_INPUT_FORM_LUMINA_8BIT;
    case LISA_CAMERA_PIXFMT_RGB565:
    case LISA_CAMERA_PIXFMT_YUV422:
    default:
        return LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR;
    }
}

/* DVP 驱动上下文结构体 */
typedef struct {
    lisa_device_t *dvp_dev;                              /* DVP 设备句柄 */
    uint8_t gpdma_ch;                                    /* DMA 通道 */
    lisa_camera_frame_callback_t callback;               /* 帧完成回调 */
    lisa_camera_get_free_fb_t get_free_fb;               /* 获取空闲帧回调 */
    lisa_camera_get_free_fb_from_isr_t get_free_fb_isr;  /* 获取空闲帧回调(ISR) */
    void *user_data;                                     /* 用户数据 */
    volatile uint8_t stop_flag;                          /* 停止标志 */
    lisa_camera_fb_t *ping_fb;                           /* Ping 缓冲区 */
    lisa_camera_fb_t *pong_fb;                           /* Pong 缓冲区 */
    lisa_camera_fb_t *current_fb;                        /* 当前帧缓冲区 */
} camera_bus_dvp_priv_t;

static camera_bus_dvp_priv_t camera_bus_dvp_priv = {
    .dvp_dev = NULL,
    .gpdma_ch = 0,
    .callback = NULL,
    .get_free_fb = NULL,
    .get_free_fb_isr = NULL,
    .user_data = NULL,
    .stop_flag = 1,
#ifdef DVP_USE_PINGPONG_DMA
    .ping_fb = NULL,
    .pong_fb = NULL,
#else
    .current_fb = NULL,
#endif
};

static void dvp_event_callback(lisa_dvp_event_t event, void *user_data)
{
    camera_bus_dvp_priv_t *priv = (camera_bus_dvp_priv_t *)user_data;
    if (priv->stop_flag) {
        return;
    }

    LOGI("%s: event:%d", __func__, event);
    lisa_camera_fb_t *completed_fb = NULL;
    lisa_camera_fb_t *next_fb = NULL;

    if (event & LISA_DVP_EVENT_PING_DONE) {
        /* Ping 块完成 */
        completed_fb = priv->ping_fb;

        /* 通过回调获取新的帧缓冲区替换 Ping */
        if (priv->get_free_fb_isr) {
            next_fb = priv->get_free_fb_isr(priv->user_data);
            if (next_fb) {
                /* 有空闲缓冲区,使用新缓冲区替换 Ping */
                priv->ping_fb = next_fb;
                /* 重新加载 Ping 缓冲区 */
                lisa_dvp_reload_pingpong(priv->dvp_dev, next_fb->buf);
            } else {
                /* 没有空闲缓冲区,复用当前 Ping 缓冲区继续接收,丢弃本帧数据 */
                LOGW("No free fb for ping reload, reuse current buffer and drop frame");
                lisa_dvp_reload_pingpong(priv->dvp_dev, priv->ping_fb->buf);
                completed_fb = NULL;  /* 不通知上层,丢弃本帧 */
            }
        }
    } else if (event & LISA_DVP_EVENT_PONG_DONE) {
        /* Pong 块完成 */
        completed_fb = priv->pong_fb;

        /* 通过回调获取新的帧缓冲区替换 Pong */
        if (priv->get_free_fb_isr) {
            next_fb = priv->get_free_fb_isr(priv->user_data);
            if (next_fb) {
                /* 有空闲缓冲区,使用新缓冲区替换 Pong */
                priv->pong_fb = next_fb;
                /* 重新加载 Pong 缓冲区 */
                lisa_dvp_reload_pingpong(priv->dvp_dev, next_fb->buf);
            } else {
                /* 没有空闲缓冲区,复用当前 Pong 缓冲区继续接收,丢弃本帧数据 */
                LOGW("No free fb for pong reload, reuse current buffer and drop frame");
                lisa_dvp_reload_pingpong(priv->dvp_dev, priv->pong_fb->buf);
                completed_fb = NULL;  /* 不通知上层,丢弃本帧 */
            }
        }
    }
    else if (event & LISA_DVP_EVENT_DONE) {
        /* 普通传输完成 */
        completed_fb = priv->current_fb;

        /* 通过回调获取下一个空闲帧缓冲区并继续接收 */
        if (priv->get_free_fb_isr) {
            next_fb = priv->get_free_fb_isr(priv->user_data);
            if (next_fb) {
                /* 有空闲缓冲区,使用新缓冲区继续接收 */
                priv->current_fb = next_fb;
                lisa_dvp_reload(priv->dvp_dev, priv->current_fb->buf, priv->current_fb->len);
            } else {
                /* 没有空闲缓冲区,复用当前缓冲区继续接收,丢弃本帧数据 */
                LOGW("No free frame buffer available, reuse current buffer and drop frame");
                lisa_dvp_reload(priv->dvp_dev, priv->current_fb->buf, priv->current_fb->len);
                completed_fb = NULL;  /* 不通知上层,丢弃本帧 */
            }
        }
    }

    /* 通知上层帧已完成 (仅当成功获取到新缓冲区时才交付数据) */
    if (completed_fb && priv->callback) {
        priv->callback(completed_fb, priv->user_data);
    }
}

static int lisa_camera_bus_dvp_init(lisa_device_t *dev, const lisa_camera_bus_config_t *bus_config)
{
    int32_t ret = 0;
    const lisa_camera_bus_dvp_config_t *dvp_config = &bus_config->config.dvp;
    camera_bus_dvp_priv_t *priv = (camera_bus_dvp_priv_t *)dev->priv_data;

    priv->gpdma_ch = bus_config->dma_channel;
    priv->dvp_dev = dvp_config->dvp_dev;

    lisa_dvp_config_t dvp_cfg = {
        .dvp_hal_config = {
            .frame_width    = bus_config->width,
            .frame_height   = bus_config->height,
            .pixel_offset   = dvp_config->pixel_offset,
            .line_offset    = dvp_config->line_offset,
            .input_format   = convert_to_dvp_pixel_format(bus_config->pixel_format),
            .pclk_polarity  = (dvp_config->pclk_polarity == 1) ? LISA_DVP_POL_RISING : LISA_DVP_POL_FALLING,
            .vsync_polarity = (dvp_config->vsync_polarity == 1) ? LISA_DVP_POL_RISING : LISA_DVP_POL_FALLING,
            .hsync_polarity = (dvp_config->hsync_polarity == 1) ? LISA_DVP_POL_RISING : LISA_DVP_POL_FALLING,
            .data_align     = (dvp_config->data_align == 1) ? LISA_DVP_DATA_ALIGN_LEFT : LISA_DVP_DATA_ALIGN_RIGHT,
        },
        .gpdma_ch = bus_config->dma_channel,
    };

    ret = lisa_dvp_setup(priv->dvp_dev, &dvp_cfg, dvp_event_callback, priv);
    if (ret != 0) {
        LOGE("lisa_dvp_setup failed %d", ret);
        return -1;
    }

    ret = lisa_dvp_enable_clockout(priv->dvp_dev, dvp_config->dvp_freq);
    if (ret != 0) {
        LOGE("lisa_dvp_enable_clockout failed %d", ret);
        return -1;
    }

    return 0;
}

static int lisa_camera_bus_dvp_start_capture(lisa_device_t *dev, lisa_camera_frame_callback_t callback,
                                              lisa_camera_get_free_fb_t get_free_fb,
                                              lisa_camera_get_free_fb_from_isr_t get_free_fb_from_isr,
                                              void *data)
{
    int32_t ret = 0;
    camera_bus_dvp_priv_t *priv = (camera_bus_dvp_priv_t *)dev->priv_data;

    if (!priv->stop_flag) {
        LOGW("camera dvp already start.");
        return 0;
    }

    /* 保存回调函数 */
    priv->callback = callback;
    priv->get_free_fb = get_free_fb;
    priv->get_free_fb_isr = get_free_fb_from_isr;
    priv->user_data = data;

#ifdef DVP_USE_PINGPONG_DMA
    /* PingPong 模式：通过回调获取两个缓冲区 */
    priv->ping_fb = get_free_fb ? get_free_fb(data) : NULL;
    if (!priv->ping_fb) {
        LOGE("No free frame buffer for ping");
        return -1;
    }

    priv->pong_fb = get_free_fb ? get_free_fb(data) : NULL;
    if (!priv->pong_fb) {
        LOGE("No free frame buffer for pong");
        lisa_camera_release_fb(dev, priv->ping_fb);
        priv->ping_fb = NULL;
        return -1;
    }

    priv->stop_flag = 0;

    /* 启动 PingPong DVP 传输 */
    ret = lisa_dvp_start_pingpong(priv->dvp_dev, priv->ping_fb->buf, priv->pong_fb->buf, priv->ping_fb->len);
    if (ret != 0) {
        LOGE("lisa_dvp_start_pingpong failed %d", ret);
        priv->stop_flag = 1;
        return -1;
    }

    LOGI("DVP PingPong capture started");
#else
    /* 普通模式：通过回调获取一个缓冲区 */
    priv->current_fb = get_free_fb ? get_free_fb(data) : NULL;
    if (!priv->current_fb) {
        LOGE("No free frame buffer available");
        return -1;
    }

    priv->stop_flag = 0;

    /* 启动普通 DVP 传输 */
    ret = lisa_dvp_start(priv->dvp_dev, priv->current_fb->buf, priv->current_fb->len);
    if (ret != 0) {
        LOGE("lisa_dvp_start failed %d", ret);
        priv->stop_flag = 1;
        return -1;
    }

    LOGI("DVP Normal capture started");
#endif

    return 0;
}

static int lisa_camera_bus_dvp_stop_capture(lisa_device_t *dev)
{
    camera_bus_dvp_priv_t *priv = (camera_bus_dvp_priv_t *)dev->priv_data;

    if (priv->stop_flag) {
        LOGW("camera dvp already stop.");
        return 0;
    }
    priv->stop_flag = 1;

    lisa_dvp_stop(priv->dvp_dev);

#ifdef DVP_USE_PINGPONG_DMA
    /* 释放 Ping 缓冲区 */
    if (priv->ping_fb != NULL) {
        lisa_camera_release_fb(dev, priv->ping_fb);
        priv->ping_fb = NULL;
    }

    /* 释放 Pong 缓冲区 */
    if (priv->pong_fb != NULL) {
        lisa_camera_release_fb(dev, priv->pong_fb);
        priv->pong_fb = NULL;
    }

    LOGI("DVP PingPong capture stopped");
#else
    /* 释放当前帧缓冲区 */
    if (priv->current_fb != NULL) {
        lisa_camera_release_fb(dev, priv->current_fb);
        priv->current_fb = NULL;
    }

    LOGI("DVP Normal capture stopped");
#endif

    return 0;
}

const lisa_camera_bus_if_t lisa_camera_bus_dvp_if = {
    .init = lisa_camera_bus_dvp_init,
    .start_capture = lisa_camera_bus_dvp_start_capture,
    .stop_capture  = lisa_camera_bus_dvp_stop_capture,
};

static int camera_bus_dvp_init(void)
{
    return 0;
}
LISA_DEVICE_REGISTER(camera_bus, &lisa_camera_bus_dvp_if, &camera_bus_dvp_priv, NULL, camera_bus_dvp_init, LISA_DEVICE_PRIORITY_HIGH);