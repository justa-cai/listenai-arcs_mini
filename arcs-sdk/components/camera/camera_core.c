#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "cache.h"
#include "arcs_ap.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_DVP.h"

#include "sysheap.h"
#include "sensor.h"
#include "camera_xfer.h"
#include "camera_core.h"
#include "systick.h"
#include "lisa_log.h"

#define CAMERA_XCLK_OUT_PAD         CSK_IOMUX_PAD_A
#define CAMERA_XCLK_OUT_PIN         26

#define CAMERA_PWDN_PAD             CSK_IOMUX_PAD_B
#define CAMERA_PWDN_PIN             9

#define CAMERA_DETECT_WHILE         0

#if CONFIG_OV2640_SUPPORT
#include "ov2640.h"
#endif
#if CONFIG_OV7725_SUPPORT
#include "ov7725.h"
#endif
#if CONFIG_OV3660_SUPPORT
#include "ov3660.h"
#endif
#if CONFIG_OV5640_SUPPORT
#include "ov5640.h"
#endif
#if CONFIG_NT99141_SUPPORT
#include "nt99141.h"
#endif
#if CONFIG_OV7670_SUPPORT
#include "ov7670.h"
#endif
#if CONFIG_GC2145_SUPPORT
#include "gc2145.h"
#endif
#if CONFIG_GC032A_SUPPORT
#include "gc032a.h"
#endif
#if CONFIG_GC0328_SUPPORT
#include "gc0328.h"
#endif
#if CONFIG_GC0308_SUPPORT
#include "gc0308.h"
#endif
#if CONFIG_GC0310_SUPPORT
#include "gc0310.h"
#endif
#if CONFIG_BF3005_SUPPORT
#include "bf3005.h"
#endif
#if CONFIG_BF20A6_SUPPORT
#include "bf20a6.h"
#endif
#if CONFIG_BF3901_SUPPORT
#include "bf3901.h"
#endif
#if CONFIG_SC101IOT_SUPPORT
#include "sc101iot.h"
#endif
#if CONFIG_SC030IOT_SUPPORT
#include "sc030iot.h"
#endif
#if CONFIG_SC031GS_SUPPORT
#include "sc031gs.h"
#endif
#if CONFIG_OV9655_SUPPORT
#include "ov9655.h"
#endif

typedef struct {
    sensor_t sensor;
    struct cam_ipeg_mem cam_mem[MAX_BUF_NUM];
    uint8_t cam_mem_count;
    struct cam_xfer *cam_xfer;
    void *cam_queue_in;
    void *cam_queue_out;
} camera_state_t;

typedef struct {
    camera_model_t model;
    int (*detect)(int slv_addr, sensor_id_t *id);
    int (*init)(sensor_t *sensor);
} sensor_func_t;

static camera_state_t *s_state = NULL;

static const sensor_func_t g_sensors[] = {
#if CONFIG_OV7725_SUPPORT
    {CAMERA_OV7725, ov7725_detect, ov7725_init},
#endif
#if CONFIG_OV7670_SUPPORT
    {CAMERA_OV7670, ov7670_detect, ov7670_init},
#endif
#if CONFIG_OV2640_SUPPORT
    {CAMERA_OV2640, ov2640_detect, ov2640_init},
#endif
#if CONFIG_OV3660_SUPPORT
    {CAMERA_OV3660, ov3660_detect, ov3660_init},
#endif
#if CONFIG_OV5640_SUPPORT
    {CAMERA_OV5640, ov5640_detect, ov5640_init},
#endif
#if CONFIG_NT99141_SUPPORT
    {CAMERA_NT99141, nt99141_detect, nt99141_init},
#endif
#if CONFIG_GC2145_SUPPORT
    {CAMERA_GC2145, gc2145_detect, gc2145_init},
#endif
#if CONFIG_GC032A_SUPPORT
    {CAMERA_GC032A, gc032a_detect, gc032a_init},
#endif
#if CONFIG_GC0328_SUPPORT
    {CAMERA_GC0328, gc0328_detect, gc0328_init},
#endif
#if CONFIG_GC0308_SUPPORT
    {CAMERA_GC0308, gc0308_detect, gc0308_init},
#endif
#if CONFIG_GC0310_SUPPORT
    {CAMERA_GC0310, gc0310_detect, gc0310_init},
#endif
#if CONFIG_BF3005_SUPPORT
    {CAMERA_BF3005, bf3005_detect, bf3005_init},
#endif
#if CONFIG_BF20A6_SUPPORT
    {CAMERA_BF20A6, bf20a6_detect, bf20a6_init},
#endif
#if CONFIG_BF3901_SUPPORT
    {CAMERA_BF3901, bf3901_detect, bf3901_init},
#endif
#if CONFIG_SC101IOT_SUPPORT
    {CAMERA_SC101IOT, sc101iot_detect, sc101iot_init},
#endif
#if CONFIG_SC030IOT_SUPPORT
    {CAMERA_SC030IOT, sc030iot_detect, sc030iot_init},
#endif
#if CONFIG_SC031GS_SUPPORT
    {CAMERA_SC031GS, sc031gs_detect, sc031gs_init},
#endif
#if CONFIG_OV9655_SUPPORT
    {CAMERA_OV9655, ov9655_detect, ov9655_init},
#endif
};


