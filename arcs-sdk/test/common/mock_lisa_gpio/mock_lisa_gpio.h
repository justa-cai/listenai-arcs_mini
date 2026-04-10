#ifndef MOCK_LISA_GPIO_H
#define MOCK_LISA_GPIO_H

#include "lisa_gpio.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(int, mock_lisa_gpio_configure, lisa_device_t *, uint32_t, lisa_gpio_flags_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_lisa_gpio_write_pin, lisa_device_t *, uint32_t, uint32_t);

void mock_lisa_gpio_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_LISA_GPIO_H */
