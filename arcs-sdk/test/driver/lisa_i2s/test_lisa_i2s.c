/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_lisa_i2s.c
 * @brief LISA I2S 驱动功能测试
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_i2s.h"
#include "lisa_device.h"
#include "IOMuxManager.h"

#define I2S_DEVICE_NAME        "i2s0"
#define BLOCK_SIZE             1024
#define TEST_BUFFER_SIZE       2048

/* I2S0 引脚定义 */
#define I2S0_BCK    CSK_IOMUX_PAD_A, 12, 9
#define I2S0_LRCK   CSK_IOMUX_PAD_A, 13, 9
#define I2S0_DIN    CSK_IOMUX_PAD_A, 14, 9
#define I2S0_DOUT   CSK_IOMUX_PAD_A, 15, 9

static lisa_device_t *g_i2s_dev = NULL;
static volatile uint32_t g_tx_done_count = 0;
static volatile uint32_t g_rx_done_count = 0;
static volatile uint32_t g_tx_underrun_count = 0;
static volatile uint32_t g_rx_overrun_count = 0;
static volatile uint32_t g_error_count = 0;

static uint8_t g_tx_buffer[TEST_BUFFER_SIZE] __attribute__((aligned(32)));
static uint8_t g_rx_buffer[TEST_BUFFER_SIZE] __attribute__((aligned(32)));

/**
 * @brief 配置 I2S0 引脚复用
 * @note 为满足不同板型测试场景，配置 I2S 设备的 pinmux
 */
#ifdef CONFIG_BOARD_ARCS_EVB
static void lisa_i2s0_pinmux_init(void)
{
    IOMuxManager_PinConfigure(I2S0_LRCK);   // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S0_BCK);    // BCK
    IOMuxManager_PinConfigure(I2S0_DOUT);   // DOUT
    IOMuxManager_PinConfigure(I2S0_DIN);    // DIN
}
#else
static void lisa_i2s0_pinmux_init(void)
{
    /* 其他板型的引脚配置 */
}
#endif

static void i2s_test_callback(lisa_i2s_event_t event, void *user_data)
{
    (void)user_data;


    if (event & LISA_I2S_EVENT_TX_DONE) {
        g_tx_done_count++;
    }
    if (event & LISA_I2S_EVENT_RX_DONE) {
        g_rx_done_count++;
    }
    if (event & LISA_I2S_EVENT_TX_UNDERRUN) {
        g_tx_underrun_count++;
    }
    if (event & LISA_I2S_EVENT_RX_OVERRUN) {
        g_rx_overrun_count++;
    }
    if (event & LISA_I2S_EVENT_ERROR) {
        g_error_count++;
    }
}

static void fill_tx_buffer(void)
{
    uint32_t *data = (uint32_t *)g_tx_buffer;
    for (uint32_t i = 0; i < TEST_BUFFER_SIZE / sizeof(uint32_t); i++) {
        data[i] = i;
    }
}

void setUp(void)
{
    /* 重置事件计数器 */
    g_tx_done_count = 0;
    g_rx_done_count = 0;
    g_tx_underrun_count = 0;
    g_rx_overrun_count = 0;
    g_error_count = 0;
    
    /* 清空缓冲区 */
    memset(g_tx_buffer, 0, sizeof(g_tx_buffer));
    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
}

void tearDown(void)
{
    /* 停止所有 I2S 传输 */
    if (g_i2s_dev) {
        (void)lisa_i2s_trigger(g_i2s_dev, LISA_I2S_DIRECTION_BOTH, LISA_I2S_CMD_STOP);
        
        /* 清除回调函数，避免影响下一个测试 */
        (void)lisa_i2s_set_callback(g_i2s_dev, NULL, NULL);
    }

    /* 重置事件计数器 */
    g_tx_done_count = 0;
    g_rx_done_count = 0;
    g_tx_underrun_count = 0;
    g_rx_overrun_count = 0;
    g_error_count = 0;
    
    /* 短暂延时，确保硬件状态稳定 */
    vTaskDelay(pdMS_TO_TICKS(10));
}

