/*
 * i2s_codec_out.c
 *
 *  Created on: Nov. 9, 2020 for VENUS (CP)
 *  Ported on: Dec. 18, 2023 for ARCS (AP)
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

#define USE_I2S_IDX    0 //1
//#define CHK_ADC_SDM    1 //0 //

extern void *g_i2c_dev;
extern void *gpio_A;

#define DEF_CODEC_I2C_ADDR_7BIT    0x1a
static bool WM8960_write_reg(uint8_t reg, uint16_t dat)
{
    bool ret;
    uint8_t I2C_Data[2];

    I2C_Data[0] = (reg << 1) | ((dat >> 8) & 0x01); //RegAddr
    I2C_Data[1] = dat & 0xFF; //RegValue
    ret = i2c_write(g_i2c_dev, DEF_CODEC_I2C_ADDR_7BIT, I2C_Data, 2, 50); // FIXME: timeout = 2ms?
//    ret = i2c_write(g_i2c_dev, DEF_CODEC_I2C_ADDR_7BIT, I2C_Data, 2, 100); // FIXME: timeout = 100ms?
    return ret;
}


typedef struct {
    uint16_t    co256N;  // N*256 coefficient
    uint16_t    sampDiv; // optional value of ADCDIV[2:0] or DACDIV[2:0]
} WM8960_CLK;

#define WM8960_CLK_CNT  7
static const WM8960_CLK wm8960_clk_array[WM8960_CLK_CNT] = {
        { 256 * 1, 0x0 },
        { 256 + 256/2, 0x1 },
        { 256 * 2, 0x2 },
        { 256 * 3, 0x3 },
        { 256 * 4, 0x4 },
        { 256 * 5 + 256/2, 0x5 },
        { 256 * 6, 0x6 }
};

typedef struct {
    uint32_t    mclk;   // input master clock or XTAL
    uint32_t    tclk;   // target clock output
    uint8_t    preDiv;  // PLLPRESCALE value
    uint8_t    postDiv; // SYSCLKDIV[1:0]
    uint8_t    rsvd;
    uint8_t    pllN;    // PLL N value
    uint32_t   pllK;    // PLL K value
} WM8960_PLL_CLK;

#define SOURCE_MCLK     12000000 // HZ
#define SOURCE_MCLK2    24000000 // HZ
#define SOURCE_MCLK3    48000000 // HZ
#define TARGET_SYSCLK   12288000 // HZ
#define TARGET_SYSCLK2  24576000 // HZ

#define PLL_PRESCALE_THRU   0 // divided by 1
#define PLL_PRESCALE_DIV2   1 // divided by 2

#define SYSCLK_DIV_THRU   0 // divided by 1
#define SYSCLK_DIV_DIV2   2 // divided by 2

#define WM8960_PLL_CLK_CNT  3
static const WM8960_PLL_CLK wm8960_pll_clks[WM8960_PLL_CLK_CNT] = {
        { SOURCE_MCLK, TARGET_SYSCLK, PLL_PRESCALE_THRU, SYSCLK_DIV_DIV2, 0, 8, 0x3126E8 }, // recommended
        { SOURCE_MCLK2, TARGET_SYSCLK, PLL_PRESCALE_DIV2, SYSCLK_DIV_DIV2, 0, 8, 0x3126E8 }, // recommended
        { SOURCE_MCLK3, TARGET_SYSCLK2, PLL_PRESCALE_DIV2, SYSCLK_DIV_DIV2, 0, 8, 0x3126E8 },
};

// ADCLRC share the same frame clock as DACLRC if USE_DACLRC_ONLY is 1
#define USE_DACLRC_ONLY  1 // 0 // 1


//#include "apc.h" // for codec_clk_enable
static void enable_i2s_mclk()
{
//    codec_clk_enable();

#if (USE_I2S_IDX == 0) // use I2S0
    IOMuxManager_PinConfigure(I2S0_MCLK_GROUP, I2S0_MCLK, I2S0_MCLK_FUNC); // MCLK use DBG_CLK
#elif (USE_I2S_IDX == 1) // use I2S1
    IOMuxManager_PinConfigure(I2S1_MCLK_GROUP, I2S1_MCLK, I2S1_MCLK_FUNC); // MCLK use DBG_CLK
#else // other I2S
    return NULL;
#endif
    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 3; // output I2S MCLK (that is, XTAL CLK OUTPUT)
    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
}

// if mclk = 0, use WM8960 own clock.
static bool WM8960_init(I2S_PROTOCOL i2s_prt, uint16_t mic_in_bmp, uint16_t spk_out_bmp,
                        bool as_master, uint8_t ch_bits, uint32_t sampling_freq, uint32_t mclk, uint32_t *bck_master_p)
{
    bool ret;
    uint8_t byte_val;
    uint16_t val, i, val1, val2;
    uint32_t sys_clk = 0;

    if (bck_master_p != NULL)  *bck_master_p = 0;

    //Reset Device
    //ret = WM8960_write_reg(0x0f, 0x0000);
    ret = WM8960_write_reg(0x0f, 0xFFFF);
    if (!ret)   return ret;

    //Set Power Source
    //ret = WM8960_write_reg(0x19, 1<<8 | 1<<7 | 1<<6);
    //bit[8:7] = 01b 2x50k divider enabled(for playback/record), bit[6] for VREF
    val = (1<<7) | (1<<6);
    if (mic_in_bmp != 0)    val |= (1<<1); // bit[1] for Microphone Bias Enable, SHOULD BE SET!!
    if (mic_in_bmp & 0x1)   val |= (1<<5) | (1<<3); // Left MIC: bit[5] for PGA, bit[3] for ADC
    if (mic_in_bmp & 0x2)   val |= (1<<4) | (1<<2); // Right MIC: bit[4] for PGA, bit[2] for ADC
    ret = WM8960_write_reg(0x19, val);
    if (!ret)   return ret;

    //Power Management (2)
    //ret = WM8960_write_reg(0x1A, 1<<8 | 1<<7 | 1<<6 | 1<<5 | 1<<4 | 1<<3);
    val1 = 0;
    if (spk_out_bmp & 0x1)   val1 |= (1<<8) | (1<<6) | (1<<4); // left SPK
    if (spk_out_bmp & 0x2)   val1 |= (1<<7) | (1<<5) | (1<<3); // right SPK
    ret = WM8960_write_reg(0x1A, val1);
    if (!ret)   return ret;

    //Power Management (3)
    //ret = WM8960_write_reg(0x2F, 1<<3 | 1<<2);
    val2 = 0;
    if (mic_in_bmp & 0x1)   val2 |= (1<<5); // left MIC: Input PGA
    if (mic_in_bmp & 0x2)   val2 |= (1<<4); // right MIC: Input PGA
    if (spk_out_bmp & 0x1)   val2 |= (1<<3); // left SPK: Output Mixer
    if (spk_out_bmp & 0x2)   val2 |= (1<<2); // right SPK: Output Mixer
    ret = WM8960_write_reg(0x2F, val2);
    if (!ret)   return ret;

    //Configure clock

    //ADC_DIV = 000b (SYSCLK / (1.0 * 256)), DAC_DIV = 000b (SYSCLK / (1.0 * 256)),
    //SYS_CLK_DIV = 00b (Divide SYSCLK by 1), CLK_SEL = 0b (SYSCLK derived from MCLK)
    //ret = WM8960_write_reg(0x04, 0x0000);
    //if (!ret)   return ret;
    //ret = WM8960_write_reg(0x04, 2<<1 | 3<<3);
    //if (!ret)   return ret;

    if (mclk == 0) { // NOT use mclk, use XTAL or other external clock
            CLOGW("%s: NO XTAL or other external clock!\n"); //FIXME:
            return CSK_DRIVER_ERROR_PARAMETER; //FIXME:
    } // end if else mclk


    while (mclk != 0) {
        uint32_t tmp;
        for (i=0; i<WM8960_CLK_CNT; i++) {
            tmp = sampling_freq * wm8960_clk_array[i].co256N;
            if (mclk == tmp || mclk == (tmp << 1)) { // SYSCLKDIV = 1 or 2
                //NOTE: the same sampling rate is required for both MIC and SPK here...
                val = (wm8960_clk_array[i].sampDiv << 3) |  // ADCDIV[2:0]
                      (wm8960_clk_array[i].sampDiv << 6);   // DACDIV[2:0]
                if (mclk == (tmp << 1))
                    val |= 0x2 << 1; // SYSCLKDIV[1:0] = 2, indicates divided by 2

                ret = WM8960_write_reg(0x04, val);
                if (!ret)   return ret;
                sys_clk = tmp;
                break;
            }
        } // end for i

        // just in the wm8960_clk_array, skip out of while body
        if (i < WM8960_CLK_CNT)
            break;

        // use internal PLL
        for (i=0; i<WM8960_PLL_CLK_CNT; i++) {
            if (mclk == wm8960_pll_clks[i].mclk) {
                // SDM=1, Fractional mode; PLLPRESCALE value; pLLN
                val = (1 << 5) | (wm8960_pll_clks[i].preDiv << 4) | wm8960_pll_clks[i].pllN;
                ret = WM8960_write_reg(0x34, val);
                if (!ret)   return ret;
                ret = WM8960_write_reg(0x35, (wm8960_pll_clks[i].pllK >> 16) & 0xFF); // PLLK[23:16]
                if (!ret)   return ret;
                ret = WM8960_write_reg(0x36, (wm8960_pll_clks[i].pllK >> 8) & 0xFF); // PLLK[15:8]
                if (!ret)   return ret;
                ret = WM8960_write_reg(0x37, (wm8960_pll_clks[i].pllK >> 0) & 0xFF); // PLLK[7:0]
                if (!ret)   return ret;
                break;
            }
        } //end for i

        if (i >= WM8960_PLL_CLK_CNT) {
            CLOGW("%s: CANNOT setup WM8960 clock via PLL!\n", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        byte_val = wm8960_pll_clks[i].postDiv;
        tmp = wm8960_pll_clks[i].tclk;
        for (i=0; i<WM8960_CLK_CNT; i++) {
            if (tmp == sampling_freq * wm8960_clk_array[i].co256N) {
                val1 |= 1 << 0; // PLL_EN=1, Power up PLL
                ret = WM8960_write_reg(0x1A, val1);
                if (!ret)   return ret;

                val = (wm8960_clk_array[i].sampDiv << 3) |  // ADCDIV[2:0]
                      (wm8960_clk_array[i].sampDiv << 6);   // DACDIV[2:0]
                val |= (byte_val & 0x3) << 1; // SYSCLKDIV[1:0]
                val |= 0x1 << 0; // CLKSEL=1, indicates SYSCLK derived from PLL output
                ret = WM8960_write_reg(0x04, val);
                if (!ret)   return ret;
                sys_clk = tmp;
                break;
            }
        } // end for i

        if (i >= WM8960_CLK_CNT) {
            CLOGW("%s: CANNOT setup WM8960 clock (SYSCLKDIV) via PLL!\n", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        break;
    } // end while mclk

    // evaluate BCLKDIV from sys_clk
    if (sys_clk > 0 && as_master) {
        uint32_t div_max, bck;
        byte_val = 0; // default value (bck = sys_clk)
        bck = sys_clk;
        div_max = sys_clk / (sampling_freq * 2 * (ch_bits + 2)); // FIXME: channel count is fixed at 2? +2 for delay cycles...
        //div_max = sys_clk / (sampling_freq * 2 * (ch_bits + 0)); // TEST ONLY!!

        if ((sys_clk & 0x1F) == 0 && div_max >= 32) { // multiple of 32
            byte_val = 0xF;
            bck = sys_clk >> 5;
        } else if ((sys_clk % 24) == 0 && div_max >= 24) { // multiple of 24
            byte_val = 0xC;
            bck = sys_clk / 24;
        } else if ((sys_clk % 22) == 0 && div_max >= 22) { // multiple of 22
            byte_val = 0xB;
            bck = sys_clk / 22;
        } else if ((sys_clk & 0xF) == 0 && div_max >= 16) { // multiple of 16
            byte_val = 0xA;
            bck = sys_clk >> 4;
        } else if ((sys_clk % 12) == 0 && div_max >= 12) { // multiple of 12
            byte_val = 0x9;
            bck = sys_clk / 12;
        } else if ((sys_clk % 11) == 0 && div_max >= 11) { // multiple of 11
            byte_val = 0x8;
            bck = sys_clk / 11;
        } else if ((sys_clk & 0x7) == 0 && div_max >= 8) { // multiple of 8
            byte_val = 0x7;
            bck = sys_clk >> 3;
        } else if ((sys_clk % 6) == 0 && div_max >= 6) { // multiple of 6
            byte_val = 0x6;
            bck = sys_clk / 6;
        } else if ((sys_clk & 0x3) == 0 && div_max >= 4) { // multiple of 4
            byte_val = 0x4;
            bck = sys_clk >> 2;
        } else if ((sys_clk % 3) == 0 && div_max >= 3) { // multiple of 3
            byte_val = 0x3;
            bck = sys_clk / 3;
        }

        ret = WM8960_write_reg(0x08, (0x7 << 6) | byte_val); // BCLKDIV @ Clocking (R8)
        if (!ret)   return ret;
        if (bck_master_p != NULL) *bck_master_p = bck;
    }

    //TODO:Configure ADC/DAC Control 1 & 2, note:
    // Control 1 bit[3] DACMU (DAC Digital Soft Mute), 1 = Mute, 0 = No Mute
    // Control 2 bit[3] DACSMM (DAC Soft Mute Mode, for unmute), 1 = ramp up, 0 = change immediately
    ret = WM8960_write_reg(0x05, 0x0000); // ADC/DAC Control 1
    if (!ret)   return ret;
    ret = WM8960_write_reg(0x06, 0x0008); // ADC/DAC Control 2, DACSMM = 1, unmute ramp up
    if (!ret)   return ret;

    //Configure audio interface, bit[4] LRCK polarity or DSP A/B select
    //bit[3:2] Data Word Length, bit[1:0] FORMAT (10b = I2S Format)
    //ret = WM8960_write_reg(0x07, 0x0002);
    switch (i2s_prt) {
    case I2S_PROTO_PHILIPS:
        val = (0 << 4) | 0x2;
        break;
    case I2S_PROTO_LEFT:
        val = (0x1 << 4) | 0x1;
        break;
    case I2S_PROTO_RIGHT:
        val = (0x1 << 4) | 0x0;
        break;
    case I2S_PROTO_PCMMODE_A:
        val = (0x0 << 4) | 0x03; // | (0x1 << 7); // BCLK inverted
        break;
    case I2S_PROTO_PCMMODE_B:
        val = (0x1 << 4) | 0x03;
        break;
    default:
       return false;
    } //end switch i2s_prt

    switch (ch_bits) {
    case 16:
        val |= 0x0 << 2;
        break;
    case 20:
        val |= 0x1 << 2;
        break;
    case 24:
        val |= 0x2 << 2;
        break;
    case 32:
        val |= 0x3 << 2;
        //Right Justified mode does not support 32-bit data (see WM8960 SPEC P52 )
        if (i2s_prt == I2S_PROTO_RIGHT)
            return false;
        break;
    default:
        return false;
    } //end switch ch_bits

    // wm8960 acts as master (provide BCK & LRCK) and csk6001 as slave if MIC recording is used
    //if (mic_in_bmp != 0)
    //    val |= 0x1 << 6;

    // wm8960 acts as master (provide BCK & LRCK) or slave
    if (as_master)
        val |= (0x1 << 6); // Enable master mode
    else
        val &= ~(0x1 << 6); // Enable slave mode

    ret = WM8960_write_reg(0x07, val);
    if (!ret)   return ret;

    // ADCLRC/GPIO1 Pin Function Select as GPIO1, that is,
    // ADCLRC share the same frame clock as DACLRC
    // BSD: IT'S IMPORTANT for pin connection between I2S module and WM8960!!
#if USE_DACLRC_ONLY
    ret = WM8960_write_reg(0x09, (0x1 << 6));
    if (!ret)   return ret;
#endif // USE_DACLRC_ONLY

    /*********PGA*********/

    //Input PGA
    if (mic_in_bmp & 0x1) {
        ret = WM8960_write_reg(0x00, 0x003F | 0x0100);
        if (!ret)   return ret;
    }
    if (mic_in_bmp & 0x2) {
        ret = WM8960_write_reg(0x01, 0x003F | 0x0100); // 63 = +30dB
        //ret = WM8960_write_reg(0x01, 0x002B | 0x0100); //TEST: 43 = +15dB
        if (!ret)   return ret;
    }

    //Input Signal Path
    if (mic_in_bmp & 0x1) {
//        ret = WM8960_write_reg(0x20, 0x0008 | 0x0100);
        ret = WM8960_write_reg(0x20, 0x0048 | 0x0100); //TEST: LMIC2B=1, LMP2=1, LMN1=1
        if (!ret)   return ret;
    }
    if (mic_in_bmp & 0x2) {
//        ret = WM8960_write_reg(0x21, 0x0008 | 0x0100);
        ret = WM8960_write_reg(0x21, 0x0048 | 0x0100); //TEST: RMIC2B=1, RMP2=1, RMN1=1
        if (!ret)   return ret;
    }

    //Input Boost Mixer
    if (mic_in_bmp & 0x1) {
        ret = WM8960_write_reg(0x2B, 0x0000);
        //ret = WM8960_write_reg(0x2B, 0x000A); //TEST: bit[3:1] = LIN2BOOST[2:0] = 5, 0dB
        if (!ret)   return ret;
    }
    if (mic_in_bmp & 0x2) {
        ret = WM8960_write_reg(0x2C, 0x0000);
        //ret = WM8960_write_reg(0x2C, 0x000A); //TEST: bit[3:1] = RIN2BOOST[2:0] = 5, 0dB
        if (!ret)   return ret;
    }

    /*********ADC*********/

    //ADC Digital Volume Control
    if (mic_in_bmp & 0x1) {
        ret = WM8960_write_reg(0x15, 0x00C3 | 0x0100);
        if (!ret)   return ret;
    }
    if (mic_in_bmp & 0x2) {
        ret = WM8960_write_reg(0x16, 0x00C3 | 0x0100);
        if (!ret)   return ret;
    }

    /*********ALC Control*********/

    //Noise Gate Control
    ret = WM8960_write_reg(0x14, 0x00F9);
    //ret = WM8960_write_reg(0x14, 0x0); //TEST: NO ALC!!
    if (!ret)   return ret;

    /*********OUTPUT SIGNAL PATH*********/

    //TODO: ONLY FOR headphone jack?
    //Configure HP_L and HP_R OUTPUTS, note: bit[6:0] LOUT1VOL / ROUT1VOL
    // = 1101111b = 1111111b - 0xF = 6dB - 15dB = -10dB ?
    if (spk_out_bmp & 0x1) {
        ret = WM8960_write_reg(0x02, 0x0079 | 0x0100);  //LOUT1 Volume Set, 0x006f
        if (!ret)   return ret;
    }
    if (spk_out_bmp & 0x2) {
        ret = WM8960_write_reg(0x03, 0x0079 | 0x0100);  //ROUT1 Volume Set, 0x006f
        if (!ret)   return ret;
    }

    //TODO: ONLY FOR speaker?
    //Configure SPK_LP/SPK_LN and SPK_RP/SPK_RN, note: bit[6:0] SPKLVOL / SPKRVOL
    // = 1111010 = 1111111b - 0x5 = 6dB - 5dB = 1 dB ?
    if (spk_out_bmp & 0x1) {
        ret = WM8960_write_reg(0x28, 0x0077 | 0x0100); //Left Speaker Volume, 0x007a
        if (!ret)   return ret;
    }
    if (spk_out_bmp & 0x2) {
        ret = WM8960_write_reg(0x29, 0x0077 | 0x0100); //Right Speaker Volume, 0x007a
        if (!ret)   return ret;
    }

    //Enable the OUTPUTS, note: bit[7:6] SPK_OP_EN
    // SPK_OP_EN = 11b, Left and right speakers enabled
    //ret = WM8960_write_reg(0x31, 0x00F7); //Enable Class D Speaker Outputs
    val = 0x37;
    if (spk_out_bmp & 0x1)   val |= (1<<6); // left
    if (spk_out_bmp & 0x2)   val |= (1<<7); // right
    ret = WM8960_write_reg(0x31, val); //Enable Class D Speaker Outputs
    if (!ret)   return ret;

    //TODO: Configure DAC volume, note: bit[7:0] LDACVOL / RDACVOL
    // = 11111111b, indicates 0dB; = 00000000b, indicates Digital Mute
    // = 00000001b, indicates -127dB, 0.5dB added step by step, up to 0dB
    if (spk_out_bmp & 0x1) {
        ret = WM8960_write_reg(0x0a, 0x00ff | 0x0100);
        if (!ret)   return ret;
    }
    if (spk_out_bmp & 0x2) {
        ret = WM8960_write_reg(0x0b, 0x00ff | 0x0100);
        if (!ret)   return ret;
    }

    //3D, note: bit[4:1] 3DDEPTH (3D Stereo Depth)
    // = 1111b, indicates 100% (maximum 3D effect)
    //ret = WM8960_write_reg(0x10, 0x000F);
    //if (!ret)   return ret;

    //Configure MIXER,
    //bit[8] Left/Right DAC to L/R Output Mixer, 1 indicates Enable Path
    //bit[7] L/R INPUT3 to L/R Output Mixer, 1 indicates Enable Path
    if (spk_out_bmp & 0x1) {
        ret = WM8960_write_reg(0x22, 1<<8 | 1<<7);
        if (!ret)   return ret;
    }
    if (spk_out_bmp & 0x2) {
        ret = WM8960_write_reg(0x25, 1<<8 | 1<<7);
        if (!ret)   return ret;
    }

    //Jack Detect
    //HPSWEN (bit6) = 1, Headphone switch enabled
    //HPSWPOL (bit5) = 0, HPDETECT high = headphone
    ret = WM8960_write_reg(0x18, 1<<6 | 1<<2 | 0<<5); //BSD: LRCM (bit2) = 1, refer to USE_DACLRC_ONLY
    if (!ret)   return ret;

    ret = WM8960_write_reg(0x17, 0x01C3); // bit[3:2]=00b, left data = left ADC; right data =right ADC
    //ret = WM8960_write_reg(0x17, 0x01C0); // bit[3:2]=00b, left data = left ADC; right data =right ADC
    //ret = WM8960_write_reg(0x17, 0x01C7); //TEST: bit[3:2]=01b, left data = left ADC; right data = left ADC
    //ret = WM8960_write_reg(0x17, 0x01CB); //TEST: bit[3:2]=10b, left data = right ADC; right data = right ADC
    //ret = WM8960_write_reg(0x17, 0x01CF); //TEST: bit[3:2]=11b, left data = right ADC; right data = left ADC
    if (!ret)   return ret;

    // Headphone Switch Input Select: HPSEL (bit[3:2]):
    // 0X, GPIO1 used for jack detect input (Requires ADCLRC pin to be configured as a GPIO)
    ret = WM8960_write_reg(0x30, 0x0004);
    //ret = WM8960_write_reg(0x30, 0x000c);//0x000D,0x0005 // FIXME: 0x0009, 0x0008  it seems no effect once modified
    if (!ret)   return ret;

    return true;
}


