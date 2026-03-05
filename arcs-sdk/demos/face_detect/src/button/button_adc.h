#pragma once

#if CONFIG_ADC_BUTTON

#include "button.h"

typedef uint32_t adc_unit_t;

typedef struct {
    void *adc_handle;                               /**< handle of adc unit, if NULL will create new one internal, else will use the handle */
    adc_unit_t unit_id;                               /**< ADC unit */
    uint8_t adc_channel;                             /**< Channel of ADC */
    uint8_t button_index;                            /**< button index on the channel */
    uint16_t min;                                    /**< min voltage in mv corresponding to the button */
    uint16_t max;                                    /**< max voltage in mv corresponding to the button */
} button_adc_config_t;

typedef void (*button_adc_cb_t)(button_event_t evt, button_adc_config_t *user_data);

bool button_new_adc_device(button_config_t *button_config, button_adc_config_t *adc_config, button_adc_cb_t cb);

#endif /* CONFIG_ADC_BUTTON */
