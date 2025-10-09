#ifndef DMA_IRQ_PROXY_H
#define DMA_IRQ_PROXY_H

#include <stdint.h>
#include "irq_proxy.h"

int32_t dma_irq_proxy_bind(uint8_t ch, irq_proxy_channel_t channel);
int32_t dma_irq_proxy_register_callback(irq_proxy_channel_t ch, irq_proxy_callback_fn callback);

#endif