// get LR channel bitmap and total channel count for ch_bmp
static uint8_t get_lrbmp_chcnt(uint16_t ch_bmp, uint8_t use_tdm, uint8_t *ch_cnt_p)
{
    uint8_t i, ch_cnt;
    uint8_t lr_bmp;

    // Calculate channel count from channel bitmap
    for (i = 0, ch_cnt = 0; i < 16; i++) {
        if (ch_bmp & (0x1 << i)) {
            ch_cnt++;
        }
    }

    if (ch_cnt > 2) {
        assert(use_tdm);
//        lr_bmp = 0x1; // use Left channel only
        lr_bmp = 0x3; // use both
    } else if (ch_cnt == 2) {
        lr_bmp = 0x3; // both L&R channels
    } else {
        lr_bmp = ch_bmp & 0xFF;
    }

    if(ch_cnt_p != NULL)
        *ch_cnt_p = ch_cnt;
    return lr_bmp;
}


// initialize i2s output, return I2S OUT device pointer
static void * init_i2s_out_only(uint16_t lr_bmp, uint8_t ch_cnt, uint8_t use_tdm, uint8_t ch_bits, bool is_master,
                            I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, uint32_t bck_slave, CSK_I2S_SignalEvent_t cb_event)
{
    uint32_t protocol, format;
    int32_t iret;
    void *i2s_dev;

#if (USE_I2S_IDX == 0) // use I2S0
    i2s_dev = I2S0();
    if (i2s_dev == NULL)
        return NULL;

#if 0 // CHK_ADC_SDM
    //sdm config pin
    IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.all = (0x2 << 10) | (0x3 << 6);

    IP_CMN_SYS->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; // aud_debug_clk
    IP_CMN_SYS->REG_TEST_CTRL.bit.DBG_OUT_SEL = 0; // aud_debug_data_out
    IP_CMN_SYS->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
    IP_CMN_SYS->REG_TEST_CTRL.bit.DBG_OUT_EN = 1;

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, 18); //adc_clk @ A10

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 0, 19); //sdm_out_l: d0 @ A00
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 1, 19); //sdm_out_l: d1 @ A01
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 2, 19); //sdm_out_l: d2 @ A02

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 3, 19); //sdm_out_r: d0 @ A03
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, 19); //sdm_out_r: d1 @ A04
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, 19); //sdm_out_r: d2 @ A05

