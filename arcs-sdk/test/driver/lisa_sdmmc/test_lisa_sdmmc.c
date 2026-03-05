/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_lisa_sdmmc.c
 * @brief LISA SDMMC driver unit tests
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>

#include "lisa_device.h"
#include "lisa_sdmmc.h"
#include "IOMuxManager.h"

/* SDMMC 引脚定义 (根据硬件原理图配置) */
#define SDMMC_CLK_PAD       CSK_IOMUX_PAD_A
#define SDMMC_CLK_PIN       6
#define SDMMC_CMD_PAD       CSK_IOMUX_PAD_A
#define SDMMC_CMD_PIN       7
#define SDMMC_DAT0_PAD      CSK_IOMUX_PAD_A
#define SDMMC_DAT0_PIN      5
#define SDMMC_DAT1_PAD      CSK_IOMUX_PAD_A
#define SDMMC_DAT1_PIN      4
#define SDMMC_DAT2_PAD      CSK_IOMUX_PAD_A
#define SDMMC_DAT2_PIN      9
#define SDMMC_DAT3_PAD      CSK_IOMUX_PAD_A
#define SDMMC_DAT3_PIN      8
#define SDMMC_FUNC          CSK_IOMUX_FUNC_ALTER15

/*=======Test Configuration=====*/
#define DISK_DEVICE_NAME    "sdmmc0"
#define SECTOR_SIZE         512
#define TEST_SECTOR_START   2048    /* Start at 1MB offset to avoid filesystem */
#define TEST_SECTOR_COUNT   8

/*=======Test Buffers=====*/
static uint8_t write_buf[SECTOR_SIZE * TEST_SECTOR_COUNT] __attribute__((aligned(64)));
static uint8_t read_buf[SECTOR_SIZE * TEST_SECTOR_COUNT] __attribute__((aligned(64)));

/*=======Global State=====*/
static lisa_device_t *g_disk = NULL;

/*=======Pin Configuration=====*/
static void sdmmc_pin_config(void)
{
    IOMuxManager_PinConfigure(SDMMC_CLK_PAD, SDMMC_CLK_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_CMD_PAD, SDMMC_CMD_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT0_PAD, SDMMC_DAT0_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT1_PAD, SDMMC_DAT1_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT2_PAD, SDMMC_DAT2_PIN, SDMMC_FUNC);
    IOMuxManager_PinConfigure(SDMMC_DAT3_PAD, SDMMC_DAT3_PIN, SDMMC_FUNC);
}

/*=======Setup and Teardown=====*/
void setUp(void)
{
    memset(write_buf, 0, sizeof(write_buf));
    memset(read_buf, 0, sizeof(read_buf));
}

void tearDown(void)
{
}

/*=======Helper Functions=====*/
static void fill_pattern(uint8_t *buf, size_t size, uint8_t seed)
{
    for (size_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)((seed + i) & 0xFF);
    }
}

/*=======Test Cases=====*/

/* Test: Get disk device */
void test_disk_get_device(void)
{
    lisa_device_t *disk = lisa_device_get(DISK_DEVICE_NAME);
    TEST_ASSERT_NOT_NULL_MESSAGE(disk, "Failed to get disk device");
    g_disk = disk;
}

/* Test: Initialize disk */
void test_disk_init(void)
{
    TEST_ASSERT_NOT_NULL(g_disk);
    int ret = lisa_sdmmc_probe(g_disk);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "Disk init failed");
}

/* Test: Check disk status */
void test_disk_status(void)
{
    TEST_ASSERT_NOT_NULL(g_disk);
    int status = lisa_sdmmc_status(g_disk);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_SDMMC_STATUS_OK, status, "Disk not ready");
}

/* Test: Get sector count */
void test_disk_get_sector_count(void)
{
    TEST_ASSERT_NOT_NULL(g_disk);
    uint32_t sector_count = 0;
    int ret = lisa_sdmmc_get_sector_count(g_disk, &sector_count);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, sector_count, "Sector count should be > 0");
}

/* Test: Get sector size */
void test_disk_get_sector_size(void)
{
    TEST_ASSERT_NOT_NULL(g_disk);
    uint32_t sector_size = 0;
    int ret = lisa_sdmmc_get_sector_size(g_disk, &sector_size);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(SECTOR_SIZE, sector_size, "Expected 512 byte sectors");
}

/* Test: Single sector write and read */
void test_disk_single_sector_rw(void)
{
    TEST_ASSERT_NOT_NULL(g_disk);
    int ret;

    /* Prepare test data */
    fill_pattern(write_buf, SECTOR_SIZE, 0xAA);

    /* Write */
    ret = lisa_sdmmc_write(g_disk, write_buf, TEST_SECTOR_START, 1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "Single sector write failed");

    /* Sync */
    lisa_sdmmc_sync(g_disk);

    /* Read back */
    ret = lisa_sdmmc_read(g_disk, read_buf, TEST_SECTOR_START, 1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "Single sector read failed");

    /* Verify */
    TEST_ASSERT_EQUAL_MEMORY(write_buf, read_buf, SECTOR_SIZE);
}

/* Test: Multi sector write and read */
void test_disk_multi_sector_rw(void)
{
    TEST_ASSERT_NOT_NULL(g_disk);
    int ret;
    size_t total_size = SECTOR_SIZE * TEST_SECTOR_COUNT;

    /* Prepare test data */
    fill_pattern(write_buf, total_size, 0x55);

    /* Write */
    ret = lisa_sdmmc_write(g_disk, write_buf, TEST_SECTOR_START + 8, TEST_SECTOR_COUNT);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "Multi sector write failed");

    /* Sync */
    lisa_sdmmc_sync(g_disk);

    /* Read back */
    ret = lisa_sdmmc_read(g_disk, read_buf, TEST_SECTOR_START + 8, TEST_SECTOR_COUNT);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LISA_DEVICE_OK, ret, "Multi sector read failed");

    /* Verify */
    TEST_ASSERT_EQUAL_MEMORY(write_buf, read_buf, total_size);
}

/* Test: Sync operation */
void test_disk_sync(void)
{
    TEST_ASSERT_NOT_NULL(g_disk);
    int ret = lisa_sdmmc_sync(g_disk);
    TEST_ASSERT_EQUAL_INT(LISA_DEVICE_OK, ret);
}

/* Test: Invalid ioctl command */
void test_disk_ioctl_invalid(void)
{
    TEST_ASSERT_NOT_NULL(g_disk);
    uint32_t dummy = 0;
    int ret = lisa_sdmmc_ioctl(g_disk, 0xFF, &dummy);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LISA_DEVICE_OK, ret, "Invalid ioctl should fail");
}

/*=======Main=====*/
int main(void)
{
    /* Configure SDMMC pins before any disk operations */
    sdmmc_pin_config();

    UnityBegin("test/driver/lisa_sdmmc/test_lisa_sdmmc.c");

    /* Initialization tests */
    RUN_TEST(test_disk_get_device, __LINE__);
    RUN_TEST(test_disk_init, __LINE__);
    RUN_TEST(test_disk_status, __LINE__);

    /* Info query tests */
    RUN_TEST(test_disk_get_sector_count, __LINE__);
    RUN_TEST(test_disk_get_sector_size, __LINE__);

    /* Read/Write tests */
    RUN_TEST(test_disk_single_sector_rw, __LINE__);
    RUN_TEST(test_disk_multi_sector_rw, __LINE__);
    RUN_TEST(test_disk_sync, __LINE__);

    /* Error handling tests */
    RUN_TEST(test_disk_ioctl_invalid, __LINE__);

    return UnityEnd();
}
