#pragma once

#if CONFIG_GPIO_BUTTON

#include "button.h"

typedef struct {
    // button_handle_t *button_handle;                              
    uint32_t gpio_port;
    uint32_t gpio_pin_num;
} button_gpio_config_t;

typedef void (*button_gpio_cb_t)(button_event_t evt, button_gpio_config_t *user_data);

bool button_new_gpio_device(button_config_t *button_config, button_gpio_config_t *gpio_config, button_gpio_cb_t cb);

#endif  /* CONFIG_GPIO_BUTTON */