static int _camera_reqbuf(unsigned int count, size_t frame_size)
{
    int i;

    if (count < 2) {
        count = 3;
    }

    if(s_state == NULL) return -1;

    s_state->cam_mem_count = count;

    for (i = 0; i < count; i ++) {
        s_state->cam_mem[i].index = i;
        s_state->cam_mem[i].buf.size = frame_size;

        s_state->cam_mem[i].buf.addr = exram_malloc(32, frame_size);

        if (s_state->cam_mem[i].buf.addr == NULL) {
            LOGE("spi malloc size: %d failed\n", frame_size);
            return -1;
        }

        int cam_mem_addr = (int)&s_state->cam_mem[i];
        if (!xQueueSend(s_state->cam_queue_in, &cam_mem_addr, 0)) {
            LOGE("fill spi in queue error %d", i);
        }
    }

    return 0;
}

static int _camera_freebuf(void)
{
    // struct spi_ipeg_mem *spi_mem;
    int i;
    if(s_state == NULL) return -1;
    for (i = 0; i < s_state->cam_mem_count; i++) {
        free(s_state->cam_mem[i].buf.addr);
    }

    return 0;
}

_FAST_TEXT struct cam_ipeg_mem *camera_dqbuf(struct cam_ipeg_mem *cam_mem, unsigned int timeout_msec)
{
    int ret;

    if(s_state == NULL || s_state->cam_queue_out == NULL) {
        LOGE("s_stat null or cam_queue_out null\n");
        return NULL;
    }
    ret = xQueueReceive(s_state->cam_queue_out, &cam_mem, pdMS_TO_TICKS(timeout_msec));
    if (!ret) {
        LOGE("spi queue recv error");
        return NULL;
    }

    HAL_InvalidateDCache_by_Addr(cam_mem->buf.addr, cam_mem->buf.size);

    return cam_mem;
}

void camera_qbuf(struct cam_ipeg_mem *cam_mem)
{
    if(s_state == NULL || s_state->cam_queue_in == NULL) {
        LOGE("s_stat null or cam_queue_out null\n");
        return;
    }
    if (!xQueueSend(s_state->cam_queue_in, &cam_mem, 10)) {
        LOGE("enqueue error");
    }

}

void camera_qreset(void)
{
    struct cam_ipeg_mem *spi_mem;

    if(s_state == NULL || s_state->cam_queue_out == NULL) {
        LOGE("s_stat null or cam_queue_out null\n");
        return;
    }
    while (xQueueReceive(s_state->cam_queue_out, &spi_mem, 0)) {
        camera_qbuf(spi_mem);
    }
}


