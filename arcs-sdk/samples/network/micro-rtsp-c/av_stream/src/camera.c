#include "camera.h"
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include "lisa_device.h"
#include "lisa_camera.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"
#include "turbojpeg.h"

/* 硬件JPEG编码配置 */
#define USE_HARDWARE_JPEG_ENCODER  1  // 1:使用硬件编码  0:使用软件编码(TurboJPEG)

/* 调试开关 - 控制性能统计日志输出 */
#define RTSP_DEBUG_TIMING          0  // 1:开启耗时打印  0:关闭

#if USE_HARDWARE_JPEG_ENCODER
/* 硬件JPEG编码头文件 */
#include "jpeg_encoder_ext.h"

/* 硬件JPEG编码全局变量 */
static jpeg_enc_ctx_t g_jpeg_encoder_ctx;
static uint8_t *g_hw_jpeg_output_buffer = NULL;
static uint32_t g_hw_jpeg_output_size = 0;
#endif

#define TAG "camera"
#include <lisa_log.h>

/* 摄像头设备配置 */
#define CAMERA_DEVICE    "camera"
#define DVP_DEVICE       "dvp0"
#define DMA_CHANNEL      2

/* 摄像头引脚定义 (ARCS_EVB) */
#ifdef CONFIG_BOARD_ARCS_EVB
#define CAM_PWDN_PIN    7
#define CAM_HSYNC_PIN   10
#define CAM_VSYNC_PIN   11
#define CAM_PCLK_PIN    12
#define CAM_MCLK_PIN    26
#define CAM_D0_PIN      13
#define CAM_D1_PIN      14
#define CAM_D2_PIN      15
#define CAM_D3_PIN      16
#define CAM_D4_PIN      17
#define CAM_D5_PIN      18
#define CAM_D6_PIN      19
#define CAM_D7_PIN      20
#endif

/* 摄像头全局变量 */
static lisa_device_t *g_camera_dev = NULL;
static uint32_t g_camera_width = 0;   /* 摄像头实际宽度 */
static uint32_t g_camera_height = 0;  /* 摄像头实际高度 */

/* JPEG 编码全局变量 */
static tjhandle g_jpeg_compressor = NULL;
static unsigned char *g_jpeg_buffer = NULL;
static unsigned long g_jpeg_buffer_size = 0;

#ifdef CONFIG_BOARD_ARCS_EVB
/**
 * @brief GPIOB 引脚配置
 */
static void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, CAM_PWDN_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

/**
 * @brief UART1 引脚配置
 */
static void lisa_uart1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER3);
}

/**
 * @brief I2C0 引脚配置
 */
static void lisa_i2c0_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 8);
}

/**
 * @brief DVP 引脚配置
 */
static void lisa_dvp_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_HSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_VSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_PCLK_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D0_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D1_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D2_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D3_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D4_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D5_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D6_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D7_PIN, CSK_IOMUX_FUNC_ALTER16);
}
#endif

/**
 * @brief 将 YUV422 打包格式(YUYV)转换为平面格式
 * @param yuyv_data YUYV 打包数据
 * @param width 图像宽度
 * @param height 图像高度
 * @param y_plane 输出的 Y 平面
 * @param u_plane 输出的 U 平面
 * @param v_plane 输出的 V 平面
 */
static void yuyv_to_yuv_planes(const uint8_t *yuyv_data, int width, int height,
                               uint8_t *y_plane, uint8_t *u_plane, uint8_t *v_plane)
{
    const uint8_t *src = yuyv_data;
    int y_index = 0;
    int uv_index = 0;

    /* YUYV 格式: Y0 U0 Y1 V0 | Y2 U1 Y3 V1 | ... */
    /* 每4个字节包含2个像素的完整信息 */
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j += 2) {
            /* 提取 Y0, U0, Y1, V0 */
            y_plane[y_index++] = src[0];  /* Y0 */
            u_plane[uv_index] = src[1];   /* U0 */
            y_plane[y_index++] = src[2];  /* Y1 */
            v_plane[uv_index] = src[3];   /* V0 */

            uv_index++;
            src += 4;
        }
    }
}

#if USE_HARDWARE_JPEG_ENCODER
/**
 * @brief 硬件JPEG编码 - 使用YUV422进行硬件编码
 * @param raw_data 原始图像数据（YUV422 YUYV格式）
 * @param width 图像宽度
 * @param height 图像高度
 * @param jpeg_data 输出的 JPEG 数据指针
 * @param jpeg_size 输出的 JPEG 数据大小
 * @return 0:成功, -1:失败
 */
