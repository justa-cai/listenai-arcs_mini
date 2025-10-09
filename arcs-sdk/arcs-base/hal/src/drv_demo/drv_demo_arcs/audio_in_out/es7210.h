/*
 * ES7210.h
 *
 *  Created on: Sep 24, 2019
 *      Author: haohe
 *
 *  Modified by bauldeng, 2020.5.7.
 *
 */

#ifndef SRC_MAIN_ES7210_H_
#define SRC_MAIN_ES7210_H_

// please refer to castor4002_es7210_micboard.pdf
#define ES7210_1_I2C_ADDR   0x43 // 100 0011B
#define ES7210_2_I2C_ADDR   0x40 // 100 0000B


#define ES7210_RESET_CTL_REG00		    0x00
#define ES7210_CLK_ON_OFF_REG01		    0x01
#define ES7210_MCLK_CTL_REG02		    0x02
#define ES7210_MST_CLK_CTL_REG03	    0x03
#define ES7210_MST_LRCDIVH_REG04	    0x04
#define ES7210_MST_LRCDIVL_REG05	    0x05
#define ES7210_DIGITAL_PDN_REG06	    0x06
#define ES7210_ADC_OSR_REG07		    0x07
#define ES7210_MODE_CFG_REG08		    0x08

#define ES7210_TCT0_CHPINI_REG09	    0x09
#define ES7210_TCT1_CHPINI_REG0A	    0x0A
#define ES7210_CHIP_STA_REG0B		    0x0B

#define ES7210_IRQ_CTL_REG0C		    0x0C
#define ES7210_MISC_CTL_REG0D		    0x0D
#define ES7210_DMIC_CTL_REG10		    0x10

#define ES7210_SDP_CFG1_REG11		    0x11
#define ES7210_SDP_CFG2_REG12		    0x12

#define ES7210_ADC_AUTOMUTE_REG13	    0x13
#define ES7210_ADC34_MUTE_REG14		    0x14
#define ES7210_ADC12_MUTE_REG15		    0x15

#define ES7210_ALC_SEL_REG16		    0x16
#define ES7210_ALC_COM_CFG1_REG17	    0x17
#define ES7210_ALC34_LVL_REG18		    0x18
#define ES7210_ALC12_LVL_REG19		    0x19
#define ES7210_ALC_COM_CFG2_REG1A	    0x1A
#define ES7210_ALC4_MAX_GAIN_REG1B	    0x1B
#define ES7210_ALC3_MAX_GAIN_REG1C	    0x1C
#define ES7210_ALC2_MAX_GAIN_REG1D	    0x1D
#define ES7210_ALC1_MAX_GAIN_REG1E	    0x1E

#define ES7210_ADC34_HPF2_REG20		    0x20
#define ES7210_ADC34_HPF1_REG21		    0x21
#define ES7210_ADC12_HPF2_REG22		    0x22
#define ES7210_ADC12_HPF1_REG23		    0x23

#define ES7210_CHP_ID1_REG3D		    0x3D
#define ES7210_CHP_ID0_REG3E		    0x3E
#define ES7210_CHP_VER_REG3F		    0x3F

#define ES7210_ANALOG_SYS_REG40		    0x40

#define ES7210_MICBIAS12_REG41		    0x41
#define ES7210_MICBIAS34_REG42		    0x42
#define ES7210_MIC1_GAIN_REG43		    0x43
#define ES7210_MIC2_GAIN_REG44		    0x44
#define ES7210_MIC3_GAIN_REG45		    0x45
#define ES7210_MIC4_GAIN_REG46		    0x46
#define ES7210_MIC1_LP_REG47		    0x47
#define ES7210_MIC2_LP_REG48		    0x48
#define ES7210_MIC3_LP_REG49		    0x49
#define ES7210_MIC4_LP_REG4A		    0x4A
#define ES7210_MIC12_PDN_REG4B		    0x4B
#define ES7210_MIC34_PDN_REG4C		    0x4C

