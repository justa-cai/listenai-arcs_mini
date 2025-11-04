/*
 * adc_pdm.h
 *
 *
 */

#ifndef __ADC_PDM_H
#define __ADC_PDM_H

#include "arcs_ap.h"
#include "apc.h"
#include "apc_reg.h"
#include "Driver_ADC_PDM.h"
//#include "pmuctrl_reg.h" // for pmuctrl regiger (power)
#include "audio_codec.h" // CODEC Registers etc.

//====================== Register-related Macros & Functions =================

// Sample Rate: refer to CODEC / AON_CODEC registers
static const REG_VAL_MAP sc_ADC_SR_val_map[] = {
        { 0x0, 8000 },
        { 0x3, 16000 },
        //{ 0x7, 44100 },
        { 0x8, 48000 }
};

// Over Sample Ratio: refer to CODEC / AON_CODEC registers
static const REG_VAL_MAP sc_ADC_OSR_val_map[] = {
        { 0x0, 500 },
        { 0x1, 250 },
        { 0x2, 125 },
        { 0x3, 100 },
        { 0x4, 50 }
};

//AMIC Sample Rate : Over Sample Ratio : RC_CTRL
static const VAL_TRIPLE sc_AMIC_SR_OSR[] = {
        { 48000, 250, 1 },
//        { 24000, 500, 1 },
//        { 32000, 125, 0 },
        { 16000, 250, 0 },
        { 8000, 500, 0 },
//        { 44100, 250, 1 },
//        { 22050, 500, 1 }
};

//DMIC Sample Rate : Over Sample Ratio : RC_CTRL (NOT USED, reserved 0)
static const VAL_TRIPLE sc_DMIC_SR_OSR[] = {
        { 16000, 50, 0 },
        { 8000, 100, 0 },
        { 16000, 125, 0 },
        { 8000, 250, 0 },
        { 48000, 50, 0 },
        { 16000, 250, 0 },
        { 8000, 500, 0 }
};

//====================== Register-irrelevant definitions =====================

// ADC_PDM device flags
#define ADC_PDM_FLAG_INITIALIZED          (1UL << 0)     // ADC_PDM initialized
#define ADC_PDM_FLAG_POWERED              (1UL << 1)     // ADC_PDM powered on
#define ADC_PDM_FLAG_CONFIGURED           (1UL << 2)     // ADC_PDM configured
#define ADC_PDM_FLAG_LOW_POWER            (1UL << 3)     // Low power state, only valid when powered on

// ADC_PDM information (Run-time)
typedef struct _ADC_PDM_INFO
{
    CSK_ADC_PDM_SignalEvent_t cb_event; // event callback
    uint32_t usr_param; // user parameter of event callback

    //TODO:

    uint32_t flags: 6; // ADC/PDM driver flags
    uint32_t use_pdm: 1; // 0: use ADC (Analog MIC), 1: use PDM (DMIC)
    uint32_t use_16bits: 1; // 0: use 32bits sample, 1: 16bits sample
    uint32_t ch_bmp: 2; // current acquired IN channel bitmap (Left@bit0, Right@bit1)
    uint32_t ch_mix: 2; // mix Left & Right IN channel data if both is configured,
                       // 0: Not mixed, 1: mixed @ left channel
    uint32_t rsvd1: 3;
    uint32_t rc_ctrl: 1; // 0 = 4MHz mode, 1 = 12MHz mode. used in ADC only.
    uint32_t over_samp_ratio: 16;

    uint32_t samp_freq; // current sample rate
    uint16_t d_vol_left; // digital volume of left channel
    uint16_t d_vol_right; // digital volume of right channel

    CSK_ADCPDM_STATUS status; // ADC/PDM status flags

} ADC_PDM_INFO;


typedef struct {
    uint8_t dev_idx; // 0=ADC/PDM01, 1=ADC/PDM23
    uint8_t apc_dch; // which APC IN dual_channel?
    uint8_t data_len; // channel data length (in bit)
    uint8_t rsvd1;

    CSK_ADC_PDM_RegDef *reg;  // pointer to CODEC(ADC) registers
    ADC_PDM_INFO *info;  // ADC/PDM run-time information
} ADC_PDM_GRP;


#endif /* __ADC_PDM_H */
