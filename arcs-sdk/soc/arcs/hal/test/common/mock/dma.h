#ifndef DMA_H
#define DMA_H

#include <stdint.h>

#define DMA_EVENT_TRANSFER_COMPLETE 0x01
#define DMA_CACHE_SYNC_NOP 0
#define DMA_CHANNEL_ANY 0xFF

void dma_initialize(void);
int dma_channel_is_reserved(uint8_t ch);
uint8_t dma_channel_reserve(uint8_t ch, void (*handler)(uint32_t, uint32_t, uint32_t),
                            uint32_t arg, uint32_t flags);
int dma_memcpy(uint8_t ch, const void *src, void *dst, uint32_t len);

#endif