static int encode_to_jpeg_hardware(const uint8_t *raw_data, int width, int height,
                                   unsigned char **jpeg_data, unsigned long *jpeg_size)
{
    int32_t ret = -1;
#if RTSP_DEBUG_TIMING
    uint32_t start_time, end_time;
#endif

    if (g_hw_jpeg_output_buffer == NULL) {
        LOGE("Hardware JPEG output buffer not initialized");
        return -1;
    }

#if RTSP_DEBUG_TIMING
    start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif

    /* 调用新的硬件编码接口 */
    ret = jpeg_encoder_encode(&g_jpeg_encoder_ctx,
                              (void *)raw_data,
                              (void *)g_hw_jpeg_output_buffer,
                              &g_hw_jpeg_output_size);

#if RTSP_DEBUG_TIMING
    end_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif

    if (ret == 0) {
        *jpeg_data = g_hw_jpeg_output_buffer;
        *jpeg_size = g_hw_jpeg_output_size;

#if RTSP_DEBUG_TIMING
        LOGI("硬件JPEG编码耗时: %u ms, 输出大小: %u bytes",
             end_time - start_time, g_hw_jpeg_output_size);
#endif
    } else {
        LOGE("硬件JPEG编码失败, 错误码: %d", ret);
    }

    return ret;
}
#endif

/**
 * @brief 软件JPEG编码 - 使用TurboJPEG库
 * @param raw_data 原始图像数据（YUV422 YUYV格式）
 * @param width 图像宽度
 * @param height 图像高度
 * @param jpeg_data 输出的 JPEG 数据指针
 * @param jpeg_size 输出的 JPEG 数据大小
 * @return 0:成功, -1:失败
 */
static int encode_to_jpeg_software(const uint8_t *raw_data, int width, int height,
                                   unsigned char **jpeg_data, unsigned long *jpeg_size)
{
    int ret;
    uint8_t *y_plane = NULL;
    uint8_t *u_plane = NULL;
    uint8_t *v_plane = NULL;
    const unsigned char *srcPlanes[3];
    int strides[3];
#if RTSP_DEBUG_TIMING
    uint32_t start_time, end_time;
#endif

    if (!g_jpeg_compressor) {
        LOGE("JPEG compressor not initialized");
        return -1;
    }

#if RTSP_DEBUG_TIMING
    start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif

    /* 分配 YUV 平面缓冲区 */
    y_plane = (uint8_t *)malloc(width * height);
    u_plane = (uint8_t *)malloc(width * height / 2);  /* 422格式: U/V是Y的一半 */
    v_plane = (uint8_t *)malloc(width * height / 2);

    if (!y_plane || !u_plane || !v_plane) {
        LOGE("Failed to allocate YUV plane buffers");
        free(y_plane);
        free(u_plane);
        free(v_plane);
        return -1;
    }

    /* 将 YUYV 打包格式转换为 YUV 平面格式 */
    yuyv_to_yuv_planes(raw_data, width, height, y_plane, u_plane, v_plane);

    /* 设置平面指针和步长 */
    srcPlanes[0] = y_plane;
    srcPlanes[1] = u_plane;
    srcPlanes[2] = v_plane;
    strides[0] = width;      /* Y 平面步长 */
    strides[1] = width / 2;  /* U 平面步长 (422格式) */
    strides[2] = width / 2;  /* V 平面步长 (422格式) */

    /* 使用 TurboJPEG 直接从 YUV 平面压缩为 JPEG */
    ret = tjCompressFromYUVPlanes(g_jpeg_compressor, srcPlanes, width, strides, height,
                                  TJSAMP_422, jpeg_data, jpeg_size, 85, 0);

    /* 释放临时缓冲区 */
    free(y_plane);
    free(u_plane);
    free(v_plane);

    if (ret < 0) {
        LOGE("Failed to compress image: %s", tjGetErrorStr2(g_jpeg_compressor));
        return -1;
    }

#if RTSP_DEBUG_TIMING
    end_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    LOGI("软件JPEG编码耗时: %u ms, 输出大小: %lu bytes", end_time - start_time, *jpeg_size);
#endif

    return 0;
}

