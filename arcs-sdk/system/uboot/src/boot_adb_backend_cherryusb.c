#include "boot_adb_backend.h"

#include <stdint.h>

#include "adb_device.h"
#include "arcs_ap.h"
#include "syslog.h"
#include "usbd_core.h"

extern const struct usb_descriptor boot_adb_cherryusb_descriptor;
extern const uint8_t boot_adb_cherryusb_in_ep;
extern const uint8_t boot_adb_cherryusb_out_ep;

#define BOOT_ADB_CHERRYUSB_BUSID 0

static struct usbd_interface boot_adb_cherryusb_intf;

static void boot_adb_backend_cherryusb_soft_disconnect(void)
{
#ifdef USB_ARCS_POWER_SOFTCONN
    IP_USBC->POWER &= ~USB_ARCS_POWER_SOFTCONN;
#endif
}

static void boot_adb_backend_cherryusb_soft_connect(void)
{
#ifdef USB_ARCS_POWER_SOFTCONN
    IP_USBC->POWER |= USB_ARCS_POWER_SOFTCONN;
#endif
}

static void boot_adb_backend_cherryusb_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;

    switch (event) {
    case USBD_EVENT_RESET:
        printk("boot adb: usb reset backend=cherryusb\n");
        break;
    case USBD_EVENT_CONNECTED:
        printk("boot adb: usb connected backend=cherryusb\n");
        break;
    case USBD_EVENT_DISCONNECTED:
        printk("boot adb: usb disconnected backend=cherryusb\n");
        break;
    case USBD_EVENT_CONFIGURED:
        printk("boot adb: usb configured backend=cherryusb\n");
        break;
    default:
        break;
    }
}

static bool boot_adb_backend_cherryusb_usb_stack_init(void)
{
    usbd_desc_register(BOOT_ADB_CHERRYUSB_BUSID, &boot_adb_cherryusb_descriptor);
    usbd_add_interface(
        BOOT_ADB_CHERRYUSB_BUSID,
        adb_dev_init_intf(BOOT_ADB_CHERRYUSB_BUSID,
                          &boot_adb_cherryusb_intf,
                          boot_adb_cherryusb_in_ep,
                          boot_adb_cherryusb_out_ep));

    if (usbd_initialize(BOOT_ADB_CHERRYUSB_BUSID,
                        (uintptr_t)USBC_BASE,
                        boot_adb_backend_cherryusb_event_handler) != 0) {
        return false;
    }

    boot_adb_backend_cherryusb_soft_disconnect();
    return true;
}

static bool boot_adb_backend_cherryusb_usb_task_start(void)
{
    return true;
}

bool boot_adb_backend_bind_runtime_ops(boot_adb_runtime_ops_t *ops)
{
    if (ops == NULL) {
        return false;
    }

    ops->usb_stack_init = boot_adb_backend_cherryusb_usb_stack_init;
    ops->usb_task_start = boot_adb_backend_cherryusb_usb_task_start;
    ops->soft_disconnect = boot_adb_backend_cherryusb_soft_disconnect;
    ops->soft_connect = boot_adb_backend_cherryusb_soft_connect;
    return true;
}
