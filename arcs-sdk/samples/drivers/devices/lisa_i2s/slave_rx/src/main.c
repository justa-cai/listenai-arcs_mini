/*
 * LISA I2S Slave RX 示例
 * 演示从模式接收音频数据
 * 从设备需要连接到主设备，由主设备提供时钟信号
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h> 
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_device.h"
#include "lisa_i2s.h"

#include "IOMuxManager.h"

#define TAG "main"
#include <lisa_log.h>

#if CONFIG_LISA_I2S0
#define I2S_DEVICE  "i2s0"
#endif

#if CONFIG_LISA_I2S1
#define I2S_DEVICE  "i2s1"
#endif

#define I2S0_BCK    CSK_IOMUX_PAD_A, 12, 9
#define I2S0_LRCK   CSK_IOMUX_PAD_A, 13, 9
#define I2S0_DIN    CSK_IOMUX_PAD_A, 14, 9
#define I2S0_DOUT   CSK_IOMUX_PAD_A, 15, 9

#define I2S1_BCK    CSK_IOMUX_PAD_A, 12, 10
#define I2S1_LRCK   CSK_IOMUX_PAD_A, 13, 10
#define I2S1_DIN    CSK_IOMUX_PAD_A, 14, 10
#define I2S1_DOUT   CSK_IOMUX_PAD_A, 15, 10

/**
 * @brief 为满足不同板型示例场景，重定向i2s设备的pinmux配置
 */
#ifdef CONFIG_BOARD_ARCS_EVB
#if CONFIG_LISA_I2S0
void lisa_i2s0_pinmux()
{
    IOMuxManager_PinConfigure(I2S0_LRCK);   // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S0_BCK);    // BCK
    IOMuxManager_PinConfigure(I2S0_DOUT);   // DOUT
    IOMuxManager_PinConfigure(I2S0_DIN);    // DIN
}
#endif
#if CONFIG_LISA_I2S1
void lisa_i2s1_pinmux()
{
    IOMuxManager_PinConfigure(I2S1_LRCK);   // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S1_BCK);    // BCK
    IOMuxManager_PinConfigure(I2S1_DOUT);   // DOUT
    IOMuxManager_PinConfigure(I2S1_DIN);    // DIN
}
#endif
#endif

/**
 * @brief I2S事件回调函数
 * @param event 事件类型（RX完成、RX溢出等）
 * @param user_data 用户数据指针
 */
static void i2s_event_callback(lisa_i2s_event_t event, void *user_data)
{
    if (event & LISA_I2S_EVENT_RX_DONE) {
        LISA_LOGI(TAG, "RX completed");
    }
    if (event & LISA_I2S_EVENT_RX_OVERRUN) {
        LISA_LOGE(TAG, "RX overrun error");
    }
}

int main(int argc, char **argv)
{
    LISA_LOGI(TAG, "========================================");
    LISA_LOGI(TAG, "  LISA I2S SLAVE RX Example");
    LISA_LOGI(TAG, "========================================");

    /* 1. 获取 I2S 设备 */
    lisa_device_t *i2s_dev = lisa_device_get(I2S_DEVICE);
    if (!i2s_dev) {
        LISA_LOGI(TAG, "Failed to get I2S device");
        return -1;
    }

    /* 2. 配置 I2S: （从模式，标准 I2S，16位，16kHz，立体声，rx, 单次传输16ms音频数据） */
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_RX();
    config.mode = LISA_I2S_MODE_SLAVE;
    int ret = lisa_i2s_configure(i2s_dev, &config);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to configure I2S: %d", ret);
        return -1;
    }

    /* 3. 设置回调函数 */
    ret = lisa_i2s_set_callback(i2s_dev, (lisa_i2s_event_callback_t)i2s_event_callback, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to set callback: %d", ret);
        return -1;
    }

    /* 4. 启动 I2S 接收 */
    ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_RX, LISA_I2S_CMD_START);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to start I2S: %d", ret);
        return -1;
    }

    /* 5. 循环接收数据 */
    uint8_t *rx_buffer = NULL;
    uint32_t len = 0;
    while (1) {
        ret = lisa_i2s_read(i2s_dev, &rx_buffer, &len, 100);
        if (ret < 0) {
            LISA_LOGE(TAG, "Failed to receive data: %d", ret);
        } else {
            LISA_LOGI(TAG, "Received %d bytes data", len);

            LISA_LOGI(TAG, "[0] %x [1] %x [2] %x [3] %x\n[4] %x [5] %x [6] %x [7] %x\n",
                    rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3], 
                    rx_buffer[4], rx_buffer[5], rx_buffer[6], rx_buffer[7]);
        }
    }

    /* 6. 停止 I2S 接收 */
    ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_RX, LISA_I2S_CMD_STOP);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to stop I2S: %d", ret);
        return -1;
    }
    
    LISA_LOGI(TAG, "\nI2S SLAVE RX completed");

    return 0;
}
