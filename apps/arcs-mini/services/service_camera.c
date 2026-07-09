#include "stdint.h"
#include "stdbool.h"
#include <string.h>

#define TAG "service_camera"

#include "lisa_log.h"
#include "lisa_kv.h"
#include "lisa_device.h"
#include "lisa_camera.h"

#include "IOMuxManager.h"
#include "board.h"

#include "service_camera.h"

/* ---- 配置 --------------------------------------------------------------- */

#define CAMERA_DEVICE          "camera"
#define DVP_DEVICE             "dvp0"
#ifdef CONFIG_BOARD_ARCS_MINI_V3
#define I2C_DEVICE             "i2c1"
#else
#define I2C_DEVICE             "i2c0"
#endif
#define DMA_CHANNEL            4
#define CAMERA_HMIRROR         1 // 0: 画面正常; 1: 水平翻转 (照镜子效果)
#define CAMERA_VFLIP           0 // 0: 画面正常; 1: 垂直翻转 (画面颠倒)
#ifdef CONFIG_BOARD_ARCS_MINI_V3
/* v3 camera PWDN is active high. Keep it low long enough before sensor probe. */
#define CAMERA_V3_PWDN_RELEASE_LEVEL    0
#define CAMERA_V3_PWDN_RELEASE_DELAY_US 100000
#endif

/*
 * Sensor 采集 640×480 全画幅 → DVP 直出 → CPU 最近邻 resize 到 320×240。
 * GC0328 硬件跳采 (0x59+0x5A) 内部需读完整个模拟窗口再抽点，实测比软件 resize 慢。
 */

#define CAMERA_CAPTURE_WIDTH  640
#define CAMERA_CAPTURE_HEIGHT 480
#define CAMERA_OUTPUT_WIDTH   320
#define CAMERA_OUTPUT_HEIGHT  240
#define CAMERA_CAPTURE_DROP_STALE_FRAMES 2

/* ---- 模块级状态 --------------------------------------------------------- */

struct camera_context {
    bool inited;
    bool streaming;
    lisa_device_t *camera_dev;
    lisa_device_t *i2c_dev;
    lisa_device_t *dvp_dev;
    lisa_camera_pixel_format_t pixel_format;
    uint16_t width;
    uint16_t height;
};

static struct camera_context cam_ctx;

/* ---- 内部工具函数 ------------------------------------------------------- */

static void service_camera_context_reset(void)
{
    memset(&cam_ctx, 0, sizeof(cam_ctx));
    cam_ctx.pixel_format = LISA_CAMERA_PIXFMT_RGB565;
}

static inline uint32_t service_camera_frame_bytes(uint16_t width, uint16_t height)
{
    return (uint32_t)width * (uint32_t)height * 2U;
}

static int service_camera_start_streaming(void)
{
    int ret;

    if (cam_ctx.streaming) {
        return 0;
    }

    ret = lisa_camera_start(cam_ctx.camera_dev);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Start stream failed: %d", ret);
        return -4;
    }

    cam_ctx.streaming = true;
    LISA_LOGI(TAG, "Camera stream started");
    return 0;
}

static void service_camera_stop_stream(void)
{
    if (!cam_ctx.streaming) {
        return;
    }

    lisa_camera_stop(cam_ctx.camera_dev);
    cam_ctx.streaming = false;
    LISA_LOGI(TAG, "Camera stream stopped");
}

static int service_camera_capture_frame(lisa_camera_fb_t **fb, uint8_t drop_frames)
{
    lisa_camera_fb_t *frame = NULL;
    int ret;

    for (uint8_t i = 0; i <= drop_frames; i++) {
        ret = lisa_camera_capture(cam_ctx.camera_dev, &frame);
        if (ret != LISA_DEVICE_OK || !frame || !frame->buf || frame->width == 0) {
            LISA_LOGE(TAG, "Capture frame failed: %d", ret);
            return -5;
        }

        if (i < drop_frames) {
            lisa_camera_release_fb(cam_ctx.camera_dev, frame);
            frame = NULL;
            continue;
        }

        *fb = frame;
        return 0;
    }

    return -5;
}

/* ---- 公开 API ----------------------------------------------------------- */