#else
    // FIXME: WM8960 CODEC is connected to I2S0
	//IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; //10:dmic,31:aud_debug_clk
	//IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 1; //4: aud_dmic_clk,13:fsclk,1:adc_mclk_test

	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_IN_SEL = 2; //3bit sdm
	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.DEBUG_MODE = 2; //adc_dig_debug_mode
	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELB = 0x5; //data
	//IP_CMN_IOMUX->REG_PAD_GPIOA_12.bit.PAD_GPIOA_12_FSEL = 11; //clk
	//sdm 3bit
	//IP_CMN_IOMUX->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_FSEL = 20; //sdm0
	//IP_CMN_IOMUX->REG_PAD_GPIOA_17.bit.PAD_GPIOA_17_FSEL = 20;//sdm1
	//IP_CMN_IOMUX->REG_PAD_GPIOA_18.bit.PAD_GPIOA_18_FSEL = 20; //sdm2

    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_LRCK, I2S0_FUNC); // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_BCK, I2S0_FUNC); // BCK
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_DOUT, I2S0_FUNC); // DOUT
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_DIN, I2S0_FUNC); // DIN
    //IOMuxManager_PinConfigure(I2S0_MCLK_GROUP, I2S0_MCLK, I2S0_MCLK_FUNC); // MCLK use DBG_CLK
