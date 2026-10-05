#ifndef __LED_H__
#define __LED_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void service_led_init(void);
void service_led_on(void);
void service_led_off(void);
void service_led_blink(uint32_t on_ms, uint32_t off_ms);
bool service_led_is_on(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
