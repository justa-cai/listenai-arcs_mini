/**
 * @file lisa_btn.c
 * @brief Button driver - supports both ADC and GPIO buttons
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "lisa_log.h"
#include "lisa_thread.h"
#include "flexible_button.h"
#include "lisa_btn.h"
#include "lisa_adc.h"
#include "lisa_device.h"
#ifdef CONFIG_BOARD_ARCS_MINI
#include "lisa_gpio.h"
#else
#include "Driver_GPIO.h"
#endif

#define TAG "btn"
#define BTN_DEBUG_ENABLE    (0)
#define BTN_MAX_ADC_COUNT   (8)
#define BTN_MAX_GPIO_COUNT  (8)

/* ADC按键上下文 */
struct adc_btn_ctx {
    bool is_init;
    lisa_btn_adc_mode_t mode;
    uint8_t button_count;
    flex_button_t btns[BTN_MAX_ADC_COUNT];
    lisa_device_t *adc_dev;
    uint16_t ref_voltage;
    uint8_t resolution;
    struct {
        uint8_t adc_channel;
        const lisa_btn_adc_range_t *ranges;
    } one_to_many;
    struct {
        uint8_t *channels;
        lisa_btn_adc_range_t *ranges;
    } one_to_one;
    lisa_btn_cb_t callback;
    void *user_data;
    lisa_btn_time_config_t time_config;
};

/* GPIO按键上下文 */
struct gpio_btn_ctx {
    bool is_init;
    uint8_t button_count;
    flex_button_t btns[BTN_MAX_GPIO_COUNT];
#ifdef CONFIG_BOARD_ARCS_MINI
    lisa_device_t *gpio_dev;
#else
    void *gpio_res;
#endif
    const lisa_btn_gpio_item_t *buttons;
    lisa_btn_cb_t callback;
    void *user_data;
    lisa_btn_time_config_t time_config;
};

static struct adc_btn_ctx g_adc_ctx = {0};
static struct gpio_btn_ctx g_gpio_ctx = {0};
static bool g_scan_task_created = false;
static uint16_t g_scan_period = 5;

/* ===== ADC按键读取 ===== */

static uint16_t adc_read_voltage(uint32_t channel)
{
    uint16_t adc_value = 0;
    int ret;
    
    if (!g_adc_ctx.adc_dev) {
        return 0xFFFF;
    }
    
    ret = lisa_adc_read(g_adc_ctx.adc_dev, channel, &adc_value);
    if (ret != 0) {
        return 0xFFFF;
    }
    
    return LISA_ADC_RAW_TO_MV(adc_value, g_adc_ctx.ref_voltage, g_adc_ctx.resolution);
}

static uint8_t adc_btn_read_one_to_many(uint8_t btn_id)
{
    uint16_t voltage = adc_read_voltage(g_adc_ctx.one_to_many.adc_channel);
    
    if (voltage == 0xFFFF) {
        return 0;
    }
    
    if (btn_id < g_adc_ctx.button_count) {
        const lisa_btn_adc_range_t *range = &g_adc_ctx.one_to_many.ranges[btn_id];
        if (voltage >= range->voltage_min && voltage <= range->voltage_max) {
#if BTN_DEBUG_ENABLE
            LISA_LOGI(TAG, "ADC Btn%d pressed: %dmV", btn_id, voltage);
#endif
            return 1;
        }
    }
    
    return 0;
}

static uint8_t adc_btn_read_one_to_one(uint8_t btn_id)
{
    if (btn_id >= g_adc_ctx.button_count || !g_adc_ctx.one_to_one.channels) {
        return 0;
    }
    
    uint8_t channel = g_adc_ctx.one_to_one.channels[btn_id];
    uint16_t voltage = adc_read_voltage(channel);
    
    if (voltage == 0xFFFF) {
        return 0;
    }
    
    const lisa_btn_adc_range_t *range = &g_adc_ctx.one_to_one.ranges[btn_id];
    if (voltage >= range->voltage_min && voltage <= range->voltage_max) {
        return 1;
    }
    
    return 0;
}

