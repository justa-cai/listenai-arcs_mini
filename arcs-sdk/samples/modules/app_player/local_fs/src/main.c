/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief app_player 本地文件系统播放示例
 *
 * 本示例演示如何使用 app_player 从 SD 卡播放音频文件,包括:
 * 1. 初始化文件系统 (SD 卡挂载)
 * 2. 创建播放器实例
 * 3. 播放、暂停、恢复、停止音频文件
 */

#include "FreeRTOS.h"
#include "task.h"

#define TAG "app_player_sample"
#include "lisa_log.h"

#include "IOMuxManager.h"
#include "app_player.h"
#include "lisa_gpio.h"
#include "lsfs.h"
#include "lvfs.h"
#include "disk/disk_access.h"
#include "lisa_sdmmc.h"

/* 文件系统配置 */
#define SDMMC_DEVICE      "SD:"
#define SDMMC_MOUNT_POINT "/"SDMMC_DEVICE

/* PA 控制 GPIO 配置 */
#ifdef CONFIG_BOARD_ARCS_EVB
#define PA_PIN_NUM     27
#define PA_GPIO_DEVICE "gpioa"

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, PA_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
}
#endif

static struct lsfs_mount_t sdmmc_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = SDMMC_MOUNT_POINT,
    .fs_data = NULL,
};

/**
 * @brief 播放器事件回调函数
 */
static void player_event_callback(app_player_t *player, app_player_event_t event, void *user_data)
{
    switch (event) {
    case APP_PLAYER_EVENT_PREPARED:
        LOGI("Player prepared");
        break;
    case APP_PLAYER_EVENT_PLAYING:
        LOGI("Player playing");
        break;
    case APP_PLAYER_EVENT_COMPLETED:
        LOGI("Player completed");
        break;
    case APP_PLAYER_EVENT_ERROR:
        LOGE("Player error");
        break;
    case APP_PLAYER_EVENT_STOPPED:
        LOGI("Player stopped");
        break;
    default:
        break;
    }
}

/**
 * @brief PA 控制回调函数
 */
static int pa_control_callback(int onoff)
{
    LOGI("PA %s", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE), PA_PIN_NUM,
                                onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}

/**
 * @brief 初始化文件系统
 */
static int init_filesystem(void)
{
    int ret;

    /* 1. 初始化 SD 卡和磁盘子系统 */
    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);

    /* 2. 初始化 LVFS 和 LSFS */
    lvfs_init();
    lsfs_init();

    /* 3. 挂载文件系统 */
    ret = lsfs_mount(&sdmmc_mnt);
    if (ret != 0) {
        LOGI("Mount failed, formatting SD card...");
        ret = lsfs_mkfs(LSFS_FATFS, SDMMC_DEVICE, NULL, 0);
        if (ret == 0) {
            ret = lsfs_mount(&sdmmc_mnt);
        }
    }

    if (ret != 0) {
        LOGE("Failed to mount filesystem: %d", ret);
    }
    return ret;
}

/**
 * @brief 初始化 PA 控制 GPIO
 */
static int init_pa_gpio(void)
{
    int ret;
    lisa_device_t *gpio_dev = lisa_device_get(PA_GPIO_DEVICE);

    if (!lisa_device_ready(gpio_dev)) {
        LOGE("Error: %s device not ready", PA_GPIO_DEVICE);
        return -1;
    }

    ret = lisa_gpio_configure(gpio_dev, PA_PIN_NUM,
                              LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
    if (ret != 0) {
        LOGE("GPIO configure failed: %d", ret);
    }
    return ret;
}

/**
 * @brief 播放测试示例
 */
static void playback_demo(app_player_t *player)
{
    int ret;
    const char *audio_file = "/SD:/001_network_suc.mp3";

    LOGI("Starting playback demo...");

    /* 播放音频文件 */
    ret = app_player_play(player, audio_file);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to play: %d", ret);
        return;
    }

    /* 等待 500ms 后暂停 */
    vTaskDelay(pdMS_TO_TICKS(500));
    ret = app_player_pause(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to pause: %d", ret);
    }

    /* 等待 500ms 后恢复播放 */
    vTaskDelay(pdMS_TO_TICKS(500));
    ret = app_player_resume(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to resume: %d", ret);
    }

    /* 等待 500ms 后停止 */
    vTaskDelay(pdMS_TO_TICKS(500));
    ret = app_player_stop_sync(player);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to stop: %d", ret);
    }

    LOGI("Playback demo completed");
}

int main(int argc, char **argv)
{
    int ret;
    app_player_t *player = NULL;

    LOGI("App Player Local Filesystem Sample");

    /* 初始化文件系统 */
    ret = init_filesystem();
    if (ret != 0) {
        return -1;
    }

    /* 初始化 PA 控制 GPIO */
    ret = init_pa_gpio();
    if (ret != 0) {
        return -1;
    }

    /* 初始化 app_player 模块 */
    app_player_config_t config = {
        .pa_ctrl_callback = pa_control_callback
    };
    ret = app_player_init(&config);
    if (ret != APP_PLAYER_OK) {
        LOGE("App player init failed: %d", ret);
        return -1;
    }

    /* 创建播放器实例 */
    player = app_player_create("tone");
    if (player == NULL) {
        LOGE("Failed to create player");
        return -1;
    }

    /* 注册事件回调 */
    ret = app_player_register_callback(player, player_event_callback, NULL);
    if (ret != APP_PLAYER_OK) {
        LOGE("Failed to register callback: %d", ret);
        app_player_destroy(player);
        return -1;
    }

    /* 执行播放测试 */
    playback_demo(player);

    /* 主循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
