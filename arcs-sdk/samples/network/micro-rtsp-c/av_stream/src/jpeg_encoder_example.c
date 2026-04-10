/* JPEG编码器使用示例
 *
 * 这个示例展示了如何使用JPEG编码接口
 * 相比原来的jpeg_encoder_ext函数，这个接口避免了每次编码都重复初始化硬件
 */

#include "jpeg_encoder_ext.h"
#include <stdlib.h>
#include "IOMuxManager.h"
#include "lisa_device.h"
#include "lisa_uart.h"
#include "FreeRTOS.h"
#include "projdefs.h"
#include <lisa_log.h>

/* 全局编码器上下文 */
static jpeg_enc_ctx_t g_jpeg_encoder_ctx;
static uint8_t g_encoder_initialized = 0;

/* ========================================================================
 * 串口配置
 * ======================================================================== */

void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER3);
}

static lisa_device_t *uart_dev = NULL;
static volatile bool uart_tx_done = true;  /* 发送完成标志 */

/**
 * @brief UART 事件回调函数
 */
static void uart_event_callback(lisa_uart_event_t event, void *user_data)
{
    if (event == LISA_UART_EVENT_TX_DONE) {
        uart_tx_done = true;
    }
}

/**
 * @brief 初始化串口
 */
static int serial_init(void)
{
    /* 获取 UART 设备 */
    uart_dev = lisa_device_get("uart1");
    if (!lisa_device_ready(uart_dev)) {
        LOGE("UART1 device not ready");
        return -1;
    }

    /* 配置 UART: 3Mbps, 8N1, DMA 模式 */
    lisa_uart_config_t uart_cfg = {
        .baudrate = 3000000,
        .data_bits = LISA_UART_DATA_BITS_8,
        .stop_bits = LISA_UART_STOP_BITS_1,
        .parity = LISA_UART_PARITY_NONE,
        .flow_ctrl = LISA_UART_FLOW_CONTROL_NONE,
        .transfer_mode = LISA_UART_TRANSFER_MODE_DMA,
        .dma_tx_channel = 0xFF,  /* 自动分配 */
        .dma_rx_channel = 0xFF,
    };

    int ret = lisa_uart_configure(uart_dev, &uart_cfg);
    if (ret != LISA_DEVICE_OK) {
        LOGE("UART configure failed: %d", ret);
        return ret;
    }

    /* 设置事件回调 */
    lisa_uart_set_callback(uart_dev, uart_event_callback, NULL);

    return 0;
}

/**
 * @brief 异步发送数据
 */
static void serial_send(const uint8_t *data, uint32_t len)
{
    LOGI("Send JPEG: %02x %02x %02x ... %02x %02x %02x, len=%u",
         data[0], data[1], data[2],
         data[len - 3], data[len - 2], data[len - 1], len);

    /* 标记为发送中 */
    uart_tx_done = false;

    /* 异步发送 */
    int ret = lisa_uart_write_async(uart_dev, data, len);
    if (ret < 0) {
        LOGE("UART send failed: %d", ret);
        uart_tx_done = true;  /* 发送失败，恢复标志 */
    }
}

/**
 * @brief 初始化JPEG编码器（应用启动时调用一次）
 *
 * @param width 图像宽度
 * @param height 图像高度
 * @param format 输入格式 (0x00=YUV422, 0x01=YUV420, 0x05=RGB888等)
 * @return int32_t 0表示成功
 */
int32_t app_jpeg_encoder_init(uint16_t width, uint16_t height, uint8_t format)
{
    jpeg_enc_cfg_t cfg = {
        .width = width,
        .height = height,
        .input_format = format,
    };

    int32_t ret = jpeg_encoder_init(&g_jpeg_encoder_ctx, &cfg);
    if (ret == 0) {
        g_encoder_initialized = 1;
    }

    return ret;
}

/**
 * @brief 编码一帧图像（每次需要编码时调用）
 *
 * @param in_buf 输入图像数据
 * @param out_buf 输出JPEG数据缓冲区
 * @param out_size 输出JPEG数据大小
 * @return int32_t 0表示成功
 */
int32_t app_jpeg_encode_frame(void *in_buf, void *out_buf, uint32_t *out_size)
{
    if (!g_encoder_initialized) {
        return -1;
    }

    return jpeg_encoder_encode(&g_jpeg_encoder_ctx, in_buf, out_buf, out_size);
}

