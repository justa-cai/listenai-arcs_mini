#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "lisa_log.h"
#include "lisa_device.h"
#include "lisa_camera.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"
#include "service_camera.h"
#include "board.h"

#define TAG "service_camera"

#define CAMERA_DEVICE "camera"
#define DVP_DEVICE    "dvp0"
#define I2C_DEVICE    "i2c0"
#define DMA_CHANNEL   4

struct camera_context {
    uint32_t inited: 1;
    lisa_device_t *camera_dev;
    lisa_device_t *i2c_dev;
    lisa_device_t *dvp_dev;
    lisa_device_t *gpioa;
    lisa_device_t *gpiob;
    lisa_camera_pixel_format_t pixel_format;
    uint16_t width;
    uint16_t height;
};

static struct camera_context cam_ctx = {
    .inited = 0,
    .camera_dev = NULL,
    .i2c_dev = NULL,
    .dvp_dev = NULL,
    .gpioa = NULL,
    .gpiob = NULL,
    .pixel_format = LISA_CAMERA_PIXFMT_RGB565,
    .width = 0,
    .height = 0,
};

static void service_camera_context_reset(void)
{
    cam_ctx.inited = 0;
    cam_ctx.camera_dev = NULL;
    cam_ctx.i2c_dev = NULL;
    cam_ctx.dvp_dev = NULL;
    cam_ctx.gpioa = NULL;
    cam_ctx.gpiob = NULL;
    cam_ctx.pixel_format = LISA_CAMERA_PIXFMT_RGB565;
    cam_ctx.width = 0;
    cam_ctx.height = 0;
}

int service_camera_init(void)
{
    int ret;

    if (cam_ctx.inited) {
        LISA_LOGW(TAG, "Camera already initialized");
        return 0;
    }

    service_camera_context_reset();

    cam_ctx.camera_dev = lisa_device_get(CAMERA_DEVICE);
    if (!cam_ctx.camera_dev || !lisa_device_ready(cam_ctx.camera_dev)) {
        LISA_LOGE(TAG, "Camera device not ready");
        return -1;
    }

    cam_ctx.i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!cam_ctx.i2c_dev || !lisa_device_ready(cam_ctx.i2c_dev)) {
        LISA_LOGE(TAG, "I2C device not ready");
        return -2;
    }

    cam_ctx.dvp_dev = lisa_device_get(DVP_DEVICE);
    if (!cam_ctx.dvp_dev || !lisa_device_ready(cam_ctx.dvp_dev)) {
        LISA_LOGE(TAG, "DVP device not ready");
        return -3;
    }

    cam_ctx.gpioa = lisa_device_get("gpioa");
    cam_ctx.gpiob = lisa_device_get("gpiob");

    lisa_camera_config_t config = {
        .hw_config =
            {
                .mclk_pad = CSK_IOMUX_PAD_A,
                .mclk_pin = CAM_MCLK_PIN,
                .xclk_delay_us = 0,
                .i2c_dev = cam_ctx.i2c_dev,
            },
        .xclk_freq_hz = 18000000,
        .fb_count = 3,
        .enable_hmirror = false,
        .enable_vflip = false,
        .enable_colorbar = false,
    };

    ret = lisa_camera_setup(cam_ctx.camera_dev, &config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Failed to setup camera: %d", ret);
        return -4;
    }

    lisa_camera_bus_config_t bus_config = {
        .dma_channel = DMA_CHANNEL,
        .bus_type = LISA_CAMERA_BUS_DVP,
        .config.dvp =
            {
                .dvp_dev = cam_ctx.dvp_dev,
                .dvp_freq = config.xclk_freq_hz,
                .data_align = 1,
                .line_offset = 0,
                .pixel_offset = 0,
                .pclk_polarity = 0,
                .vsync_polarity = 1,
                .hsync_polarity = 1,
            },
    };

    lisa_camera_crop_t crop = {
        .x = 0,
        .y = 0,
        .width = 320,
        .height = 240,
    };

    ret = lisa_camera_set_crop(cam_ctx.camera_dev, &crop);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Failed to set crop: %d", ret);
        return -5;
    }

    int flip = 0;
    if (lisa_kv_get_int("user.camera.flip", &flip) != 0) {
        flip = 1;
    }

    if (flip) {
        lisa_camera_set_hmirror(cam_ctx.camera_dev, true);
        lisa_camera_set_vflip(cam_ctx.camera_dev, true);
    } else {
        lisa_camera_set_hmirror(cam_ctx.camera_dev, false);
        lisa_camera_set_vflip(cam_ctx.camera_dev, false);
    }

    lisa_camera_get_framesize(cam_ctx.camera_dev, &bus_config.width, &bus_config.height);
    bus_config.pixel_format = lisa_camera_get_pixformat(cam_ctx.camera_dev);

    cam_ctx.width = bus_config.width;
    cam_ctx.height = bus_config.height;
    cam_ctx.pixel_format = bus_config.pixel_format;

    ret = lisa_camera_attach_bus(cam_ctx.camera_dev, &bus_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Failed to attach bus: %d", ret);
        return -6;
    }

    cam_ctx.inited = 1;
    LISA_LOGI(TAG, "Camera initialized: %ux%u, format=%d", cam_ctx.width, cam_ctx.height, cam_ctx.pixel_format);

    return 0;
}

int service_camera_capture(uint8_t *buffer, uint32_t buffer_len)
{
    lisa_camera_fb_t *fb = NULL;
    int ret;

    if (!cam_ctx.inited || !cam_ctx.camera_dev) {
        LISA_LOGE(TAG, "Camera not initialized");
        return -1;
    }

    if (buffer == NULL || buffer_len < cam_ctx.width * cam_ctx.height * 2) {
        LISA_LOGE(TAG, "Invalid parameters");
        return -2;
    }

    ret = lisa_camera_start(cam_ctx.camera_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Failed to start camera: %d", ret);
        return -3;
    }

    ret = lisa_camera_capture(cam_ctx.camera_dev, &fb);
    if (ret != LISA_DEVICE_OK || fb == NULL) {
        LISA_LOGE(TAG, "Capture failed: %d", ret);
        lisa_camera_stop(cam_ctx.camera_dev);
        return -4;
    }

    if (buffer_len < fb->len) {
        LISA_LOGE(TAG, "Buffer len %d is smaller than fb len %d", buffer_len, fb->len);
        lisa_camera_release_fb(cam_ctx.camera_dev, fb);
        lisa_camera_stop(cam_ctx.camera_dev);
        return -5;
    }

    memcpy(buffer, fb->buf, fb->len);

    lisa_camera_release_fb(cam_ctx.camera_dev, fb);
    lisa_camera_stop(cam_ctx.camera_dev);

    uint16_t *pixels = (uint16_t *)buffer;
    for (uint32_t i = 0; i < cam_ctx.width * cam_ctx.height; i++) {
        uint16_t pixel = pixels[i];
        pixels[i] = (pixel >> 8) | (pixel << 8);
    }

    return 0;
}

int service_camera_get_framesize(uint16_t *width, uint16_t *height)
{
    if (!cam_ctx.inited || !cam_ctx.camera_dev) {
        LISA_LOGE(TAG, "Camera not initialized");
        return -1;
    }

    if (!width || !height) {
        LISA_LOGE(TAG, "Invalid parameters");
        return -2;
    }

    *width = cam_ctx.width;
    *height = cam_ctx.height;

    return 0;
}

bool service_camera_is_inited(void)
{
    return cam_ctx.inited;
}
