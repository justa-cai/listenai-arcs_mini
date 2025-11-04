#include "camera.h"
#include "log_print.h"
#include "csk_driver.h"
#include "sw_i2c.h"

/* camera */
#ifndef CONFIG_OV2640_SUPPORT
#define CONFIG_OV2640_SUPPORT       0
#endif
#ifndef CONFIG_OV7725_SUPPORT
#define CONFIG_OV7725_SUPPORT       0
#endif
#ifndef CONFIG_OV3660_SUPPORT
#define CONFIG_OV3660_SUPPORT       0
#endif
#ifndef CONFIG_OV5640_SUPPORT
#define CONFIG_OV5640_SUPPORT       1
#endif
#ifndef CONFIG_NT99141_SUPPORT
#define CONFIG_NT99141_SUPPORT      0
#endif
#ifndef CONFIG_OV7670_SUPPORT
#define CONFIG_OV7670_SUPPORT       0
#endif
#ifndef CONFIG_GC2145_SUPPORT
#define CONFIG_GC2145_SUPPORT       0
#endif
#ifndef CONFIG_GC032A_SUPPORT
#define CONFIG_GC032A_SUPPORT       0
#endif
#ifndef CONFIG_GC0328_SUPPORT
#define CONFIG_GC0328_SUPPORT       1
#endif
#ifndef CONFIG_GC0308_SUPPORT
#define CONFIG_GC0308_SUPPORT       1
#endif
#ifndef CONFIG_GC0310_SUPPORT
#define CONFIG_GC0310_SUPPORT       1
#endif
#ifndef CONFIG_BF3005_SUPPORT
#define CONFIG_BF3005_SUPPORT       0
#endif
#ifndef CONFIG_BF20A6_SUPPORT
#define CONFIG_BF20A6_SUPPORT       0
#endif
#ifndef CONFIG_SC101IOT_SUPPORT
#define CONFIG_SC101IOT_SUPPORT     0
#endif
#ifndef CONFIG_SC030IOT_SUPPORT
#define CONFIG_SC030IOT_SUPPORT     0
#endif
#ifndef CONFIG_SC031GS_SUPPORT
#define CONFIG_SC031GS_SUPPORT      0
#endif
#ifndef CONFIG_OV9655_SUPPORT
#define CONFIG_OV9655_SUPPORT       0
#endif

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
    camera_model_t model;
    int (*detect)(int slv_addr, sensor_id_t *id);
    int (*init)(sensor_t *sensor);
} sensor_func_t;

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


static uint8_t i2c_index = 0;

static int camera_drv_init(uint8_t index)
{
    sw_i2c_port_callback_t i2c_port;

    //return csk_i2c_init(i2c_index);

    switch(index)
    {
        case QSPI_IN_I2C_INDEX:
            CAMERA_LOGI("sw_qspi_in_i2c init index=%d", index);
            i2c_port.gpio_init    = sw_qspi_in_i2c_gpio_init;
            i2c_port.scl_set      = sw_qspi_in_i2c_scl_gpio_set;
            i2c_port.scl_clr      = sw_qspi_in_i2c_scl_gpio_clr;
            i2c_port.sda_set      = sw_qspi_in_i2c_sda_gpio_set;
            i2c_port.sda_clr      = sw_qspi_in_i2c_sda_gpio_clr;
            i2c_port.sda_get      = sw_qspi_in_i2c_sda_gpio_get;
            i2c_port.sda_dirout   = sw_qspi_in_i2c_sda_gpio_setdir_output;
            i2c_port.sda_dirin    = sw_qspi_in_i2c_sda_gpio_setdir_input;
            break;

        case DVP_I2C_INDEX:
            CAMERA_LOGI("sw_dvp_i2c init index=%d", index);
            i2c_port.gpio_init    = sw_dvp_i2c_gpio_init;
            i2c_port.scl_set      = sw_dvp_i2c_scl_gpio_set;
            i2c_port.scl_clr      = sw_dvp_i2c_scl_gpio_clr;
            i2c_port.sda_set      = sw_dvp_i2c_sda_gpio_set;
            i2c_port.sda_clr      = sw_dvp_i2c_sda_gpio_clr;
            i2c_port.sda_get      = sw_dvp_i2c_sda_gpio_get;
            i2c_port.sda_dirout   = sw_dvp_i2c_sda_gpio_setdir_output;
            i2c_port.sda_dirin    = sw_dvp_i2c_sda_gpio_setdir_input;
            break;

        default:
            CAMERA_LOGI("error index=%d", index);
            break;
    }

    i2c_index = index;
    return sw_i2c_init(i2c_index, &i2c_port);
}

static uint8_t camera_read_reg8(uint8_t slv_addr, uint8_t reg)
{
    //return csk_i2c_read_reg8(i2c_index, slv_addr, reg);
    return sw_i2c_read_reg8(i2c_index, slv_addr, reg);
}

static uint8_t camera_read_reg16(uint8_t slv_addr, uint16_t reg)
{
    //return csk_i2c_read_reg16(i2c_index, slv_addr, reg);
    return sw_i2c_read_reg16(i2c_index, slv_addr, reg);
}

