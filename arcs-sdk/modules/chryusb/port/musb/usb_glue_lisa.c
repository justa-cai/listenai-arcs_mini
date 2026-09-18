/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// #include "appinc.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ClockManager.h"
#include "soc/chip.h"

#include "usb_config.h"

/* SoC headers may define USB_POWER_* with the same names as CherryUSB MUSB. */
#ifdef USB_POWER_SOFTCONN
#undef USB_POWER_SOFTCONN
#endif
#ifdef USB_POWER_RESET
#undef USB_POWER_RESET
#endif
#ifdef USB_POWER_RESUME
#undef USB_POWER_RESUME
#endif
#include "usb_musb_reg.h"

// clang-format off
static struct musb_fifo_cfg musb_device_table[] = {
    { .ep_num =  0, .style = FIFO_TXRX, .maxpacket = 64   },
    { .ep_num =  1, .style = FIFO_TXRX, .maxpacket = 1024 },
#if defined(CONFIG_ADB)
    /* ADB is full duplex: sharing the RAM address lets OUT overwrite IN.
     * Split the same 1 KiB allocation into two independent HS bulk FIFOs. */
    { .ep_num =  2, .style = FIFO_TX,   .maxpacket = 512  },
    { .ep_num =  2, .style = FIFO_RX,   .maxpacket = 512  },
#else
    { .ep_num =  2, .style = FIFO_TXRX, .maxpacket = 1024 },
#endif
    { .ep_num =  3, .style = FIFO_TXRX, .maxpacket = 1024 },
    { .ep_num =  4, .style = FIFO_TXRX, .maxpacket = 512  },
    { .ep_num =  5, .style = FIFO_TXRX, .maxpacket = 64   },
    { .ep_num =  6, .style = FIFO_TXRX, .maxpacket = 64   },
    { .ep_num =  7, .style = FIFO_TXRX, .maxpacket = 64   },
};
// clang-format on

static struct musb_fifo_cfg musb_host_table[] = {
    { .ep_num =  0, .style = FIFO_TXRX, .maxpacket = 64   },
    { .ep_num =  1, .style = FIFO_TXRX, .maxpacket = 1024 },
    { .ep_num =  2, .style = FIFO_TXRX, .maxpacket = 1024 },
    { .ep_num =  3, .style = FIFO_TXRX, .maxpacket = 1024 },
    { .ep_num =  4, .style = FIFO_TXRX, .maxpacket = 512  },
    { .ep_num =  5, .style = FIFO_TXRX, .maxpacket = 64   },
    { .ep_num =  6, .style = FIFO_TXRX, .maxpacket = 64   },
    { .ep_num =  7, .style = FIFO_TXRX, .maxpacket = 64   },
};
// clang-format on

uint8_t usbd_get_musb_fifo_cfg(struct musb_fifo_cfg **cfg)
{
    *cfg = musb_device_table;
    return sizeof(musb_device_table) / sizeof(musb_device_table[0]);
}

uint8_t usbh_get_musb_fifo_cfg(struct musb_fifo_cfg **cfg)
{
    *cfg = musb_host_table;
    return sizeof(musb_host_table) / sizeof(musb_host_table[0]);
}

uint32_t usb_get_musb_ram_size(void)
{
    return 4096;
}

#ifdef CONFIG_USB_DCACHE_ENABLE
void usb_dcache_flush(uintptr_t addr, uint32_t size)
{
    HAL_FlushDCache_by_Addr(addr, size);
}
void usb_dcache_clean(uintptr_t addr, uint32_t size)
{
    HAL_FlushDCache_by_Addr(addr, size);
}
void usb_dcache_invalidate(uintptr_t addr, uint32_t size)
{
    HAL_InvalidateDCache_by_Addr(addr, size);
}
#endif//CONFIG_USB_DCACHE_ENABLE

void usbd_musb_delay_ms(uint8_t ms)
{
    /* implement later */
}

#if CONFIG_CHERRYUSB_DEVICE_MUSB_LISA
static void usbd_irq_handler(void)
{
    void USBD_IRQHandler(uint8_t busid);
    USBD_IRQHandler(0);
}

void usb_dc_low_level_init(void)
{
    // enable usb clock
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 1;

    /* Register USB ISR */
    register_ISR(IRQ_USBC_VECTOR, usbd_irq_handler, NULL);
    enable_IRQ(IRQ_USBC_VECTOR);
}

void usb_dc_low_level_deinit(void)
{
    disable_IRQ(IRQ_USBC_VECTOR);
    register_ISR(IRQ_USBC_VECTOR, NULL, NULL);
    __HAL_CRM_USB_CLK_DISABLE();
}
#endif

#if CONFIG_CHERRYUSB_HOST_MUSB_LISA
struct usbh_bus;
static void usbh_irq_handler(void)
{
    extern void USBH_IRQHandler(uint8_t busid);
    USBH_IRQHandler(0);

}

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    // enable usb clock
    // __HAL_CRM_USB_CLK_ENABLE();
    // IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0;
    // IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 1;

    /* Register USB ISR */
    register_ISR(IRQ_USBC_VECTOR, usbh_irq_handler, NULL);
    enable_IRQ(IRQ_USBC_VECTOR);
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    disable_IRQ(IRQ_USBC_VECTOR);
    register_ISR(IRQ_USBC_VECTOR, NULL, NULL);
    __HAL_CRM_USB_CLK_DISABLE();
}
#endif
