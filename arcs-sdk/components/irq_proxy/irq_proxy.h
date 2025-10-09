#ifndef IRQ_PROXY_H
#define IRQ_PROXY_H

#include <stdint.h>

typedef enum {
    IRQ_PROXY_CHAN_ROTATE_DMA = 0,
    IRQ_PROXY_CHAN_FUNC_REQUEST,     // CP请求AP执行函数
    IRQ_PROXY_CHAN_FUNC_COMPLETE,    // AP通知CP函数执行完成
    IRQ_PROXY_CHAN_LVGL_BLEND_DMA,  // LVGL blend 用到CPDMA
    IRQ_PROXY_CHAN_MAX
} irq_proxy_channel_t;

typedef int32_t (*irq_proxy_callback_fn)(void);

typedef struct {
    irq_proxy_callback_fn callback;
} irq_proxy_device_t;

void irq_proxy_init(void);
int32_t irq_proxy_register_callback(irq_proxy_channel_t channel, 
                                  irq_proxy_callback_fn callback);
int32_t irq_proxy_trigger(irq_proxy_channel_t channel);

#endif /* IRQ_PROXY_H */