// ES7210 acts as master
static unsigned char adES7210Conf_1m[][2] = {

    {ES7210_RESET_CTL_REG00                   , 0xff},          //reset
    {ES7210_RESET_CTL_REG00                   , 0x32},          //reset
    {ES7210_TCT0_CHPINI_REG09                 , 0x30},          //setup chip initialization time
    {ES7210_TCT1_CHPINI_REG0A                 , 0x30},          //setup power up time

    {ES7210_ADC12_HPF1_REG23                  , 0x2a},          //setup  HPF
    {ES7210_ADC12_HPF2_REG22                  , 0x0a},          //setup  HPF
    {ES7210_ADC34_HPF1_REG21                  , 0x2a},          //setup  HPF
    {ES7210_ADC34_HPF2_REG20                  , 0x0a},          //setup  HPF
//  {ES7210_MODE_CFG_REG08                    , 0x40},          //set LRCK mode, this example is used for 8 microphone.  If 6mic used, here is 0x30
//  {ES7210_DMIC_CTL_REG10                    , 0x00},          // dmic
    // bit[7:5] SP_WL: 000 – 24-bit, 001 – 20-bit, 010 – 18-bit, 011 – 16-bit, 100 – 32-bit
    // bit[4] SP_LRP: 0 – L/R normal polarity, 1 – L/R invert polarity
    // bit[1:0] SP_PROTOCAL: 00 - I2S, 01 - Left Justified, 10 - Reserved, 11 - DSP mode
    {ES7210_SDP_CFG1_REG11                    , 0x93},          //[M] 32bit
    // bit[2] CASCADE_CHIPFLAG: When set SDOUT_MODE=11, 0 – last ES7210 in TDM, 1 – not the last ES7210 in TDM
    // bit[1:0] SDOUT_MODE: 00 - TDM disabled, 01 - TDM DSP, 10 - TDM I2S/Left Justified, 11 - multiple LRCK TDM
    {ES7210_SDP_CFG2_REG12                    , 0x01},          //[M] multiple LRCK TDM  mode, if the chip is the first chip in tdm link, here is 0x07, otherwise, 0x03

//    {ES7210_ALC4_MAX_GAIN_REG1B               , 0xDF},          // ADC4 MAX GAIN
//    {ES7210_ALC3_MAX_GAIN_REG1C               , 0xDF},          // ADC3 MAX GAIN
//    {ES7210_ALC2_MAX_GAIN_REG1D               , 0xDF},          // ADC2 MAX GAIN
//    {ES7210_ALC1_MAX_GAIN_REG1E               , 0xDF},          // ADC1 MAX GAIN
    {ES7210_ALC4_MAX_GAIN_REG1B               , 0xBF},          // ADC4 MAX GAIN (digital), default value
    {ES7210_ALC3_MAX_GAIN_REG1C               , 0xBF},          // ADC3 MAX GAIN (digital), default value
    {ES7210_ALC2_MAX_GAIN_REG1D               , 0xBF},          // ADC2 MAX GAIN (digital), default value
    {ES7210_ALC1_MAX_GAIN_REG1E               , 0xBF},          // ADC1 MAX GAIN (digital), default value

    {ES7210_ANALOG_SYS_REG40                  , 0xC3},          //power up analog                                                                                           //
    {ES7210_MICBIAS12_REG41                   , 0x70},          //power up micbias and pga
    {ES7210_MICBIAS34_REG42                   , 0x70},          //power up micbias and pga

//    {ES7210_MIC1_GAIN_REG43                   , 0x1E},          //[M] pga1 gain (analog) = 37.5db
//    {ES7210_MIC2_GAIN_REG44                   , 0x1E},          //[M] pga1 gain (analog) = 37.5db
//    {ES7210_MIC3_GAIN_REG45                   , 0x1E},          //[M] pga1 gain (analog) = 37.5db
//    {ES7210_MIC4_GAIN_REG46                   , 0x1E},          //[M] pga1 gain (analog) = 37.5db
    {ES7210_MIC1_GAIN_REG43                   , 0x11},          //[M] pga1 gain (analog) = 3db
    {ES7210_MIC2_GAIN_REG44                   , 0x11},          //[M] pga1 gain (analog) = 3db
    {ES7210_MIC3_GAIN_REG45                   , 0x11},          //[M] pga1 gain (analog) = 3db
    {ES7210_MIC4_GAIN_REG46                   , 0x11},          //[M] pga1 gain (analog) = 3db

    {ES7210_MIC1_LP_REG47                     , 0x08},          //set lp_pga
    {ES7210_MIC2_LP_REG48                     , 0x08},          //set lp_pga
    {ES7210_MIC3_LP_REG49                     , 0x08},          //set lp_pga
    {ES7210_MIC4_LP_REG4A                     , 0x08},          //set lp_pga

    {ES7210_CLK_ON_OFF_REG01                    , 0x20},        // default value (slave mode to turn off master mode SCLK & LRCK)
    {ES7210_MODE_CFG_REG08                      , 0x10},        // default value (2 ch for SDOUT_MODE=11b, single speed, slave mode)
    {ES7210_MST_CLK_CTL_REG03                   , 0x04},        // default value (MCLK/BCK = 4)
    {ES7210_MST_LRCDIVH_REG04                   , 0x01},        // default value (MCLK/LRCK bit[11:8] = 1)
    {ES7210_MST_LRCDIVL_REG05                   , 0x00},        // default value (MCLK/LRCK bit[7:0] = 0)
    {ES7210_ADC_OSR_REG07                       , 0x20},        // default value (adc osr = 32)
    //FIXME: should use 0x84 if bypass dll and disable clock_doubler, adc div =4
    // 0xC1 = bypass dll and enable clock_doubler, adc_div=1,
    // but REG03 bit[7] = 0 indicate use external MCLK directly, so this setting is useless!!
//    {ES7210_MCLK_CTL_REG02                    , 0xC1},        //bypass dll and disable clock_doubler, adc div =4
    {ES7210_MCLK_CTL_REG02                    , 0x4F},          //not bypass dll and enable clock_doubler, adc div = 15
    {ES7210_DIGITAL_PDN_REG06                 , 0x00},          //power up dll

    {ES7210_MIC12_PDN_REG4B                   , 0x0F},          //Power down ADC and PGA, then will be startup automatically
    {ES7210_MIC34_PDN_REG4C                   , 0x0F},          //Power down ADC and PGA, then will be startup automatically
    {ES7210_RESET_CTL_REG00                   , 0x01}           //reset
};

