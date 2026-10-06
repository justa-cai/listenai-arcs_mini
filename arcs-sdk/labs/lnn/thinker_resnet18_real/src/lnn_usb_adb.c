/*
 * lnn_usb_adb.c - LNN demo 的最小 ADB 服务（shell + reboot recovery）
 *
 * 移植自 samples/subsys/usb/device/cherryusb_adb：只保留 adb 协议所需的
 * vendor bulk 端点。启用后可直接在 demo 固件上：
 *   adb shell              # 查看运行日志 / shell 命令
 *   adb reboot recovery    # 重启进 BOOT recovery，配合 auto.sh 烧录
 *   adb reboot hard        # 普通硬复位
 * 免去每次烧录手动进 BOOT 模式。
 */
#include <lisa_log.h>
#include "soc/chip.h"

#include "FreeRTOS.h"
#include "task.h"

#include "usbd_core.h"
#include "adb.h"
#include "adb_device.h"
#include "adb_reboot.h"
#include "adb_shell.h"

#include "PowerManager.h"

#define LOG_TAG "lnn_adb"

extern const struct usb_descriptor cherryusb_adb_descriptor;
extern const uint8_t g_adb_in_ep;
extern const uint8_t g_adb_out_ep;

static uint8_t g_usb_busid;
static struct usbd_interface adb_intf;

/* adb reboot recovery 的落点：写 AON 恢复标志后全片复位，
 * 下次 boot 据此进入 BOOT ADB recovery（对齐产品 power_reboot_recovery） */
void sys_platform_recovery(void)
{
#define BOOT_INFO_RECOVER_REASON_SOFT 0x00000100u
#define SYSTEM_SW_RESET_KEY           0xCAFE000Au
    uint32_t boot_info = IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    boot_info |= BOOT_INFO_RECOVER_REASON_SOFT | (1u << 31); /* RECOVERY_REQ */
    IP_AON_CTRL->REG_AON_DIG_RSVD4.all = boot_info;
    (void)IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    __RWMB();

    while (1) {
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CMN_RST_EN = 1;
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2CP_RST_EN = 1;
        IP_SYSCTRL->REG_SW_RESET_CP1.bit.CMNSW2AP_RST_EN = 1;
        __RWMB();
        IP_SYSCTRL->REG_SW_RESET_CP0.all = SYSTEM_SW_RESET_KEY;
        __RWMB();
    }
#undef BOOT_INFO_RECOVER_REASON_SOFT
#undef SYSTEM_SW_RESET_KEY
}

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;
    switch (event) {
    case USBD_EVENT_CONFIGURED:
        LOGI("usb configured");
        break;
    case USBD_EVENT_DISCONNECTED:
        LOGW("usb disconnected");
        break;
    default:
        break;
    }
}

int lnn_usb_adb_start(void)
{
    /* USB 时钟与 PHY（对齐产品 app_usb 流程） */
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    usbd_desc_register(g_usb_busid, &cherryusb_adb_descriptor);
    usbd_add_interface(g_usb_busid,
                       adb_dev_init_intf(g_usb_busid, &adb_intf,
                                         g_adb_in_ep, g_adb_out_ep));

    adb_init();
#if CONFIG_ADB_REBOOT
    adb_reboot_init();
#endif
#if CONFIG_ADB_SHELL
    adb_shell_init();
#endif

    usbd_initialize(g_usb_busid, USBC_BASE, usbd_event_handler);
    LOGI("adb ready (shell + reboot recovery)");
    return 0;
}

static void lnn_task_usb(void *arg)
{
    (void)arg;
    lnn_usb_adb_start();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int lnn_usb_adb_init(void)
{
    if (xTaskCreate(lnn_task_usb, "lnn_adb", 4096, NULL, 5, NULL) != pdPASS) {
        LOGE("create usb task failed");
        return -1;
    }
    return 0;
}
