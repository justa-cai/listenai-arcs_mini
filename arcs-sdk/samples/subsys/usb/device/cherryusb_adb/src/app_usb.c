#include "log_print.h"
#include "arcs_ap.h"
#include "lisa_log.h"

#include "usbd_core.h"
#include "adb.h"
#include "adb_device.h"
#include "adb_shell.h"
#include "adb_sync.h"

#include "FreeRTOS.h"
#include "task.h"

extern const struct usb_descriptor cherryusb_adb_descriptor;
extern const uint8_t g_adb_in_ep;
extern const uint8_t g_adb_out_ep;

static uint8_t g_usb_busid;
static struct usbd_interface adb_intf;

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;
    switch (event) {
    case USBD_EVENT_RESET:
        LOGI("USB reset\n");
        break;
    case USBD_EVENT_CONNECTED:
        LOGI("USB connected\n");
        break;
    case USBD_EVENT_DISCONNECTED:
        LOGI("USB disconnected\n");
        break;
    case USBD_EVENT_CONFIGURED:
        LOGI("USB configured\n");
        break;
    default:
        break;
    }
}

int cherryusb_adb_start(void)
{
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    usbd_desc_register(g_usb_busid, &cherryusb_adb_descriptor);
    usbd_add_interface(g_usb_busid,
                       adb_dev_init_intf(g_usb_busid, &adb_intf,
                                         g_adb_in_ep, g_adb_out_ep));

    adb_init();
#if CONFIG_ADB_SHELL
    adb_shell_init();
#endif
#if CONFIG_ADB_SYNC
    adb_sync_init();
#endif

    usbd_initialize(g_usb_busid, USBC_BASE, usbd_event_handler);
    LOGI("CherryUSB ADB initialized\n");
    return 0;
}