#endif

#elif (USE_I2S_IDX == 1) // use I2S1
    i2s_dev = I2S1();
    if (i2s_dev == NULL)
        return NULL;

    // FIXME: WM8960 CODEC is connected to I2S1
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_LRCK, I2S1_FUNC); // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_BCK, I2S1_FUNC); // BCK
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DOUT, I2S1_FUNC); // DOUT
    //IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DIN, I2S1_FUNC); // DIN
//    IOMuxManager_PinConfigure(I2S1_MCLK_GROUP, I2S1_MCLK, I2S1_MCLK_FUNC); // MCLK use DBG_CLK

#else // other I2S
    return NULL;
#endif

//    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; // output I2S MCLK
//    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;

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

//    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev, 0, lr_bmp, 0, 0); // NO request for ECHO channel
    I2S_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_out_left = DMA_CH_AUD_TX_DEF;
    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev,
                        (lr_bmp << I2S_BMP_FLAG_OUT_POS), &dma_chs); // NO request for ECHO channel
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = I2S_PowerControl(i2s_dev, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    iret = I2S_Control(i2s_dev,
                        (is_master ? CSK_I2S_MODE_MASTER : CSK_I2S_MODE_SLAVE) | protocol | format |
                        //(is_master ? CSK_I2S_MODE_MASTER_12P288M : CSK_I2S_MODE_SLAVE) | protocol | format |
                        (use_tdm ? CSK_I2S_TDM_CHS(ch_cnt) : CSK_I2S_TDM_CHS(0)) |
                        CSK_I2S_TXCH_STEREO_SRC_STEREO, // CSK_I2S_TXCH_MONO_SRC_MONO , may be changed via I2S_Control
                        (is_master ? sampling_freq : bck_slave / sampling_freq)); //use default 24Mhz input clock
                        //(is_master ? (I2S_CLKIN_12P288MHZ << 24) | sampling_freq : bck_slave / sampling_freq)); //use 12.288Mhz input clock
                        //(is_master ? (I2S_CLKIN_24P576MHZ << 24) | sampling_freq : bck_slave / sampling_freq)); //use 24.576Mhz input clock
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    //i2s_initialized = true;
    LOGD("%s: I2S module is initialized (as %s) successfully!!\r\n", __func__, (is_master ? "MASTER" : "SLAVE"));

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}

