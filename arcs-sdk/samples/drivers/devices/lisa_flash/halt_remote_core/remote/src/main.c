/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA Flash 双核环境示例 (AP 核)
 *
 * 本示例演示 AP 核在双核环境下的运行，作为远端核心，它会在 CP 核执行 Flash 操作时
 * 被自动停止，确保 Flash 访问的安全性。
 *
 * ## 功能说明
 * 1. AP 核持续运行一个计数任务
 * 2. 当 CP 核执行 Flash 操作时，AP 核会被 IPC 机制自动停止
 * 3. Flash 操作完成后，AP 核自动恢复运行
 * 4. 通过串口输出可以观察到 AP 核的停止和恢复过程
 *
 * @note 配置 CONFIG_LISA_FLASH_ARCS_HALT_BY_REMOTE_CORE=y 允许被远端核心停止
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "sys_init.h"
#include "soc/chip.h"
#include "sys/boot_core.h"

#include "FreeRTOS.h"
#include "task.h"

#include "IOMuxManager.h"

#define TAG "remote_main"
#include <lisa_log.h>

void ipc_utils_before_halt_by_peer_core(void)
{
    LISA_LOGI(TAG, "%s", __func__);
}

void ipc_utils_after_resume_by_peer_core(void)
{
    LISA_LOGI(TAG, "%s", __func__);
}

int main(int argc, char **argv)
{
    uint32_t heartbeat = 0;

    LISA_LOGI(TAG, "\n========================================");
    LISA_LOGI(TAG, "=== LISA Flash Dual-Core Example ===");
    LISA_LOGI(TAG, "===      (AP Core - Remote)        ===");
    LISA_LOGI(TAG, "========================================");
    LISA_LOGI(TAG, "Observe the log messages indicating halt and resume events.\n");
    LISA_LOGI(TAG, "AP flash ready");

    while (1) {
        LISA_LOGI(TAG, "AP heartbeat %lu", (unsigned long)heartbeat++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return 0;
}

static int boot_cp(void)
{
    int ret = sys_boot_core(1, 0x30080000);
    if (ret != 0) {
        LISA_LOGE(TAG, "CP core boot failed: %d", ret);
        return ret;
    }

    LISA_LOGI(TAG, "CP core booted.\n");
    return 0;
}

SYS_INIT(boot_cp, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 1);      /* 优先级 */
