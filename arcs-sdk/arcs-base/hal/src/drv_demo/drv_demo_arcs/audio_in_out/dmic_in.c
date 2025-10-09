/*
 * dmic_in.c
 *
 *  Created on: Dec. 22, 2020
 *  Ported on: Mar. 25, 2024 for ARCS_C0
 *      Author: bauldeng
 */

#include "main.h"

#define DEBUG_LOG 1 // 0
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
//#define LOGD(format, ...)   printf(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

//extern void *gpio_A;

void * init_dmic_in(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_ADC_PDM_SignalEvent_t cb_event)
{
    void *pdm_grp;
    int32_t iret, SR_reg_val, OSR_reg_val;

    // Check the channel count
    if (dev_bmp == 0) {
        CLOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

    // Check sample rate
    switch (sampling_freq) {
    case 8000:
        SR_reg_val = CSK_ADCPDM_SR_8KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_500;
        //OSR_reg_val = CSK_ADCPDM_OSR_250;
        //OSR_reg_val = CSK_ADCPDM_OSR_100;
        break;
    case 16000:
        SR_reg_val = CSK_ADCPDM_SR_16KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_250;
        //OSR_reg_val = CSK_ADCPDM_OSR_125;
        //OSR_reg_val = CSK_ADCPDM_OSR_50;
        break;
    case 48000: //FIXME: unsupported on DMIC?
        SR_reg_val = CSK_ADCPDM_SR_48KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_50;
        break;
//    case 44100: //FIXME: unsupported on DMIC?
//        SR_reg_val = CSK_ADCPDM_SR_44P1KHZ;
//        OSR_reg_val = CSK_ADCPDM_OSR_50;
//        break;
    default:
        CLOGE("%s: CANNOT support sample rate: %d\n", __func__, sampling_freq);
        return NULL;
    }

    // Get DMIC01 device group
    pdm_grp = ADC_PDM01();
    if (pdm_grp == NULL)
        return NULL;

    // DMIC01 Pin
    //IOMuxManager_PinConfigure(DMIC_GROUP, DMIC01_CLK, DMIC_FUNC); // CLK
    //IOMuxManager_PinConfigure(DMIC_GROUP, DMIC01_DAT, DMIC_FUNC); // DAT

//    uint8_t use_flags = ADC_PDM_USE_PDM;
//    if (use_16bits) use_flags |= ADC_PDM_USE_16BITS;
//    iret = ADC_PDM_Initialize(pdm_grp, cb_event, (uint32_t)pdm_grp, dev_bmp, use_flags);

    ADC_PDM_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;

    uint8_t dev_bmp_flag = (dev_bmp << ADC_PDM_BMP_FLAG_IN_POS) | ADC_PDM_BMP_FLAG_USE_PDM;
    if (use_16bits) dev_bmp_flag |= ADC_PDM_BMP_FLAG_USE_16BITS;
    iret = ADC_PDM_Initialize(pdm_grp, cb_event, (uint32_t)pdm_grp, dev_bmp_flag, &dma_chs);
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = ADC_PDM_PowerControl(pdm_grp, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    iret = ADC_PDM_Control(pdm_grp, SR_reg_val | OSR_reg_val | CSK_ADCPDM_RXCFG_MIXED | CSK_ADCPDM_HPF_SET,
                            CSK_ADCPDM_ARG_HPF1_EN | CSK_ADCPDM_ARG_HPF2_EN | CSK_ADCPDM_ARG_HPF2_CUT(3));
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Volume settings for L/R ADC devices
    uint32_t gain_d=0, vol_flag=0;
    if (dev_bmp & ADC_PDM_BMP_LEFT) {
        gain_d |= ADC_PDM_GAIN_D_VAL(10); // 0 dB
        vol_flag |= ADC_PDM_VOL_FLAG_D_LEFT;
    }
    if (dev_bmp & ADC_PDM_BMP_RIGHT) {
        gain_d |= ADC_PDM_GAIN_D_VAL(10) << 16; // 0 dB
        vol_flag |= ADC_PDM_VOL_FLAG_D_RIGHT;
    }
    iret = ADC_PDM_SetVolume(pdm_grp, 0, gain_d, vol_flag);
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
    iret = ADC_PDM_SetMute(pdm_grp, mute_val, dev_bmp);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    //dmic pin config
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_06.bit.PAD_AON_GPIOB_06_FSEL = 5; // DMIC CLK @ B02
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_07.bit.PAD_AON_GPIOB_07_FSEL = 5; // DMIC DATA @ B03
    IP_CMN_IOMUX->REG_PAD_GPIOB_06.bit.PAD_GPIOB_06_FSEL = 24; // CLK
    IP_CMN_IOMUX->REG_PAD_GPIOB_07.bit.PAD_GPIOB_07_FSEL = 24; // DATA

    CLOGD("%s: PDM/DMIC module is initialized successfully!!\r\n", __func__);

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        ADC_PDM_Uninitialize(pdm_grp);
        return NULL;
    }

    return pdm_grp;
}

