/*
 * i2s_codec_in.c
 *
 *  Created on: Nov. 9, 2020 for VENUS (CP)
 *  Ported on: Dec. 18, 2023 for ARCS (AP)
 *
 */

#include "main.h"
#include "es7210.h"

#define DEBUG_LOG 1 // 0
#if DEBUG_LOG
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
//#define LOGD(format, ...)   printf(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

#define USE_I2S_IDX    1 // 0

extern void *g_i2c_dev;

#define CLK_REG_CNT 8

// called only for Master mode
static bool make_ES7210_Master_Clock_CFG(uint32_t ext_mclk, uint32_t sampling_freq, uint8_t *cfg_regs, uint8_t cfg_cnt)
{
    uint32_t div, inner_mclk;
    if (cfg_regs == NULL || cfg_cnt != CLK_REG_CNT || ext_mclk == 0)
        return false;

    // REGISTER 0X01 - CLOCK OFF, DEFAULT 00100000
    // bit[6]=1, turn off slave mode SCLK and LRCK internally
    // bit[5]=0, turn on master mode SCLK and LRCK
    cfg_regs[0] = 0x40;

    // REGISTER 0X02 - MAIN CLOCK CONTROL, DEFAULT 00000010
    // bit[7] DLL_BYPASS, 1 - bypass DLL
    // bit[6] CLKDBL_VALID, 1 - use clock doubler
    // bit[4:0] CLK_ADC_DIV, ADC clock divide
    cfg_regs[1] = (0<<7) | (1<<6) | (15<<0); //FIXME:
    inner_mclk = ext_mclk * 4 * 2 / 15;

    // REGISTER 0X03 - MASTER CLOCK CONTROL, DEFAULT 00000100
    // bit[7]=0, MCLK from pad; =1, MCLK from clock doubler
    // bit[6:0], try to use 1600kHz now(i.e. 24Mhz/1M)
    //div = ext_mclk / 1600000;
    div = inner_mclk / 1600000;
    if(div > 127)
        return false;
    //cfg_regs[2] = div;
    cfg_regs[2] = (1<<7) | div;

    // REGISTER 0X04 - MASTER LRCK DIVIDER 1, DEFAULT 00000001
    // REGISTER 0X05 - MASTER LRCK DIVIDER 0, DEFAULT 00000000
    // M_LRCK_DIV hold 12 bits
    //div = ext_mclk / sampling_freq;
    div = inner_mclk / sampling_freq;
    if (div > 4096)
        return false;
    cfg_regs[3] = (div >> 8) & 0xF;
    cfg_regs[4] = div & 0xFF;

    // REGISTER 0X06 - POWER DOWN, DEFAULT 00000000
    // bit[6] DLL_POWER_DOWN, 1 - power down DLL (stop MCLK first)
    cfg_regs[5] = 0x0;

    // REGISTER 0X07 - ADC OSR CONFIG, DEFAULT 00100000
    uint32_t osr = inner_mclk / 16 / sampling_freq;
    assert(osr < 64);
    cfg_regs[6] = osr;

    // REGISTER 0X08 - MODE CONFIG, DEFAULT 00010000
    // bit[0] = 1, master mode
    cfg_regs[7] = 0x11;

    return true;
}


static bool make_ES7210_SDP_CFG(I2S_PROTOCOL i2s_prt, uint8_t ch_bits, uint8_t *cfg1_p, uint8_t *cfg2_p)
{
    bool ret = false;
    uint8_t cfg1=0, cfg2=0;

    // I2S protocol
    switch (i2s_prt) {
    case I2S_PROTO_PHILIPS:
        cfg1 |= (0x0 << 4) | 0x0;
        cfg2 |= 0x2; //FIXME: 0x2;
        break;

    case I2S_PROTO_LEFT:
        cfg1 |= (0x0 << 4) | 0x01;
        cfg2 |= 0x2; //FIXME: 0x2;
        break;

    case I2S_PROTO_PCMMODE_A:
        cfg1 |= (0x0 << 4) | 0x03;
        cfg2 |= 0x1;
        break;

    case I2S_PROTO_PCMMODE_B:
        cfg1 |= (0x1 << 4) | 0x03;
        cfg2 |= 0x1;
        break;

    default:
       return false;
    } //end switch

    // Data bits
    switch (ch_bits) {
    case 16:
        cfg1 |= 0x3 << 5;
        break;

    case 20:
        cfg1 |= 0x1 << 5;
        break;

    case 24:
        cfg1 |= 0x0 << 5;
        break;

    case 32:
        cfg1 |= 0x4 << 5;
        break;

    default:
        return false;
    }

    if (cfg1_p != NULL) *cfg1_p = cfg1;
    if (cfg2_p != NULL) *cfg2_p = cfg2;

    return true;
}

