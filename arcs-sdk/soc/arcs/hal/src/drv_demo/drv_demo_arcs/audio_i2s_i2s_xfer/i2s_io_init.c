/*
 * i2s_io_init.c
 *
 *  Created on: Apr. 10, 2024 for MARS
 *
 *      Author: bauldeng
 */

#include "main.h"

#define DEBUG_LOG 1 // 0
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

#define DMA_CH_AUD_TX_DEF           ((uint8_t) 2)   // CLASSD/I2S TX
#define DMA_CH_AUD_RX_DEF           ((uint8_t) 1)   // ADC/DMIC/I2S RX
#define DMA_CH_AUD_ECHO_DEF         ((uint8_t) 3)   // CLASSD/I2S TX LOOPBACK


bool enable_i2s_mclk(uint8_t i2s_idx)
{
    if (i2s_idx == 0) // use I2S0
        IOMuxManager_PinConfigure(I2S0_MCLK); // MCLK use DBG_CLK

#if (CHIP_I2S_DEV_CNT >= 2)
    else if (i2s_idx == 1) // use I2S1
        IOMuxManager_PinConfigure(I2S1_MCLK); // MCLK use DBG_CLK
#endif // (CHIP_I2S_DEV_CNT >= 2)

    else // other I2S
        return false;

    IP_CMN_SYS->REG_TEST_CTRL.bit.DBG_CLK_SEL = 3; // output I2S MCLK (that is, XTAL CLK OUTPUT)
    IP_CMN_SYS->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;

    return true;
}


// initialize i2s output, return I2S OUT device pointer
void * i2s_out_init(uint8_t i2s_idx, uint32_t dev_bmp_flag, CSK_I2S_SignalEvent_t cb_event)
{
    int32_t iret;
    void *i2s_dev;

    if (i2s_idx == 0) { // use I2S0
        i2s_dev = I2S0();
        if (i2s_dev == NULL)
            return NULL;

        IOMuxManager_PinConfigure(I2S0_LRCK); // LRCK (FS, WS)
        IOMuxManager_PinConfigure(I2S0_BCK); // BCK
        IOMuxManager_PinConfigure(I2S0_DOUT); // DOUT
        //IOMuxManager_PinConfigure(I2S0_DIN); // DIN

#if (CHIP_I2S_DEV_CNT >= 2)
    } else if (i2s_idx == 1) { // use I2S1
        i2s_dev = I2S1();
        if (i2s_dev == NULL)
            return NULL;

        IOMuxManager_PinConfigure(I2S1_LRCK); // LRCK (FS, WS)
        IOMuxManager_PinConfigure(I2S1_BCK); // BCK
        IOMuxManager_PinConfigure(I2S1_DOUT); // DOUT
        //IOMuxManager_PinConfigure(I2S1_DIN); // DIN
#endif // (CHIP_I2S_DEV_CNT >= 2)

    } else { // other I2S
        return NULL;
    }

    // OUT channel is required, and ECHO channel is NOT necessary
    if ((dev_bmp_flag & I2S_BMP_FLAG_OUT_STEREO) == 0) {
        LOGD("%s: NO I2S OUT channel", __func__);
        return NULL;
    }

    I2S_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    if ((dev_bmp_flag & I2S_BMP_FLAG_OUT_LEFT) == I2S_BMP_FLAG_OUT_LEFT) //[OUT]: left or stereo
        dma_chs.dma_ch_out_left = DMA_CH_AUD_TX_DEF;
    else //[OUT]: right only
        dma_chs.dma_ch_out_right = DMA_CH_AUD_TX_DEF;
    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev, dev_bmp_flag, &dma_chs);
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = I2S_PowerControl(i2s_dev, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}


// initialize i2s input, return I2S IN device pointer
void * i2s_in_init(uint8_t i2s_idx, uint32_t dev_bmp_flag, CSK_I2S_SignalEvent_t cb_event)
{
    int32_t iret;
    void *i2s_dev;

    if (i2s_idx == 0) { // use I2S0
        i2s_dev = I2S0();
        if (i2s_dev == NULL)
            return NULL;

        IOMuxManager_PinConfigure(I2S0_LRCK); // LRCK (FS, WS)
        IOMuxManager_PinConfigure(I2S0_BCK); // BCK
        //IOMuxManager_PinConfigure(I2S0_DOUT); // DOUT
        IOMuxManager_PinConfigure(I2S0_DIN); // DIN

#if (CHIP_I2S_DEV_CNT >= 2)
    } else if (i2s_idx == 1) { // use I2S1
        i2s_dev = I2S1();
        if (i2s_dev == NULL)
            return NULL;

        IOMuxManager_PinConfigure(I2S1_LRCK); // LRCK (FS, WS)
        IOMuxManager_PinConfigure(I2S1_BCK); // BCK
        //IOMuxManager_PinConfigure(I2S1_DOUT); // DOUT
        IOMuxManager_PinConfigure(I2S1_DIN); // DIN
#endif // (CHIP_I2S_DEV_CNT >= 2)

    } else { // other I2S
        return NULL;
    }

    // IN channel is required, and ECHO channel is NOT necessary
    if ((dev_bmp_flag & I2S_BMP_FLAG_IN_STEREO) == 0) {
        LOGD("%s: NO I2S IN channel", __func__);
        return NULL;
    }

    I2S_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    if ((dev_bmp_flag & I2S_BMP_FLAG_IN_LEFT) == I2S_BMP_FLAG_IN_LEFT) //[IN]: left or stereo
        dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;
    else //[IN]: right only
        dma_chs.dma_ch_in_right = DMA_CH_AUD_RX_DEF;

    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev, dev_bmp_flag, &dma_chs);
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = I2S_PowerControl(i2s_dev, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}


