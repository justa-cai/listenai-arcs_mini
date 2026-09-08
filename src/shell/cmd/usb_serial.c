/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <string.h>

#include "app_usb_cherry.h"
#include "lisa_log.h"
#include "shell.h"
#include "usbh_core.h"
#include "usbh_serial.h"

#define TAG "usb-serial-cmd"

static const char *app_usb_serial_name(struct usbh_serial *serial)
{
    if (serial == NULL || serial->hport == NULL) {
        return "unknown";
    }

    return serial->hport->config.intf[serial->intf].devname;
}

void usbh_serial_run(struct usbh_serial *serial)
{
    LISA_LOGI(TAG, "USB Host serial attached: %s", app_usb_serial_name(serial));
}

void usbh_serial_stop(struct usbh_serial *serial)
{
    LISA_LOGI(TAG, "USB Host serial detached: %s", app_usb_serial_name(serial));
}

static int shell_usb_serial(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();

    if (app_usb_active_role() != APP_USB_ROLE_HOST) {
        shellPrint(shell, "USB serial unavailable: active role is %s\r\n",
                   app_usb_role_name(app_usb_active_role()));
        return -1;
    }

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "status") == 0)) {
        struct usbh_serial *serial = usbh_find_class_instance("/dev/ttyACM0");

        shellPrint(shell, "USB Host serial: /dev/ttyACM0 %s\r\n",
                   serial == NULL ? "not connected" : "connected");
        return serial == NULL ? -1 : 0;
    }

    return usbh_serial(argc, argv);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 usb_serial, shell_usb_serial,
                 USB Host serial: status or <device> <-b|-t|-w|-r|-x> ...);