#if 0
// initialize i2s input, return I2S IN device pointer
static void * init_i2s_in_only(uint16_t lr_bmp, uint8_t ch_cnt, uint8_t use_tdm, uint8_t ch_bits, bool is_master,
                            I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, uint32_t bck_slave, CSK_I2S_SignalEvent_t cb_event)
{
    uint32_t protocol, format;
    int32_t iret;
    void *i2s_dev;

#if (USE_I2S_IDX == 0) // use I2S0
    i2s_dev = I2S0();
    if (i2s_dev == NULL)
        return NULL;

	//IO Config
	IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; //iis mclk,10:dmic,31:aud_debug_clk
	IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 15; //4: aud_dmic_clk,13:fsclk,1:adc_mclk_test,15:dac_sdm_clk,10:dac mclk
	IP_CMN_IOMUX->REG_PAD_GPIOA_10.bit.PAD_GPIOA_10_FSEL = 18; //clk
	//IP_CMN_IOMUX->REG_PAD_GPIOA_08.bit.PAD_GPIOA_08_FSEL = 11; //debug clk-->iis mclk

	// for dac 4bit data
	IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_OUT_SEL = 0;
	IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_OUT_EN = 1;
	//IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 17;
	//IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 1; //10:classd mclk test,13:fsclk,1:adc_mclk_test
	//IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_IN_SEL = 2; //3bit sdm
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.DEBUG_MODE = 6; //adc_dig_debug_mode
	IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELB = 0x4; //data
	//IP_CMN_IOMUX->REG_PAD_GPIOA_12.bit.PAD_GPIOA_12_FSEL = 11; //clk

	//dac 4bit sdm out
	IP_CMN_IOMUX->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_FSEL = 19; //d0
	IP_CMN_IOMUX->REG_PAD_GPIOA_17.bit.PAD_GPIOA_17_FSEL = 19;//d1
	IP_CMN_IOMUX->REG_PAD_GPIOA_18.bit.PAD_GPIOA_18_FSEL = 19; //d2
	IP_CMN_IOMUX->REG_PAD_GPIOA_19.bit.PAD_GPIOA_19_FSEL = 19; //d3

    // FIXME: WM8960 CODEC is connected to I2S0
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_LRCK, I2S0_FUNC); // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_BCK, I2S0_FUNC); // BCK
    //IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_DOUT, I2S0_FUNC); // DOUT
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_DIN, I2S0_FUNC); // DIN

