/*
 * LISA I2S Slave RX & TX 示例
 * 演示从模式同时发送和接收音频数据
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

#define I2S_DEVICE  "i2s1"

#define BLOCK_SIZE     1024

#define I2S1_BCK    CSK_IOMUX_PAD_A, 12, 10
#define I2S1_LRCK   CSK_IOMUX_PAD_A, 13, 10
#define I2S1_DIN    CSK_IOMUX_PAD_A, 14, 10
#define I2S1_DOUT   CSK_IOMUX_PAD_A, 15, 10

static uint8_t tx_buffer[BLOCK_SIZE];

/**
 * @brief 为满足不同板型示例场景，重定向i2s设备的pinmux配置
 */
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_i2s1_pinmux()
{
    IOMuxManager_PinConfigure(I2S1_LRCK);   // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S1_BCK);    // BCK
    IOMuxManager_PinConfigure(I2S1_DOUT);   // DOUT
    IOMuxManager_PinConfigure(I2S1_DIN);    // DIN
}
#endif

/**
 * @brief I2S事件回调函数
 * @param event 事件类型（TX完成、TX下溢、RX完成、RX溢出等）
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

    if (event & LISA_I2S_EVENT_RX_DONE) {
        LISA_LOGI(TAG, "RX completed");
    }
    if (event & LISA_I2S_EVENT_RX_OVERRUN) {
        LISA_LOGI(TAG, "RX overrun error");
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

/**
 * @brief I2S发送任务
 * @param pvParameters I2S设备指针
 * @note 循环发送固定的测试数据
 */
static void i2s_tx_task(void *pvParameters)
{
    lisa_device_t *i2s_dev = (lisa_device_t *)pvParameters;
    
    while (1) {
        int ret = lisa_i2s_write(i2s_dev, (uint32_t *)tx_buffer, BLOCK_SIZE / sizeof(uint32_t), 100);
        if (ret < 0) {
            LISA_LOGE(TAG, "Failed to send data: %d", ret);
        }
    }

    vTaskDelete(NULL);
}

/**
 * @brief I2S接收任务
 * @param pvParameters I2S设备指针
 * @note 循环接收数据
 */
static void i2s_rx_task(void *pvParameters)
{
    lisa_device_t *i2s_dev = (lisa_device_t *)pvParameters;

    int ret = 0;
    uint32_t len = 0;
    uint8_t *rx_buffer = NULL;
    
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

    vTaskDelete(NULL);
}


int main(int argc, char **argv)
{
    LISA_LOGI(TAG, "========================================");
    LISA_LOGI(TAG, "  LISA I2S SLAVE TX RX Example");
    LISA_LOGI(TAG, "========================================");

    /* 初始化发送缓冲区 */
    init_tx_buffer();

    /* 1. 获取 I2S 设备 */
    lisa_device_t *i2s_dev = lisa_device_get(I2S_DEVICE);
    if (!i2s_dev) {
        LISA_LOGI(TAG, "Failed to get I2S device");
        return -1;
    }

    /* 2. 配置 I2S: 使用默认配置并设置为从模式双向模式（同时TX和RX） */
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
    config.mode = LISA_I2S_MODE_SLAVE;
    config.direction = LISA_I2S_DIRECTION_BOTH;

    /* 对应(16位，16kHz，立体声)16ms的音频数据，即一次性传输16ms的音频数据
     * 注意config.block_size一定要是32的倍数
     */
    config.block_size = BLOCK_SIZE;
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

    /* 5. 启动 I2S 收发 */
    ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_BOTH, LISA_I2S_CMD_START);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to start I2S: %d", ret);
        return -1;
    }

    /* 6. 创建接收和发送任务 */
    xTaskCreate(i2s_rx_task, "i2s rx", 4096, i2s_dev, 9, NULL);
    xTaskCreate(i2s_tx_task, "i2s tx", 4096, i2s_dev, 8, NULL);

    /* 7. 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 8. 停止 I2S 收发 */
    ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_BOTH, LISA_I2S_CMD_STOP);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to stop I2S: %d", ret);
        return -1;
    }
    
    LISA_LOGI(TAG, "\nI2S Slave completed");

    return 0;
}