/**
 * @brief 将摄像头原始数据编码为 JPEG (支持硬件和软件编码)
 * @param raw_data 原始图像数据（YUV422 YUYV格式）
 * @param width 图像宽度
 * @param height 图像高度
 * @param pixel_format 像素格式
 * @param jpeg_data 输出的 JPEG 数据指针
 * @param jpeg_size 输出的 JPEG 数据大小
 * @return 0:成功, -1:失败
 */
static int encode_to_jpeg(const uint8_t *raw_data, int width, int height,
                         int pixel_format, unsigned char **jpeg_data, unsigned long *jpeg_size)
{
#if USE_HARDWARE_JPEG_ENCODER
    /* 使用硬件JPEG编码 */
    return encode_to_jpeg_hardware(raw_data, width, height, jpeg_data, jpeg_size);
#else
    /* 使用软件JPEG编码 */
    return encode_to_jpeg_software(raw_data, width, height, jpeg_data, jpeg_size);
#endif
}

/**
 * @brief 初始化 JPEG 编码器
 */
static int jpeg_init(void)
{
#if USE_HARDWARE_JPEG_ENCODER
    /* 硬件JPEG编码器初始化 */
    LOGI("初始化硬件JPEG编码器");

    /* 预分配输出缓冲区 - 根据摄像头分辨率预估 */
    uint32_t estimated_size = g_camera_width * g_camera_height;
    g_hw_jpeg_output_buffer = (uint8_t *)malloc(estimated_size);
    if (g_hw_jpeg_output_buffer == NULL) {
        LOGE("Failed to allocate hardware JPEG output buffer (size: %u)", estimated_size);
        return -1;
    }
    g_hw_jpeg_output_size = 0;

    /* 初始化硬件JPEG编码器上下文 */
    jpeg_enc_cfg_t enc_cfg = {
        .width = g_camera_width,
        .height = g_camera_height,
        .input_format = 0x00,  /* YUV422格式 */
    };
    g_jpeg_encoder_ctx.in_ch = dma_2d_ch6;
    g_jpeg_encoder_ctx.out_ch = dma_2d_ch8;
    g_jpeg_encoder_ctx.transfer_ch = dma_2d_ch7;
    int32_t ret = jpeg_encoder_init(&g_jpeg_encoder_ctx, &enc_cfg);
    if (ret != 0) {
        LOGE("Failed to initialize JPEG encoder: %d", ret);
        free(g_hw_jpeg_output_buffer);
        g_hw_jpeg_output_buffer = NULL;
        return -1;
    }

    LOGI("硬件JPEG编码器初始化成功, 输出缓冲区: %u bytes", estimated_size);
    return 0;
#else
    /* 软件JPEG编码器初始化 */
    LOGI("初始化软件JPEG编码器(TurboJPEG)");

    g_jpeg_compressor = tjInitCompress();
    if (!g_jpeg_compressor) {
        LOGE("Failed to create JPEG compressor: %s", tjGetErrorStr());
        return -1;
    }

    LOGI("软件JPEG编码器初始化成功");
    return 0;
#endif
}

/**
 * @brief 初始化摄像头和JPEG编码器
 */