/**
 * @brief 初始化Camera硬件配置
 */
 static int camera_hw_init(const hw_config_t *hw_config)
 {
     if (!hw_config) {
         LOGE("Hardware config is NULL");
         return -1;
     }
     void *gpio_dev = hw_config->pin_config.pwdn.pad == CSK_IOMUX_PAD_A ? GPIOA() : GPIOB();
     
     GPIO_SetDir(gpio_dev, 1 << hw_config->pin_config.pwdn.pin, CSK_GPIO_DIR_OUTPUT);
     GPIO_PinWrite(gpio_dev, 1 << hw_config->pin_config.pwdn.pin, 0);
     SysTick_Delay_Us(hw_config->pwdn_delay_us);

     // 配置外部时钟输出
     if (hw_config->xclk_freq_hz > 0) {
        LOGI("Enabling XCLK output: %d Hz\r\n", hw_config->xclk_freq_hz);
        IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIDEO_CLK = 0x1;  // bit15
        IP_AP_CFG->REG_CLK_CFG0.bit.ENA_VIC_CLK = 0x1;    // bit21
        // IP_AP_CFG->REG_SW_RESET.bit.VIC_RESET = 1;        // bit9
        DVP_EnableClockout(DVP0(), hw_config->xclk_freq_hz);
     }
     // 时钟输出后延时
     SysTick_Delay_Us(hw_config->xclk_delay_us);

     return 0;
 }

int camera_init(const camera_config_t *config)
{
    int ret = -1;
    uint32_t i = 0;
    int buf_size = 0;
    camera_sensor_info_t *sensor_info;

    if (s_state != NULL) {
        LOGE("Already initialized\n");
        return -1;
    }

    if (NULL == config) {
         LOGE("config is NULL\n");
        return -1;
    }

    s_state = calloc(1, sizeof(camera_state_t));
    if (!s_state) {
        LOGE("Failed to allocate memory for camera state\n");
        return -1;
    }

    s_state->cam_mem_count = config->buf_count;
    s_state->cam_queue_out = xQueueCreate(s_state->cam_mem_count, sizeof(struct cam_ipeg_mem *));
    s_state->cam_queue_in  = xQueueCreate(s_state->cam_mem_count, sizeof(struct cam_ipeg_mem *));

    switch (config->pixel_format) {
    case PIXFORMAT_RGB565:
    case PIXFORMAT_YUV422:
        buf_size = resolution[config->frame_size].width * resolution[config->frame_size].height * 2;
        break;
    case PIXFORMAT_GRAYSCALE:
        buf_size = resolution[config->frame_size].width * resolution[config->frame_size].height;
        break;
    default:
        break;
    }
    _camera_reqbuf(config->buf_count, buf_size);

    struct cam_xfer_queue queue;
    queue.width = resolution[config->frame_size].width;
    queue.height = resolution[config->frame_size].height;
    queue.queue_in = s_state->cam_queue_in;
    queue.queue_out = s_state->cam_queue_out;
    s_state->cam_xfer = cam_xfer_init(&config->xfer_config, &queue);

    hw_config_t *hw_config = (hw_config_t *)&config->hw_config;
    hw_config->xclk_freq_hz = config->xclk_freq_hz;
    camera_hw_init(hw_config);

    sensor_twi_init(&config->i2c_config);

    do {
        for (i = 0; i < (sizeof(g_sensors) / sizeof(sensor_func_t)); i++) {
            sensor_info = camera_sensor_get_info(g_sensors[i].model);
            if (NULL != sensor_info) {
                if (g_sensors[i].detect(sensor_info->sccb_addr, &s_state->sensor.id)) {
                    s_state->sensor.slv_addr = sensor_info->sccb_addr;
                    s_state->sensor.xclk_freq_hz = config->xclk_freq_hz;
                    g_sensors[i].init(&s_state->sensor);
                    break;
                }
            }
        }
    } while (CAMERA_DETECT_WHILE);

    if (i == (sizeof(g_sensors) / sizeof(sensor_func_t))) {
        LOGE("[%s:%d] camera not detected\r\n", __func__, __LINE__);
        ret = -1;
        goto fail;
    }

    s_state->sensor.reset(&s_state->sensor);

    if (s_state->sensor.set_window) {
        s_state->sensor.set_window(&s_state->sensor, config->roi.x, config->roi.y, config->roi.w, config->roi.h);
    }
    
    if (s_state->sensor.set_hmirror) {
        s_state->sensor.set_hmirror(&s_state->sensor, config->is_h_mirror);
    }

    if (s_state->sensor.set_vflip) {
        s_state->sensor.set_vflip(&s_state->sensor, config->is_v_flip);
    }

    if (config->colorbar) {
        s_state->sensor.set_colorbar(&s_state->sensor, 1);
    }

    framesize_t framesize = (framesize_t)config->frame_size;
    pixformat_t pix_format = (pixformat_t)config->pixel_format;

    if (PIXFORMAT_JPEG == pix_format && (!sensor_info->support_jpeg)) {
        LOGE("JPEG format is not supported on this sensor\r\n");
        ret = -1;
        goto fail;
    }

    if (framesize > sensor_info->max_size) {
         LOGW("The frame size is exeeds the maximum for this sensor, it will be forced to the maximum possible value\r\n");
        framesize = sensor_info->max_size;
    }

    s_state->sensor.status.framesize = framesize;
    s_state->sensor.pixformat = pix_format;

     LOGI("Setting frame size to %dx%d\r\n", resolution[framesize].width, resolution[framesize].height);
    if (s_state->sensor.set_framesize(&s_state->sensor, framesize) != 0) {
        LOGE("Failed to set frame size\r\n");
        ret = -1;
        goto fail;
    }

    s_state->sensor.set_pixformat(&s_state->sensor, pix_format);
    if (s_state->sensor.id.PID == OV2640_PID) {
        s_state->sensor.set_gainceiling(&s_state->sensor, GAINCEILING_2X);
        s_state->sensor.set_bpc(&s_state->sensor, false);
        s_state->sensor.set_wpc(&s_state->sensor, true);
        s_state->sensor.set_lenc(&s_state->sensor, true);
    }

    if (pix_format == PIXFORMAT_JPEG) {
        s_state->sensor.set_quality(&s_state->sensor, config->jpeg_quality);
    }
    s_state->sensor.init_status(&s_state->sensor);

    if (s_state->sensor.stop) {
        s_state->sensor.stop(&s_state->sensor);
    }

// #ifndef CONFIG_CORE_SPINLOCK
//     sensor_twi_exit();          // 是释放I2C给CP核使用
// #endif
    return 0;

fail:
    camera_deinit();
    return ret;
}

