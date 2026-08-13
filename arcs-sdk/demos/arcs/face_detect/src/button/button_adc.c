#if CONFIG_ADC_BUTTON
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "lisa_log.h"

#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "Driver_GPADC.h"
#include "adc_sample.h"

#include "PowerManager.h"
#include "ClockManager.h"

#include "flexible_button.h"
#include "button_adc.h"

#define TAG "adc_btn"

#define ADC_UNIT_NUM            CONFIG_ADC_UNIT_NUM
#define ADC_BUTTON_MAX_CHANNEL  CONFIG_ADC_MAX_CHANNEL
#define ADC_BUTTON_MAX_BUTTON   CONFIG_ADC_BUTTON_MAX_BUTTON_PER_CHANNEL
#define NO_OF_SAMPLES           CONFIG_ADC_BUTTON_SAMPLE_TIMES     //Multisampling

typedef struct {
    uint16_t min;
    uint16_t max;
} button_data_t;

typedef struct {
    uint8_t channel;
    uint8_t is_init;
    button_data_t btns[ADC_BUTTON_MAX_BUTTON];
} btn_adc_channel_t;

typedef struct {
    uint32_t is_configured;
    btn_adc_channel_t ch[ADC_BUTTON_MAX_CHANNEL];
    uint8_t ch_num;
} btn_adc_unit_t;

typedef struct {
    btn_adc_unit_t unit[ADC_UNIT_NUM];
} button_adc_t;

typedef struct {
    flex_button_t base;
    button_adc_cb_t cb; 
    button_adc_config_t cfg;
} button_adc_obj;

static button_adc_t g_button = {0};

static int find_unused_channel(adc_unit_t unit_id)
{
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (0 == g_button.unit[unit_id].ch[i].is_init) {
            return i;
        }
    }
    return -1;
}

static int find_channel(adc_unit_t unit_id, uint8_t channel)
{
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (channel == g_button.unit[unit_id].ch[i].channel) {
            return i;
        }
    }
    return -1;
}

static uint32_t get_adc_channel(uint8_t channel)
{
    switch (channel) {
        case 0:
            return CSK_GPADC_CHANNEL_SEL_0;
        case 1:
            return CSK_GPADC_CHANNEL_SEL_1;
        case 2:
            return CSK_GPADC_CHANNEL_SEL_2;
        case 3:
            return CSK_GPADC_CHANNEL_SEL_3;
        case 4:
            return CSK_GPADC_CHANNEL_SEL_4;
        case 5:
            return CSK_GPADC_CHANNEL_SEL_5;
        default:
            LISA_LOGW(TAG, "channel:%d is not supported", channel);
            return CSK_GPADC_CHANNEL_SEL_0;
            break;
    }
}

static uint32_t get_adc_voltage(adc_unit_t unit_id, uint8_t channel)
{
    uint32_t adc_reading = 0;
    int adc_raw = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        // adc_oneshot_read(g_button.unit[unit_id].adc_handle, channel, &adc_raw);
        adc_raw = adc_sample_get_channel_value(0);
        adc_reading += adc_raw;
    }
    adc_reading /= NO_OF_SAMPLES;
    return adc_reading;
}

uint8_t button_adc_get_key_level(void *button_driver)
{
    flex_button_t *btn = (flex_button_t *)button_driver;
    button_adc_obj *adc_btn = __containerof(btn, button_adc_obj, base);
    static uint16_t vol = 0;
    uint32_t ch = adc_btn->cfg.adc_channel;
    uint32_t index = adc_btn->cfg.button_index;
    LISA_RETURN_ON_FALSE(ch < ADC_BUTTON_MAX_CHANNEL, 0);
    LISA_RETURN_ON_FALSE(index < ADC_BUTTON_MAX_BUTTON, 0);

    int ch_index = find_channel(adc_btn->cfg.unit_id, ch);
    LISA_RETURN_ON_FALSE(ch_index >= 0, 0);

    /** It starts only when the elapsed time is more than 1ms */
    // if ((esp_timer_get_time() - g_button.unit[adc_btn->unit_id].ch[ch_index].last_time) > 1000) {
    //     vol = get_adc_voltage(adc_btn->unit_id, ch);
    //     g_button.unit[adc_btn->unit_id].ch[ch_index].last_time = esp_timer_get_time();
    // }
    vol = get_adc_voltage(adc_btn->cfg.unit_id, ch);
    // g_button.unit[adc_btn->unit_id].ch[ch_index].last_time = esp_timer_get_time();
    // g_button.unit[adc_btn->cfg.unit_id].ch[ch_index].last_time = 0;

    if (vol <= g_button.unit[adc_btn->cfg.unit_id].ch[ch_index].btns[index].max &&
            vol >= g_button.unit[adc_btn->cfg.unit_id].ch[ch_index].btns[index].min) {
        return true;
    }
    return false;
}