int camera_init(void)
{
    int ret;

    LOGI("=== Initializing Camera ===");

    /* 获取摄像头设备 */
    g_camera_dev = lisa_device_get(CAMERA_DEVICE);
    if (!lisa_device_ready(g_camera_dev)) {
        LOGE("Error: %s device not ready", CAMERA_DEVICE);
        return -1;
    }
    LOGI("%s device ready", CAMERA_DEVICE);

    /* 配置 GPIO */
    lisa_device_t *gpioa = lisa_device_get("gpioa");
    lisa_gpio_configure(gpioa, 23, LISA_GPIO_CONFIG_OUTPUT_HIGH);

    /* 获取 I2C 设备 */
    lisa_device_t *i2c_dev = lisa_device_get("i2c0");
    if (!lisa_device_ready(i2c_dev)) {
        LOGE("Error: %s device not ready", "i2c0");
        return -1;
    }

#ifdef CONFIG_BOARD_ARCS_EVB
    /* 配置摄像头参数 */
    lisa_camera_config_t config = {
        .hw_config = {
            .mclk_pad = CSK_IOMUX_PAD_A,
            .mclk_pin = CAM_MCLK_PIN,
            .pwdn_gpio_dev = lisa_device_get("gpiob"),
            .pwdn_pin = CAM_PWDN_PIN,
            .pwdn_delay_us = 0,
            .xclk_delay_us = 0,
            .i2c_dev = i2c_dev,
        },
        .xclk_freq_hz = 25000000,
        .fb_count = 3,
        .enable_hmirror = false,
        .enable_vflip = false,
        .enable_colorbar = false,
    };
#else
    /* 其他板型的默认配置 */
    lisa_camera_config_t config = {
        .hw_config = {
            .i2c_dev = i2c_dev,
        },
        .xclk_freq_hz = 12000000,
        .fb_count = 3,
        .enable_hmirror = false,
        .enable_vflip = false,
        .enable_colorbar = false,
    };
#endif

    LOGI("Setting up camera...");
    ret = lisa_camera_setup(g_camera_dev, &config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to setup camera: %d", ret);
        return -1;
    }

    /* 获取摄像头能力 */
    lisa_camera_capabilities_t caps;
    ret = lisa_camera_get_capabilities(g_camera_dev, &caps);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to get capabilities: %d", ret);
        return -1;
    }
    LOGI("Camera capabilities: max_width=%u, max_height=%u, supported_formats=0x%08X",
         caps.max_width, caps.max_height, caps.supported_formats);

    /* 配置 DVP 总线接口 */
    lisa_camera_bus_config_t bus_config = {
        .dma_channel = DMA_CHANNEL,
        .bus_type = LISA_CAMERA_BUS_DVP,
        .config.dvp = {
            .dvp_dev        = lisa_device_get(DVP_DEVICE),
            .dvp_freq       = config.xclk_freq_hz,
            .data_align     = 1,
            .line_offset    = 0,
            .pixel_offset   = 0,
            .pclk_polarity  = 0,
            .vsync_polarity = 1,
            .hsync_polarity = 1,
        }
    };
    lisa_camera_get_framesize(g_camera_dev, &bus_config.width, &bus_config.height);
    bus_config.pixel_format = lisa_camera_get_pixformat(g_camera_dev);

    /* 保存摄像头实际宽高到全局变量 */
    g_camera_width = bus_config.width;
    g_camera_height = bus_config.height;
    LOGI("Camera resolution: %ux%u", g_camera_width, g_camera_height);

    ret = lisa_camera_attach_bus(g_camera_dev, &bus_config);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to attach bus: %d", ret);
        return -1;
    }

    /* 启动摄像头 */
    LOGI("Starting camera...");
    ret = lisa_camera_start(g_camera_dev);
    if (ret != LISA_DEVICE_OK) {
        LOGE("Failed to start camera: %d", ret);
        return -1;
    }

    LOGI("Camera initialized successfully");

    /* 初始化 JPEG 编码器 */
    if (jpeg_init() != 0) {
        LOGE("Failed to initialize JPEG encoder");
        return -1;
    }

    return 0;
}

/**
 * @brief 捕获一帧图像并转换为 JPEG
 */
int camera_capture_frame(uint8_t **buffer, uint32_t *len)
{
    lisa_camera_fb_t *fb = NULL;
    int ret;

    /* 捕获一帧图像 */
    ret = lisa_camera_capture(g_camera_dev, &fb);
    if (ret != LISA_DEVICE_OK || fb == NULL) {
        LOGE("Capture failed: %d", ret);
        return -1;
    }

    /* 获取像素格式 */
    int pixel_format = lisa_camera_get_pixformat(g_camera_dev);

#if !USE_HARDWARE_JPEG_ENCODER
    /* 软件编码模式: 释放之前的 JPEG 缓冲区 */
    if (g_jpeg_buffer) {
        tjFree(g_jpeg_buffer);
        g_jpeg_buffer = NULL;
    }
#endif

    /* 将原始数据编码为 JPEG */
    ret = encode_to_jpeg(fb->buf, fb->width, fb->height, pixel_format,
                        &g_jpeg_buffer, &g_jpeg_buffer_size);
    if (ret != 0) {
        LOGE("Failed to encode to JPEG");
        lisa_camera_release_fb(g_camera_dev, fb);
        return -1;
    }

    /* 返回 JPEG 数据 */
    *buffer = g_jpeg_buffer;
    *len = g_jpeg_buffer_size;

    /* 释放摄像头帧缓冲区 */
    lisa_camera_release_fb(g_camera_dev, fb);

    return 0;
}

/**
 * @brief 获取摄像头实际分辨率
 */
void camera_get_resolution(uint32_t *width, uint32_t *height)
{
    if (width) *width = g_camera_width;
    if (height) *height = g_camera_height;
}