// MICxGAIN_SETTING @ bit[3:0], MICx GAIN, value range (0~14, 0dB ~ 37.5dB)
#define ANALOG_GAIN_S     0x8 // 0xA // 30dB
#define ANALOG_GAIN_M     0x8 // 0xA // 30dB

// set ES7210 as slave
static bool ES7210_init_slave(uint32_t ext_mclk, uint32_t sampling_freq, I2S_PROTOCOL i2s_prt, uint16_t ch_bmp, uint8_t ch_bits)
{
    uint8_t sendBuf[2];
    uint8_t cfg1, cfg2, clk_regs[CLK_REG_CNT];
    uint8_t i, idx, *val_p;
    bool ret;

    ret = make_ES7210_SDP_CFG(i2s_prt, ch_bits, &cfg1, &cfg2);
    if (!ret)   return ret;

    assert(g_i2c_dev != NULL);

    // 1st ES7210 Codec
    for (i = 1; i < ES7210_REG_LEN1; i++) {
        idx = adES7210Conf_1[i][0];
        val_p = &adES7210Conf_1[i][1];

        if (idx == ES7210_SDP_CFG1_REG11) {
            *val_p = cfg1;
        } else if (idx == ES7210_SDP_CFG2_REG12) {
            *val_p = cfg2;
        }  else if (idx == ES7210_MIC1_GAIN_REG43) {
            *val_p = (ch_bmp & (0x1 << 0)) ? ((0x1<<4) | ANALOG_GAIN_S) : 0;
        }  else if (idx == ES7210_MIC2_GAIN_REG44) {
            *val_p = (ch_bmp & (0x1 << 1)) ? ((0x1<<4) | ANALOG_GAIN_S) : 0;
        }  else if (idx == ES7210_MIC3_GAIN_REG45) {
            *val_p = (ch_bmp & (0x1 << 2)) ? ((0x1<<4) | ANALOG_GAIN_S) : 0;
        }  else if (idx == ES7210_MIC4_GAIN_REG46) {
            *val_p = (ch_bmp & (0x1 << 3)) ? ((0x1<<4) | ANALOG_GAIN_S) : 0;
        }

        sendBuf[0] = (uint8_t) (adES7210Conf_1[i][0] & 0xFF);
        sendBuf[1] = (uint8_t) (adES7210Conf_1[i][1] & 0xFF);
        ret = i2c_write(g_i2c_dev, ES7210_1_I2C_ADDR, sendBuf, 2, 2); // FIXME: timeout = 2ms?
        if (!ret)   return ret;
    }

    return true;
}


