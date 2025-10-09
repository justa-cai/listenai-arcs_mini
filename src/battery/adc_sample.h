/**
 * @file adc_samples.h
 * @brief 
 * @version 0.1
 * @date 2025-04-16
 * 
 * @copyright Copyright (C) 2025 ANHUI LISTENAI Co., Ltd. All Rights Reserved.
 */

#ifndef __ADC_SAMPLE_H__
#define __ADC_SAMPLE_H__

#ifdef __cplusplus
extern "C" {
#endif


void adc_sample_init(uint32_t ref, bool enable_scale, uint32_t channel);

uint16_t adc_sample_get_channel_value(uint32_t channel);


#ifdef __cplusplus
}
#endif

#endif