static int adc_channel_io_init(adc_unit_t unit_id, uint8_t channel)
{
    LISA_RETURN_ON_FALSE(unit_id < ADC_UNIT_NUM, false);
    LISA_RETURN_ON_FALSE(channel < ADC_BUTTON_MAX_CHANNEL, false);

    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, channel + 2, CSK_AON_IOMUX_FUNC_ALTER3);

    switch (channel) {
        case 0:
            IP_AON_IOMUX->REG_PAD_AON_GPIOB_02.bit.PAD_AON_GPIOB_02_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;
            break;
        case 1:
            IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;
            break;
        case 2:
            IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;
            break;
        case 3:
            IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;
            break;
        case 4:
            IP_AON_IOMUX->REG_PAD_AON_GPIOB_06.bit.PAD_AON_GPIOB_06_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;
            break;
        case 5:
            IP_AON_IOMUX->REG_PAD_AON_GPIOB_07.bit.PAD_AON_GPIOB_07_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;
            break;
        default:
            break;
    }
}

static void button_adc_evt_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;
    button_adc_obj *adc_btn = __containerof(btn, button_adc_obj, base);
    // LISA_LOGI(TAG, "btn id:%d, event_id:%d", btn->id, btn->event);

    if (adc_btn->cb) {
        adc_btn->cb((button_event_t)(btn->event), &adc_btn->cfg);
    }
}

bool button_new_adc_device(button_config_t *button_config, button_adc_config_t *adc_config, button_adc_cb_t cb)
{
    /* check */
    LISA_RETURN_ON_FALSE((button_config && adc_config && cb), false);

    LISA_RETURN_ON_FALSE(adc_config->unit_id < ADC_UNIT_NUM, false);
    LISA_RETURN_ON_FALSE(adc_config->adc_channel < ADC_BUTTON_MAX_CHANNEL, false);
    LISA_RETURN_ON_FALSE(adc_config->button_index < ADC_BUTTON_MAX_BUTTON, false);
    LISA_RETURN_ON_FALSE(adc_config->min < adc_config->max, false);

    LISA_RETURN_ON_FALSE(button_config->short_press_time_ms > 0, false);
    LISA_RETURN_ON_FALSE(button_config->long_press_time_ms > 0, false);
    LISA_RETURN_ON_FALSE(button_config->long_hold_time_ms > 0, false);
    LISA_RETURN_ON_FALSE(button_config->long_press_time_ms > button_config->short_press_time_ms, false);
    LISA_RETURN_ON_FALSE(button_config->long_hold_time_ms > button_config->long_press_time_ms, false);
    LISA_RETURN_ON_FALSE(button_config->press_logic_level < 2, false);

    button_adc_obj *adc_btn = calloc(1, sizeof(button_adc_obj));
    LISA_RETURN_ON_FALSE(adc_btn, false);
    
    /* find channel */
    int ch_index = find_channel(adc_config->unit_id, adc_config->adc_channel);
    if (ch_index >= 0) { /**< the channel has been initialized */
        LISA_GOTO_ON_FALSE(g_button.unit[adc_config->unit_id].ch[ch_index].btns[adc_config->button_index].max == 0, err);
    } else { /**< this is a new channel */
        int unused_ch_index = find_unused_channel(adc_config->unit_id);
        LISA_GOTO_ON_FALSE(unused_ch_index >= 0, err);
        ch_index = unused_ch_index;
    }

    /* adc init */
    if (0 == g_button.unit[adc_config->unit_id].is_configured) {
        // if (NULL == adc_config->adc_handle) {
            // adc init
            adc_sample_init();
            // LISA_GOTO_ON_FALSE(adc_config->adc_handle, err);
            g_button.unit[adc_config->unit_id].is_configured = 1;
        // } else {
        //     g_button.unit[adc_config->unit_id].adc_handle = *adc_config->adc_handle;
        //     LOGI(TAG, "ADC1 has been initialized");
        //     g_button.unit[adc_config->unit_id].is_configured = 1;
        // }
    }

    /* adc channel io init */
    if (0 == g_button.unit[adc_config->unit_id].ch[ch_index].is_init) {
        // adc_channel_io_init(adc_config->unit_id, adc_config->adc_channel);
        g_button.unit[adc_config->unit_id].ch[ch_index].channel = adc_config->adc_channel;
        g_button.unit[adc_config->unit_id].ch[ch_index].is_init = 1;
        // g_button.unit[adc_config->unit_id].ch[ch_index].last_time = 0;
    }
    g_button.unit[adc_config->unit_id].ch[ch_index].btns[adc_config->button_index].max = adc_config->max;
    g_button.unit[adc_config->unit_id].ch[ch_index].btns[adc_config->button_index].min = adc_config->min;
    g_button.unit[adc_config->unit_id].ch_num++;

    adc_btn->cb = cb;
    memcpy(&adc_btn->cfg, adc_config, sizeof(button_adc_config_t));
    
    /* register to flex_button */
    adc_btn->base.usr_button_read = button_adc_get_key_level;
    adc_btn->base.cb = button_adc_evt_cb;
    adc_btn->base.pressed_logic_level = 1;
    adc_btn->base.short_press_start_tick = FLEX_MS_TO_SCAN_CNT(button_config->short_press_time_ms);
    adc_btn->base.long_press_start_tick = FLEX_MS_TO_SCAN_CNT(button_config->long_press_time_ms);
    adc_btn->base.long_hold_start_tick = FLEX_MS_TO_SCAN_CNT(button_config->long_hold_time_ms);
    button_create(&adc_btn->base);
    // *handle = adc_btn;

    return true;
err:
    if (adc_btn) {
        free(adc_btn);
    }
    return false;
}

#endif /* CONFIG_ADC_BUTTON */
