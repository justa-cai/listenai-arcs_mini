/**
 * @file power_manager.c
 * @brief Power management functions for system boot and shutdown
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#include <stdio.h>

#include "power_manager.h"
#include "lisa_time.h"
#include "lisa_log.h"
#include "lisa_gpio.h"
#include "board.h"
#include "chip.h"           /* IP_AON_IOMUX */
#include "PowerManager.h"   /* __HAL_PMU_WholeChip_RST_ENABLE */
#include "uboot_features_api.h"
#include "uboot_power_api.h"
#include "sys/reboot.h"
#if defined(CONFIG_CHERRYUSB) && CONFIG_CHERRYUSB
#include "app_usb_cherry.h"
#endif

#define TAG "power_mgr"

#define POWER_KEY_PAD CONFIG_POWER_MANAGER_POWER_KEY_GPIO_PAD
#define POWER_EN_PAD  CONFIG_POWER_MANAGER_POWER_EN_GPIO_PAD
#define USB_DET_PAD   CONFIG_POWER_MANAGER_USB_DETECT_GPIO_PAD

#define BOOT_INFO_REBOOT_CNT_MASK     0x000000FFu
#define BOOT_INFO_RECOVER_REASON_MASK 0x0000FF00u
#define BOOT_INFO_RECOVER_REASON_SOFT 0x00000100u
#define BOOT_INFO_HANDSHAKE_TIMEOUT   (1u << 29)
#define BOOT_INFO_WDT_FLAG            (1u << 30)
#define BOOT_INFO_RECOVERY_REQ        (1u << 31)
#define SYSTEM_SW_RESET_KEY           0xCAFE000Au

static lisa_device_t *power_key_dev = NULL;
static lisa_device_t *power_en_dev = NULL;
static lisa_device_t *usb_det_dev = NULL;

static void (*shutdown_cb)(void) = NULL;

static void power_gpio_init(void)
{
    power_key_dev = lisa_device_get(POWER_KEY_PAD);
    lisa_gpio_configure(power_key_dev, POWER_KEY_PIN, LISA_GPIO_CONFIG_INPUT_PULLUP);

    /* 上一轮可能走了 power_reboot_soft 把 AON force-output 打开，
     * 清掉，否则 GPIO 写入不生效，后续 power_shutdown / stage0 latch_set
     * 都会失灵。*/
    volatile uint32_t *aon_iomux =
        (volatile uint32_t *)&IP_AON_IOMUX->REG_PAD_AON_GPIOB_00.all + POWER_EN_PIN;
    *aon_iomux &= ~(0x1E00000u);

    /* 新 boot 已拉起 PWR_LOCK，OUTPUT_HIGH 接管避免掉电窗口 */
    power_en_dev = lisa_device_get(POWER_EN_PAD);
    lisa_gpio_flags_t en_cfg = uboot_features_has(UBOOT_FEATURE_POWER_GUARD)
                                   ? LISA_GPIO_CONFIG_OUTPUT_HIGH
                                   : LISA_GPIO_CONFIG_OUTPUT_LOW;
    lisa_gpio_configure(power_en_dev, POWER_EN_PIN, en_cfg);

    usb_det_dev = lisa_device_get(USB_DET_PAD);
    lisa_gpio_configure(usb_det_dev, USB_DET_PIN, LISA_GPIO_CONFIG_INPUT_PULLDOWN);

    LISA_LOGI(TAG, "Power GPIO initialized - Button: %s(%d), Latch: %s(%d), USB Detect: %s(%d)", POWER_KEY_PAD,
              POWER_KEY_PIN, POWER_EN_PAD, POWER_EN_PIN, USB_DET_PAD, USB_DET_PIN);
}

static bool power_button_pressed(void)
{
    return lisa_gpio_read_pin(power_key_dev, POWER_KEY_PIN) == LISA_GPIO_LOW;
}