void test_i2s_get_device(void)
{
    /* 初始化 I2S 引脚复用 */
    lisa_i2s0_pinmux_init();
    
    /* 获取 I2S 设备 */
    g_i2s_dev = lisa_device_get(I2S_DEVICE_NAME);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_i2s_dev, "Failed to get i2s device");
}

void test_i2s_set_callback(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    int ret = lisa_i2s_set_callback(g_i2s_dev, i2s_test_callback, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "set callback failed");
}

void test_i2s_configure_tx(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
    config.block_size = BLOCK_SIZE;
    
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "configure TX failed");
}

void test_i2s_configure_rx(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_RX();
    config.block_size = BLOCK_SIZE;
    
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "configure RX failed");
}

void test_i2s_configure_both(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
    config.direction = LISA_I2S_DIRECTION_BOTH;
    config.block_size = BLOCK_SIZE;
    
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "configure BOTH failed");
}

void test_i2s_get_config(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
    config.block_size = BLOCK_SIZE;
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    lisa_i2s_config_t get_config = {0};
    ret = lisa_i2s_get_config(g_i2s_dev, &get_config);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    TEST_ASSERT_EQUAL_UINT32(BLOCK_SIZE, get_config.block_size);
    TEST_ASSERT_EQUAL_INT(LISA_I2S_MODE_MASTER, get_config.mode);
    TEST_ASSERT_EQUAL_INT(LISA_I2S_DIRECTION_TX, get_config.direction);
}

void test_i2s_write(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
    config.block_size = BLOCK_SIZE;
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    ret = lisa_i2s_set_callback(g_i2s_dev, i2s_test_callback, NULL);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    fill_tx_buffer();
    
    uint32_t *data = (uint32_t *)g_tx_buffer;
    ret = lisa_i2s_write(g_i2s_dev, data, BLOCK_SIZE / sizeof(uint32_t), 100);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "write failed");
}

void test_i2s_trigger_tx(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
    config.block_size = BLOCK_SIZE;
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    ret = lisa_i2s_set_callback(g_i2s_dev, i2s_test_callback, NULL);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    fill_tx_buffer();
    uint32_t *data = (uint32_t *)g_tx_buffer;
    
    /* 写入多个 buffer，避免 underrun */
    for (int i = 0; i < 3; i++) {
        ret = lisa_i2s_write(g_i2s_dev, data, BLOCK_SIZE / sizeof(uint32_t), 100);
        TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    }
    
    ret = lisa_i2s_trigger(g_i2s_dev, LISA_I2S_DIRECTION_TX, LISA_I2S_CMD_START);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "start TX failed");
    
    /* 运行一段时间，期间持续写入数据保持队列不为空 */
    for (int i = 0; i < 20; i++) {
        vTaskDelay(pdMS_TO_TICKS(5));
        /* 非阻塞写入，如果队列满了就跳过 */
        ret = lisa_i2s_write(g_i2s_dev, data, BLOCK_SIZE / sizeof(uint32_t), 0);
        (void)ret;  /* 忽略返回值，队列满是正常的 */
    }
    
    ret = lisa_i2s_trigger(g_i2s_dev, LISA_I2S_DIRECTION_TX, LISA_I2S_CMD_STOP);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "stop TX failed");
    
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, g_tx_done_count, "no TX done events");
}

void test_i2s_trigger_rx(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_RX();
    config.block_size = BLOCK_SIZE;
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    ret = lisa_i2s_set_callback(g_i2s_dev, i2s_test_callback, NULL);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    ret = lisa_i2s_trigger(g_i2s_dev, LISA_I2S_DIRECTION_RX, LISA_I2S_CMD_START);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "start RX failed");
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ret = lisa_i2s_trigger(g_i2s_dev, LISA_I2S_DIRECTION_RX, LISA_I2S_CMD_STOP);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "stop RX failed");
}

