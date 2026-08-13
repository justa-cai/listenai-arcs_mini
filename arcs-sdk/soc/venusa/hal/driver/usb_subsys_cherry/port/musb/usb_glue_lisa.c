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
#include "venusa_ap.h"

#include "usb_config.h"
#include "usb_musb_reg.h"

// clang-format off
/* Default FIFO layout (can be overridden by application via __WEAK).
 * Total FIFO RAM = 4096 bytes.
 * EP0=64, EP1=512, EP2=512, EP3=512, EP4-7=64
 * Total: 64 + 512 + 512 + 512 + 64*4 = 1856 bytes => fits in 4096 */
static struct musb_fifo_cfg musb_device_table[] = {
    { .ep_num =  0, .style = FIFO_TXRX, .maxpacket = 64   },
    { .ep_num =  1, .style = FIFO_TXRX, .maxpacket = 512  },
    { .ep_num =  2, .style = FIFO_TXRX, .maxpacket = 512  },
    { .ep_num =  3, .style = FIFO_TXRX, .maxpacket = 512  },
    { .ep_num =  4, .style = FIFO_TXRX, .maxpacket = 64   },
    { .ep_num =  5, .style = FIFO_TXRX, .maxpacket = 64   }, 
    { .ep_num =  6, .style = FIFO_TXRX, .maxpacket = 64   }, 
    { .ep_num =  7, .style = FIFO_TXRX, .maxpacket = 64   }, 
};
// clang-format on

__WEAK uint8_t usbd_get_musb_fifo_cfg(struct musb_fifo_cfg **cfg)
{
    *cfg = musb_device_table;
    return sizeof(musb_device_table) / sizeof(musb_device_table[0]);
}

__WEAK uint8_t usbh_get_musb_fifo_cfg(struct musb_fifo_cfg **cfg)
{
    *cfg = musb_device_table;
    return sizeof(musb_device_table) / sizeof(musb_device_table[0]);
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

static void usbd_irq_handler(void)
{
    void USBD_IRQHandler(uint8_t busid);
    USBD_IRQHandler(0);
}

void usb_dc_low_level_init(void)
{

#if 0
    // enable usb clock
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 1;   //set as device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 1;

    /* Register USB ISR */
    register_ISR(IRQ_USBC_VECTOR, usbd_irq_handler, NULL);
    enable_IRQ(IRQ_USBC_VECTOR);
#endif
}

void usb_dc_low_level_deinit(void)
{
    disable_IRQ(IRQ_USBC_VECTOR);
    register_ISR(IRQ_USBC_VECTOR, NULL, NULL);
    __HAL_CRM_USB_CLK_DISABLE();
}
