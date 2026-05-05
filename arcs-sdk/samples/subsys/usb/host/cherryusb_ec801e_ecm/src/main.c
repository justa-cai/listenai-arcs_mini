/*
 * Copyright (c) 2026, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "ClockManager.h"
#include "arcs_ap.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/tcpip.h"

#include "sys_init.h"
#include "usbh_core.h"

#include "ec801e_usb_at.h"
#include "net_probe.h"
#include "usb_config.h"

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
    printf("[USB] bus=%u hub=%u port=%u intf=%u event=%u (%s)\r\n",
           busid, hub_index, hub_port, intf, event, usb_event_name(event));
}

int main(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("  CherryUSB Host EC801E ECM Example\r\n");
    printf("========================================\r\n");
    printf("[INFO] Expected modem USB ID: 2c7c:0903\r\n");
    printf("[INFO] USB AT recovery target: intf=3 (vendor AT interface)\r\n");
    printf("[INFO] Required PDP precondition: CGDCONT(cid=1,APN) is already valid\r\n");
    printf("[INFO] Initializing lwIP core...\r\n");

    tcpip_init(NULL, NULL);

    printf("[INFO] Initializing USB Host...\r\n");
    usbh_initialize(0, 0x41000000UL, usbh_event_handler);

    printf("[INFO] USB Host initialized\r\n");
    printf("[INFO] Connect EC801E to the USB_ARCS host port\r\n");

    printf("[INFO] Preparing EC801E over USB AT interface...\r\n");
    if (ec801e_usb_prepare_for_ecm() < 0) {
        printf("[ERROR] Failed to prepare EC801E USB ECM mode\r\n");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }

    ec801e_net_probe_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return 0;
}

static int usb_host_init(void)
{
    printf("[INIT] Configuring USB PHY for Host mode...\r\n");

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x0;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    printf("[INIT] USB Host PHY configured:\r\n");
    printf("       - IDDIG = 0 (Host mode)\r\n");
    printf("       - Clock enabled\r\n");
    printf("       - 16-bit data bus\r\n");
    return 0;
}

SYS_INIT(usb_host_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
