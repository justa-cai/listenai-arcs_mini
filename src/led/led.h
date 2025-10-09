#ifndef __LED_H__
#define __LED_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Public APIs */
void app_led_init(void);
void app_led_on(void);
void app_led_off(void);
void app_led_blink(uint32_t on_ms, uint32_t off_ms);
void app_led_stop(void);

/* Platform hooks (weak) - implement in BSP if needed */
void led_hw_init(void);
void led_hw_on(void);
void led_hw_off(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