static uint8_t adc_flex_btn_read_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;
    uint8_t btn_id = btn->id;
    uint8_t pressed = 0;
    
    if (g_adc_ctx.mode == LISA_BTN_ADC_MODE_ONE_TO_MANY) {
        pressed = adc_btn_read_one_to_many(btn_id);
    } else {
        pressed = adc_btn_read_one_to_one(btn_id);
    }
    
    return pressed ? 0 : 1;
}

static void adc_flex_btn_event_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;
    
    LISA_LOGI(TAG, "[ADC Btn%d] event=%d", btn->id, btn->event);
    
    if (g_adc_ctx.callback) {
        g_adc_ctx.callback((lisa_btn_event_t)btn->event, btn->id, g_adc_ctx.user_data);
    }
}

/* ===== GPIO按键读取 ===== */

static uint8_t gpio_flex_btn_read_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;
    uint8_t btn_id = btn->id;
    
    if (btn_id >= g_gpio_ctx.button_count || !g_gpio_ctx.buttons || 
#ifdef CONFIG_BOARD_ARCS_MINI
        !g_gpio_ctx.gpio_dev
#else
        !g_gpio_ctx.gpio_res
#endif
    ) {
        return 1;  /* 释放 */
    }
    
    const lisa_btn_gpio_item_t *item = &g_gpio_ctx.buttons[btn_id];
#ifdef CONFIG_BOARD_ARCS_MINI
    int32_t value = lisa_gpio_read_pin(g_gpio_ctx.gpio_dev, item->pin_num);
    if (value < 0) {
        return 1;  /* 读取失败，返回释放 */
    }

    uint8_t pin_level = (value == LISA_GPIO_HIGH) ? 1 : 0;
#else
    uint32_t pin_mask = (1UL << item->pin_num);
    
    int32_t value = GPIO_PinRead(g_gpio_ctx.gpio_res, pin_mask);
    if (value < 0) {
        return 1;  /* 读取失败，返回释放 */
    }
    
    /* 检查是否按下 */
    uint8_t pin_level = (value & pin_mask) ? 1 : 0;
#endif
    uint8_t pressed = (pin_level == item->active_level) ? 1 : 0;
    
#if BTN_DEBUG_ENABLE
    if (pressed) {
        LISA_LOGI(TAG, "GPIO Btn%d pressed: pin=%d, value=%d", btn_id, item->pin_num, pin_level);
    }
#endif
    
    return pressed ? 0 : 1;
}

static void gpio_flex_btn_event_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;
    
    LISA_LOGI(TAG, "[GPIO Btn%d] event=%d", btn->id, btn->event);
    
    if (g_gpio_ctx.callback) {
        g_gpio_ctx.callback((lisa_btn_event_t)btn->event, btn->id, g_gpio_ctx.user_data);
    }
}

/* ===== 扫描任务 ===== */

static void btn_scan_task(void *arg)
{
    LISA_LOGI(TAG, "Button scan task started");
    lisa_thread_mdelay(2000);
    
    while (1) {
        flex_button_scan();
        lisa_thread_mdelay(g_scan_period);
    }
}

static int create_scan_task_if_needed(uint16_t scan_period)
{
    if (g_scan_task_created) {
        return 0;
    }
    
    g_scan_period = scan_period;
    
    lisa_thread_attr_t attr = {
        .name = "btn",
        .stack_size = 2048,
        .priority = 6,
    };
    lisa_thread_t *td = lisa_thread_create(&attr, btn_scan_task, NULL);
    if (!td) {
        LISA_LOGE(TAG, "Failed to create scan task");
        return -1;
    }
    
    g_scan_task_created = true;
    return 0;
}

/* ===== 公共接口 ===== */

