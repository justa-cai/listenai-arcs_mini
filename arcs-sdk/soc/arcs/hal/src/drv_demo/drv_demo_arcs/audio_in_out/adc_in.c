/*
 * adc_in.c
 *
 *  Created on: Apr. 14, 2021 for VENUS
 *  Ported on: Mar. 4, 2024 for ARCS_C0
 *      Author: bauldeng
 */

#include "main.h"

#define DEBUG_LOG   1 //0 //
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
//#define LOGD(format, ...)   printf(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG


#define ADC_SINGLE_INPUT    0 //1 // 1: Single-ended, 0: Differential

void * init_adc_in(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_ADC_PDM_SignalEvent_t cb_event)
{
    void *adc_grp;
    int32_t iret, SR_reg_val, OSR_reg_val;

    // Check the channel count
    if (dev_bmp == 0) {
        CLOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

    // Check sample rate, SR * OSR = 4M or 12M is required!!
    switch (sampling_freq) {
    case 8000:
        SR_reg_val = CSK_ADCPDM_SR_8KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_500; // CSK_ADCPDM_OSR_250
        break;
    case 16000:
        SR_reg_val = CSK_ADCPDM_SR_16KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_250; // CSK_ADCPDM_OSR_125
        break;
    case 48000:
        SR_reg_val = CSK_ADCPDM_SR_48KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_250; // CSK_ADCPDM_OSR_50
        break;
//    case 44100:
//        SR_reg_val = CSK_ADCPDM_SR_44P1KHZ;
//        OSR_reg_val = CSK_ADCPDM_OSR_250; // CSK_ADCPDM_OSR_50
//        break;
    default:
        CLOGE("%s: CANNOT support sample rate: %d\n", __func__, sampling_freq);
        return NULL;
    }

    // Get ADC device group
    adc_grp = ADC_PDM01();
    //adc_grp = ADC_PDM23();
    if (adc_grp == NULL)
        return NULL;

    // FIXME/TODO: ADC P/N use dedicated pins, so NO IOMUX settings are required...

//    uint8_t use_flags = 0; // ADC_PDM_USE_PDM
//    if (use_16bits) use_flags |= ADC_PDM_USE_16BITS;
//    iret = ADC_PDM_Initialize(adc_grp, cb_event, (uint32_t)adc_grp, dev_bmp, use_flags);

    ADC_PDM_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;

    uint8_t dev_bmp_flag = dev_bmp << ADC_PDM_BMP_FLAG_IN_POS; // ADC_PDM_USE_PDM
    if (use_16bits) dev_bmp_flag |= ADC_PDM_BMP_FLAG_USE_16BITS;
    iret = ADC_PDM_Initialize(adc_grp, cb_event, (uint32_t)adc_grp, dev_bmp_flag, &dma_chs);
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = ADC_PDM_PowerControl(adc_grp, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    uint32_t control = SR_reg_val | OSR_reg_val | CSK_ADCPDM_RXCFG_MIXED | CSK_ADCPDM_HPF_SET;
    uint32_t arg = CSK_ADCPDM_ARG_HPF1_EN | CSK_ADCPDM_ARG_HPF2_EN | CSK_ADCPDM_ARG_HPF2_CUT(3);

#if ADC_SINGLE_INPUT
    control |= CSK_ADCPDM_PGA_INPUT_SET;
    arg |= CSK_ADCPDM_ARG_LPGA_INPUT_SINGLE | CSK_ADCPDM_ARG_RPGA_INPUT_SINGLE;
#endif

    iret = ADC_PDM_Control(adc_grp, control, arg);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Volume settings for L/R ADC devices
    uint32_t gain_a=0, gain_d=0, vol_flag=0;
    if (dev_bmp & ADC_PDM_BMP_LEFT) {
        gain_a |= ADC_PDM_GAIN_A_VAL(0); // 0 dB (0x6)
        gain_d |= ADC_PDM_GAIN_D_VAL(0); // 0 dB (0x55)
        //gain_a |= ADC_PDM_GAIN_A_VAL(1); // 1 dB
        //gain_d |= ADC_PDM_GAIN_D_VAL(18); // 30, 25, 20, 15, 5 dB
        vol_flag |= ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_D_LEFT;
    }
    if (dev_bmp & ADC_PDM_BMP_RIGHT) {
        gain_a |= ADC_PDM_GAIN_A_VAL(0) << 16; // 0 dB (0x6 << 16)
        gain_d |= ADC_PDM_GAIN_D_VAL(0) << 16; // 0 dB (0x55 << 16)
        //gain_a |= ADC_PDM_GAIN_A_VAL(1) << 16; // 1 dB
        //gain_d |= ADC_PDM_GAIN_D_VAL(18) << 16; // 30, 25, 20, 15, 5 dB
        vol_flag |= ADC_PDM_VOL_FLAG_A_RIGHT | ADC_PDM_VOL_FLAG_D_RIGHT;
    }
    iret = ADC_PDM_SetVolume(adc_grp, gain_a, gain_d, vol_flag);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Mute settings for L/R ADC device
    uint8_t mute_val = 0;
    if (dev_bmp & ADC_PDM_BMP_LEFT) {
        mute_val &= ~ADC_PDM_BMP_LEFT;
    }
    if (dev_bmp & ADC_PDM_BMP_RIGHT) {
        mute_val &= ~ADC_PDM_BMP_RIGHT;
    }
    iret = ADC_PDM_SetMute(adc_grp, mute_val, dev_bmp);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // for set mic pin
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_OEN_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_OEN_REG = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_IE_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_IE_REG = 0x0; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_UP = 0x0; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_DN = 0x0; // 1 bits

    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_OEN_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_OEN_REG = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_IE_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_IE_REG = 0x0; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_UP = 0x0; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_DN = 0x0; // 1 bits

    IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_OEN_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_OEN_REG = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_IE_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_IE_REG = 0x0; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_UP = 0x0; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_DN = 0x0; // 1 bits

    IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_OEN_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_OEN_REG = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_IE_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_IE_REG = 0x0; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_FRC = 0x1; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_UP = 0x0; // 1 bits
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_DN = 0x0; // 1 bits

	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_FSEL = 21; // MIC0_INP
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_FSEL = 21; // MIC0_INN
	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_FSEL = 21; // MIC1_INP
	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_FSEL = 21; // MIC1_INN

    LOGD("%s: ADC module is initialized successfully!!\r\n", __func__);

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        ADC_PDM_Uninitialize(adc_grp);
        return NULL;
    }

    return adc_grp;
}
