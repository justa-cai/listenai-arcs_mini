#pragma once

#include <stdint.h>

int adc_sample_init(void);

int adc_sample_get_channel_value(uint32_t channel);