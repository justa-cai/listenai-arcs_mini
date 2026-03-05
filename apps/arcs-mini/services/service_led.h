#ifndef __LED_H__
#define __LED_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void service_led_init(void);
void service_led_on(void);
void service_led_off(void);
void service_led_blink(uint32_t on_ms, uint32_t off_ms);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