int service_camera_init(void)
{
    int ret;

    if (cam_ctx.inited) {
        LISA_LOGW(TAG, "Camera already initialized");
        return 0;
    }

    service_camera_context_reset();

    /* ================================================================
     * Phase 1: 设备获取
     * ================================================================ */
    cam_ctx.camera_dev = lisa_device_get(CAMERA_DEVICE);
    cam_ctx.i2c_dev    = lisa_device_get(I2C_DEVICE);
    cam_ctx.dvp_dev    = lisa_device_get(DVP_DEVICE);

    if (!cam_ctx.camera_dev || !lisa_device_ready(cam_ctx.camera_dev)) {
        LISA_LOGE(TAG, "Camera device not ready");
        return -1;
    }
    if (!cam_ctx.i2c_dev || !lisa_device_ready(cam_ctx.i2c_dev)) {
        LISA_LOGE(TAG, "I2C device not ready");
        return -2;
    }
    if (!cam_ctx.dvp_dev || !lisa_device_ready(cam_ctx.dvp_dev)) {
        LISA_LOGE(TAG, "DVP device not ready");
        return -3;
    }

#ifdef CONFIG_BOARD_ARCS_MINI_V3
    lisa_device_t *pwdn_gpio_dev = lisa_device_get(CAM_PWDN_DEVICE_NAME);
    if (!pwdn_gpio_dev || !lisa_device_ready(pwdn_gpio_dev)) {
        LISA_LOGE(TAG, "Camera PWDN GPIO device not ready");
        return -4;
    }
#endif

    /* ================================================================
     * Phase 2: Sensor 硬件初始化 (probe + reset + clock)
     * ================================================================ */
    lisa_camera_config_t config = {
        .hw_config = {
            .mclk_pad      = CSK_IOMUX_PAD_A,
            .mclk_pin      = CAM_MCLK_PIN,
#ifdef CONFIG_BOARD_ARCS_MINI_V3
            .pwdn_gpio_dev        = pwdn_gpio_dev,
            .pwdn_pin             = CAM_PWDN_PIN,
            .pwdn_inactive_level  = CAMERA_V3_PWDN_RELEASE_LEVEL,
            .pwdn_delay_us        = CAMERA_V3_PWDN_RELEASE_DELAY_US,
#endif
            .xclk_delay_us = 0,
            .i2c_dev       = cam_ctx.i2c_dev,
        },
        .xclk_freq_hz    = 18000000,
        .fb_count        = 2,
        .enable_colorbar = false,
    };

    ret = lisa_camera_setup(cam_ctx.camera_dev, &config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Sensor setup failed: %d", ret);
        return -4;
    }

    /* ================================================================
     * Phase 3: Sensor 流水线配置 (crop → mirror)
     * ================================================================ */

    /* 3a. crop 窗口 (0x50, 0x51-0x58) — 全传感器采集尺寸 */
    lisa_camera_crop_t crop = {
        .x      = 0,
        .y      = 0,
        .width  = CAMERA_CAPTURE_WIDTH,
        .height = CAMERA_CAPTURE_HEIGHT,
    };
    ret = lisa_camera_set_crop(cam_ctx.camera_dev, &crop);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "Crop window config failed: %d", ret);
        return -5;
    }

    /* 3b. 镜像/翻转 (0x17)
     * hmirror: 水平翻转 — 画面左右对调, 照镜子效果
     * vflip:   垂直翻转 — 画面上下颠倒
     * 默认值由 CAMERA_HMIRROR / CAMERA_VFLIP 设定,
     * KV (user.camera.hmirror / user.camera.flip) 存在时覆盖默认值。 */
    int hmirror = CAMERA_HMIRROR;
    int vflip   = CAMERA_VFLIP;
    lisa_kv_get_int("user.camera.hmirror", &hmirror);
    lisa_kv_get_int("user.camera.flip",     &vflip);
    lisa_camera_set_hmirror(cam_ctx.camera_dev, hmirror);
    lisa_camera_set_vflip(cam_ctx.camera_dev, vflip);

    /*
     * 硬件跳采 (备选方案, 当前未启用):
     *
     * GC0328 支持通过 0x59/0x5A 寄存器在 sensor 内部做像素抽点。
     * 用法: crop 设为 640×480, 调用 set_subsample(row, col) 设置跳采比例,
     *       DVP 按 CAMERA_OUTPUT 尺寸捕获, capture 中只需 memcpy + 字节交换。
     *
     * 实测: 硬件跳采 ~250ms/帧, 软件 resize ~95ms/帧。
     * 原因: GC0328 跳采需先内部读完整个 640×480 模拟窗口再走抽点流水线,
     *       加上 EXTEND_PCLK 拉长输出时钟, 最终比 DVP 直出 640×480 + CPU
     *       最近邻 resize 慢 2.5 倍。保留此注释供后续芯片验证参考。
     */

    // /* 3c. 跳采比例 (0x59, 0x5A): 全幅 ÷ 输出 */
    // uint8_t row_ratio = CAMERA_CAPTURE_HEIGHT / CAMERA_OUTPUT_HEIGHT;
    // uint8_t col_ratio = CAMERA_CAPTURE_WIDTH  / CAMERA_OUTPUT_WIDTH;
    // ret = lisa_camera_set_subsample(cam_ctx.camera_dev, row_ratio, col_ratio);
    // if (ret != LISA_DEVICE_OK) {
    //     LISA_LOGE(TAG, "Subsample config failed: %d", ret);
    //     return -6;
    // }

    /* ================================================================
     * Phase 4: DVP 总线配置
     *
     * GC0328 8-bit 并行接口:
     *   data_align=1     左对齐 (sensor 数据 → DVP bit[11:4])
     *   pclk_polarity=0  下降沿采样
     *   vsync/hsync=1    高电平有效
     * ================================================================ */
    lisa_camera_bus_config_t bus_config = {
        .dma_channel = DMA_CHANNEL,
        .bus_type    = LISA_CAMERA_BUS_DVP,
        .config.dvp  = {
            .dvp_dev        = cam_ctx.dvp_dev,
            .dvp_freq       = config.xclk_freq_hz,
            .data_align     = 1,
            .pclk_polarity  = 0,
            .vsync_polarity = 1,
            .hsync_polarity = 1,
        },
    };

    /* DVP 按采集尺寸 640×480 捕获，resize 在 capture 中完成 */
    bus_config.width        = CAMERA_CAPTURE_WIDTH;
    bus_config.height       = CAMERA_CAPTURE_HEIGHT;
    bus_config.pixel_format = lisa_camera_get_pixformat(cam_ctx.camera_dev);

    cam_ctx.width        = CAMERA_OUTPUT_WIDTH;
    cam_ctx.height       = CAMERA_OUTPUT_HEIGHT;
    cam_ctx.pixel_format = bus_config.pixel_format;

    ret = lisa_camera_attach_bus(cam_ctx.camera_dev, &bus_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(TAG, "DVP bus attach failed: %d", ret);
        return -6;
    }

    /* ================================================================
     * Phase 5: 就绪
     * ================================================================ */
    cam_ctx.inited = true;
    LISA_LOGI(TAG, "Camera ready: %ux%u RGB565, hmirror=%d, vflip=%d",
              cam_ctx.width, cam_ctx.height, hmirror, vflip);

    return 0;
}