int camera_start(void)
{
    if(s_state == NULL) return -1;
    if (s_state->sensor.start) {
        s_state->sensor.start(&s_state->sensor);
    }
    s_state->cam_xfer->ops->cam_xfer_start();
    return 0;
}

int camera_stop(void)
{
    if(s_state == NULL) return -1;
    s_state->cam_xfer->ops->cam_xfer_stop();
    if (s_state->sensor.stop) {
        s_state->sensor.stop(&s_state->sensor);
    }
    return 0;
}

int camera_deinit(void)
{
    _camera_freebuf();

    sensor_twi_exit();

    if(s_state == NULL) return -1;

    cam_xfer_deinit(s_state->cam_xfer);

    vQueueDelete(s_state->cam_queue_in);
    vQueueDelete(s_state->cam_queue_out);
    free(s_state);
    s_state = NULL;

    return 0;
}

int camera_set_gainceiling(uint8_t gain)
{
    if(s_state == NULL) return -1;
    s_state->sensor.set_gainceiling(&s_state->sensor, gain);
    return 0;
}

int camera_set_window(int x, int y, int w, int h)
{
    if(s_state == NULL) return -1;
    s_state->sensor.set_window(&s_state->sensor, x, y, w, h);

    return 0;
}

int camera_get_window(uint16_t *w, uint16_t *h)
{
    if(s_state == NULL) return -1;
    s_state->sensor.get_window(&s_state->sensor, w, h);

    return 0;
}

int camera_set_handle_mode(bool hmirror, bool vflip)
{
    if(s_state == NULL) return -1;
    s_state->sensor.set_hmirror(&s_state->sensor, hmirror);
    s_state->sensor.set_vflip(&s_state->sensor, vflip);

    return 0;
}

int camera_set_sensor_reg(int reg, int value, int mask)
{
    if(s_state == NULL) return -1;
    s_state->sensor.set_reg(&s_state->sensor, reg, mask, value);

    return 0;
}

int camera_get_sensor_reg(int reg, int* value,  int mask)
{
    if(s_state == NULL) return -1;
    *value = s_state->sensor.get_reg(&s_state->sensor, reg, mask);

    return 0;
}

void camera_senor_release(void)
{
    sensor_twi_exit();
    if (s_state)
        memset(&s_state->sensor, 0, sizeof(s_state->sensor));
}
