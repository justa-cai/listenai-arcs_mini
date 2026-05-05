#ifndef __BOOT_GPIO_CHECK_H__
#define __BOOT_GPIO_CHECK_H__

#include <stdbool.h>

#ifdef CONFIG_BOOT_CHECK_POWER_KEY
bool boot_gpio_check_power_key_triggered(void);
#else
static inline bool boot_gpio_check_power_key_triggered(void)
{
    return false;
}
#endif

#endif /* __BOOT_GPIO_CHECK_H__ */
