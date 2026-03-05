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
 * @note 配置 CONFIG_ARCS_HAL_IPC_HALT_PEER_CORE=y 允许被远端核心停止
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "sys_init.h"
#include "arcs_ap.h"

#include "FreeRTOS.h"
#include "task.h"

#include "ipc_slave.h"
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
    LISA_LOGI(TAG, "\n========================================");
    LISA_LOGI(TAG, "=== LISA Flash Dual-Core Example ===");
    LISA_LOGI(TAG, "===      (AP Core - Remote)        ===");
    LISA_LOGI(TAG, "========================================");
    LISA_LOGI(TAG, "Observe the log messages indicating halt and resume events.\n");

    while (1) {

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return 0;
}

static int app_ipc_init(void)
{
    ipc_mem_init(0);
    ic_lock_init();
    ipc_slave_init(NULL);
    LISA_LOGI(TAG, "IPC remote initialized.\n");
}

static int boot_cp(void)
{

    /*CP boot flash address(CONFIG_MEM_FLASH_BASE)*/
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = 0x30080000;
    /* reset c-core */
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
    LISA_LOGI(TAG, "CP core booted.\n");
}

SYS_INIT(boot_cp, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);  /* 优先级 */
SYS_INIT(app_ipc_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 1); /* 优先级 */