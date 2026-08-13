/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "wifi_pm_dc_ap"

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "IOMuxManager.h"
#include "arcs_ap.h"
#include "ic_lock.h"
#include "ipc.h"
#include "ipc_shared.h"
#include "lisa_log.h"
#include "memap.h"
#include "pm.h"
#include "vrtc.h"

#define AP_CONTEXT_TASK_STACK_SIZE 1024
#define AP_CONTEXT_TASK_PRIORITY   3
#define AP_CONTEXT_LOG_PERIOD_MS   5000
#define AP_CONTEXT_MAGIC           0x41504354U
#define AP_CONTEXT_SEED            0xA5A50000U

#ifndef EXPECTED_CP_FLASH_BASE
#define EXPECTED_CP_FLASH_BASE 0x30100000U
#endif

#ifndef EXPECTED_AP_PSRAM_BASE
#define EXPECTED_AP_PSRAM_BASE 0x28000000U
#endif

#ifndef EXPECTED_AP_PSRAM_SIZE
#define EXPECTED_AP_PSRAM_SIZE 0x00400000U
#endif

_Static_assert(MEM_CP_FLASH_BASE == EXPECTED_CP_FLASH_BASE,
               "remote/memap.h MEM_CP_FLASH_BASE must match CP prj.conf");
_Static_assert(CONFIG_MEM_PSRAM_BASE == EXPECTED_AP_PSRAM_BASE,
               "AP PSRAM base must be 0x28000000");
_Static_assert(CONFIG_MEM_PSRAM_SIZE == EXPECTED_AP_PSRAM_SIZE,
               "AP PSRAM size must be 4M");

static volatile uint32_t g_ap_magic;
static volatile uint32_t g_ap_counter;
static volatile uint32_t g_ap_checksum;
static volatile uint32_t g_ap_failed;

void lisa_uart1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER3);
}

static uint32_t ap_checksum(uint32_t counter)
{
    return AP_CONTEXT_SEED ^ (counter * 2654435761UL) ^ (counter >> 3);
}

static void ap_context_init(void)
{
    g_ap_magic = AP_CONTEXT_MAGIC;
    g_ap_counter = 0;
    g_ap_checksum = ap_checksum(0);
    g_ap_failed = 0;
}

static void ap_context_task(void *arg)
{
    (void)arg;

    while (1) {
        uint32_t counter = g_ap_counter;
        uint32_t expected = ap_checksum(counter);

        if ((g_ap_magic != AP_CONTEXT_MAGIC) || (g_ap_checksum != expected)) {
            g_ap_failed = 1;
            LOGE("AP CTX CHECK FAILED: magic=0x%08lx counter=%lu checksum=0x%08lx expected=0x%08lx",
                 (unsigned long)g_ap_magic,
                 (unsigned long)counter,
                 (unsigned long)g_ap_checksum,
                 (unsigned long)expected);
        } else {
            counter++;
            g_ap_counter = counter;
            g_ap_checksum = ap_checksum(counter);
            LOGI("AP ctx alive: counter=%lu checksum=0x%08lx",
                 (unsigned long)counter,
                 (unsigned long)g_ap_checksum);
        }

        vTaskDelay(pdMS_TO_TICKS(AP_CONTEXT_LOG_PERIOD_MS));
    }
}

static void boot_cp_from_flash(void)
{
    LOGI("boot cp from flash: 0x%08lx", (unsigned long)MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

int main(int argc, char **argv)
{
    BaseType_t task_ret;

    (void)argc;
    (void)argv;

    LOGI("=== wifi_pm_dual_core AP ===");

    ap_context_init();
    boot_cp_from_flash();

    if (pm_init() != 0) {
        LOGE("AP pm_init failed");
        return -1;
    }

    if (vrtc_init() != 0) {
        LOGE("AP vrtc_init failed");
        return -1;
    }

    pm_register_gpio_retention(CSK_IOMUX_PAD_A, 4);
    pm_register_gpio_retention(CSK_IOMUX_PAD_A, 5);
    LOGI("AP pm/vrtc init ok");

    task_ret = xTaskCreate(ap_context_task,
                           "ap_ctx",
                           AP_CONTEXT_TASK_STACK_SIZE,
                           NULL,
                           AP_CONTEXT_TASK_PRIORITY,
                           NULL);
    if (task_ret != pdPASS) {
        LOGE("AP context task create failed");
        return -1;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    return 0;
}