/**
 * @brief 停止JPEG编码器（应用退出时调用）
 *
 * @return int32_t 0表示成功
 */
int32_t app_jpeg_encoder_stop(void)
{
    if (!g_encoder_initialized) {
        return -1;
    }

    int32_t ret = jpeg_encoder_stop(&g_jpeg_encoder_ctx);
    if (ret == 0) {
        // g_encoder_initialized = 0;
    }

    return ret;
}

/**
 * @brief 生成纯色YUV422图像用于测试
 *
 * @param width 图像宽度
 * @param height 图像高度
 * @param y Y分量值 (亮度, 0-255)
 * @param u U分量值 (色度, 0-255)
 * @param v V分量值 (色度, 0-255)
 * @return uint8_t* 返回分配的图像缓冲区,需要调用者释放
 */
uint8_t* generate_solid_yuv422_image(uint16_t width, uint16_t height,
                                     uint8_t y, uint8_t u, uint8_t v)
{
    /* YUV422格式: Y0 U0 Y1 V0 (每2个像素4字节) */
    uint32_t image_size = width * height * 2;
    uint8_t *image = (uint8_t*)malloc(image_size);

    if (!image) {
        return NULL;
    }

    /* 填充YUV422数据 */
    for (uint32_t i = 0; i < image_size; i += 4) {
        image[i + 0] = y;  /* Y0 */
        image[i + 1] = u;  /* U0 */
        image[i + 2] = y;  /* Y1 */
        image[i + 3] = v;  /* V0 */
    }

    return image;
}

/**
 * @brief 测试160x160纯色YUV422图像编码
 *
 * @return int32_t 0表示成功
 */
// int32_t test_160x160_solid_color_encode(void)
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    int32_t ret = 0;
    uint8_t *input_image = NULL;
    uint8_t *output_jpeg = NULL;
    uint32_t jpeg_size = 0;

    /* 初始化串口 */
    if (serial_init() != 0) {
        LOGE("Serial init failed");
        return -1;
    }
    LOGI("Serial initialized (3Mbps)");

    /* 1. 初始化编码器 */
    ret = app_jpeg_encoder_init(160, 160, 0x00); /* YUV422格式 */
    if (ret != 0) {
        return ret;
    }

    /* 2. 生成纯色图像 */
    /* 红色: Y=76, U=84, V=255 */
    input_image = generate_solid_yuv422_image(160, 160, 76, 84, 255);
    if (!input_image) {
        app_jpeg_encoder_stop();
        return -1;
    }

    /* 3. 分配输出缓冲区 */
    output_jpeg = (uint8_t*)malloc((size_t)160 * 160 * 2);
    if (!output_jpeg) {
        free(input_image);
        app_jpeg_encoder_stop();
        return -1;
    }

    while (1) {
        /* 4. 执行编码 */
        ret = app_jpeg_encode_frame(input_image, output_jpeg, &jpeg_size);
        if (ret != 0) {
            free(output_jpeg);
            free(input_image);
            app_jpeg_encoder_stop();
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    LOGI("JPEG encode success, size=%u bytes", jpeg_size);

    /* 5. 通过串口发送JPEG图像 (只发送一张) */
    serial_send(output_jpeg, jpeg_size);

    /* 6. 等待发送完成 */
    while (!uart_tx_done) {
        /* 等待发送完成 */
    }
    LOGI("JPEG send completed");

    while (1);
    /* 7. 清理资源 */
    free(output_jpeg);
    free(input_image);
    app_jpeg_encoder_stop();

    return 0;
}

/* 使用示例 */
#if 0
void example_usage(void)
{
    /* 1. 应用启动时初始化编码器（只调用一次） */
    app_jpeg_encoder_init(640, 480, 0x00); /* YUV422格式 */

    /* 2. 每次需要编码时调用（可以连续调用多次） */
    uint8_t *input_image = ...; /* 摄像头图像数据 */
    uint8_t *output_jpeg = malloc(640 * 480 * 2); /* JPEG输出缓冲区 */
    uint32_t jpeg_size = 0;

    for (int i = 0; i < 100; i++) {
        /* 快速编码，无重复初始化 */
        app_jpeg_encode_frame(input_image, output_jpeg, &jpeg_size);

        /* 发送JPEG数据... */
        send_jpeg_data(output_jpeg, jpeg_size);
    }

    /* 3. 应用退出时停止编码器 */
    app_jpeg_encoder_stop();
    free(output_jpeg);
}
#endif