// set ES7210 as master, set BCK/LRCK (maybe setup PLL...)
static bool ES7210_init_master(uint32_t ext_mclk, uint32_t sampling_freq, I2S_PROTOCOL i2s_prt, uint16_t ch_bmp, uint8_t ch_bits)
{
    uint8_t sendBuf[4];
    uint8_t i, idx, *val_p;
    uint8_t cfg1, cfg2, clk_regs[CLK_REG_CNT];
    bool ret, checked;

    // ES7210 works as I2S master
    ret = make_ES7210_Master_Clock_CFG(ext_mclk, sampling_freq, clk_regs, CLK_REG_CNT);
    if (!ret)   return ret;

    ret = make_ES7210_SDP_CFG(i2s_prt, ch_bits, &cfg1, &cfg2);
    if (!ret)   return ret;
    //cfg2 |= (1 << 6); // bit[6]=1, use master mode SCLK/LRCK to power up

    assert(g_i2c_dev != NULL);

    // 1st ES7210 Codec
    for (i = 1; i < ES7210_REG_LEN_1M; i++) {
        idx = adES7210Conf_1m[i][0];
        val_p = &adES7210Conf_1m[i][1];
        switch(idx) {
        case ES7210_SDP_CFG1_REG11:
            *val_p = cfg1;
            break;
        case ES7210_SDP_CFG2_REG12:
            *val_p = cfg2;
            break;
        case ES7210_MIC1_GAIN_REG43:
            *val_p = (ch_bmp & (0x1 << 0)) ? ((0x1<<4) | ANALOG_GAIN_M) : 0;
            break;
        case ES7210_MIC2_GAIN_REG44:
            *val_p = (ch_bmp & (0x1 << 1)) ? ((0x1<<4) | ANALOG_GAIN_M) : 0;
            break;
        case ES7210_MIC3_GAIN_REG45:
            *val_p = (ch_bmp & (0x1 << 2)) ? ((0x1<<4) | ANALOG_GAIN_M) : 0;
            break;
        case ES7210_MIC4_GAIN_REG46:
            *val_p = (ch_bmp & (0x1 << 3)) ? ((0x1<<4) | ANALOG_GAIN_M) : 0;
            break;
        case ES7210_CLK_ON_OFF_REG01:
            *val_p = clk_regs[0];
            break;
        case ES7210_MCLK_CTL_REG02:
            *val_p = clk_regs[1];
            break;
        case ES7210_MST_CLK_CTL_REG03:
            *val_p = clk_regs[2];
            break;
        case ES7210_MST_LRCDIVH_REG04:
            *val_p = clk_regs[3];
            break;
        case ES7210_MST_LRCDIVL_REG05:
            *val_p = clk_regs[4];
            break;
        case ES7210_DIGITAL_PDN_REG06:
            *val_p = clk_regs[5];
            break;
        case ES7210_ADC_OSR_REG07:
            *val_p = clk_regs[6];
            break;
        case ES7210_MODE_CFG_REG08:
            *val_p = clk_regs[7];
            break;
        }

        sendBuf[0] = (uint8_t) (adES7210Conf_1m[i][0] & 0xFF);
        sendBuf[1] = (uint8_t) (adES7210Conf_1m[i][1] & 0xFF);
        ret = i2c_write(g_i2c_dev, ES7210_1_I2C_ADDR, sendBuf, 2, 2); // FIXME: timeout = 2ms?
        if (!ret)   return ret;
    }

    return true;
}


// initialize i2s input, return I2S IN device pointer
static void * init_i2s_input(uint16_t ch_bmp, uint8_t ch_cnt, uint8_t use_tdm, uint8_t ch_bits, bool is_master,
                            I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event)
{
    uint32_t i, protocol, format;
    int32_t iret;
    void *i2s_dev;

#if (USE_I2S_IDX == 0) // use I2S0
    i2s_dev = I2S0();
    if (i2s_dev == NULL)
        return NULL;

    // FIXME: ES7210 CODEC is connected to I2S0
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_LRCK, I2S0_FUNC); // LRCK (FS, WS,)
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_BCK, I2S0_FUNC); // BCK
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_DIN, I2S0_FUNC); // DIN

    IOMuxManager_PinConfigure(I2S0_GROUP, I2S1_MCLK, I2S1_FUNC); // MCLK (using I2S1_MCLK)

#elif (USE_I2S_IDX == 1) // use I2S1
    i2s_dev = I2S1();
    if (i2s_dev == NULL)
        return NULL;

    // FIXME: WM8960 CODEC is connected to I2S1
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_LRCK, I2S1_FUNC); // LRCK (FS, WS,)
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_BCK, I2S1_FUNC); // BCK
    //IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DOUT, I2S1_FUNC); // DOUT
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DIN, I2S1_FUNC); // DIN

    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_MCLK, I2S1_FUNC); // MCLK

#else // other I2S
    return NULL;
