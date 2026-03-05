#if CONFIG_GPIO_BUTTON

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lisa_log.h"

#include "Driver_GPIO.h"
#include "IOMuxManager.h"

#include "flexible_button.h"
#include "button_gpio.h"

#define TAG "gpio_btn"

#define GPIO_PORT_NUM 2

typedef struct {
    button_gpio_cb_t cb;           /**< button callback */
    button_gpio_config_t cfg;      /**< button config */
    flex_button_t base;          /**< button driver */
} button_gpio_obj;

static bool gpio_is_valid(uint8_t gpio_port, uint8_t gpio_pin_num);

uint8_t button_gpio_get_key_level(void *button_driver)
{
    flex_button_t *btn = (flex_button_t *)button_driver;
    uint8_t gpio_level = 0;
    button_gpio_obj *gpio_btn = __containerof(btn, button_gpio_obj, base);

    LISA_RETURN_ON_FALSE(gpio_btn->cfg.gpio_port < GPIO_PORT_NUM, false);
    LISA_RETURN_ON_FALSE(gpio_is_valid(gpio_btn->cfg.gpio_port, gpio_btn->cfg.gpio_pin_num), false);

    if (gpio_btn->cfg.gpio_port == CSK_IOMUX_PAD_A){
        gpio_level = GPIO_PinRead(GPIOA(), (1UL << gpio_btn->cfg.gpio_pin_num));
    } else {
        gpio_level = GPIO_PinRead(GPIOB(), (1UL << gpio_btn->cfg.gpio_pin_num));
    }

    return gpio_level;
}

static void button_gpio_evt_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;
    button_gpio_obj *gpio_btn = __containerof(btn, button_gpio_obj, base);
    // LISA_LOGI(TAG, "gpio btn id:%d, event_id:%d", btn->id, btn->event);

    if (gpio_btn->cb) {
        gpio_btn->cb((button_event_t)(btn->event), &gpio_btn->cfg);
    }
}

static bool gpio_is_valid(uint8_t gpio_port, uint8_t gpio_pin_num)
{
    if (gpio_port == CSK_IOMUX_PAD_A) {
        return (gpio_pin_num < CSK_IOMUX_PAD_A_MAX_PIN);
    } else if (gpio_port == CSK_IOMUX_PAD_B) {
        return (gpio_pin_num < CSK_IOMUX_PAD_B_MAX_PIN);
    } else {
        return false;
    }
}

static void button_gpio_init(uint8_t gpio_port, uint8_t gpio_pin_num)
{
    IOMuxManager_PinConfigure(gpio_port, gpio_pin_num, CSK_IOMUX_FUNC_DEFAULT);
    if (gpio_port == CSK_IOMUX_PAD_A){
        GPIO_SetDir(GPIOA(), (1UL << gpio_pin_num), CSK_GPIO_DIR_INPUT);
    } else {
        GPIO_SetDir(GPIOB(), (1UL << gpio_pin_num), CSK_GPIO_DIR_INPUT);
    }
}

bool button_new_gpio_device(button_config_t *button_config, button_gpio_config_t *gpio_config, button_gpio_cb_t cb)
{
    /* check */
    LISA_RETURN_ON_FALSE((button_config && gpio_config && cb), false);
    LISA_RETURN_ON_FALSE(gpio_config->gpio_port < GPIO_PORT_NUM, false);
    LISA_RETURN_ON_FALSE(gpio_is_valid(gpio_config->gpio_port, gpio_config->gpio_pin_num), false);

    LISA_RETURN_ON_FALSE(button_config->short_press_time_ms > 0, false);
    LISA_RETURN_ON_FALSE(button_config->long_press_time_ms > 0, false);
    LISA_RETURN_ON_FALSE(button_config->long_hold_time_ms > 0, false);
    LISA_RETURN_ON_FALSE(button_config->long_press_time_ms > button_config->short_press_time_ms, false);
    LISA_RETURN_ON_FALSE(button_config->long_hold_time_ms > button_config->long_press_time_ms, false);
    LISA_RETURN_ON_FALSE(button_config->press_logic_level < 2, false);

    button_gpio_obj *gpio_btn = calloc(1, sizeof(button_gpio_obj));
    LISA_RETURN_ON_FALSE(gpio_btn, false);
    
    /* gpio init */
    button_gpio_init(gpio_config->gpio_port, gpio_config->gpio_pin_num);
    
    gpio_btn->cb = cb;
    memcpy(&gpio_btn->cfg, gpio_config, sizeof(button_gpio_config_t));

    gpio_btn->base.usr_button_read = button_gpio_get_key_level;
    gpio_btn->base.cb = button_gpio_evt_cb;
    gpio_btn->base.pressed_logic_level = button_config->press_logic_level;
    gpio_btn->base.short_press_start_tick = FLEX_MS_TO_SCAN_CNT(button_config->short_press_time_ms);
    gpio_btn->base.long_press_start_tick = FLEX_MS_TO_SCAN_CNT(button_config->long_press_time_ms);
    gpio_btn->base.long_hold_start_tick = FLEX_MS_TO_SCAN_CNT(button_config->long_hold_time_ms);

    button_create(&gpio_btn->base);

    return true;
err:
    if (gpio_btn) {
        free(gpio_btn);
    }
    return false;
}

#endif  /* CONFIG_GPIO_BUTTON */