int lisa_btn_adc_init(const lisa_btn_adc_config_t *config)
{
    int ret;
    uint8_t i;
    
    if (!config || !config->callback) {
        LISA_LOGE(TAG, "Invalid ADC config");
        return -1;
    }
    
    if (g_adc_ctx.is_init) {
        LISA_LOGW(TAG, "ADC buttons already initialized");
        return 0;
    }
    
    memset(&g_adc_ctx, 0, sizeof(g_adc_ctx));
    
    g_adc_ctx.mode = config->mode;
    g_adc_ctx.ref_voltage = config->ref_voltage;
    g_adc_ctx.resolution = config->resolution;
    g_adc_ctx.callback = config->callback;
    g_adc_ctx.user_data = config->user_data;
    g_adc_ctx.time_config = config->time_config;
    
    g_adc_ctx.adc_dev = lisa_device_get(config->adc_dev_name);
    if (!g_adc_ctx.adc_dev) {
        LISA_LOGE(TAG, "Failed to get ADC device: %s", config->adc_dev_name);
        return -2;
    }
    LISA_LOGI(TAG, "ADC device: %s", config->adc_dev_name);
    
    if (config->mode == LISA_BTN_ADC_MODE_ONE_TO_MANY) {
        g_adc_ctx.button_count = config->one_to_many.button_count;
        g_adc_ctx.one_to_many.adc_channel = config->one_to_many.adc_channel;
        g_adc_ctx.one_to_many.ranges = config->one_to_many.ranges;
        
        lisa_adc_channel_config_t adc_cfg = {
            .reference = LISA_ADC_REF_VDD_3V6,
            .resolution = LISA_ADC_RESOLUTION_10BIT,
        };
        ret = lisa_adc_channel_setup(g_adc_ctx.adc_dev, config->one_to_many.adc_channel, &adc_cfg);
        if (ret != 0) {
            LISA_LOGE(TAG, "ADC channel %d setup failed: %d", config->one_to_many.adc_channel, ret);
            return -3;
        }
        LISA_LOGI(TAG, "ADC channel %d configured", config->one_to_many.adc_channel);
        
    } else {
        g_adc_ctx.button_count = config->one_to_one.button_count;
        LISA_LOGW(TAG, "One-to-one mode not fully implemented");
    }
    
    if (g_adc_ctx.button_count > BTN_MAX_ADC_COUNT) {
        g_adc_ctx.button_count = BTN_MAX_ADC_COUNT;
    }
    
    for (i = 0; i < g_adc_ctx.button_count; i++) {
        g_adc_ctx.btns[i].id = i;
        g_adc_ctx.btns[i].usr_button_read = adc_flex_btn_read_cb;
        g_adc_ctx.btns[i].cb = adc_flex_btn_event_cb;
        g_adc_ctx.btns[i].pressed_logic_level = 0;
        g_adc_ctx.btns[i].short_press_start_tick = FLEX_MS_TO_SCAN_CNT(config->time_config.short_press_time);
        g_adc_ctx.btns[i].long_press_start_tick = FLEX_MS_TO_SCAN_CNT(config->time_config.long_press_time);
        g_adc_ctx.btns[i].long_hold_start_tick = FLEX_MS_TO_SCAN_CNT(config->time_config.long_hold_time);
        
        flex_button_register(&g_adc_ctx.btns[i]);
        LISA_LOGI(TAG, "ADC Button %d registered", i);
    }
    
    ret = create_scan_task_if_needed(config->time_config.scan_period);
    if (ret != 0) {
        return -4;
    }
    
    g_adc_ctx.is_init = true;
    LISA_LOGI(TAG, "ADC button init done: %d buttons, mode=%d", g_adc_ctx.button_count, g_adc_ctx.mode);
    
    return 0;
}

