#include "dma_irq_proxy.h"
#include "dma.h"
#include "log_print.h"

static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    if ( DMA_EVENT_TRANSFER_COMPLETE & (event_info & 0xFF)) {
        irq_proxy_trigger(usr_param);
	}
}

int32_t dma_irq_proxy_bind(uint8_t ch, irq_proxy_channel_t channel)
{
    uint8_t real_ch;

    real_ch = dma_channel_reserve(ch, DMA_DrvEvent, channel, DMA_CACHE_SYNC_AUTO);
    if (real_ch == DMA_CHANNEL_ANY) {
        CLOGE("[FAILED] NO free CP DMA channel!!");
        return -1;
    }

    return 0;
}

int32_t dma_irq_proxy_register_callback(irq_proxy_channel_t ch, irq_proxy_callback_fn callback)
{
    irq_proxy_register_callback(ch, callback);
    return 0;
}
