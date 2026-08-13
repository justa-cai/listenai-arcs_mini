/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usb_device.h"

#include "usb_device_config.h"
#include "usbd_core.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "soc/chip.h"
#pragma GCC diagnostic pop

static void usb_device_port_init(void)
{
#if defined(CONFIG_SOC_ARCS) && (CONFIG_SOC_ARCS == 1)
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
#elif defined(CONFIG_SOC_VENUSA) && (CONFIG_SOC_VENUSA == 1)
    IP_CMN_SYSCFG->REG_PERI_CLK_CFG7.bit.ENA_USB_CLK = 0x01;
#endif
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
}

int usb_device_start(usb_device_event_cb_t event_cb)
{
    usb_device_port_init();
    usbd_initialize(USB_DEVICE_BUS_ID, USBC_BASE, event_cb);
    return 0;
}
