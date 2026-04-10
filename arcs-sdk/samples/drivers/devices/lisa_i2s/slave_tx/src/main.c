/*
 * LISA I2S Slave TX 示例
 * 演示从模式发送音频数据
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

#define I2S_DEVICE  "i2s0"

#define BLOCK_SIZE     1024

#define I2S0_BCK    CSK_IOMUX_PAD_A, 12, 9
#define I2S0_LRCK   CSK_IOMUX_PAD_A, 13, 9
#define I2S0_DIN    CSK_IOMUX_PAD_A, 14, 9
#define I2S0_DOUT   CSK_IOMUX_PAD_A, 15, 9

static uint8_t tx_buffer[BLOCK_SIZE];

/**
 * @brief 为满足不同板型示例场景，重定向i2s设备的pinmux配置
 */
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_i2s0_pinmux()
{
    IOMuxManager_PinConfigure(I2S0_LRCK);   // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S0_BCK);    // BCK
    IOMuxManager_PinConfigure(I2S0_DOUT);   // DOUT
    IOMuxManager_PinConfigure(I2S0_DIN);    // DIN
}
#endif

/**
 * @brief I2S事件回调函数
 * @param event 事件类型（TX完成、TX下溢等）
 * @param user_data 用户数据指针
 */
static void i2s_event_callback(lisa_i2s_event_t event, void *user_data)
{
    if (event & LISA_I2S_EVENT_TX_DONE) {
        LISA_LOGI(TAG, "TX completed");
    }
    if (event & LISA_I2S_EVENT_TX_UNDERRUN) {
        LISA_LOGI(TAG, "TX underrun error");
    }
}

/**
 * @brief 初始化发送缓冲区
 * @note 填充递增的uint8_t序列：0, 1, 2, 3, ...
 */
static void init_tx_buffer(void)
{
    for (int i = 0; i < BLOCK_SIZE; i++) {
        tx_buffer[i] = i;
    }
}

int main(int argc, char **argv)
{
    LISA_LOGI(TAG, "========================================");
    LISA_LOGI(TAG, "  LISA I2S SLAVE TX Example");
    LISA_LOGI(TAG, "========================================");

    /* 初始化发送缓冲区 */
    init_tx_buffer();

    /* 1. 获取 I2S 设备 */
    lisa_device_t *i2s_dev = lisa_device_get(I2S_DEVICE);
    if (!i2s_dev) {
        LISA_LOGI(TAG, "Failed to get I2S device");
        return -1;
    }

    /* 2. 配置 I2S: （从模式，标准 I2S，16位，16kHz，立体声，tx, 单次传输16ms音频数据） */
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
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

    /* 4. 预先加载要发送的数据, (建议write的次数最多CONFIG_LISA_I2S_BLOCK_COUNT，最少1个) */
    for(int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT - 1; i++) {
        ret = lisa_i2s_write(i2s_dev, (uint32_t *)tx_buffer, BLOCK_SIZE / sizeof(uint32_t), 0);
        if (ret < 0) {
            LISA_LOGE(TAG, "Failed to send data: %d", ret);
            return -1;
        }
    }

    /* 5. 启动 I2S 发送 */
    ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_TX, LISA_I2S_CMD_START);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to start I2S: %d", ret);
        return -1;
    }

    /* 6. 循环发送数据 */
    while (1) {
        ret = lisa_i2s_write(i2s_dev, (uint32_t *)tx_buffer, BLOCK_SIZE / sizeof(uint32_t), 100);
        if (ret < 0) {
            LISA_LOGE(TAG, "Failed to send data: %d", ret);
        }
    }

    /* 7. 停止 I2S 发送 */
    ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_TX, LISA_I2S_CMD_STOP);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to stop I2S: %d", ret);
        return -1;
    }
    
    LISA_LOGI(TAG, "\nI2S SLAVE TX completed");

    return 0;
}