// initialize i2s input & output, return I2S IN/OUT device pointer
void * i2s_inout_init(uint8_t i2s_idx, uint32_t dev_bmp_flag, CSK_I2S_SignalEvent_t cb_event)
{
    int32_t iret;
    void *i2s_dev;

    if (i2s_idx == 0) { // use I2S0
        i2s_dev = I2S0();
        if (i2s_dev == NULL)
            return NULL;

        IOMuxManager_PinConfigure(I2S0_LRCK); // LRCK (FS, WS)
        IOMuxManager_PinConfigure(I2S0_BCK); // BCK
        IOMuxManager_PinConfigure(I2S0_DOUT); // DOUT
        IOMuxManager_PinConfigure(I2S0_DIN); // DIN

#if (CHIP_I2S_DEV_CNT >= 2)
    } else if (i2s_idx == 1) { // use I2S1
        i2s_dev = I2S1();
        if (i2s_dev == NULL)
            return NULL;

        IOMuxManager_PinConfigure(I2S1_LRCK); // LRCK (FS, WS)
        IOMuxManager_PinConfigure(I2S1_BCK); // BCK
        IOMuxManager_PinConfigure(I2S1_DOUT); // DOUT
        IOMuxManager_PinConfigure(I2S1_DIN); // DIN
#endif // (CHIP_I2S_DEV_CNT >= 2)

    } else { // other I2S
        return NULL;
    }

    // IN & OUT channel is required, and ECHO channel is NOT necessary
    if (!(dev_bmp_flag & I2S_BMP_FLAG_IN_STEREO) || !(dev_bmp_flag & I2S_BMP_FLAG_OUT_STEREO)) {
        LOGD("%s: NO I2S IN/OUT channel", __func__);
        return NULL;
    }

    I2S_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    if ((dev_bmp_flag & I2S_BMP_FLAG_IN_LEFT) == I2S_BMP_FLAG_IN_LEFT) //[IN]: left or stereo
        dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;
    else //[IN]: right only
        dma_chs.dma_ch_in_right = DMA_CH_AUD_RX_DEF;
    if ((dev_bmp_flag & I2S_BMP_FLAG_OUT_LEFT) == I2S_BMP_FLAG_OUT_LEFT) //[OUT]: left or stereo
        dma_chs.dma_ch_out_left = DMA_CH_AUD_TX_DEF;
    else //[OUT]: right only
        dma_chs.dma_ch_out_right = DMA_CH_AUD_TX_DEF;

    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev, dev_bmp_flag, &dma_chs);
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = I2S_PowerControl(i2s_dev, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}


// configure i2s with parameters, ch_bits < 0 indicates _LOW, e.g. 24BIT_LOW
int32_t i2s_config(void *i2s_dev, bool is_master, uint8_t use_tdm, uint8_t ch_cnt, int8_t ch_bits,
                  I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, uint32_t bck_slave)
{
    uint32_t protocol, format;
    int32_t iret;

    assert(i2s_dev != NULL);

    if (ch_cnt == 0 || (ch_cnt > 2 && !use_tdm))
        return CSK_DRIVER_ERROR_PARAMETER;

    if (i2s_prt >= I2S_PROTO_COUNT || i2s_prt == I2S_PROTO_UNKNOWN)
        return CSK_DRIVER_ERROR_PARAMETER;

    // protocol
    protocol = i2s_prt << CSK_I2S_PROTO_Pos;

    // data format
    switch (ch_bits) {
    case 16:
        format = CSK_I2S_DATA_FORMAT_DUAL_16BIT;
        break;

    case 20: // use high 3 bytes of word by default
        format = CSK_I2S_DATA_FORMAT_20BIT_HIGH;
        //NOTE: CSK_I2S_DATA_FORMAT_20BIT_LOW is NOT supported!
        break;

    case 24: // use high 3 bytes of word by default
        format = CSK_I2S_DATA_FORMAT_24BIT_HIGH;
        break;

    case -24: // use low 3 bytes of word
        format = CSK_I2S_DATA_FORMAT_24BIT_LOW;
        break;

    case 32:
        format = CSK_I2S_DATA_FORMAT_32BIT;
        break;

    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    iret = I2S_Control(i2s_dev,
                        (is_master ? CSK_I2S_MODE_MASTER : CSK_I2S_MODE_SLAVE) | protocol | format |
                        (ch_cnt > 1 ? CSK_I2S_TXCH_STEREO_SRC_STEREO : CSK_I2S_TXCH_MONO_SRC_MONO) |
                        (ch_cnt > 1 ? CSK_I2S_RXCH_MIXED : CSK_I2S_RXCH_SEPA) |
                        (use_tdm ? CSK_I2S_TDM_CHS(ch_cnt) : CSK_I2S_TDM_CHS(0)),
                        (is_master ? sampling_freq : bck_slave / sampling_freq)); //use default 24Mhz input clock
    if (iret != CSK_DRIVER_OK)
        return iret;

    LOGD("%s: I2S module is configured (as %s) successfully!!\r\n", __func__, (is_master ? "MASTER" : "SLAVE"));
    return iret;
}


void i2s_abort_rx(void *i2s_dev)
{
    I2S_Abort_Channels(i2s_dev, CH_BMP_STEREO, 0, 0);
}

void i2s_abort_tx(void *i2s_dev)
{
    I2S_Abort_Channels(i2s_dev, 0, CH_BMP_STEREO, 0);
}

void i2s_abort_txrx(void *i2s_dev)
{
    I2S_Abort_Channels(i2s_dev, CH_BMP_STEREO, CH_BMP_STEREO, 0);
}

void i2s_close(void *i2s_dev)
{
    I2S_Uninitialize(i2s_dev);
}
