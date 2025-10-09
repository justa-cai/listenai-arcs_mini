/*
 * dac_out.c
 *
 *  Created on: Apr. 14, 2021 for VENUS
 *  Ported on: Mar. 4, 2024 for ARCS_C0
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

extern void *gpio_A;

//void * init_adc_in(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_ADC_PDM_SignalEvent_t cb_event)
void * init_dac_out(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_DAC_SignalEvent_t cb_event)
{
    void *dac_grp;
    int32_t iret, SR_reg_val, OSR_reg_val;

    // Check the channel count
    if (dev_bmp == 0) {
        CLOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

    // Check sample rate
    switch (sampling_freq) {
    case 8000:
        SR_reg_val = CSK_DAC_SR_8KHZ;
        OSR_reg_val = CSK_DAC_OSR_250;
        break;
//    case 12000:
//        SR_reg_val = CSK_DAC_SR_12KHZ;
//        OSR_reg_val = CSK_DAC_OSR_250;
//        break;
    case 16000:
        SR_reg_val = CSK_DAC_SR_16KHZ;
        OSR_reg_val = CSK_DAC_OSR_250;
        break;
    case 24000:
        SR_reg_val = CSK_DAC_SR_24KHZ;
        OSR_reg_val = CSK_DAC_OSR_250;
        break;
    case 32000:
        SR_reg_val = CSK_DAC_SR_32KHZ;
        OSR_reg_val = CSK_DAC_OSR_125;
        break;
    case 48000:
        SR_reg_val = CSK_DAC_SR_48KHZ;
        OSR_reg_val = CSK_DAC_OSR_125;
        break;
    case 96000:
        SR_reg_val = CSK_DAC_SR_96KHZ;
        OSR_reg_val = CSK_DAC_OSR_125;
        break;
//    case 11025:
//        SR_reg_val = CSK_DAC_SR_11P025KHZ;
//        OSR_reg_val = CSK_DAC_OSR_250;
//        break;
//    case 22050:
//        SR_reg_val = CSK_DAC_SR_22P05KHZ;
//        OSR_reg_val = CSK_DAC_OSR_250;
//        break;
//    case 44100:
//        SR_reg_val = CSK_DAC_SR_44P1KHZ;
//        OSR_reg_val = CSK_DAC_OSR_125;
//        break;
    default:
        CLOGE("%s: CANNOT support sample rate: %d\n", __func__, sampling_freq);
        return NULL;
    }

    // Get DAC device group
    dac_grp = DAC01();
    if (dac_grp == NULL)
        return NULL;

    // FIXME/TODO: DAC P/N use dedicated pins, so NO IOMUX settings are required...

//    uint8_t use_flags = 0;
//    if (use_16bits) use_flags |= DAC_USE_16BITS;
//    iret = DAC_Initialize(dac_grp, cb_event, (uint32_t)dac_grp, dev_bmp, 0, use_flags);

    DAC_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_out_left = DMA_CH_AUD_TX_DEF;

    uint8_t dev_bmp_flag = dev_bmp << DAC_BMP_FLAG_OUT_POS; // ADC_PDM_USE_PDM
    if (use_16bits) dev_bmp_flag |= DAC_BMP_FLAG_USE_16BITS;
    iret = DAC_Initialize(dac_grp, cb_event, (uint32_t)dac_grp, dev_bmp_flag, &dma_chs);

    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = DAC_PowerControl(dac_grp, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

//    uint32_t tx_config;
//    if ((dev_bmp & DAC_BMP_STEREO) == DAC_BMP_STEREO)
//        tx_config = CSK_DAC_TXCFG_STEREO_SRC_STEREO;
//    else
//        tx_config = CSK_DAC_TXCFG_MONO_SRC_MONO;

    //TODO: set arg = CSK_DAC_ARG_SOFT_MUTE_EN | CSK_DAC_ARG_SOFT_MUTE_SPD(1)
    iret = DAC_Control(dac_grp, SR_reg_val | OSR_reg_val | CSK_DAC_SOFT_MUTE_SET, // | tx_config
                    CSK_DAC_ARG_SOFT_MUTE_EN | CSK_DAC_ARG_SOFT_MUTE_SPD(0x3)); // arg bit[4:0] = soft mute setting
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Volume settings for L/R DAC devices
    uint32_t gain_a=0, gain_d=0, vol_flag=0;
    if (dev_bmp & DAC_BMP_LEFT) {
        gain_a |= DAC_GAIN_A_VAL(0); // 0 dB
        gain_d |= DAC_GAIN_D_VAL(0); // 0 dB
//        gain_a |= DAC_GAIN_A_VAL(-6); // -6 dB
//        gain_d |= DAC_GAIN_D_VAL(-10); // -10 dB
        vol_flag |= DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_D_LEFT;
    }

/*
    if (dev_bmp & DAC_BMP_RIGHT) {
        gain_a |= DAC_GAIN_A_VAL(-6) << 16; // -6 dB
        gain_d |= DAC_GAIN_D_VAL(-20) << 16; // -20 dB (-6dB works, but 0dB doesn't work?)
//        gain_a |= DAC_GAIN_A_VAL(-6) << 16; // -6 dB
//        gain_d |= DAC_GAIN_D_VAL(6) << 16; // 6 dB
        vol_flag |= DAC_VOL_FLAG_A_RIGHT | DAC_VOL_FLAG_D_RIGHT;
    }
*/

    iret = DAC_SetVolume(dac_grp, gain_a, gain_d, vol_flag);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Mute settings for L/R DAC device
    uint8_t mute_val = 0;
    if (dev_bmp & DAC_BMP_LEFT) {
        //mute_val = DAC_BMP_LEFT;
        mute_val &= ~DAC_BMP_LEFT;
    }
/*
    if (dev_bmp & DAC_BMP_RIGHT) {
        //mute_val = DAC_BMP_RIGHT;
        mute_val &= ~DAC_BMP_RIGHT;
    }
*/

    iret = DAC_SetMute(dac_grp, mute_val, dev_bmp);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

/*  //FIXME: check if PA chip is used...
    // For the Venus6001 QFN64 MPW reference design board,
    // it's necessary to pull-up PA4 to enable DAC.
    // It seems to make no difference now...
    assert(gpio_A != NULL);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, 0); //PA4 as GPIO (func=0)
    GPIO_SetDir(gpio_A, CSK_GPIO_PIN4, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gpio_A, CSK_GPIO_PIN4, 1);
*/

    CLOGD("%s: DAC module is initialized successfully!!\r\n", __func__);

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        DAC_Uninitialize(dac_grp);
        return NULL;
    }

    return dac_grp;
}

