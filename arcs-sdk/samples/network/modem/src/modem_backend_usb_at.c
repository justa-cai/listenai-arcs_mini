/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "modem_usb"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdio.h>

#include "ClockManager.h"
#include "arcs_ap.h"

#include "FreeRTOS.h"
#include "task.h"

#include "sys_init.h"
#include "usbh_core.h"
#include "usbh_serial.h"

#include "modem_backend.h"
#include "transport/at_transport_usbh_serial.h"

#define EC801E_USB_VID                  0x2c7c
#define EC801E_USB_PID_ECM              0x0903
#define EC801E_USB_AT_INTF_NUM          3
#define EC801E_USB_AT_MAX_TTYS          4
#define EC801E_USB_AT_SCAN_DELAY_MS     500
#define EC801E_USB_AT_POLL_INTERVAL_MS  20
#define EC801E_USB_HOST_BUSID           0
#define EC801E_USB_HOST_BASE            0x41000000UL

static bool s_usb_host_initialized;

static const char *usb_event_name(uint8_t event)
{
    switch (event) {
    case 1:
        return "CONNECTED";
    case 2:
        return "DISCONNECTED";
    case 3:
        return "REMOVED";
    default:
        return "OTHER";
    }
}

static void usbh_event_handler(uint8_t busid, uint8_t hub_index, uint8_t hub_port,
                               uint8_t intf, uint8_t event)
{
    LISA_LOGI(LOG_TAG, "USB bus=%u hub=%u port=%u intf=%u event=%u (%s)",
              busid, hub_index, hub_port, intf, event, usb_event_name(event));
}

static void sample_modem_usb_host_ensure_initialized(void)
{
    if (s_usb_host_initialized) {
        return;
    }

    LISA_LOGI(LOG_TAG, "Initializing USB host for modem AT backend");
    usbh_initialize(EC801E_USB_HOST_BUSID, EC801E_USB_HOST_BASE, usbh_event_handler);
    s_usb_host_initialized = true;
}

static bool sample_modem_usb_matches_ec801e_at(const struct usbh_serial *serial)
{
    if (!serial || !serial->hport) {
        return false;
    }

    return serial->hport->device_desc.idVendor == EC801E_USB_VID &&
           serial->hport->device_desc.idProduct == EC801E_USB_PID_ECM &&
           serial->intf == EC801E_USB_AT_INTF_NUM;
}

static lisa_modem_t *sample_modem_usb_try_open(void)
{
    char devname[16];

    for (int i = 0; i < EC801E_USB_AT_MAX_TTYS; ++i) {
        struct usbh_serial *serial;
        at_transport_t *transport;
        at_transport_usbh_serial_config_t transport_cfg = AT_TRANSPORT_USBH_SERIAL_CONFIG_DEFAULT();
        lisa_modem_t *modem;

        (void)snprintf(devname, sizeof(devname), "/dev/ttyUSB%d", i);
        serial = usbh_serial_open(devname, USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
        if (!serial) {
            continue;
        }

        if (!sample_modem_usb_matches_ec801e_at(serial)) {
            usbh_serial_close(serial);
            continue;
        }

        transport_cfg.rx_timeout_ms = EC801E_USB_AT_POLL_INTERVAL_MS;
        transport = at_transport_usbh_serial_create(serial, true, &transport_cfg);
        if (!transport) {
            usbh_serial_close(serial);
            continue;
        }

        LISA_LOGI(LOG_TAG, "Using EC801E USB AT interface %u via %s", serial->intf, devname);
        modem = lisa_modem_create_with_transport(transport, true);
        if (modem) {
            return modem;
        }
    }

    return NULL;
}

const char *sample_modem_backend_name(void)
{
    return "usb-at";
}

lisa_modem_t *sample_modem_open(const char *uart_dev)
{
    bool announced = false;

    (void)uart_dev;
    sample_modem_usb_host_ensure_initialized();

    while (1) {
        lisa_modem_t *modem = sample_modem_usb_try_open();

        if (modem) {
            return modem;
        }

        if (!announced) {
            LISA_LOGI(LOG_TAG, "Waiting for EC801E USB AT interface (intf=%u)",
                      EC801E_USB_AT_INTF_NUM);
            announced = true;
        }
        vTaskDelay(pdMS_TO_TICKS(EC801E_USB_AT_SCAN_DELAY_MS));
    }
}

void sample_modem_close(lisa_modem_t *modem)
{
    lisa_modem_destroy(modem);
}

static int usb_host_init(void)
{
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x0;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
    return 0;
}

SYS_INIT(usb_host_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