static int camera_write_reg8(uint8_t slv_addr, uint8_t reg, uint8_t value)
{
    //return csk_i2c_write_reg8(i2c_index, slv_addr, reg, value);
    return sw_i2c_write_reg8(i2c_index, slv_addr, reg, value);
}

static int camera_write_reg16(uint8_t slv_addr, uint16_t reg, uint8_t value)
{
    //return csk_i2c_write_reg16(i2c_index, slv_addr, reg, value);
    return sw_i2c_write_reg16(i2c_index, slv_addr, reg, value);
}

static void camera_delay_ms(uint32_t nms)
{
    DELAY_MS(nms);
}

static void camera_delay_us(uint32_t nus)
{
    DELAY_US(nus);
}

static int camera_log(const char* format, ...)
{
    //VIDEO_LOG(format);
    //printf();
#if UART_ENABLE
    va_list ap;

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
#endif

    return 0;
}


static sensor_port_callback_t sensor_callback = {
    .drv_init = camera_drv_init,
    .read_reg8 = camera_read_reg8,
    .read_reg16 = camera_read_reg16,
    .write_reg8 = camera_write_reg8,
    .write_reg16 = camera_write_reg16,
    .delay_ms = camera_delay_ms,
    .delay_us = camera_delay_us,
    .log = camera_log,
};


static void camera_log_test(void)
{
    CAMERA_LOGI("[%s:%d] i2c_index=%d", __func__, __LINE__, i2c_index);
    CAMERA_LOGI("%f %02f", 1.6, 6.6666*8.8888);
    CAMERA_LOGI("%s %p", "hello world", i2c_index);
    CAMERA_LOGI("i2c_index=%d 0x%x", i2c_index, &i2c_index);
}


int camera_init(const camera_config_t *config)
{
    int ret = -1;
    uint32_t i = 0;
    sensor_t sensor;
    sensor_id_t id;
    camera_sensor_info_t *sensor_info;

    camera_sensor_port_callback(&sensor_callback);

    if (NULL == config) {
        CAMERA_LOG("[%s:%d] config is NULL", __func__, __LINE__);
        goto fail;
    }

    do {
        for (i = 0; i < (sizeof(g_sensors) / sizeof(sensor_func_t)); i++)
        {
            sensor_info = camera_sensor_get_info(g_sensors[i].model);
            if (NULL != sensor_info)
            {
                CAMERA_LOGI("I2C detected %s start", sensor_info->name);
                camera_drv_init(config->sccb_i2c_port);
                if (g_sensors[i].detect(sensor_info->sccb_addr, &sensor.id))
                {
                    CAMERA_LOGI("Detected %s camera, slv_addr=0x%x", sensor_info->name, sensor_info->sccb_addr);
                    sensor.slv_addr = sensor_info->sccb_addr;
                    sensor.xclk_freq_hz = config->xclk_freq_hz;
                    g_sensors[i].init(&sensor);
                    break;
                }
            }
        }
    } while(CAMERA_DETECT_WHILE);

    if (i == (sizeof(g_sensors) / sizeof(sensor_func_t)))
    {
        CAMERA_LOGE("[%s:%d] camera not detected", __func__, __LINE__);
        ret = -1;
        goto fail;
    }


    CAMERA_LOGI("Doing SW reset of sensor");
    sensor.reset(&sensor);

    if(config->colorbar) {
        sensor.set_colorbar(&sensor, 1);
    }


    framesize_t frame_size = (framesize_t) config->frame_size;
    pixformat_t pix_format = (pixformat_t) config->pixel_format;

    if (PIXFORMAT_JPEG == pix_format && (!sensor_info->support_jpeg)) {
        CAMERA_LOGE("JPEG format is not supported on this sensor");
        ret = -1;
        goto fail;
    }

    if (frame_size > sensor_info->max_size) {
        CAMERA_LOGW("The frame size exceeds the maximum for this sensor, it will be forced to the maximum possible value");
        frame_size = sensor_info->max_size;
    }

    sensor.status.framesize = frame_size;
    sensor.pixformat = pix_format;


    CAMERA_LOGI("Setting frame size to %dx%d", resolution[frame_size].width, resolution[frame_size].height);
    if (sensor.set_framesize(&sensor, frame_size) != 0) {
        CAMERA_LOGE("Failed to set frame size");
        ret = -1;
        goto fail;
    }

    sensor.set_pixformat(&sensor, pix_format);


    if (sensor.id.PID == OV2640_PID) {
        sensor.set_gainceiling(&sensor, GAINCEILING_2X);
        sensor.set_bpc(&sensor, false);
        sensor.set_wpc(&sensor, true);
        sensor.set_lenc(&sensor, true);
    }

    if (pix_format == PIXFORMAT_JPEG) {
        sensor.set_quality(&sensor, config->jpeg_quality);
    }
    sensor.init_status(&sensor);


    return 0;

fail:
    camera_deinit();
    return ret;
}


int camera_deinit(void)
{
    return 0;
}