void test_i2s_read(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_RX();
    config.block_size = BLOCK_SIZE;
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    ret = lisa_i2s_trigger(g_i2s_dev, LISA_I2S_DIRECTION_RX, LISA_I2S_CMD_START);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    
    uint8_t *rx_data = NULL;
    uint32_t rx_len = 0;
    ret = lisa_i2s_read(g_i2s_dev, &rx_data, &rx_len, 200);
    
    lisa_i2s_trigger(g_i2s_dev, LISA_I2S_DIRECTION_RX, LISA_I2S_CMD_STOP);
    
    if (ret == LISA_DEVICE_OK) {
        TEST_ASSERT_NOT_NULL(rx_data);
        TEST_ASSERT_GREATER_THAN_UINT32(0, rx_len);
    }
}

void test_i2s_different_sample_rates(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_sample_rate_t rates[] = {
        LISA_I2S_SAMPLE_RATE_8K,
        LISA_I2S_SAMPLE_RATE_16K,
        LISA_I2S_SAMPLE_RATE_48K,
    };
    
    for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
        lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
        config.sample_rate = rates[i];
        config.block_size = BLOCK_SIZE;
        
        int ret = lisa_i2s_configure(g_i2s_dev, &config);
        TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "configure with different sample rate failed");
    }
}

void test_i2s_different_data_widths(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_data_width_t widths[] = {
        LISA_I2S_DATA_WIDTH_16BIT,
        LISA_I2S_DATA_WIDTH_24BIT_HIGH,
        LISA_I2S_DATA_WIDTH_32BIT,
    };
    
    for (size_t i = 0; i < sizeof(widths) / sizeof(widths[0]); i++) {
        lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
        config.data_width = widths[i];
        config.block_size = BLOCK_SIZE;
        
        int ret = lisa_i2s_configure(g_i2s_dev, &config);
        TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "configure with different data width failed");
    }
}

void test_i2s_master_slave_mode(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
    config.mode = LISA_I2S_MODE_MASTER;
    config.block_size = BLOCK_SIZE;
    int ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "configure master mode failed");
    
    config.mode = LISA_I2S_MODE_SLAVE;
    ret = lisa_i2s_configure(g_i2s_dev, &config);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "configure slave mode failed");
}

void test_i2s_invalid_parameters(void)
{
    TEST_ASSERT_NOT_NULL(g_i2s_dev);
    
    int ret = lisa_i2s_configure(g_i2s_dev, NULL);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_ERR_INVALID, ret);
    
    lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
    ret = lisa_i2s_configure(NULL, &config);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_ERR_INVALID, ret);
    
    ret = lisa_i2s_write(g_i2s_dev, NULL, 100, 0);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_ERR_INVALID, ret);
    
    uint32_t data[10];
    ret = lisa_i2s_write(g_i2s_dev, data, 0, 0);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_ERR_INVALID, ret);
}

int main(void)
{
    UnityBegin("test/driver/lisa_i2s/test_lisa_i2s.c");

    RUN_TEST(test_i2s_get_device, __LINE__);
    RUN_TEST(test_i2s_set_callback, __LINE__);
    RUN_TEST(test_i2s_configure_tx, __LINE__);
    RUN_TEST(test_i2s_configure_rx, __LINE__);
    RUN_TEST(test_i2s_configure_both, __LINE__);
    RUN_TEST(test_i2s_get_config, __LINE__);
    RUN_TEST(test_i2s_write, __LINE__);
    RUN_TEST(test_i2s_trigger_tx, __LINE__);
    RUN_TEST(test_i2s_trigger_rx, __LINE__);
    RUN_TEST(test_i2s_read, __LINE__);
    RUN_TEST(test_i2s_different_sample_rates, __LINE__);
    RUN_TEST(test_i2s_different_data_widths, __LINE__);
    RUN_TEST(test_i2s_master_slave_mode, __LINE__);
    RUN_TEST(test_i2s_invalid_parameters, __LINE__);

    return UnityEnd();
}