//    //IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_MCLK, I2S0_FUNC); // MCLK
//    IOMuxManager_PinConfigure(I2S0_MCLK_GROUP, I2S0_MCLK, I2S0_MCLK_FUNC); // MCLK use DBG_CLK

#elif (USE_I2S_IDX == 1) // use I2S1
    i2s_dev = I2S1();
    if (i2s_dev == NULL)
        return NULL;

    // FIXME: WM8960 CODEC is connected to I2S1
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_LRCK, I2S1_FUNC); // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_BCK, I2S1_FUNC); // BCK
    //IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DOUT, I2S1_FUNC); // DOUT
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DIN, I2S1_FUNC); // DIN

//    //IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_MCLK, I2S1_FUNC); // MCLK
//    IOMuxManager_PinConfigure(I2S1_MCLK_GROUP, I2S1_MCLK, I2S1_MCLK_FUNC); // MCLK use DBG_CLK

#else // other I2S
    return NULL;
#endif

//    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; // output I2S MCLK
//    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;

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

    //iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev, lr_bmp, 0, 0, 0); // NO request for ECHO channel
    I2S_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;
    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev,
                        (lr_bmp << I2S_BMP_FLAG_IN_POS), &dma_chs); // NO request for ECHO channel
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = I2S_PowerControl(i2s_dev, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    iret = I2S_Control(i2s_dev,
                        (is_master ? CSK_I2S_MODE_MASTER : CSK_I2S_MODE_SLAVE) | protocol | format |
                        (use_tdm ? CSK_I2S_TDM_CHS(ch_cnt) : CSK_I2S_TDM_CHS(0)) |
                        (ch_cnt > 1 ? CSK_I2S_RXCH_MIXED : CSK_I2S_RXCH_SEPA), //default, may be changed via I2S_Control
                        (is_master ? sampling_freq : bck_slave / sampling_freq));
                        //(is_master ? (I2S_CLKIN_12P288MHZ << 24) | sampling_freq : bck_slave / sampling_freq)); //use 12.288Mhz input clock
                        //(is_master ? (I2S_CLKIN_24P576MHZ << 24) | sampling_freq : bck_slave / sampling_freq)); //use 24.576Mhz input clock
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    //i2s_initialized = true;
    CLOGD("%s: I2S module is initialized (as %s) successfully!!\r\n", __func__, (is_master ? "MASTER" : "SLAVE"));

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}
#else

void configure_io() {
    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; // iis mclk, 10:dmic, 31:aud_debug_clk
    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
    IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 15; // 4: aud_dmic_clk, 13:fsclk, 1:adc_mclk_test, 15:dac_sdm_clk, 10:dac mclk
    //IP_CMN_IOMUX->REG_PAD_GPIOA_10.bit.PAD_GPIOA_10_FSEL = 18; // clk

    // for dac 4bit data
    //IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_OUT_SEL = 0;
    //IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_OUT_EN = 1;
    //IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.DEBUG_MODE = 6; // adc_dig_debug_mode
    //IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELB = 0x4; // data

    // dac 4bit sdm out
    //IP_CMN_IOMUX->REG_PAD_GPIOA_16.bit.PAD_GPIOA_16_FSEL = 19; // d0
    //IP_CMN_IOMUX->REG_PAD_GPIOA_17.bit.PAD_GPIOA_17_FSEL = 19; // d1
    //IP_CMN_IOMUX->REG_PAD_GPIOA_18.bit.PAD_GPIOA_18_FSEL = 19; // d2
    //IP_CMN_IOMUX->REG_PAD_GPIOA_19.bit.PAD_GPIOA_19_FSEL = 19; // d3
}