static bool power_latch_set(bool state)
{
    return lisa_gpio_write_pin(power_en_dev, POWER_EN_PIN, state ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}

void power_init(const power_config_t *config)
{
    if (config && config->on_shutdown) {
        shutdown_cb = config->on_shutdown;
    }

    power_gpio_init();
}

bool power_wait_settle(void)
{
    if (power_is_usb_plugged()) {
        power_latch_set(true);
        LISA_LOGI(TAG, "USB connected, power on directly");
        return true;
    }

    while (1) {
        uint64_t elapsed = lisa_os_get_tick_ms();

        if (!power_button_pressed()) {
            LISA_LOGI(TAG, "Power button released at %llu ms before settle time", elapsed);
            return false;
        }

        if (elapsed >= POWER_BUTTON_HOLD_TIME_MS) {
            LISA_LOGI(TAG, "Power button held for required settle time: %llu ms", elapsed);
            break;
        }

        lisa_thread_mdelay(POWER_SAMPLE_INTERVAL_MS);
    }

    power_latch_set(true);
    LISA_LOGI(TAG, "Latch set, system powered on");

    return true;
}

bool power_is_usb_plugged(void)
{
    return lisa_gpio_read_pin(usb_det_dev, USB_DET_PIN) == LISA_GPIO_HIGH;
}

void power_shutdown(void)
{
    LISA_LOGI(TAG, "Executing system shutdown...");

    if (shutdown_cb) {
        LISA_LOGI(TAG, "Calling shutdown callback...");
        shutdown_cb();
    }

    /* USB 插着时拉低 PWR_LOCK 无效（VBUS 维持 VCC），新 boot 走 stage0
     * 假关机。老 boot 回退到直接拉低，USB 下不生效。*/
    if (power_is_usb_plugged() && uboot_features_has(UBOOT_FEATURE_POWER_GUARD)) {
        LISA_LOGI(TAG, "USB plugged, soft shutdown via stage0");
        uboot_shutdown_request();
    }

    LISA_LOGI(TAG, "Goodbye!");
    power_latch_set(false);
}

void power_reboot_soft(void)
{
    sys_arch_reboot(SYS_REBOOT_SOFT);

    while (1) {
        __asm__ volatile("wfi");
    }
}

void power_reboot_recovery(void)
{
    uint32_t reset_cause = IP_AON_CTRL->REG_SYSRST_STATUS.all;
    IP_AON_CTRL->REG_SYSRST_STATUS.all = reset_cause;
    (void)IP_AON_CTRL->REG_SYSRST_STATUS.all;

    uint32_t boot_info = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    boot_info &= ~(BOOT_INFO_REBOOT_CNT_MASK | BOOT_INFO_RECOVER_REASON_MASK |
                   BOOT_INFO_HANDSHAKE_TIMEOUT | BOOT_INFO_WDT_FLAG | BOOT_INFO_RECOVERY_REQ);
    boot_info |= BOOT_INFO_RECOVER_REASON_SOFT | BOOT_INFO_RECOVERY_REQ;
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = boot_info;
    (void)IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    __RWMB();

    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all &= ~(0xFu << 21);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all |= (0xEu << 21);
    (void)IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all;
    __RWMB();

    while (1) {
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN = 1;
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN = 1;
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN = 1;
        __RWMB();
        IP_SYSCTRL->REG_SW_RESET_CP0.all = SYSTEM_SW_RESET_KEY;
        __RWMB();
    }
}

void ipc_auto_init_failure_handler(int error)
{
    printf("IPC shared memory is unavailable or incompatible (%d), entering recovery\n", error);
    power_reboot_recovery();
}

void sys_platform_recovery(void)
{
#if defined(CONFIG_CHERRYUSB) && CONFIG_CHERRYUSB
    /*
     * `adb reboot recovery` reaches this callback directly and bypasses the
     * shell `recovery` command.  Detach CherryUSB before resetting so the
     * host observes a complete disconnect and can enumerate the boot ADB
     * device instead of keeping the application transport offline.
     */
    app_usb_prepare_reboot();
#endif
    power_reboot_recovery();
}
