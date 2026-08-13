#include "adc_key.h"

void test_adc_button(button_adc_cb_t cb)
{
    button_adc_config_t btn_adc_cfg = {
        .unit_id = 0,
        .adc_channel = 4,       // PB6
    };

    button_config_t btn_cfg = {
        .short_press_time_ms = 1500,
        .long_press_time_ms = 2000,
        .long_hold_time_ms = 4500,
        .press_logic_level = 1,
    };

    const uint16_t vol[10] = {0, 1115, 1650};

    for (int i = 0; i < CONFIG_ADC_BUTTON_MAX_BUTTON_PER_CHANNEL; i++) {
        btn_adc_cfg.button_index = i;

        if (i == 0) {
            btn_adc_cfg.min = (0 + vol[i]) / 2;
        } else {
            btn_adc_cfg.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == (CONFIG_ADC_BUTTON_MAX_BUTTON_PER_CHANNEL - 1)) {
            btn_adc_cfg.max = (vol[i] + 3300) / 2;
        } else {
            btn_adc_cfg.max = (vol[i] + vol[i + 1]) / 2;
        }

        button_new_adc_device(&btn_cfg, &btn_adc_cfg, cb);
    }

}