#define ES7210_REG_LEN_1M             ( sizeof( adES7210Conf_1m ) / sizeof( adES7210Conf_1m[0] ) )


//static const unsigned char adES7210Conf_1[][2] = {
static unsigned char adES7210Conf_1[][2] = {

    {ES7210_RESET_CTL_REG00		              , 0xff},          //reset
    {ES7210_RESET_CTL_REG00		              , 0x32},          //reset
    {ES7210_TCT0_CHPINI_REG09	              , 0x30},          //setup chip initialization time
    {ES7210_TCT1_CHPINI_REG0A	              , 0x30},          //setup power up time

    {ES7210_ADC12_HPF1_REG23		          , 0x2a},          //setup  HPF
    {ES7210_ADC12_HPF2_REG22		          , 0x0a},          //setup  HPF
    {ES7210_ADC34_HPF1_REG21		          , 0x2a},          //setup  HPF
    {ES7210_ADC34_HPF2_REG20		          , 0x0a},          //setup  HPF
//	{ES7210_MODE_CFG_REG08		              , 0x40},          //set LRCK mode, this example is used for 8 microphone.  If 6mic used, here is 0x30
//	{ES7210_DMIC_CTL_REG10		              , 0x00},			// dmic
	// bit[7:5] SP_WL: 000 – 24-bit, 001 – 20-bit, 010 – 18-bit, 011 – 16-bit, 100 – 32-bit
	// bit[4] SP_LRP: 0 – L/R normal polarity, 1 – L/R invert polarity
	// bit[1:0] SP_PROTOCAL: 00 - I2S, 01 - Left Justified, 10 - Reserved, 11 - DSP mode
    {ES7210_SDP_CFG1_REG11		              , 0x93},          //[M] 32bit
    // bit[2] CASCADE_CHIPFLAG: When set SDOUT_MODE=11, 0 – last ES7210 in TDM, 1 – not the last ES7210 in TDM
    // bit[1:0] SDOUT_MODE: 00 - TDM disabled, 01 - TDM DSP, 10 - TDM I2S/Left Justified, 11 - multiple LRCK TDM
    {ES7210_SDP_CFG2_REG12		              , 0x01},          //[M] multiple LRCK TDM  mode, if the chip is the first chip in tdm link, here is 0x07, otherwise, 0x03

    {ES7210_ALC4_MAX_GAIN_REG1B               , 0xDF},          // ADC4 MAX GAIN (mic digital)
    {ES7210_ALC3_MAX_GAIN_REG1C               , 0xDF},          // ADC3 MAX GAIN (mic digital)
    {ES7210_ALC2_MAX_GAIN_REG1D               , 0xDF},          // ADC2 MAX GAIN (mic digital)
    {ES7210_ALC1_MAX_GAIN_REG1E               , 0xDF},          // ADC1 MAX GAIN (mic digital)

	{ES7210_ANALOG_SYS_REG40		          , 0xC3},          //power up analog                                                                                           //
    {ES7210_MICBIAS12_REG41		              , 0x70},          //power up micbias and pga
    {ES7210_MICBIAS34_REG42		              , 0x70},          //power up micbias and pga
    {ES7210_MIC1_GAIN_REG43		              , 0x1E},          //[M] pga1 gain = 37.5db
    {ES7210_MIC2_GAIN_REG44		              , 0x1E},          //[M] pga1 gain = 37.5db
    {ES7210_MIC3_GAIN_REG45                   , 0x1E},          //[M] pga1 gain = 37.5db
    {ES7210_MIC4_GAIN_REG46                   , 0x1E},          //[M] pga1 gain = 37.5db
    {ES7210_MIC1_LP_REG47		              , 0x08},          //set lp_pga
    {ES7210_MIC2_LP_REG48		              , 0x08},          //set lp_pga
    {ES7210_MIC3_LP_REG49		              , 0x08},          //set lp_pga
    {ES7210_MIC4_LP_REG4A		              , 0x08},          //set lp_pga

//    {ES7210_CLK_ON_OFF_REG01                    , 0x20},        // default value (slave mode to turn off master mode SCLK & LRCK)
//    {ES7210_MODE_CFG_REG08                      , 0x10},        // default value (2 ch for SDOUT_MODE=11b, single speed, slave mode)
//    {ES7210_MST_CLK_CTL_REG03                   , 0x04},        // default value (MCLK/BCK = 4)
//    {ES7210_MST_LRCDIVH_REG04                   , 0x01},        // default value (MCLK/LRCK bit[11:8] = 1)
//    {ES7210_MST_LRCDIVL_REG05                   , 0x00},        // default value (MCLK/LRCK bit[7:0] = 0)

    {ES7210_ADC_OSR_REG07   	              , 0x20},          // default value (adc osr = 32)
    //FIXME: should use 0x84 if bypass dll and disable clock_doubler, adc div =4
    // 0xC1 = bypass dll and enable clock_doubler, adc_div=1,
    // but REG03 bit[7] = 0 indicate use external MCLK directly, so this setting is useless!!
    //NOTE: not bypass dll + power down dll will make I2S cannot receive valid data (only 0 or 0xFF is received!!)...
//    {ES7210_MCLK_CTL_REG02                    , 0xC1},          //bypass dll and disable clock_doubler, adc div =4
    {ES7210_MCLK_CTL_REG02                    , 0x81},          //bypass dll and disable clock_doubler, adc div = 1
//    {ES7210_MCLK_CTL_REG02                    , 0x02},          //not bypass dll and disable clock_doubler, adc div = 2
    {ES7210_DIGITAL_PDN_REG06	              , 0x04},          //power down dll
//    {ES7210_DIGITAL_PDN_REG06	              , 0x0},          //power up dll

    {ES7210_MIC12_PDN_REG4B		              , 0x0F},          //Power down ADC and PGA, then will be startup automatically
    {ES7210_MIC34_PDN_REG4C		              , 0x0F},          //Power down ADC and PGA, then will be startup automatically
    {ES7210_RESET_CTL_REG00		              , 0x71},          //reset
    {ES7210_RESET_CTL_REG00		              , 0x41}           //reset
};