// initialize i2s input, return I2S IN device pointer
static void * init_i2s_in_only(uint16_t lr_bmp, uint8_t ch_cnt, uint8_t use_tdm, uint8_t ch_bits, bool is_master,
                            I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, uint32_t bck_slave, CSK_I2S_SignalEvent_t cb_event)
{
    uint32_t protocol, format;
    int32_t iret;
    void *i2s_dev;

    // IO Config
    configure_io();

	if (USE_I2S_IDX == 0) { // use I2S0
		i2s_dev = I2S0();
		if (i2s_dev == NULL)
			return NULL;

		// FIXME: WM8960 CODEC is connected to I2S0
		IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_LRCK, I2S0_FUNC); // LRCK (FS, WS)
		IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_BCK, I2S0_FUNC); // BCK
		IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_DIN, I2S0_FUNC); // DIN
	} else if (USE_I2S_IDX == 1) { // use I2S1
		i2s_dev = I2S1();
		if (i2s_dev == NULL)
			return NULL;

		// FIXME: WM8960 CODEC is connected to I2S1
		IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_LRCK, I2S1_FUNC); // LRCK (FS, WS)
		IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_BCK, I2S1_FUNC); // BCK
		IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DIN, I2S1_FUNC); // DIN
	} else { // other I2S
		return NULL;
	}

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

    //iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev, lr_bmp, 0, 0, 0); // NO request for ECHO channel
    I2S_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;
    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev,
                        (lr_bmp << I2S_BMP_FLAG_IN_POS), &dma_chs); // NO request for ECHO channel
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = I2S_PowerControl(i2s_dev, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    iret = I2S_Control(i2s_dev,
                        (is_master ? CSK_I2S_MODE_MASTER : CSK_I2S_MODE_SLAVE) | protocol | format |
                        (use_tdm ? CSK_I2S_TDM_CHS(ch_cnt) : CSK_I2S_TDM_CHS(0)) |
                        (ch_cnt > 1 ? CSK_I2S_RXCH_MIXED : CSK_I2S_RXCH_SEPA), //default, may be changed via I2S_Control
                        (is_master ? sampling_freq : bck_slave / sampling_freq));
                        //(is_master ? (I2S_CLKIN_12P288MHZ << 24) | sampling_freq : bck_slave / sampling_freq)); //use 12.288Mhz input clock
                        //(is_master ? (I2S_CLKIN_24P576MHZ << 24) | sampling_freq : bck_slave / sampling_freq)); //use 24.576Mhz input clock
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    //i2s_initialized = true;
    CLOGD("%s: I2S module is initialized (as %s) successfully!!\r\n", __func__, (is_master ? "MASTER" : "SLAVE"));

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}
#endif

// initialize i2s input & output, return I2S IN/OUT device pointer
static void * init_i2s_inout(uint16_t lr_bmp, uint8_t ch_cnt, uint8_t use_tdm, uint8_t ch_bits, bool is_master,
                            I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, uint32_t bck_slave, CSK_I2S_SignalEvent_t cb_event)
{
    uint32_t protocol, format;
    int32_t iret;
    void *i2s_dev;

#if (USE_I2S_IDX == 0) // use I2S0
    i2s_dev = I2S0();
    if (i2s_dev == NULL)
        return NULL;

    // FIXME: WM8960 CODEC is connected to I2S0
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_LRCK, I2S0_FUNC); // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_BCK, I2S0_FUNC); // BCK
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_DOUT, I2S0_FUNC); // DOUT
    IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_DIN, I2S0_FUNC); // DIN

//    //IOMuxManager_PinConfigure(I2S0_GROUP, I2S0_MCLK, I2S0_FUNC); // MCLK
//    IOMuxManager_PinConfigure(I2S0_MCLK_GROUP, I2S0_MCLK, I2S0_MCLK_FUNC); // MCLK use DBG_CLK

#elif (USE_I2S_IDX == 1) // use I2S1
    i2s_dev = I2S1();
    if (i2s_dev == NULL)
        return NULL;

    // FIXME: WM8960 CODEC is connected to I2S1
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_LRCK, I2S1_FUNC); // LRCK (FS, WS)
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_BCK, I2S1_FUNC); // BCK
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DOUT, I2S1_FUNC); // DOUT
    IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_DIN, I2S1_FUNC); // DIN

//    //IOMuxManager_PinConfigure(I2S1_GROUP, I2S1_MCLK, I2S1_FUNC); // MCLK
//    IOMuxManager_PinConfigure(I2S1_MCLK_GROUP, I2S1_MCLK, I2S1_MCLK_FUNC); // MCLK use DBG_CLK

#else // other I2S
    return NULL;
#endif

//    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = 31; // output I2S MCLK
//    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;

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

    //iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev, lr_bmp, lr_bmp, 0, 0); // NO request for ECHO channel
    I2S_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;
    dma_chs.dma_ch_out_left = DMA_CH_AUD_TX_DEF;
    iret = I2S_Initialize(i2s_dev, cb_event, (uint32_t)i2s_dev,
                        (lr_bmp << I2S_BMP_FLAG_IN_POS) | (lr_bmp << I2S_BMP_FLAG_OUT_POS), &dma_chs); // NO request for ECHO channel
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = I2S_PowerControl(i2s_dev, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    iret = I2S_Control(i2s_dev,
                        (is_master ? CSK_I2S_MODE_MASTER : CSK_I2S_MODE_SLAVE) | protocol | format |
                        (use_tdm ? CSK_I2S_TDM_CHS(ch_cnt) : CSK_I2S_TDM_CHS(0)) |
                        CSK_I2S_TXCH_STEREO_SRC_STEREO | //default CSK_I2S_TXCH_MONO_SRC_MONO, may be changed via I2S_Control
                        (ch_cnt > 1 ? CSK_I2S_RXCH_MIXED : CSK_I2S_RXCH_SEPA), //default, may be changed via I2S_Control
                        (is_master ? sampling_freq : bck_slave / sampling_freq));
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    //i2s_initialized = true;
    CLOGD("%s: I2S module is initialized (as %s) successfully!!\r\n", __func__, (is_master ? "MASTER" : "SLAVE"));

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        I2S_Uninitialize(i2s_dev);
        return NULL;
    }

    return i2s_dev;
}