#endif

    if (i2s_prt >= I2S_PROTO_COUNT || i2s_prt == I2S_PROTO_UNKNOWN)
        return NULL;
    // protocol
    protocol = i2s_prt << CSK_I2S_PROTO_Pos;

    if (ch_cnt == 0 || (ch_cnt > 2 && !use_tdm))
        return NULL;

    // data format
    switch (ch_bits) {
    case 16:
        format = CSK_I2S_DATA_FORMAT_DUAL_16BIT;
        break;

    case 20: // use high 3 bytes of word by default
        format = CSK_I2S_DATA_FORMAT_20BIT_HIGH;
        //format = CSK_I2S_DATA_FORMAT_20BIT_LOW;
        break;

    case 24: // use high 3 bytes of word by default
        format = CSK_I2S_DATA_FORMAT_24BIT_HIGH;
        //format = CSK_I2S_DATA_FORMAT_24BIT_LOW;
        break;

    case 32:
        format = CSK_I2S_DATA_FORMAT_32BIT;
        break;

    default:
        return NULL;
    }

    // please refer to P30 of "CSK4002 Hardware Development Guide" for TDM mode setting
    // Demo settings are listed below (HS = Hardware Sampling)
    // 2 channels: 2 MIC, use ES7210_1 MIC1 & MIC2
    // 4 channels: 4 MIC, use ES7210_1 MIC1 ~ MIC4
    // 6 channels: 4 MIC + 2 HS, use ES7210_1 MIC1 ~ MIC4 and ES7210 MIC3(HS) ~ MIC4(HS)
    // 8 channels: 6 MIC + 2 HS, use ES7210_1 MIC1 ~ MIC4 and ES7210 MIC1 ~ MIC4

    uint8_t in_bmp;
    if (ch_cnt > 2) {
        assert(use_tdm);
        in_bmp = 0x1; // use Left channel only
    } else if (ch_cnt == 2) {
        in_bmp = 0x3; // both L&R channels
    } else {
        in_bmp = ch_bmp & 0xFF;
    }

    //iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev, in_bmp, 0, 0, 0); // NO request for ECHO channel
    I2S_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;
    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev,
                        (in_bmp << I2S_BMP_FLAG_IN_POS), &dma_chs); // NO request for ECHO channel
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = I2S_PowerControl(i2s_dev, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    iret = I2S_Control(i2s_dev,
                    (is_master? CSK_I2S_MODE_MASTER : CSK_I2S_MODE_SLAVE) | protocol | format | // as slave if MIC recording
                        (use_tdm ? CSK_I2S_TDM_CHS(ch_cnt) : CSK_I2S_TDM_CHS(0)) |
                        (ch_cnt > 1 ? CSK_I2S_RXCH_MIXED : CSK_I2S_RXCH_SEPA), //default, may be changed via I2S_Control
                        sampling_freq);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    //i2s_initialized = true;
    CLOGD("%s: I2S module is initialized successfully!!\r\n", __func__);

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}


void * init_i2s_es7210_in(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t mic_in_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event)
{
    void *i2s_dev;
    bool ret;
    uint8_t i, ch_cnt;

    // Calculate channel count from channel bitmap
    for (i = 0, ch_cnt = 0; i < 16; i++) {
        if (i2s_ch_bmp & (0x1 << i)) {
            ch_cnt++;
        }
    }

    // Check the channel count
    if (ch_cnt == 0) {
        CLOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

    // Check I2S protocol, channel count, and TDM setting
    if ((ch_cnt > 2 || use_tdm == 1) &&
        (i2s_prt != I2S_PROTO_PCMMODE_A) && (i2s_prt != I2S_PROTO_PCMMODE_B)){
        CLOGE("%s: channel_count = %d, use_tdm = %d, but I2S protocol has no TDM support!\r\n",
                __func__, ch_cnt, use_tdm);
        return NULL;
    }

    LOGD("%s: channel_count = %d, use_tdm = %d\r\n", __func__, ch_cnt, use_tdm);
    //true indicates ES7210 is master, false indicates ES7210 is slave
    //codec_is_master = true;
    //codec_is_master = false;

    // Initialize ES7210 CODEC for MIC
    if (mic_in_bmp != 0) {
        if (!i2s_master)
            ret = ES7210_init_master(24000000, sampling_freq, i2s_prt, mic_in_bmp, ch_bits);
        else
            ret = ES7210_init_slave(24000000, sampling_freq, i2s_prt, mic_in_bmp, ch_bits);
        if (!ret)
            return NULL;
    }

    // Initialize I2S module
    i2s_dev = init_i2s_input(i2s_ch_bmp, ch_cnt, use_tdm, ch_bits, i2s_master, i2s_prt, sampling_freq, cb_event);
    if (i2s_dev == NULL)
        return NULL;

    return i2s_dev;
}