/*
//static const unsigned char adES7210Conf_2[][2] = {
static unsigned char adES7210Conf_2[][2] = {

    {ES7210_RESET_CTL_REG00		              , 0xff},          //reset
    {ES7210_RESET_CTL_REG00		              , 0x32},          //reset
    {ES7210_TCT0_CHPINI_REG09	              , 0x30},          //setup chip initialization time
    {ES7210_TCT1_CHPINI_REG0A	              , 0x30},          //setup power up time
//    {ES7210_ADC34_MUTE_REG14		          , 0x03},          //7 8 output 0
    {ES7210_ADC12_HPF1_REG23		          , 0x3d},          //setup  HPF
    {ES7210_ADC12_HPF2_REG22		          , 0x04},          //setup  HPF
    {ES7210_ADC34_HPF1_REG21		          , 0x3d},          //setup  HPF
    {ES7210_ADC34_HPF2_REG20		          , 0x04},          //setup  HPF
    {ES7210_SDP_CFG1_REG11		              , 0x93},          //[M] 32bit
    {ES7210_SDP_CFG2_REG12		              , 0x01},          //[M] multiple LRCK TDM  mode, if the chip is the first chip in tdm link, here is 0x07, otherwise, 0x03

    {ES7210_ALC2_MAX_GAIN_REG1D               , 0xDF},          // ADC2 MAX GAIN
    {ES7210_ALC1_MAX_GAIN_REG1E               , 0xDF},          // ADC1 MAX GAIN

    {ES7210_ANALOG_SYS_REG40		          , 0xC3},          //power up analog                                                                  //
    {ES7210_MICBIAS12_REG41		              , 0x70},          //power up micbias and pga
    {ES7210_MICBIAS34_REG42		              , 0x70},          //power up micbias and pga
//    {ES7210_MIC1_GAIN_REG43		              , 0x10},          //[M] pga1 gain = 0db   0x10+n*3fb
//    {ES7210_MIC2_GAIN_REG44		              , 0x10},          //[M] pga1 gain = 0db   0x10+n*3fb
    {ES7210_MIC1_GAIN_REG43                   , 0x1E},          //[M] pga1 gain = 37.5db   0x10+n*3fb
    {ES7210_MIC2_GAIN_REG44                   , 0x1E},          //[M] pga1 gain = 37.5db   0x10+n*3fb
//    {ES7210_MIC3_GAIN_REG45		              , 0x1E},          //[M] pga1 gain = 37.5db   0x10+n*3fb
//    {ES7210_MIC4_GAIN_REG46		              , 0x1E},          //[M] pga1 gain = 37.5db   0x10+n*3fb
    {ES7210_MIC3_GAIN_REG45                   , 0x10},          //[M] pga1 gain = 0db   0x10+n*3fb
    {ES7210_MIC4_GAIN_REG46                   , 0x10},          //[M] pga1 gain = 0db   0x10+n*3fb
    {ES7210_MIC1_LP_REG47		              , 0x08},          //set lp_pga
    {ES7210_MIC2_LP_REG48		              , 0x08},          //set lp_pga
    {ES7210_MIC3_LP_REG49		              , 0x08},          //set lp_pga
    {ES7210_MIC4_LP_REG4A		              , 0x08},          //set lp_pga
    {ES7210_ADC_OSR_REG07		              , 0x20},          //adc osr = 32
    {ES7210_MCLK_CTL_REG02		              , 0xC1},          //bypass dll and disable clock_doubler, adc div =4
    {ES7210_DIGITAL_PDN_REG06	              , 0x04},          //power down dll
    {ES7210_MIC12_PDN_REG4B		              , 0x0F},          //Power down ADC and PGA, then will be startup automatically
    {ES7210_MIC34_PDN_REG4C		              , 0x0F},          //Power down ADC and PGA, then will be startup automatically
    {ES7210_RESET_CTL_REG00		              , 0x71},          //reset
    {ES7210_RESET_CTL_REG00		              , 0x41}           //reset
};
*/

#define ES7210_REG_LEN1             ( sizeof( adES7210Conf_1 ) / sizeof( adES7210Conf_1[0] ) )  //(0x1D + 2)
//#define ES7210_REG_LEN2             ( sizeof( adES7210Conf_2 ) / sizeof( adES7210Conf_2[0] ) )  //0x1D

#endif /* SRC_MAIN_ES7210_H_ */
