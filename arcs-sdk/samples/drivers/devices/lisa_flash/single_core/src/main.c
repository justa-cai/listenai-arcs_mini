/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA Flash 读写示例
 *
 * 本示例演示如何使用 LISA Flash 驱动进行基本的读写擦除操作：
 * 1. 获取 Flash 设备（设备已自动初始化）
 * 2. 查询 Flash 参数和布局信息
 * 3. 执行擦除、写入、读取操作
 * 4. 验证数据完整性
 *
 * @note Flash 设备在系统启动时自动初始化，配置参数通过 Kconfig 设置
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_flash.h"

#include "FreeRTOS.h"
#include "task.h"

/* Flash 设备名称 */
#define FLASH_DEVICE "flash0"

/* 测试区域偏移地址和大小 */
#define TEST_OFFSET  0x100000   /* 测试区域起始偏移 1MB */
#define TEST_SIZE    256       /* 测试数据大小 */

/* 测试数据缓冲区 */
static uint8_t write_buf[TEST_SIZE];
static uint8_t read_buf[TEST_SIZE];

/**
 * @brief 打印 Flash 设备信息
 */
static void print_flash_info(lisa_device_t *flash)
{
    printf("\n=== Flash Device Information ===\n");

    /* 获取 Flash 参数 */
    const lisa_flash_parameters_t *params = lisa_flash_get_parameters(flash);
    if (!params) {
        printf("Error: Failed to get flash parameters\n");
        return;
    }

    printf("Write block size: %zu bytes\n",
           lisa_flash_params_get_write_block_size(params));
    printf("Erase value: 0x%02X\n",
           lisa_flash_params_get_erase_value(params));
    printf("No explicit erase: %s\n",
           lisa_flash_params_get_no_explicit_erase(params) ? "Yes" : "No");

    /* 获取页面布局信息 */
    size_t layout_count = 0;
    const lisa_flash_pages_layout_t *layout = lisa_flash_page_layout(flash, &layout_count);
    if (!layout) {
        printf("Error: Failed to get flash layout\n");
        return;
    }

    printf("\n=== Flash Layout ===\n");
    printf("Layout segments: %zu\n", layout_count);

    size_t total_offset = 0;
    for (size_t i = 0; i < layout_count; i++) {
        printf("Segment %zu: %zu pages × %zu bytes (offset: 0x%zx)\n",
               i,
               layout[i].pages_count,
               layout[i].pages_size,
               total_offset);
        total_offset += layout[i].pages_count * layout[i].pages_size;
    }

    size_t total_size = lisa_flash_get_size_from_layout(layout, layout_count);
    printf("Total flash size: %zu bytes (%zu KB)\n",
           total_size, total_size / 1024);
    printf("================================\n\n");
}

/**
 * @brief Flash 读写测试
 */
static int flash_read_write_test(lisa_device_t *flash)
{
    int ret;

    printf("\n=== Flash Read/Write Test ===\n");

    /* 获取 Flash 参数 */
    const lisa_flash_parameters_t *params = lisa_flash_get_parameters(flash);
    if (!params) {
        printf("Error: Failed to get flash parameters\n");
        return -1;
    }

    /* 获取页面布局 */
    size_t layout_count = 0;
    const lisa_flash_pages_layout_t *layout = lisa_flash_page_layout(flash, &layout_count);
    if (!layout || layout_count == 0) {
        printf("Error: Failed to get flash layout\n");
        return -1;
    }

    size_t page_size = layout[0].pages_size;
    printf("Using page size: %zu bytes\n", page_size);

    /* 1. 准备测试数据 */
    printf("\n1. Preparing test data...\n");
    for (size_t i = 0; i < TEST_SIZE; i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }
    printf("   Test data prepared: %zu bytes\n", TEST_SIZE);

    /* 2. 擦除 Flash 区域 */
    if (!lisa_flash_params_get_no_explicit_erase(params)) {
        printf("\n2. Erasing flash at offset 0x%x, size %zu bytes...\n",
               TEST_OFFSET, page_size);
        ret = lisa_flash_erase(flash, TEST_OFFSET, page_size);
        if (ret != 0) {
            printf("   Error: Erase failed with code %d\n", ret);
            return ret;
        }
        printf("   Erase successful\n");
    } else {
        printf("\n2. Skipping erase (device auto-erases)\n");
    }

    /* 3. 写入数据 */
    printf("\n3. Writing %zu bytes to flash at offset 0x%x...\n",
           TEST_SIZE, TEST_OFFSET);
    ret = lisa_flash_write(flash, TEST_OFFSET, write_buf, TEST_SIZE);
    if (ret != 0) {
        printf("   Error: Write failed with code %d\n", ret);
        return ret;
    }
    printf("   Write successful\n");

    /* 4. 读取数据 */
    printf("\n4. Reading %zu bytes from flash at offset 0x%x...\n",
           TEST_SIZE, TEST_OFFSET);
    memset(read_buf, 0, TEST_SIZE);
    ret = lisa_flash_read(flash, TEST_OFFSET, read_buf, TEST_SIZE);
    if (ret != 0) {
        printf("   Error: Read failed with code %d\n", ret);
        return ret;
    }
    printf("   Read successful\n");

    /* 5. 验证数据 */
    printf("\n5. Verifying data...\n");
    bool verify_ok = true;
    for (size_t i = 0; i < TEST_SIZE; i++) {
        if (read_buf[i] != write_buf[i]) {
            printf("   Error: Data mismatch at offset %zu: "
                   "expected 0x%02X, got 0x%02X\n",
                   i, write_buf[i], read_buf[i]);
            verify_ok = false;
            break;
        }
    }

    if (verify_ok) {
        printf("   Verification successful! All %zu bytes match.\n", TEST_SIZE);
    } else {
        printf("   Verification failed!\n");
        return -1;
    }

    /* 6. 打印前16字节数据 */
    printf("\n6. First 16 bytes of data:\n");
    printf("   Write: ");
    for (size_t i = 0; i < 16 && i < TEST_SIZE; i++) {
        printf("%02X ", write_buf[i]);
    }
    printf("\n   Read:  ");
    for (size_t i = 0; i < 16 && i < TEST_SIZE; i++) {
        printf("%02X ", read_buf[i]);
    }
    printf("\n");

    printf("\n=== Test Completed Successfully ===\n\n");
    return 0;
}

int main(int argc, char **argv)
{
    printf("\n");
    printf("========================================\n");
    printf("=== LISA Flash Read/Write Example ===\n");
    printf("========================================\n");

    /* 1. 获取 Flash 设备（设备已自动初始化） */
    printf("\n1. Getting flash device '%s'...\n", FLASH_DEVICE);
    lisa_device_t *flash = lisa_device_get(FLASH_DEVICE);
    if (!flash) {
        printf("Error: Failed to get flash device '%s'\n", FLASH_DEVICE);
        return -1;
    }

    /* 2. 检查设备状态 */
    if (!lisa_device_ready(flash)) {
        printf("Error: Flash device '%s' is not ready\n", FLASH_DEVICE);
        return -1;
    }
    printf("   Flash device '%s' is ready and auto-initialized at %p\n", FLASH_DEVICE, flash);

    /* 3. 打印 Flash 信息 */
    printf("\n2. Querying flash device information...\n");
    print_flash_info(flash);

    /* 4. 执行读写测试 */
    printf("\n3. Running read/write test...\n");
    int ret = flash_read_write_test(flash);
    if (ret != 0) {
        printf("\nFlash test FAILED!\n");
        return ret;
    }

    printf("\n========================================\n");
    printf("All tests passed successfully!\n");
    printf("========================================\n\n");

}