int service_camera_capture(uint8_t *buffer, uint32_t buffer_len)
{
    lisa_camera_fb_t *fb = NULL;
    int ret;

    if (!cam_ctx.inited) {
        LISA_LOGE(TAG, "Camera not initialized");
        return -1;
    }

    if (!buffer || buffer_len < service_camera_frame_bytes(cam_ctx.width, cam_ctx.height)) {
        LISA_LOGE(TAG, "Invalid buffer: need >= %u bytes",
                  service_camera_frame_bytes(cam_ctx.width, cam_ctx.height));
        return -2;
    }

    if (cam_ctx.pixel_format != LISA_CAMERA_PIXFMT_RGB565) {
        LISA_LOGE(TAG, "Unsupported pixel format: %d", cam_ctx.pixel_format);
        return -3;
    }

    ret = service_camera_start_streaming();
    if (ret != 0) {
        return ret;
    }

    ret = service_camera_capture_frame(&fb, CAMERA_CAPTURE_DROP_STALE_FRAMES);
    if (ret != 0) {
        service_camera_stop_stream();
        return ret;
    }

    /* 最近邻下采样 640×480 → 320×240 + RGB565 字节交换 */
    const uint8_t *src = fb->buf;
    for (uint32_t dst_y = 0; dst_y < cam_ctx.height; dst_y++) {
        uint32_t src_y = (uint32_t)dst_y * fb->height / cam_ctx.height;
        const uint8_t *src_row = src + src_y * fb->width * 2U;
        uint8_t *dst_row = buffer + dst_y * cam_ctx.width * 2U;
        for (uint32_t dst_x = 0; dst_x < cam_ctx.width; dst_x++) {
            uint32_t src_x = (uint32_t)dst_x * fb->width / cam_ctx.width;
            const uint8_t *s = src_row + src_x * 2U;
            uint8_t *d = dst_row + dst_x * 2U;
            d[0] = s[1];  /* 字节交换: little-endian → 标准 RGB565 */
            d[1] = s[0];
        }
    }

    lisa_camera_release_fb(cam_ctx.camera_dev, fb);

    return 0;
}

int service_camera_stop(void)
{
    if (!cam_ctx.inited) {
        return -1;
    }

    service_camera_stop_stream();
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

    *width  = cam_ctx.width;
    *height = cam_ctx.height;

    return 0;
}

bool service_camera_is_inited(void)
{
    return cam_ctx.inited;
}