int lisa_btn_gpio_init(const lisa_btn_gpio_config_t *config)
{
    int ret;
    uint8_t i;
    
    if (!config || !config->callback || !config->buttons) {
        LISA_LOGE(TAG, "Invalid GPIO config");
        return -1;
    }
    
    if (g_gpio_ctx.is_init) {
        LISA_LOGW(TAG, "GPIO buttons already initialized");
        return 0;
    }
    
    memset(&g_gpio_ctx, 0, sizeof(g_gpio_ctx));
    
    g_gpio_ctx.button_count = config->button_count;
    g_gpio_ctx.buttons = config->buttons;
    g_gpio_ctx.callback = config->callback;
    g_gpio_ctx.user_data = config->user_data;
    g_gpio_ctx.time_config = config->time_config;
    
    /* 获取GPIO资源 (GPIOA或GPIOB) */
#ifdef CONFIG_BOARD_ARCS_MINI
    g_gpio_ctx.gpio_dev = lisa_device_get(config->gpio_dev_name);
    if (!g_gpio_ctx.gpio_dev) {
        LISA_LOGE(TAG, "Failed to get GPIO device: %s", config->gpio_dev_name);
        return -2;
    }
#else
    g_gpio_ctx.gpio_res = GPIOA();
    if (!g_gpio_ctx.gpio_res) {
        LISA_LOGE(TAG, "Failed to get GPIO resource");
        return -2;
    }
    
    /* 初始化GPIO */
    ret = GPIO_Initialize(g_gpio_ctx.gpio_res, NULL, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "GPIO initialize failed: %d", ret);
        return -3;
    }
    
    ret = GPIO_PowerControl(g_gpio_ctx.gpio_res, CSK_POWER_FULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "GPIO power control failed: %d", ret);
        return -3;
    }
#endif
    LISA_LOGI(TAG, "GPIO initialized");
    
    if (g_gpio_ctx.button_count > BTN_MAX_GPIO_COUNT) {
        g_gpio_ctx.button_count = BTN_MAX_GPIO_COUNT;
    }
    
    /* 配置GPIO引脚 */
    for (i = 0; i < g_gpio_ctx.button_count; i++) {
        const lisa_btn_gpio_item_t *item = &config->buttons[i];
#ifdef CONFIG_BOARD_ARCS_MINI
        lisa_gpio_configure(g_gpio_ctx.gpio_dev, item->pin_num, 
            LISA_GPIO_INPUT | 
            (item->pull_enable ? (item->pull_up ? LISA_GPIO_PULL_UP : LISA_GPIO_PULL_DOWN) : 0));
#else
        uint32_t pin_mask = (1UL << item->pin_num);
        
        /* 设置为输入 */
        ret = GPIO_SetDir(g_gpio_ctx.gpio_res, pin_mask, CSK_GPIO_DIR_INPUT);
        if (ret != 0) {
            LISA_LOGE(TAG, "GPIO pin %d set dir failed: %d", item->pin_num, ret);
            return -4;
        }
        
        /* 设置上下拉 */
        uint32_t pull_mode = CSK_GPIO_MODE_PULL_NONE;
        if (item->pull_enable) {
            pull_mode = item->pull_up ? CSK_GPIO_MODE_PULL_UP : CSK_GPIO_MODE_PULL_DOWN;
        }
        ret = GPIO_Control(g_gpio_ctx.gpio_res, pull_mode, pin_mask);
        if (ret != 0) {
            LISA_LOGW(TAG, "GPIO pin %d set pull mode failed: %d", item->pin_num, ret);
        }
#endif
        
        g_gpio_ctx.btns[i].id = i;
        g_gpio_ctx.btns[i].usr_button_read = gpio_flex_btn_read_cb;
        g_gpio_ctx.btns[i].cb = gpio_flex_btn_event_cb;
        g_gpio_ctx.btns[i].pressed_logic_level = 0;
        g_gpio_ctx.btns[i].short_press_start_tick = FLEX_MS_TO_SCAN_CNT(config->time_config.short_press_time);
        g_gpio_ctx.btns[i].long_press_start_tick = FLEX_MS_TO_SCAN_CNT(config->time_config.long_press_time);
        g_gpio_ctx.btns[i].long_hold_start_tick = FLEX_MS_TO_SCAN_CNT(config->time_config.long_hold_time);
        
        flex_button_register(&g_gpio_ctx.btns[i]);
        LISA_LOGI(TAG, "GPIO Button %d registered (pin=%d)", i, item->pin_num);
    }
    
    ret = create_scan_task_if_needed(config->time_config.scan_period);
    if (ret != 0) {
        return -5;
    }
    
    g_gpio_ctx.is_init = true;
    LISA_LOGI(TAG, "GPIO button init done: %d buttons", g_gpio_ctx.button_count);
    
    return 0;
}

void lisa_btn_deinit(void)
{
    g_adc_ctx.is_init = false;
    g_gpio_ctx.is_init = false;
}
