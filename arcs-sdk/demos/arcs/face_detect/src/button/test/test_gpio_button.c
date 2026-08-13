#include "button_gpio.h"
#include "IOMuxManager.h"

static void gpio_button_callback(button_event_t evt, button_gpio_config_t *user_data)
{
    printf("gpio_button_port: %d, pin:%d, event: %d\n", user_data->gpio_port, user_data->gpio_pin_num, evt);
}

void test_gpio_button(void)
{
    button_config_t btn_cfg = {
        .short_press_time_ms = 1500,
        .long_press_time_ms = 2000,
        .long_hold_time_ms = 4500,
        .press_logic_level = 0,
    };

    button_gpio_config_t btn_gpio_cfg = {0};

    btn_gpio_cfg.gpio_port = CSK_IOMUX_PAD_A;
    btn_gpio_cfg.gpio_pin_num = 12;
    button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, gpio_button_callback);

    btn_gpio_cfg.gpio_port = CSK_IOMUX_PAD_A;
    btn_gpio_cfg.gpio_pin_num = 16;
    button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, gpio_button_callback);

    btn_gpio_cfg.gpio_port = CSK_IOMUX_PAD_A;
    btn_gpio_cfg.gpio_pin_num = 27;
    button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, gpio_button_callback);

    // btn_gpio_cfg.gpio_port = CSK_IOMUX_PAD_A;
    // btn_gpio_cfg.gpio_pin_num = 0;
    // button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, gpio_button_callback);
}