// init I2S OUT device (also init WM8960 codec if spk_out_bmp != 0, or else no external codec!)
void * init_i2s_wm8960_out(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t spk_out_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event)
{
    void *i2s_dev;
    bool ret;
    uint8_t ch_cnt, lr_bmp;

    lr_bmp = get_lrbmp_chcnt(i2s_ch_bmp, use_tdm, &ch_cnt);

    // Check the channel count
    if (ch_cnt == 0) {
        CLOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

//    //NOTE: only 2 channels are supported for our reference design PCB...
//    if (ch_cnt > 2) {
//        CLOGE("%s: 2 audio channels (LR) are supported with this reference design!\r\n", __func__);
//        return NULL;
//    }

    // Check I2S protocol, channel count, and TDM setting
    if ((use_tdm == 1) &&
        (i2s_prt != I2S_PROTO_PCMMODE_A) && (i2s_prt != I2S_PROTO_PCMMODE_B)){
        CLOGE("%s: channel_count = %d, use_tdm = %d, but I2S protocol has no TDM support!\r\n",
                __func__, ch_cnt, use_tdm);
        return NULL;
    }

    LOGD("%s: channel_count = %d, use_tdm = %d\r\n", __func__, ch_cnt, use_tdm);

    //enable I2S MCLK for check
    if (spk_out_bmp == 0)
        enable_i2s_mclk();

    // Initialize WM8960 CODEC for SPK if necessary
    uint32_t bck = 0;
    if (spk_out_bmp != 0) {

        //enable I2S MCLK to make external CODEC work
        enable_i2s_mclk();

        ret = WM8960_init(i2s_prt, 0, spk_out_bmp, !i2s_master, ch_bits, sampling_freq, 24000000, (i2s_master ? NULL : &bck)); // 48000000
        //ret = WM8960_init(i2s_prt, 0, CH_BMP_LEFT, !i2s_master, ch_bits, sampling_freq, 24000000, (i2s_master ? NULL : &bck)); // 48000000
        //ret = WM8960_init(i2s_prt, 0, CH_BMP_RIGHT, !i2s_master, ch_bits, sampling_freq, 24000000, (i2s_master ? NULL : &bck)); // 48000000
        if (!ret)
            return NULL;
    }

    // Initialize I2S module
    i2s_dev = init_i2s_out_only(lr_bmp, ch_cnt, use_tdm, ch_bits, i2s_master, i2s_prt, sampling_freq, bck, cb_event);
    if (i2s_dev == NULL)
        return NULL;

    return i2s_dev;
}


void * init_i2s_wm8960_in(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t mic_in_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event)
{
    void *i2s_dev;
    bool ret;
    uint8_t ch_cnt, lr_bmp;

    lr_bmp = get_lrbmp_chcnt(i2s_ch_bmp, use_tdm, &ch_cnt);

    // Check the channel count
    if (ch_cnt == 0) {
        CLOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

//    //NOTE: only 2 channels are supported for our reference design PCB...
//    if (ch_cnt > 2) {
//        CLOGE("%s: 2 audio channels (LR) are supported with this reference design!\r\n", __func__);
//        return NULL;
//    }

    // Check I2S protocol, channel count, and TDM setting
    if ((use_tdm == 1) &&
        (i2s_prt != I2S_PROTO_PCMMODE_A) && (i2s_prt != I2S_PROTO_PCMMODE_B)){
        CLOGE("%s: channel_count = %d, use_tdm = %d, but I2S protocol has no TDM support!\r\n",
                __func__, ch_cnt, use_tdm);
        return NULL;
    }

    LOGD("%s: channel_count = %d, use_tdm = %d\r\n", __func__, ch_cnt, use_tdm);

    //enable I2S MCLK for check
    if (mic_in_bmp == 0)
        enable_i2s_mclk();

    // Initialize WM8960 CODEC for MIC if necessary
    uint32_t bck = 0;
    if (mic_in_bmp != 0) {

        //enable I2S MCLK to make external CODEC work
        enable_i2s_mclk();

        ret = WM8960_init(i2s_prt, mic_in_bmp, 0, !i2s_master, ch_bits, sampling_freq, 24000000, (i2s_master ? NULL : &bck)); // 48000000
        if (!ret)
            return NULL;
    }

    // Initialize I2S module
    i2s_dev = init_i2s_in_only(lr_bmp, ch_cnt, use_tdm, ch_bits, i2s_master, i2s_prt, sampling_freq, bck, cb_event);
    if (i2s_dev == NULL)
        return NULL;

    return i2s_dev;
}


// init I2S IN/OUT device (WM8960 codec)
void * init_i2s_wm8960_inout(bool i2s_master, uint16_t i2s_ch_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event)
{
    void *i2s_dev;
    bool ret;
    uint8_t ch_cnt, lr_bmp;

    lr_bmp = get_lrbmp_chcnt(i2s_ch_bmp, use_tdm, &ch_cnt);

    // Check the channel count
    if (ch_cnt == 0) {
        CLOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

    //NOTE: only <= 2 channels are supported for both I2S IN and OUT...
    if (ch_cnt > 2) {
        CLOGE("%s: 2 audio channels (LR) are supported with this reference design!\r\n", __func__);
        return NULL;
    }

    // Check I2S protocol, channel count, and TDM setting
    if ((use_tdm == 1) &&
        (i2s_prt != I2S_PROTO_PCMMODE_A) && (i2s_prt != I2S_PROTO_PCMMODE_B)){
        CLOGE("%s: channel_count = %d, use_tdm = %d, but I2S protocol has no TDM support!\r\n",
                __func__, ch_cnt, use_tdm);
        return NULL;
    }

    LOGD("%s: channel_count = %d, use_tdm = %d\r\n", __func__, ch_cnt, use_tdm);

    //enable I2S MCLK to make external CODEC work
    enable_i2s_mclk();

    // Initialize WM8960 CODEC for MIC
    uint32_t bck = 0;
    ret = WM8960_init(i2s_prt, lr_bmp, 0, !i2s_master, ch_bits, sampling_freq, 24000000, (i2s_master ? NULL : &bck)); // 48000000
    if (!ret)
        return NULL;

    // Initialize I2S module
    i2s_dev = init_i2s_inout(lr_bmp, ch_cnt, use_tdm, ch_bits, i2s_master, i2s_prt, sampling_freq, bck, cb_event);
    if (i2s_dev == NULL)
        return NULL;

    return i2s_dev;
}
