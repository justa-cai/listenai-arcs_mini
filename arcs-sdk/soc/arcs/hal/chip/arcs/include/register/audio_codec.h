/*
 * audio_codec.h
 *
 *
 */

#ifndef __AUDIO_CODEC_H
#define __AUDIO_CODEC_H

#include "arcs_ap.h"
#include "audio_codec_reg.h" // for CODEC registers (AUDIO_CODEC_RegDef)

//=============================== CODEC/AON_CODEC Register Map ===============================

//struct AUD_R0_RSVD_REG0_BITS
//{
//    //volatile uint32_t AUD_REG_RSVD                  : 16; // bit 0~15
//    volatile uint32_t ADC01_DESKEW_CLK_INV          : 1; // bit 0, work in AON_CODEC
//    volatile uint32_t ADC23_DESKEW_CLK_INV          : 1; // bit 1, work in CP_CODEC
//    volatile uint32_t AUD_REG_RSVD                  : 14; // bit 2~15
//    volatile uint32_t RESV_16_31                    : 16; // bit 16~31
//};
//
//union AUD_R0_RSVD_REG0 {
//    volatile uint32_t                     all;
//    struct AUD_R0_RSVD_REG0_BITS          bit;
//};

//struct AUD_R1_GLOBAL0_BITS
//{
//    volatile uint32_t DEBUG_MODE                    : 3; // bit 0~2
//    volatile uint32_t TEST_IN_SEL                   : 3; // bit 3~5
//    volatile uint32_t TEST_OUT_SELB                 : 4; // bit 6~9
//    volatile uint32_t TEST_OUT_SELA                 : 5; // bit 10~14
//    volatile uint32_t ADC_ANACLK_INV                : 1; // bit 15~15
//    volatile uint32_t AUD_DESKEW_CLK_INV            : 1; // bit 16~16
//    volatile uint32_t RESV_17_31                    : 15; // bit 17~31
//};
//
//union AUD_R1_GLOBAL0 {
//    volatile uint32_t                     all;
//    struct AUD_R1_GLOBAL0_BITS            bit;
//};

// R1_GLOBAL0
#define AUD_R1_GLOBAL0_BITS     AUDIO_CODEC_REG_AUD_R1_GLOBAL0_BITS
#define AUD_R1_GLOBAL0          AUDIO_CODEC_REG_AUD_R1_GLOBAL0

/*
struct AUD_R1_GLOBAL0_BITS
{
    volatile uint32_t DEBUG_MODE                    : 3; // bit 0~2
    volatile uint32_t TEST_IN_SEL                   : 3; // bit 3~5
    volatile uint32_t TEST_OUT_SELB                 : 4; // bit 6~9
    volatile uint32_t TEST_OUT_SELA                 : 5; // bit 10~14
    volatile uint32_t AUD_DAC_CLK_INV               : 1; // bit 15~15
    volatile uint32_t AUD_ADC_CLK_LVL_SEL           : 1; // bit 16~16
    volatile uint32_t AUD_ADC_CLK_INV               : 1; // bit 17~17
    volatile uint32_t REG_ADC_ANACLK_INV            : 1; // bit 18~18
    volatile uint32_t RESV_19_31                    : 13; // bit 19~31
};

union AUD_R1_GLOBAL0 {
    volatile uint32_t               all;
    struct AUD_R1_GLOBAL0_BITS      bit;
};
*/


//struct AUD_R2_GLOBAL1_BITS
//{
//    volatile uint32_t AUD_MB0_MILLER                : 1; // bit 0~0
//    volatile uint32_t AUD_MB0_SET                   : 2; // bit 1~2
//    volatile uint32_t AUD_MB1_MILLER                : 1; // bit 3~3
//    volatile uint32_t AUD_MB1_SET                   : 2; // bit 4~5
//    volatile uint32_t AUD_DAC_IREFSEL               : 2; // bit 6~7
//    volatile uint32_t AUD_IREFEN                    : 1; // bit 8~8
//    volatile uint32_t AUD_VMIDR_SEL                 : 1; // bit 9~9
//    volatile uint32_t AUD_VMIDEN                    : 1; // bit 10~10
//    volatile uint32_t RESV_11_31                    : 21; // bit 11~31
//};
//
//#define MB0_MILLER_EXT_CAP          (0 << 0) // bit[0]
//#define MB0_MILLER_NO_EXT_CAP       (1 << 0)
//#define MB0_SET_MASK                (0x3 << 1)
//#define MB0_SET_DISABLE             (0 << 1) // bit[2:1]
//#define MB0_SET_1P8V                (1 << 1)
//#define MB0_SET_2P0V                (2 << 1)
//#define MB0_SET_2P3V                (3 << 1)
//
//
//#define MB1_MILLER_EXT_CAP          (0 << 3) // bit[3]
//#define MB1_MILLER_NO_EXT_CAP       (1 << 3)
//#define MB1_SET_MASK                (0x3 << 4)
//#define MB1_SET_DISABLE             (0 << 4) // bit[5:4]
//#define MB1_SET_1P8V                (1 << 4)
//#define MB1_SET_2P0V                (2 << 4)
//#define MB1_SET_2P3V                (3 << 4)
//
//#define DAC_IREF_1P5UA              (0 << 6) // bit[7:6]
//#define DAC_IREF_2P0UA              (1 << 6)
//#define DAC_IREF_2P5UA              (2 << 6)
//#define DAC_IREF_3P0UA              (3 << 6)
//#define AUD_IREF_DISABLE            (0 << 8) // bit[8]
//#define AUD_IREF_ENABLE             (1 << 8)
//#define AUD_VMID_50KOHM             (0 << 9) // bit[9]
//#define AUD_VMID_200KOHM            (1 << 9)
//#define AUD_VMID_DISABLE            (0 << 10) // bit[10]
//#define AUD_VMID_ENABLE             (1 << 10)
//
//union AUD_R2_GLOBAL1 {
//    volatile uint32_t                     all;
//    struct AUD_R2_GLOBAL1_BITS            bit;
//};

// R2_GLOBAL1
#define AUD_R2_GLOBAL1_BITS     AUDIO_CODEC_REG_AUD_R2_GLOBAL1_BITS
#define AUD_R2_GLOBAL1          AUDIO_CODEC_REG_AUD_R2_GLOBAL1

#define R2_AUD_EN_IREF          (0x1 << 0)
#define R2_AUD_EN_VMID          (0x1 << 1)
#define R2_AUD_EN_MICBIAS       (0x1 << 8)

/*
struct AUD_R2_GLOBAL1_BITS
{
    volatile uint32_t AUD_EN_IREF                   : 1; // bit 0~0
    volatile uint32_t AUD_EN_VMID                   : 1; // bit 1~1
    volatile uint32_t MIC_VOUT_SEL                  : 1; // bit 2~2
    volatile uint32_t MIC_VOUT_TUNE                 : 3; // bit 3~5
    volatile uint32_t EN_MIC_ILOAD                  : 1; // bit 6~6
    volatile uint32_t EN_MIC_CAPLESS                : 1; // bit 7~7
    volatile uint32_t EN_MICBIAS                    : 1; // bit 8~8
    volatile uint32_t RESV_9_31                     : 23; // bit 9~31
};

union AUD_R2_GLOBAL1 {
    volatile uint32_t               all;
    struct AUD_R2_GLOBAL1_BITS      bit;
};
*/


//struct AUD_R5_ADC_CTRL0_BITS
//{
//    volatile uint32_t ADCSR_P                       : 4; // bit 0~3
//    volatile uint32_t ADCOSR_P                      : 3; // bit 4~6
//    volatile uint32_t ADCCLK_EN_P                   : 1; // bit 7~7
//    volatile uint32_t ADC_RESETN_P                  : 1; // bit 8~8
//    volatile uint32_t RPGA_TOEN_P                   : 1; // bit 9~9
//    volatile uint32_t LPGA_TOEN_P                   : 1; // bit 10~10
//    volatile uint32_t AUD_ADC_MCLK_SEL              : 1; // bit 11~11
//    volatile uint32_t ADC_RC_CALI_GO                : 1; // bit 12~12
//    volatile uint32_t ADC_CAP_CALI_GO               : 1; // bit 13~13
//    volatile uint32_t RESV_14_31                    : 18; // bit 14~31
//};
//
//#define BIT_INDEX_ADCCLK_EN     7
//#define BIT_VALUE_ADCCLK_EN     (1 << 7)
//
//union AUD_R5_ADC_CTRL0 {
//    volatile uint32_t                     all;
//    struct AUD_R5_ADC_CTRL0_BITS          bit;
//};


// R5 ADC_CTRL0
#define AUD_R5_ADC_CTRL0_BITS   AUDIO_CODEC_REG_AUD_R5_ADC_CTRL0_BITS
#define AUD_R5_ADC_CTRL0        AUDIO_CODEC_REG_AUD_R5_ADC_CTRL0

#define R5_ADCCLK_EN_POS     7
#define R5_ADCCLK_EN        (1 << 7)

/*
struct AUD_R5_ADC_CTRL0_BITS
{
    volatile uint32_t ADCSR                         : 4; // bit 0~3
    volatile uint32_t ADCOSR                        : 3; // bit 4~6
    volatile uint32_t ADCCLK_EN                     : 1; // bit 7~7
    volatile uint32_t REG_ADC_RSTN                  : 1; // bit 8~8
    volatile uint32_t RPGA_TOEN                     : 1; // bit 9~9
    volatile uint32_t LPGA_TOEN                     : 1; // bit 10~10
    volatile uint32_t AUD_ADC_CLK_SRC               : 1; // bit 11~11
    volatile uint32_t ADC_CAP_CALI_GO               : 1; // bit 12~12
    volatile uint32_t ADC_GAIN_COMP_SEL             : 1; // bit 13~13
    volatile uint32_t RESV_14_31                    : 18; // bit 14~31
};

union AUD_R5_ADC_CTRL0 {
    volatile uint32_t               all;
    struct AUD_R5_ADC_CTRL0_BITS    bit;
};
*/


//struct AUD_R6_ADC_CTRL1_BITS
//{
//    volatile uint32_t ADCVOL_R                      : 7; // bit 0~6
//    volatile uint32_t ADCVOL_L                      : 7; // bit 7~13
//    volatile uint32_t HPF2EN                        : 1; // bit 14~14
//    volatile uint32_t HPF1EN                        : 1; // bit 15~15
//    volatile uint32_t ADC_PGA_LEVEL_R               : 5; // bit 16~20
//    volatile uint32_t ADC_PGA_LEVEL_L               : 5; // bit 21~25
//    volatile uint32_t HPFCUT                        : 3; // bit 26~28
//    volatile uint32_t ADC_SINGLE_CH_MODE            : 1; // bit 29~29
//    volatile uint32_t ADC_HPFOUT_SEL                : 1; // bit 30~30
//    volatile uint32_t RESV_31_31                    : 1; // bit 31~31
//};
//
//#define R6_ADCR_VOL_MASK        (0x7F << 0)
//#define R6_ADCL_VOL_MASK        (0x7F << 7)
//#define R6_ADCR_VOL(n)          (((n) & 0x7F) << 0) // bit[6:0]
//#define R6_ADCL_VOL(n)          (((n) & 0x7F) << 7) // bit[13:7]
//#define R6_ADC_VOL_MIN          0x0     // -83dB
//#define R6_ADC_VOL_MAX          0x7F    // +42dB
//
//#define R6_HPF2_EN              (1 << 14)   // bit[14]
//#define R6_HPF1_EN              (1 << 15)   // bit[15]
//#define R6_ADCR_PGA_MASK        (0x1F << 16)
//#define R6_ADCL_PGA_MASK        (0x1F << 21)
//#define R6_ADCR_PGA_VOL(n)      (((n) & 0x1F) << 16)    // bit[20:16]
//#define R6_ADCL_PGA_VOL(n)      (((n) & 0x1F) << 21)    // bit[25:21]
//#define R6_ADC_PGA_VOL_MIN      0x0     // -12dB
//#define R6_ADC_PGA_VOL_MAX      0x18    // +36dB
//
//#define R6_HPF_CUT(n)           (((n) & 0x7) << 26)     // bit[28:26]
//#define R6_HPF_OUT_SEL          (1 << 30)   // bit[30]
//
//union AUD_R6_ADC_CTRL1 {
//    volatile uint32_t                     all;
//    struct AUD_R6_ADC_CTRL1_BITS          bit;
//};


// R6 ADC_CTRL1
#define AUD_R6_ADC_CTRL1_BITS   AUDIO_CODEC_REG_AUD_R6_ADC_CTRL1_BITS
#define AUD_R6_ADC_CTRL1        AUDIO_CODEC_REG_AUD_R6_ADC_CTRL1

#define R6_ADCR_VOL_MASK        (0x7F << 0)
#define R6_ADCL_VOL_MASK        (0x7F << 7)
#define R6_ADCR_VOL(n)          (((n) & 0x7F) << 0) // bit[6:0]
#define R6_ADCL_VOL(n)          (((n) & 0x7F) << 7) // bit[13:7]
#define R6_ADC_VOL_MIN          0x0     // -83dB
#define R6_ADC_VOL_MAX          0x7F    // +42dB

#define R6_HPF2_EN              (1 << 14)   // bit[14]
#define R6_HPF1_EN              (1 << 15)   // bit[15]
#define R6_ADCR_PGA_MASK        (0x1F << 16)
#define R6_ADCR_PGA_VOL(n)      (((n) & 0x1F) << 16)    // bit[20:16]
#define R6_ADCL_PGA_MASK        (0x1F << 21)
#define R6_ADCL_PGA_VOL(n)      (((n) & 0x1F) << 21)    // bit[25:21]
#define R6_ADC_PGA_VOL_MIN      0x0     // -12dB
#define R6_ADC_PGA_VOL_MAX      0x18    // +36dB
#define R6_HPF_CUT(n)           (((n) & 0x7) << 26)     // bit[28:26]
#define R6_ADC_SINGLE_CH_MODE   (1 << 29)   // bit[29]
#define R6_HPF_OUT_SEL          (1 << 30)   // bit[30]

/*
struct AUD_R6_ADC_CTRL1_BITS
{
    volatile uint32_t ADCVOL_R                      : 7; // bit 0~6
    volatile uint32_t ADCVOL_L                      : 7; // bit 7~13
    volatile uint32_t HPF2EN                        : 1; // bit 14~14
    volatile uint32_t HPF1EN                        : 1; // bit 15~15
    volatile uint32_t ADC_PGA_LEVEL_R               : 5; // bit 16~20
    volatile uint32_t ADC_PGA_LEVEL_L               : 5; // bit 21~25
    volatile uint32_t HPFCUT                        : 3; // bit 26~28
    volatile uint32_t ADC_SINGLE_CH_MODE            : 1; // bit 29~29
    volatile uint32_t ADC_HPFOUT_SEL                : 1; // bit 30~30
    volatile uint32_t RESV_31_31                    : 1; // bit 31~31
};

union AUD_R6_ADC_CTRL1 {
    volatile uint32_t               all;
    struct AUD_R6_ADC_CTRL1_BITS    bit;
};
*/


// R7 ADC_CTRL2
#define AUD_R7_ADC_CTRL2_BITS   AUDIO_CODEC_REG_AUD_R7_ADC_CTRL2_BITS
#define AUD_R7_ADC_CTRL2        AUDIO_CODEC_REG_AUD_R7_ADC_CTRL2

/*
struct AUD_R7_ADC_CTRL2_BITS
{
    volatile uint32_t NFA0                          : 14; // bit 0~13
    volatile uint32_t NFEN                          : 1; // bit 14~14
    volatile uint32_t RESV_15_15                    : 1; // bit 15~15
    volatile uint32_t NFA1                          : 14; // bit 16~29
    volatile uint32_t RESV_30_31                    : 2; // bit 30~31
};

union AUD_R7_ADC_CTRL2 {
    volatile uint32_t               all;
    struct AUD_R7_ADC_CTRL2_BITS    bit;
};
*/


// R8 ADC_CTRL3
#define AUD_R8_ADC_CTRL3_BITS   AUDIO_CODEC_REG_AUD_R8_ADC_CTRL3_BITS
#define AUD_R8_ADC_CTRL3        AUDIO_CODEC_REG_AUD_R8_ADC_CTRL3

#define R8_ERR_TOLERANCE_MIN                0
#define R8_ERR_TOLERANCE_MAX                4 //FIXME: 7?
#define R8_TARGET_LEVEL_MIN                 0
#define R8_TARGET_LEVEL_MAX                 23
#define R8_NGATE_FLOOR_MIN                  0
#define R8_NGATE_FLOOR_MAX                  22
#define R8_ALCMIN_MIN                       0
#define R8_ALCMIN_MAX                       24
#define R8_ALCMAX_MIN                       0
#define R8_ALCMAX_MAX                       24

/*
struct AUD_R8_ADC_CTRL3_BITS
{
    volatile uint32_t ALCMIN                        : 5; // bit 0~4
    volatile uint32_t ALCMAX                        : 5; // bit 5~9
    volatile uint32_t NG                            : 5; // bit 10~14
    volatile uint32_t NG_EN                         : 1; // bit 15~15
    volatile uint32_t ALCSEL_R                      : 1; // bit 16~16
    volatile uint32_t ALCSEL_L                      : 1; // bit 17~17
    volatile uint32_t ALCMODE                       : 1; // bit 18~18
    volatile uint32_t TARGET_R                      : 5; // bit 19~23
    volatile uint32_t TARGET_L                      : 5; // bit 24~28
    volatile uint32_t TOLERANCE                     : 3; // bit 29~31
};

union AUD_R8_ADC_CTRL3 {
    volatile uint32_t               all;
    struct AUD_R8_ADC_CTRL3_BITS    bit;
};
*/


// R9 ADC_CTRL4
#define AUD_R9_ADC_CTRL4_BITS   AUDIO_CODEC_REG_AUD_R9_ADC_CTRL4_BITS
#define AUD_R9_ADC_CTRL4        AUDIO_CODEC_REG_AUD_R9_ADC_CTRL4

#define R9_ALC_DECAY_MIN        0x0
#define R9_ALC_DECAY_MAX        0xF
#define R9_ALC_ATTACK_MIN       0x0
#define R9_ALC_ATTACK_MAX       0xF
#define R9_ALC_HOLD_MIN         0x0
#define R9_ALC_HOLD_MAX         0xF

#define R9_ALC_DECAY(n)         (((n) & 0xF) << 0)
#define R9_ALC_ATTACK(n)        (((n) & 0xF) << 4)
#define R9_ALC_HOLD(n)          (((n) & 0xF) << 8)
#define R9_PEAK_FASTALC_EN      (1 << 12)

// 1: double edge on DMIC0 or DMIC1, 0: single edge on both DMIC0 & DMIC1
#define R9_DMIC_MODE_DBL_EDGE   (1 << 16)
// 1: from DMIC1, 0: from DMIC0 only when DMIC_MODE=1
#define R9_DMIC_SRC_DMIC1       (1 << 21)
#define R9_DMIC_SRC_DMIC0       (0 << 21)
#define R9_DMIC_EN              (1 << 22)

#define R9_AUTORST_TYPE(n)      (((n) & 0x7) << 23)     //0x0~0x5, default 0x1 (256us)
#define R9_AUTORST_EN_R         (1 << 26)
#define R9_AUTORST_EN_L         (1 << 27)

/*
struct AUD_R9_ADC_CTRL4_BITS
{
    volatile uint32_t ALCDCY                        : 4; // bit 0~3
    volatile uint32_t ALCATK                        : 4; // bit 4~7
    volatile uint32_t ALCHLD                        : 4; // bit 8~11
    volatile uint32_t PEAK_FASTALC_EN               : 1; // bit 12~12
    volatile uint32_t RESV_13_15                    : 3; // bit 13~15
    volatile uint32_t DMIC_MODE                     : 1; // bit 16~16
    volatile uint32_t UNCONNECT                     : 2; // bit 17~18
    volatile uint32_t DMIC_LATCH_ADJ                : 2; // bit 19~20
    volatile uint32_t DMIC_SRC                      : 1; // bit 21~21
    volatile uint32_t DMIC_ENABLE                   : 1; // bit 22~22
    volatile uint32_t AUTORST_TYPE                  : 3; // bit 23~25
    volatile uint32_t AUTORST_EN_R                  : 1; // bit 26~26
    volatile uint32_t AUTORST_EN_L                  : 1; // bit 27~27
    volatile uint32_t RESV_28_31                    : 4; // bit 28~31
};

union AUD_R9_ADC_CTRL4 {
    volatile uint32_t                     all;
    struct AUD_R9_ADC_CTRL4_BITS          bit;
};
*/


// R10 ADC_CTR5
#define AUD_R10_ADC_CTRL5_BITS  AUDIO_CODEC_REG_AUD_R10_ADC_CTRL5_BITS
#define AUD_R10_ADC_CTRL5       AUDIO_CODEC_REG_AUD_R10_ADC_CTRL5

#define R10_FILGAIN_REG_MASK    (0xFFFFF << 0)  // bit[19:0]
#define R10_FILGAIN_REG(n)      (((n) & 0xFFFFF) << 0)
#define R10_FILGAIN_REGEN       (0x1 << 20)     // bit[20]

/*
struct AUD_R10_ADC_CTRL5_BITS
{
    volatile uint32_t FILGAIN_REG                   : 20; // bit 0~19
    volatile uint32_t FILGAIN_REGEN                 : 1; // bit 20~20
    volatile uint32_t OFFSET_REG                    : 10; // bit 21~30
    volatile uint32_t OFFSET_REGEN                  : 1; // bit 31~31
};

union AUD_R10_ADC_CTRL5 {
    volatile uint32_t                     all;
    struct AUD_R10_ADC_CTRL5_BITS         bit;
};
*/


//struct AUD_R11_ADC_CTRL6_BITS
//{
//    volatile uint32_t ADC_EN_R                      : 1; // bit 0~0
//    volatile uint32_t ADC_EN_L                      : 1; // bit 1~1
//    volatile uint32_t AUD_ADC_LP_RST                : 1; // bit 2~2
//    volatile uint32_t AUD_ADC_IB_CTRL               : 4; // bit 3~6
//    volatile uint32_t AUD_ADC_IDAC_CTRL             : 2; // bit 7~8
//    volatile uint32_t AUD_ADC_RC_CTRL               : 5; // bit 9~13
//    volatile uint32_t RVAL_ADC_QUAR_COV_EN          : 1; // bit 14~14
//    volatile uint32_t RCTRL_ADC_RC_CALI             : 1; // bit 15~15
//    volatile uint32_t RVAL_ADC_QUAR_COV             : 6; // bit 16~21
//    volatile uint32_t RSET_ADC_RC_CALI              : 1; // bit 22~22
//    volatile uint32_t RVAL_ADC_CAP_CALI_EN          : 1; // bit 23~23
//    volatile uint32_t RCTRL_ADC_CAP_CALI            : 1; // bit 24~24
//    volatile uint32_t RVAL_ADC_CAP_CALI             : 5; // bit 25~29
//    volatile uint32_t RSET_ADC_CAP_CALI             : 1; // bit 30~30
//    volatile uint32_t RESV_31_31                    : 1; // bit 31~31
//};
//
//#define R11_ADCL_EN             (1 << 1)
//#define R11_ADCR_EN             (1 << 0)
//#define R11_ADCLR_EN            (R11_ADCL_EN | R11_ADCR_EN)
//
//#define R11_ADC_IB_CTRL_MASK    (0xF << 3) // bit[6:3]
//#define R11_ADC_IB_CTRL(n)      (((n) & 0xF) << 3) // bit[6:3]
//#define IB_CTRL_VAL_DEF         0x8
////4M:0xE Min Power, 12M:0x2, Sub Max Power, make sure to work under limiting conditions
//#define IB_CTRL_VAL_12MHZ       0x2
//#define IB_CTRL_VAL_LP          0xE
//
//#define R11_ADC_IDAC_CTRL_MASK  (0x3 << 7) // bit[8:7]
//#define R11_ADC_IDAC_CTRL(n)    (((n) & 0x3) << 7) // bit[8:7]
//#define R11_ADC_IDAC_7P5UA_5PERM    (0x0 << 7)
//#define R11_ADC_IDAC_7P5UA          (0x1 << 7)
//#define R11_ADC_IDAC_7P5UA_6PERP    (0x2 << 7)
//#define R11_ADC_IDAC_7P5UA_13PERP   (0x3 << 7)
//
//#define R11_ADC_CBIAS_CUR_10UA  (1 << 9) // bit[9]: 1 = 10uA, 0 = 20uA
//#define R11_ADC_CBIAS_CUR_20UA  (0 << 9)
//#define R11_ADC_RCCTRL_12MHZ    (1 << 10) // bit[10]: 1 = 12MHZ, 0 = 4MHZ
//#define R11_ADC_RCCTRL_4MHZ     (0 << 10)
//
//#define R11_ADC_QUAR_COV_MASK   (0x3F << 16) //bit[21:16]
//#define R11_ADC_QUAR_COV(n)     (((n) & 0x3F) << 16) //bit[21:16]
//#define R11_ADC_RSET_RC_CALI    (1 << 22)
//#define R11_ADC_QUAR_COV_EN     (1 << 14)
//
//union AUD_R11_ADC_CTRL6 {
//    volatile uint32_t                     all;
//    struct AUD_R11_ADC_CTRL6_BITS         bit;
//};

// R11 ADC_CTRL6
#define AUD_R11_ADC_CTRL6_BITS  AUDIO_CODEC_REG_AUD_R11_ADC_CTRL6_BITS
#define AUD_R11_ADC_CTRL6       AUDIO_CODEC_REG_AUD_R11_ADC_CTRL6

#define R11_ADCR_EN              (0x1 << 0) // bit[0]: enable Right ADC (ADC1)
#define R11_ADCL_EN              (0x1 << 1) // bit[1]: enable Left ADC (ADC0)
#define R11_ADCLR_EN             (0x3 << 0) // bit[1:0]: enable both LR ADC (ADC0 & ADC1)

#define R11_ADCR_ANA_RST        (0x1 << 2) // bit[2], [RW] Right ADC analog path reset
#define R11_ADCL_ANA_RST        (0x1 << 3) // bit[3], [RW] Left ADC analog path reset
#define R11_ADC_ANA_RST         (0x1 << 4) // bit[4], [RW] //FIXME:
#define R11_ADC_LP_AUTORST_SPLIT    (0x1 << 4) // bit[5] //FIXME: what's it for��

#define R11_ADC_IB_CTRL_MASK    (0x3 << 6) // bit[7:6], IB=IBIAS
#define R11_ADC_IB_CTRL(n)      (((n) & 0x3) << 6)
#define IB_CTRL_1P5UA           0x0
#define IB_CTRL_2UA             0x1
#define IB_CTRL_2P5UA           0x2
#define IB_CTRL_3UA             0x3
#define IB_CTRL_VAL_DEF         IB_CTRL_3UA // IB_CTRL_2UA
#define IB_CTRL_VAL_12MHZ       IB_CTRL_2P5UA // make sure to work under limiting conditions
#define IB_CTRL_VAL_LP          IB_CTRL_1P5UA //FIXME:

#define R11_ADC_IDAC_CTRL_MASK  (0x3 << 8) // bit[9:8]
#define R11_ADC_IDAC_CTRL(n)    (((n) & 0x3) << 8)
#define IDAC_7P5UA_M5P          (0x0)
#define IDAC_7P5UA              (0x1)
#define IDAC_7P5UA_P6P          (0x2)
#define IDAC_7P5UA_P13P         (0x3)

#define R11_ADC_CAP_CALI_EN_SRC_MASK    (0x1 << 10) // bit[10]
#define R11_ADC_CAP_CALI_EN_SRC_REG     (0x1 << 10) // enable from register
#define R11_ADC_CAP_CALI_EN_SRC_ISM     (0x0 << 10) // enable from internal state machine?
#define R11_ADC_CAP_CALI_EN_REG_MASK    (0x1 << 11) // bit[11]
#define R11_ADC_CAP_CALI_EN_REG_ENA     (0x1 << 11) // enable
#define R11_ADC_CAP_CALI_EN_REG_DIS     (0x0 << 11) // disable

#define R11_ADC_CAP_CALI_REGVAL_MASK    (0x1F << 12) // bit[16:12]
#define R11_ADC_CAP_CALI_REGVAl(n)      (((n) & 0x1F) << 12)
#define R11_ADC_CAP_CALI_SRC_MASK       (0x1 << 17)
#define R11_ADC_CAP_CALI_SRC_REG        (0x1 << 17) // CALI setting from register
#define R11_ADC_CAP_CALI_SRC_ISM        (0x0 << 17) // CALI setting from internal state machine

/*
struct AUD_R11_ADC_CTRL6_BITS
{
    volatile uint32_t REG_AUD_EN_ADC1               : 1; // bit 0~0
    volatile uint32_t REG_AUD_EN_ADC0               : 1; // bit 1~1
    volatile uint32_t ANA_ADC1_RST_REG              : 1; // bit 2~2
    volatile uint32_t ANA_ADC0_RST_REG              : 1; // bit 3~3
    volatile uint32_t ANA_ADC_RST_REG               : 1; // bit 4~4
    volatile uint32_t ADC_LP_AUTORST_SPLIT          : 1; // bit 5~5
    volatile uint32_t AUD_ADC_IB_CTRL               : 2; // bit 6~7
    volatile uint32_t AUD_ADC_IDAC_CTRL             : 2; // bit 8~9
    volatile uint32_t ADC_CAP_CALI_EN_SRC           : 1; // bit 10~10
    volatile uint32_t ADC_CAP_CALI_EN_REG           : 1; // bit 11~11
    volatile uint32_t ADC_CAP_CALI_REG              : 5; // bit 12~16
    volatile uint32_t ADC_CAP_CALI_SRC              : 1; // bit 17~17
    volatile uint32_t RESV_18_31                    : 14; // bit 18~31
};

union AUD_R11_ADC_CTRL6 {
    volatile uint32_t               all;
    struct AUD_R11_ADC_CTRL6_BITS   bit;
};
*/


//struct AUD_R12_ADC_CTRL7_BITS
//{
//    volatile uint32_t AUD_PGABUFEN                  : 1; // bit 0~0
//    volatile uint32_t AUD_PGABUF_LP                 : 1; // bit 1~1
//    volatile uint32_t RPGA_EN                       : 1; // bit 2~2
//    volatile uint32_t AUD_RPGA_MUTE                 : 1; // bit 3~3
//    volatile uint32_t AUD_RPGA_EN_SINGLE            : 1; // bit 4~4
//    volatile uint32_t AUD_RPGA_LPR                  : 1; // bit 5~5
//    volatile uint32_t RPGA_ZCEN_P                   : 1; // bit 6~6
//    volatile uint32_t LPGA_EN                       : 1; // bit 7~7
//    volatile uint32_t AUD_LPGA_MUTE                 : 1; // bit 8~8
//    volatile uint32_t AUD_LPGA_EN_SINGLE            : 1; // bit 9~9
//    volatile uint32_t AUD_LPGA_LPR                  : 1; // bit 10~10
//    volatile uint32_t LPGA_ZCEN_P                   : 1; // bit 11~11
//    volatile uint32_t AUD_ADC_ATB                   : 3; // bit 12~14
//    volatile uint32_t AUD_ADC_IDAC_OFFSET           : 2; // bit 15~16
//    volatile uint32_t AUD_ADC_VREF_EN               : 1; // bit 17~17
//    volatile uint32_t AUD_ADC_VREF_MODE             : 1; // bit 18~18
//    volatile uint32_t AUD_ADC_VREF_CTRL             : 2; // bit 19~20
//    volatile uint32_t AUD_ADC_IDAC_TRIM             : 3; // bit 21~23
//    volatile uint32_t RVAL_AUD_IDAC_BIAS            : 1; // bit 24~24
//    volatile uint32_t RSET_AUD_IDAC_BIAS            : 1; // bit 25~25
//    volatile uint32_t RESV_26_31                    : 6; // bit 26~31
//};
//
//#define R12_PGA_BUF_EN                  (1 << 0)
//#define R12_PGA_BUF_LP                  (1 << 1)
//
//#define R12_RPGA_EN                     (1 << 2)
//#define R12_RPGA_MUTE                   (1 << 3)
//#define R12_RPGA_SINGLE                 (1 << 4)
//#define R12_RPGA_LP                     (1 << 5)
//#define R12_RPGA_ZCEN                   (1 << 6)
//
//#define R12_LPGA_EN                     (1 << 7)
//#define R12_LPGA_MUTE                   (1 << 8)
//#define R12_LPGA_SINGLE                 (1 << 9)
//#define R12_LPGA_LP                     (1 << 10)
//#define R12_LPGA_ZCEN                   (1 << 11)
//
//#define R12_ADC_ATB(n)                  (((n) & 0x7) << 12) // bit[14:12], 0~4, default 0
//#define R12_ADC_IDAC_OFFSET             (((n) & 0x3) << 15) // bit[16:15], 0~3, default 0
//#define R12_ADC_VREF_EN                 (1 << 17)
////#define R12_ADC_VREF_MODE
//#define R12_ADC_VREF_40UA               (1 << 19) // bit[19], 1=40uA, 0=20uA
//#define R12_ADC_VREF_20UA               (0 << 19)
//#define R12_ADC_VCM_BIAS_30UA           (1 << 20) // bit[20], 1=30uA, 0=10uA
//#define R12_ADC_VCM_BIAS_10UA           (0 << 20)
//#define R12_ADC_IDAC_TRIM(n)            (((n) & 0x7) << 21) // bit[23:21], default 000b
////#define R12_IDAC_BIAS_VAL
//#define R12_IDAC_BIAS_EN                (1 << 25) // bit[25]
//
//union AUD_R12_ADC_CTRL7 {
//    volatile uint32_t                     all;
//    struct AUD_R12_ADC_CTRL7_BITS         bit;
//};


// R12 ADC_CTRL7
#define AUD_R12_ADC_CTRL7_BITS  AUDIO_CODEC_REG_AUD_R12_ADC_CTRL7_BITS
#define AUD_R12_ADC_CTRL7       AUDIO_CODEC_REG_AUD_R12_ADC_CTRL7

#define R12_RPGA_VCMBUF_EN              (1 << 0) // bit[0]: Enable ADC PGA1 VCM Buffer
#define R12_LPGA_VCMBUF_EN              (1 << 1) // bit[1]: Enable ADC PGA0 VCM Buffer

#define R12_RPGA_EN                     (1 << 2) // bit[2]: Enable ADC PGA1
#define R12_RPGA_MUTE                   (1 << 3) // bit[3]: Mute ADC PGA1
#define R12_RPGA_SINGLE                 (1 << 4) // bit[4]: ADC PGA1 input mode: Single
#define R12_RPGA_DIFF                   (0 << 4) // bit[4]: ADC PGA1 input mode: Differential
#define R12_PGA_VCOM_SEL_VMID_75PER     (1 << 5) // bit[5]: ADC PGA VCOM Select: VMID*0.75
#define R12_PGA_VCOM_SEL_VMID           (0 << 5) // bit[5]: ADC PGA VCOM Select: VMID (Default)
#define R12_RPGA_ZCEN                   (1 << 6) // bit[6]: Enable ADC PGA1 Zero Crossing
#define R12_RPGA_ZCEN_REG               (1 << 7) // bit[7]: Enable ADC PGA1 Zero Crossing REG
#define R12_RPGA_ZCEN_FORCE             (1 << 8) // bit[8]: Enable ADC PGA1 Zero Crossing FORCE

#define R12_LPGA_EN                     (1 << 9) // bit[9]: Enable ADC PGA0
#define R12_LPGA_MUTE                   (1 << 10) // bit[10]: Mute ADC PGA0
#define R12_LPGA_SINGLE                 (1 << 11) // bit[11]: ADC PGA0 input mode: Single
#define R12_LPGA_DIFF                   (0 << 11) // bit[11]: ADC PGA0 input mode: Differential
#define R12_PGA_LP                      (1 << 12) // bit[12]: ADC PGA power mode: Low Power
#define R12_PGA_NORM                    (0 << 12) // bit[12]: ADC PGA power mode: Normal
#define R12_LPGA_ZCEN                   (1 << 13) // bit[13]: Enable ADC PGA0 Zero Crossing
#define R12_LPGA_ZCEN_REG               (1 << 14) // bit[14]: Enable ADC PGA1 Zero Crossing REG
#define R12_LPGA_ZCEN_FORCE             (1 << 15) // bit[15]: Enable ADC PGA1 Zero Crossing FORCE

#define R12_ADC_ATB_CTRL(n)             (((n) & 0x3) << 16) // bit[17:16], 0~2, default 0
#define R12_ADC_SAR_TEST_EN             (0x1 << 18) // bit[18]: Enable ADC SAR TEST
#define R12_ADC_SAR_DELAY_MASK          (0x7 << 19) // bit[21:19]: ADC SAR Delay Control
#define R12_ADC_SAR_DELAY_CTRL(n)        (((n) & 0x7) << 19) // 0x0~0x7: 6/7/8/9.5(default)/12/14/16/19ns
#define R12_ADC_SAR_COMP_LP             (0x1 << 22) // bit[22]: ADC SAR Comparator Low Power, 1 = Low Current
#define R12_ADC_INT2_LP                 (0x1 << 23) // bit[23]: ADC INTegrator 2 Low Power, 1 = Low Current (default)
#define R12_ADC_INT1_LP                 (0x1 << 24) // bit[24]: ADC INTegrator 1 Low Power, 1 = Low Current (default)

#define R12_ADC_IDAC_OS_MASK            (0x3 << 25) // bit[26:25]
#define R12_ADC_IDAC_OS_CTRL(n)         (((n) & 0x3) << 25)
#define IDAC_OS_0MV                 (0x0) //0MV
#define IDAC_OS_20MV                (0x1) //20MV
#define IDAC_OS_40MV                (0x2) //40MV
#define IDAC_OS_80MV                (0x3) //80MV

#define R12_IDAC_BIAS_SRC               (0x1 << 27) // bit[27], 1=from register value?
#define R12_IDAC_BIAS_REG               (0x1 << 28) // bit[28], register value?

#define R12_ADCL_VREF_EN                (0x1 << 29) // bit[29], Enable ADC0 (Left) VREF
#define R12_ADCR_VREF_EN                (0x1 << 30) // bit[30], Enable ADC1 (Right) VREF
#define R12_ADC_MODE_12MHZ              (0x1 << 31) // bit[31], 12MHz mode
#define R12_ADC_MODE_4MHZ               (0x0 << 31) // bit[31], 4MHz mode

/*
struct AUD_R12_ADC_CTRL7_BITS
{
    volatile uint32_t AUD_EN_PGA1_VCMBUF            : 1; // bit 0~0
    volatile uint32_t AUD_EN_PGA0_VCMBUF            : 1; // bit 1~1
    volatile uint32_t REG_AUD_EN_PGA1               : 1; // bit 2~2
    volatile uint32_t AUD_PGA1_MUTE                 : 1; // bit 3~3
    volatile uint32_t AUD_EN_PGA1_SINGLE            : 1; // bit 4~4
    volatile uint32_t AUD_PGA_VCOM_SEL              : 1; // bit 5~5
    volatile uint32_t RPGA_ZCEN_REG                 : 1; // bit 6~6
    volatile uint32_t REG_AUD_EN_PGA0               : 1; // bit 7~7
    volatile uint32_t AUD_PGA0_MUTE                 : 1; // bit 8~8
    volatile uint32_t AUD_EN_PGA0_SINGLE            : 1; // bit 9~9
    volatile uint32_t AUD_PGA_LPR                   : 1; // bit 10~10
    volatile uint32_t LPGA_ZCEN_REG                 : 1; // bit 11~11
    volatile uint32_t AUD_ADC_ATB_CTRL              : 2; // bit 12~13
    volatile uint32_t AUD_ADC_SAR_TEST_EN           : 1; // bit 14~14
    volatile uint32_t AUD_ADC_SAR_DELAY_CTRL        : 3; // bit 15~17
    volatile uint32_t AUD_ADC_SAR_COMP_LPR          : 1; // bit 18~18
    volatile uint32_t AUD_ADC_INT2_LPR              : 1; // bit 19~19
    volatile uint32_t AUD_ADC_INT1_LPR              : 1; // bit 20~20
    volatile uint32_t AUD_ADC_IDAC_OS_CTRL          : 2; // bit 21~22
    volatile uint32_t AUD_IDAC_BIAS_SRC             : 1; // bit 23~23
    volatile uint32_t AUD_IDAC_BIAS_REG             : 1; // bit 24~24
    volatile uint32_t AUD_EN_ADC0_VREF              : 1; // bit 25~25
    volatile uint32_t AUD_EN_ADC1_VREF              : 1; // bit 26~26
    volatile uint32_t AUD_ADC_MODE                  : 1; // bit 27~27
    volatile uint32_t RESV_28_31                    : 4; // bit 28~31
};

union AUD_R12_ADC_CTRL7 {
    volatile uint32_t               all;
    struct AUD_R12_ADC_CTRL7_BITS   bit;
};
*/


//struct AUD_R25_STATUS0_BITS
//{
//    volatile const uint32_t ADC_QUAR_COV_CALI_FLAG_INST   : 1; // bit 0~0
//    volatile const uint32_t ADC_RC_CALI_DONE              : 1; // bit 1~1
//    volatile const uint32_t ADC_RC_CALI_FAIL              : 1; // bit 2~2
//    volatile const uint32_t ADC_CAP_COV_CALI_FLAG_INST    : 1; // bit 3~3
//    volatile const uint32_t ADC_CAP_CALI_DONE             : 1; // bit 4~4
//    volatile const uint32_t ADC_CAP_CALI_FAIL             : 1; // bit 5~5
//    volatile const uint32_t ADC_QUAR_COV                  : 6; // bit 6~11
//    volatile const uint32_t ADC_CAP_CALI                  : 5; // bit 12~16
//    volatile const uint32_t RESV_17_31                    : 15; // bit 17~31
//};
//
//union AUD_R25_STATUS0 {
//    volatile const  uint32_t              all;
//    struct AUD_R25_STATUS0_BITS           bit;
//};


// R25 STATUS0
#define AUD_R25_STATUS0_BITS    AUDIO_CODEC_REG_AUD_R25_STATUS0_BITS
#define AUD_R25_STATUS0         AUDIO_CODEC_REG_AUD_R25_STATUS0

/*
struct AUD_R25_STATUS0_BITS
{
    volatile uint32_t AUD_ADC_CAP_CALI_FLAG_INST    : 1; // bit 0~0
    volatile uint32_t ADC_CAP_CALI_DONE             : 1; // bit 1~1
    volatile uint32_t ADC_CAP_CALI_FAIL             : 1; // bit 2~2
    volatile uint32_t AUD_ADC_CAP_CALI              : 5; // bit 3~7
    volatile uint32_t RESV_8_31                     : 24; // bit 8~31
};

union AUD_R25_STATUS0 {
    volatile uint32_t               all;
    struct AUD_R25_STATUS0_BITS     bit;
};
*/

//----------------------------------------------------------------------
typedef struct
{
    volatile uint32_t                     REG_RSVD0; // 0x000
    union AUD_R1_GLOBAL0                  REG_AUD_R1_GLOBAL0; // 0x004
    union AUD_R2_GLOBAL1                  REG_AUD_R2_GLOBAL1; // 0x008
    volatile uint32_t                     REG_RSVD1[2];
    union AUD_R5_ADC_CTRL0                REG_AUD_R5_ADC_CTRL0; // 0x014
    union AUD_R6_ADC_CTRL1                REG_AUD_R6_ADC_CTRL1; // 0x018
    union AUD_R7_ADC_CTRL2                REG_AUD_R7_ADC_CTRL2; // 0x01C
    union AUD_R8_ADC_CTRL3                REG_AUD_R8_ADC_CTRL3; // 0x020
    union AUD_R9_ADC_CTRL4                REG_AUD_R9_ADC_CTRL4; // 0x024
    union AUD_R10_ADC_CTRL5               REG_AUD_R10_ADC_CTRL5; // 0x028
    union AUD_R11_ADC_CTRL6               REG_AUD_R11_ADC_CTRL6; // 0x02C
    union AUD_R12_ADC_CTRL7               REG_AUD_R12_ADC_CTRL7; // 0x030
    volatile uint32_t                     REG_RSVD2[10];
    union AUD_R25_STATUS0                 REG_AUD_R25_STATUS0; // 0x05C
} CSK_ADC_PDM_RegDef;

#define CSK_ADC01               ((CSK_ADC_PDM_RegDef *)AP_CODEC_BASE)

//----------------------------------------------------------------------

//struct AUD_R15_DAC_CTRL0_BITS
//{
//    volatile uint32_t DACSR_P                       : 4; // bit 0~3
//    volatile uint32_t DACOSR_P                      : 1; // bit 4~4
//    volatile uint32_t DACCLK_EN_P                   : 1; // bit 5~5
//    volatile uint32_t DAC_RSTN_P                    : 1; // bit 6~6
//    volatile uint32_t DACR_TOEN_P                   : 1; // bit 7~7
//    volatile uint32_t DACL_TOEN_P                   : 1; // bit 8~8
//    volatile uint32_t AUD_DAC_MCLK_SEL              : 1; // bit 9~9
//    volatile uint32_t RESV_10_31                    : 22; // bit 10~31
//};
//
//#define R15_DACCLK_EN           (1 << 5)
//#define R15_DAC_RELEASE         (1 << 6)
//#define R15_DACR_ZCTO_EN        (1 << 7)
//#define R15_DACL_ZCTO_EN        (1 << 8)
//
//#define R15_DAC_MCLK_24MHZ      (0 << 9)
//#define R15_DAC_MCLK_22P05MHZ   (1 << 9)
//
//union AUD_R15_DAC_CTRL0 {
//    volatile uint32_t                     all;
//    struct AUD_R15_DAC_CTRL0_BITS         bit;
//};

#define AUD_R15_DAC_CTRL0_BITS  AUDIO_CODEC_REG_AUD_R15_DAC_CTRL0_BITS
#define AUD_R15_DAC_CTRL0       AUDIO_CODEC_REG_AUD_R15_DAC_CTRL0

#define R15_DACCLK_EN           (1 << 5)
#define R15_DAC_RELEASE         (1 << 6)
#define R15_DAC_ZCTO_EN         (1 << 7)
//#define R15_DAC_MCLK_24MHZ      (0 << 8)
//#define R15_DAC_MCLK_22P05MHZ   (1 << 8)

#define R15_DAC_IB_CTRL_MASK    (0x3 << 9) // bit[10:9], IB=IBIAS
#define R15_DAC_IB_CTRL(n)      (((n) & 0x3) << 9)
#define DAC_IB_CTRL_1P5UA       0x0
#define DAC_IB_CTRL_2UA         0x1
#define DAC_IB_CTRL_2P5UA       0x2
#define DAC_IB_CTRL_3UA         0x3
#define DAC_IB_CTRL_VAL_DEF     DAC_IB_CTRL_2UA
//#define DAC_IB_CTRL_VAL_12MHZ   DAC_IB_CTRL_2P5UA //FIXME:
#define DAC_IB_CTRL_VAL_LP      DAC_IB_CTRL_1P5UA //FIXME:

/*
struct AUD_R15_DAC_CTRL0_BITS
{
    volatile uint32_t DACSR                         : 4; // bit 0~3
    volatile uint32_t DACOSR                        : 1; // bit 4~4
    volatile uint32_t DACCLK_EN                     : 1; // bit 5~5
    volatile uint32_t REG_DAC_RSTN                  : 1; // bit 6~6
    volatile uint32_t DAC_TOEN                      : 1; // bit 7~7
    volatile uint32_t AUD_DAC_MCLK_SRC              : 1; // bit 8~8
    volatile uint32_t AUD_DAC_IB_CTRL               : 2; // bit 9~10
    volatile uint32_t RESV_11_31                    : 21; // bit 11~31
};

union AUD_R15_DAC_CTRL0 {
    volatile uint32_t               all;
    struct AUD_R15_DAC_CTRL0_BITS   bit;
};
*/


//struct AUD_R16_DAC_CTRL1_BITS
//{
//    volatile uint32_t DAC_GAIN_R                    : 8; // bit 0~7
//    volatile uint32_t DAC_GAIN_L                    : 8; // bit 8~15
//    volatile uint32_t RESV_16_31                    : 16; // bit 16~31
//};
//
//// 0x70:-113dB, 0xe1: 0dB, 0xFF:+30dB, 1dB each step
//#define R16_GAIN_R_MASK      (0xFF << 0) // bit[7:0] for digital gain of right channel
//#define R16_GAIN_L_MASK      (0xFF << 8) // bit[15:8] for digital gain of left channel
//#define R16_GAIN_R(n)      (((n) & 0xFF) << 0)
//#define R16_GAIN_L(n)      (((n) & 0xFF) << 8)
//#define R16_GAIN_MIN       0x70
//#define R16_GAIN_MAX       0xFF
//
//union AUD_R16_DAC_CTRL1 {
//    volatile uint32_t                     all;
//    struct AUD_R16_DAC_CTRL1_BITS         bit;
//};

#define AUD_R16_DAC_CTRL1_BITS  AUDIO_CODEC_REG_AUD_R16_DAC_CTRL1_BITS
#define AUD_R16_DAC_CTRL1       AUDIO_CODEC_REG_AUD_R16_DAC_CTRL1

// 0x70:-113dB, 0xe1: 0dB, 0xFF:+30dB, 1dB each step
#define R16_GAIN_MASK       (0xFF << 0) // bit[7:0] for digital gain
#define R16_GAIN(n)         (((n) & 0xFF) << 0)
#define R16_GAIN_MIN        0x70
#define R16_GAIN_MAX        0xFF

/*
struct AUD_R16_DAC_CTRL1_BITS
{
    volatile uint32_t DAC_GAIN                      : 8; // bit 0~7
    volatile uint32_t RESV_8_31                     : 24; // bit 8~31
};

union AUD_R16_DAC_CTRL1 {
    volatile uint32_t               all;
    struct AUD_R16_DAC_CTRL1_BITS   bit;
};
*/


//struct AUD_R17_DAC_CTRL2_BITS
//{
//    volatile uint32_t SOFT_SPEED                    : 4; // bit 0~3
//    volatile uint32_t SOFTMUTE_EN                   : 1; // bit 4~4
//    volatile uint32_t DACR_DWAEN                    : 1; // bit 5~5
//    volatile uint32_t DACL_DWAEN                    : 1; // bit 6~6
//    volatile uint32_t DACMU_R                       : 1; // bit 7~7
//    volatile uint32_t DACMU_L                       : 1; // bit 8~8
//    volatile uint32_t DACR_INV_BF_SDM               : 1; // bit 9~9
//    volatile uint32_t DACL_INV_BF_SDM               : 1; // bit 10~10
//    volatile uint32_t RESV_11_31                    : 21; // bit 11~31
//};
//
//#define R17_SOFT_MUTE_SPEED(n)      (((n) & 0xF) << 0)  // bit[3:0] more bigger, more slower
//#define R17_SOFT_MUTE_DISABLE       (0 << 4)
//#define R17_SOFT_MUTE_ENABLE        (1 << 4)            // bit[4]
//#define R17_DACR_DWA_DISABLE        (0 << 5)
//#define R17_DACR_DWA_ENABLE         (1 << 5)            // bit[5]
//#define R17_DACL_DWA_DISABLE        (0 << 6)
//#define R17_DACL_DWA_ENABLE         (1 << 6)            // bit[6]
//#define R17_DACR_UNMUTE             (0 << 7)
//#define R17_DACR_MUTE               (1 << 7)            // bit[7]
//#define R17_DACL_UNMUTE             (0 << 8)
//#define R17_DACL_MUTE               (1 << 8)            // bit[8]
//#define R17_DACR_DATA_INV           (1 << 9)            // bit[9]
//#define R17_DACL_DATA_INV           (1 << 10)           // bit[10]
//
//union AUD_R17_DAC_CTRL2 {
//    volatile uint32_t                     all;
//    struct AUD_R17_DAC_CTRL2_BITS         bit;
//};

#define AUD_R17_DAC_CTRL2_BITS  AUDIO_CODEC_REG_AUD_R17_DAC_CTRL2_BITS
#define AUD_R17_DAC_CTRL2       AUDIO_CODEC_REG_AUD_R17_DAC_CTRL2

#define R17_SOFT_MUTE_SPEED(n)      (((n) & 0xF) << 0)  // bit[3:0] more bigger, more slower
#define R17_SOFT_MUTE_DISABLE       (0 << 4)
#define R17_SOFT_MUTE_ENABLE        (1 << 4)            // bit[4]
#define R17_DAC_DWA_DISABLE         (0 << 5)
#define R17_DAC_DWA_ENABLE          (1 << 5)            // bit[5]
#define R17_DAC_UNMUTE              (0 << 6)
#define R17_DAC_MUTE                (1 << 6)            // bit[6]
#define R17_DAC_DATA_INV            (1 << 7)            // bit[7]

/*
struct AUD_R17_DAC_CTRL2_BITS
{
    volatile uint32_t SOFT_SPEED                    : 4; // bit 0~3
    volatile uint32_t SOFTMUTE_EN                   : 1; // bit 4~4
    volatile uint32_t DAC_DWAEN                     : 1; // bit 5~5
    volatile uint32_t DACMU                         : 1; // bit 6~6
    volatile uint32_t DAC_INV_BF_SDM                : 1; // bit 7~7
    volatile uint32_t RESV_8_31                     : 24; // bit 8~31
};

union AUD_R17_DAC_CTRL2 {
    volatile uint32_t               all;
    struct AUD_R17_DAC_CTRL2_BITS   bit;
};
*/


//struct AUD_R18_DAC_CTRL3_BITS
//{
//    volatile uint32_t DAC_DITH_TYPE                 : 2; // bit 0~1
//    volatile uint32_t DAC_DITH_BYPASS               : 1; // bit 2~2
//    volatile uint32_t DAC_SD_NZ                     : 1; // bit 3~3
//    volatile uint32_t DAC_SD_RSTN                   : 1; // bit 4~4
//    volatile uint32_t DAC_SD_AMUTE_TYPE             : 3; // bit 5~7
//    volatile uint32_t DAC_SD_AMUTE_EN_R             : 1; // bit 8~8
//    volatile uint32_t DAC_SD_AMUTE_EN_L             : 1; // bit 9~9
//    volatile uint32_t DAC_SD_LEVEL_SEL              : 1; // bit 10~10
//    volatile uint32_t DAC_DITH_NTF_EN               : 1; // bit 11~11
//    volatile uint32_t DAC_DWA_TYPE                  : 3; // bit 12~14
//    volatile uint32_t RESV_15_31                    : 17; // bit 15~31
//};
//
//#define R18_DAC_DITH_BYPASS             (1 << 2)    // bit[2]
//#define R18_DAC_SDM_RESET               (0 << 4)    // bit[4]
//#define R18_DAC_SDM_RELEASE             (1 << 4)
//#define R18_DAC_SDM_AMUTE_THRESHOLD(n)  (((n) & 0x7) << 5)  // bit[7:5], 0 =< n < 6
//#define R18_DAC_SDM_AMUTE_MAX           5
//#define R18_DACR_SDM_AMUTE_EN           (1 << 8)    // bit[8]
//#define R18_DACL_SDM_AMUTE_EN           (1 << 9)    // bit[9]
//#define R18_DAC_SDM_LEVEL_14            (0 << 10)   // bit[10]
//#define R18_DAC_SDM_LEVEL_15            (1 << 10)
//#define R18_DAC_DITH_NTF_EN             (1 << 11)   // bit[11]
////#define R18_DAC_DWA_TYPE_DEF            (1 << 12)   // bit[14:12]
//
//union AUD_R18_DAC_CTRL3 {
//    volatile uint32_t                     all;
//    struct AUD_R18_DAC_CTRL3_BITS         bit;
//};

#define AUD_R18_DAC_CTRL3_BITS  AUDIO_CODEC_REG_AUD_R18_DAC_CTRL3_BITS
#define AUD_R18_DAC_CTRL3       AUDIO_CODEC_REG_AUD_R18_DAC_CTRL3

#define R18_DAC_DITH_BYPASS             (1 << 2)    // bit[2]
#define R18_DAC_SDM_RESET               (0 << 4)    // bit[4]
#define R18_DAC_SDM_RELEASE             (1 << 4)
#define R18_DAC_SDM_AMUTE_THRESHOLD(n)  (((n) & 0x7) << 5)  // bit[7:5], 0 =< n < 6
#define R18_DAC_SDM_AMUTE_MAX           5
#define R18_DAC_SDM_AMUTE_EN            (1 << 8)    // bit[8]
#define R18_DAC_DITH_NTF_EN             (1 << 10)   // bit[10]
#define R18_DAC_DWA_TYPE_MASK           (0x7 << 11)   // bit[13:11]
#define R18_DAC_DWA_TYPE(n)             (((n) & 0x7) << 11)
#define DAC_DWA_TYPE_MAX                0x5
#define DAC_DWA_TYPE_DEF                0x1

/*
struct AUD_R18_DAC_CTRL3_BITS
{
    volatile uint32_t DAC_DITH_TYPE                 : 2; // bit 0~1
    volatile uint32_t DAC_DITH_BYPASS               : 1; // bit 2~2
    volatile uint32_t DAC_SD_NZ                     : 1; // bit 3~3
    volatile uint32_t DAC_SD_RSTN                   : 1; // bit 4~4
    volatile uint32_t DAC_SD_AMUTE_TYPE             : 3; // bit 5~7
    volatile uint32_t DAC_SD_AMUTE_EN               : 1; // bit 8~8
    volatile uint32_t DAC_SD_LEVEL_SEL              : 1; // bit 9~9
    volatile uint32_t DAC_DITH_NTF_EN               : 1; // bit 10~10
    volatile uint32_t DAC_DWA_TYPE                  : 3; // bit 11~13
    volatile uint32_t RESV_14_31                    : 18; // bit 14~31
};

union AUD_R18_DAC_CTRL3 {
    volatile uint32_t               all;
    struct AUD_R18_DAC_CTRL3_BITS   bit;
};
*/


#define AUD_R19_DAC_CTRL4_BITS  AUDIO_CODEC_REG_AUD_R19_DAC_CTRL4_BITS
#define AUD_R19_DAC_CTRL4       AUDIO_CODEC_REG_AUD_R19_DAC_CTRL4

/*
struct AUD_R19_DAC_CTRL4_BITS
{
    volatile uint32_t DAC_OFFSET                    : 16; // bit 0~15
    volatile uint32_t RESV_16_31                    : 16; // bit 16~31
};

union AUD_R19_DAC_CTRL4 {
    volatile uint32_t                     all;
    struct AUD_R19_DAC_CTRL4_BITS         bit;
};
*/


#define AUD_R20_DAC_CTRL5_BITS  AUDIO_CODEC_REG_AUD_R20_DAC_CTRL5_BITS
#define AUD_R20_DAC_CTRL5       AUDIO_CODEC_REG_AUD_R20_DAC_CTRL5

/*
struct AUD_R20_DAC_CTRL5_BITS
{
    volatile uint32_t DAC_DITH_POW                  : 26; // bit 0~25
    volatile uint32_t RESV_26_31                    : 6; // bit 26~31
};

union AUD_R20_DAC_CTRL5 {
    volatile uint32_t                     all;
    struct AUD_R20_DAC_CTRL5_BITS         bit;
};
*/

//struct AUD_R21_DAC_CTRL6_BITS
//{
//    volatile uint32_t DACEN_R                       : 1; // bit 0~0
//    volatile uint32_t DACEN_L                       : 1; // bit 1~1
//    volatile uint32_t DACR_ZC_EN_P                  : 1; // bit 2~2
//    volatile uint32_t DACL_ZC_EN_P                  : 1; // bit 3~3
//    volatile uint32_t AUD_DAC_DCT_RSTN              : 1; // bit 4~4
//    volatile uint32_t AUD_DAC_CLK_TEST_EN           : 1; // bit 5~5
//    volatile uint32_t RESV_11_31                    : 21; // bit 11~31
//};
//
//#define R21_DACL_EN         (1 << 1)
//#define R21_DACR_EN         (1 << 0)
//#define R21_DACLR_EN        (R21_DACL_EN | R21_DACR_EN)
//#define R21_DACL_ZC_EN      (1 << 3)
//#define R21_DACR_ZC_EN      (1 << 2)
//#define R21_DACLR_ZC_EN     (R21_DACL_ZC_EN | R21_DACR_ZC_EN)
//#define R21_DAC_DCT_RSTN    (0 << 4)
//#define R21_DAC_DCT_RELEASE (1 << 4)
//
//union AUD_R21_DAC_CTRL6 {
//    volatile uint32_t                     all;
//    struct AUD_R21_DAC_CTRL6_BITS         bit;
//};


#define AUD_R21_DAC_CTRL6_BITS  AUDIO_CODEC_REG_AUD_R21_DAC_CTRL6_BITS
#define AUD_R21_DAC_CTRL6       AUDIO_CODEC_REG_AUD_R21_DAC_CTRL6

#define R21_DAC_EN          (1 << 0)
#define R21_DAC_ZC_EN       (1 << 1)

/*
struct AUD_R21_DAC_CTRL6_BITS
{
    volatile uint32_t REG_AUD_EN_DAC                : 1; // bit 0~0
    volatile uint32_t DAC_ZCEN_REG                  : 1; // bit 1~1
    volatile uint32_t AUD_DAC_CLK_TEST_EN           : 1; // bit 2~2
    volatile uint32_t RESV_3_31                     : 29; // bit 3~31
};

union AUD_R21_DAC_CTRL6 {
    volatile uint32_t               all;
    struct AUD_R21_DAC_CTRL6_BITS   bit;
};
*/


//struct AUD_R22_DAC_CTRL7_BITS
//{
//    volatile uint32_t LORN_EN                       : 1; // bit 0~0
//    volatile uint32_t LORP_EN                       : 1; // bit 1~1
//    volatile uint32_t LOLN_EN                       : 1; // bit 2~2
//    volatile uint32_t LOLP_EN                       : 1; // bit 3~3
//    volatile uint32_t LORN_MUTE                     : 1; // bit 4~4
//    volatile uint32_t LORP_MUTE                     : 1; // bit 5~5
//    volatile uint32_t LOLN_MUTE                     : 1; // bit 6~6
//    volatile uint32_t LOLP_MUTE                     : 1; // bit 7~7
//    volatile uint32_t LORVOL_P                      : 4; // bit 8~11
//    volatile uint32_t LOLVOL_P                      : 4; // bit 12~15
//    volatile uint32_t RESV_16_31                    : 16; // bit 16~31
//};
//
//#define R22_LORN_EN         (1 << 0)
//#define R22_LORP_EN         (1 << 1)
//#define R22_LOLN_EN         (1 << 2)
//#define R22_LOLP_EN         (1 << 3)
//
//#define R22_LORN_MUTE       (1 << 4)
//#define R22_LORP_MUTE       (1 << 5)
//#define R22_LOLN_MUTE       (1 << 6)
//#define R22_LOLP_MUTE       (1 << 7)
//
//// 0x0:-24dB, 0xC: 0dB, 0xF:+6dB, 2dB each step
//#define R22_LOR_VOL_MASK    (0xF << 8)  // bit[11:8] for analog gain of right channel
//#define R22_LOL_VOL_MASK    (0xF << 12) // bit[15:12] for analog gain of left channel
//#define R22_LOR_VOL(n)      (((n) & 0xF) << 8)
//#define R22_LOL_VOL(n)      (((n) & 0xF) << 12)
//#define R22_LO_VOL_MIN      0x0
//#define R22_LO_VOL_MAX      0xF
//
//union AUD_R22_DAC_CTRL7 {
//    volatile uint32_t                     all;
//    struct AUD_R22_DAC_CTRL7_BITS         bit;
//};

#define AUD_R22_DAC_CTRL7_BITS  AUDIO_CODEC_REG_AUD_R22_DAC_CTRL7_BITS
#define AUD_R22_DAC_CTRL7       AUDIO_CODEC_REG_AUD_R22_DAC_CTRL7

#define R22_LON_EN          (1 << 0)
#define R22_LOP_EN          (1 << 1)
#define R22_LON_MUTE        (1 << 2)
#define R22_LOP_MUTE        (1 << 3)

// 0x0:-24dB, 0xC: 0dB, 0xF:+6dB, 2dB each step
#define R22_LO_VOL_MASK     (0xF << 4)  // bit[7:4] for analog gain
#define R22_LO_VOL(n)      (((n) & 0xF) << 4)
#define R22_LO_VOL_MIN      0x0
#define R22_LO_VOL_MAX      0xF

/*
struct AUD_R22_DAC_CTRL7_BITS
{
    volatile uint32_t REG_AUD_EN_DAC_LON            : 1; // bit 0~0
    volatile uint32_t REG_AUD_EN_DAC_LOP            : 1; // bit 1~1
    volatile uint32_t LOLN_MUTE_REG                 : 1; // bit 2~2
    volatile uint32_t LOLP_MUTE_REG                 : 1; // bit 3~3
    volatile uint32_t LOLVOL_P                      : 4; // bit 4~7
    volatile uint32_t RESV_8_31                     : 24; // bit 8~31
};

union AUD_R22_DAC_CTRL7 {
    volatile uint32_t               all;
    struct AUD_R22_DAC_CTRL7_BITS   bit;
};
*/

//----------------------------------------------------------------------
typedef struct
{
    volatile uint32_t                     REG_RSVD0; // 0x000
    union AUD_R1_GLOBAL0                  REG_AUD_R1_GLOBAL0; // 0x004
    union AUD_R2_GLOBAL1                  REG_AUD_R2_GLOBAL1; // 0x008
    volatile uint32_t                     REG_RSVD[12];
    union AUD_R15_DAC_CTRL0               REG_AUD_R15_DAC_CTRL0; // 0x03C
    union AUD_R16_DAC_CTRL1               REG_AUD_R16_DAC_CTRL1; // 0x040
    union AUD_R17_DAC_CTRL2               REG_AUD_R17_DAC_CTRL2; // 0x044
    union AUD_R18_DAC_CTRL3               REG_AUD_R18_DAC_CTRL3; // 0x048
    union AUD_R19_DAC_CTRL4               REG_AUD_R19_DAC_CTRL4; // 0x04C
    union AUD_R20_DAC_CTRL5               REG_AUD_R20_DAC_CTRL5; // 0x050
    union AUD_R21_DAC_CTRL6               REG_AUD_R21_DAC_CTRL6; // 0x054
    union AUD_R22_DAC_CTRL7               REG_AUD_R22_DAC_CTRL7; // 0x058
} CSK_DAC_RegDef;

#define CSK_DAC01               ((CSK_DAC_RegDef *)AP_CODEC_BASE)

//#define AON_PMUCTRL             ((AON_RegDef *) AON_PMUCTRL_BASE)
//#define AON_ANALOGCTRL          ((ANA_RegDef *) (AON_PMUCTRL_BASE +  0x00010000UL))
//#define AON_CODEC               ((AON_CODEC_RegDef *) AON_CODEC_BASE)

//====================== Register-related Macros & Functions =================

typedef struct {
    uint32_t reg_val;
    uint32_t real_val;
} REG_VAL_MAP;

typedef struct {
    uint32_t major;
    uint32_t minor;
} VAL_PAIR;

typedef struct {
    uint32_t major; // 1st value
    uint16_t minor; // 2nd value
    uint16_t least; // 3rd value
} VAL_TRIPLE;

#ifndef ARRAY_COUNT
#define ARRAY_COUNT(a)  (sizeof(a)/sizeof(a[0]))
#endif


/*
#define LDO25_VOSEL_2P5V    (1 << 3) // bit[3]: 1 = 2.5V (Normal work), 0 = 1.8V (For Trimming, Default)
#define LDO25_VOSEL_1P8V    (0 << 3)
#define LDO25_ENABLE        (1 << 4) // bit[4]: 1 = enable LDO25, 0 = disable LDO25
#define LDO25_DISABLE       (0 << 4)

#define LDO25_VSEL(n)       (((n) & 0x3) << 7)
#define LDO25_VSEL_MASK     (0x3 << 7)
#define LDO25_VSEL_2P45     (0x0 << 7)
#define LDO25_VSEL_2P50     (0x1 << 7)
#define LDO25_VSEL_2P55     (0x2 << 7)
#define LDO25_VSEL_2P60     (0x3 << 7)

#define LDO18_VSEL(n)       (((n) & 0x3) << 7)
#define LDO18_VSEL_MASK     (0x3 << 7)
#define LDO18_VSEL_1P80     (0x0 << 7)
#define LDO18_VSEL_1P85     (0x1 << 7)
#define LDO18_VSEL_1P90     (0x2 << 7)
#define LDO18_VSEL_1P95     (0x3 << 7)

static inline void ENABLE_LDO25() {
    //AON_ANALOGCTRL->REG_0X038.bit.LDO25VOSEL = 0x1;
    //AON_ANALOGCTRL->REG_0X038.bit.LDO25_EN = 0x1;
    //AON_ANALOGCTRL->REG_0X038.bit.LDO18_VSEL = 0x1;
    uint32_t reg_val, reg_org;
    reg_org = reg_val = AON_ANALOGCTRL->REG_0X038.all;
    reg_val &= ~LDO25_VSEL_MASK;
    reg_val |= LDO25_VOSEL_2P5V | LDO25_ENABLE | LDO25_VSEL_2P50;
    if (reg_val != reg_org)
        AON_ANALOGCTRL->REG_0X038.all = reg_val;
}

static inline void DISABLE_LDO25() {
    //AON_ANALOGCTRL->REG_0X038.bit.LDO25_EN = 0x0;
    //AON_ANALOGCTRL->REG_0X038.bit.LDO25VOSEL = 0x0;
    uint32_t reg_val = AON_ANALOGCTRL->REG_0X038.all;
    if (!(reg_val & LDO25_ENABLE))
        return;
    reg_val &= ~(LDO25_ENABLE | LDO25_VOSEL_2P5V);
    AON_ANALOGCTRL->REG_0X038.all = reg_val;
}

static inline void CONFIG_CODEC_ANALOG() {
    // CP CODEC R2 register is pseudo one, actually those fields related to
    // VMID/Vreference/MicBias etc. are subject to AON CODEC R2 register.
    //AON_CODEC->REG_AUD_R2_GLOBAL1.all  = 0x564;
    //AON_CODEC->REG_AUD_R2_GLOBAL1.all = MB0_MILLER_EXT_CAP | MB0_SET_2P0V |
    //                                    MB1_MILLER_EXT_CAP | MB1_SET_2P0V |
    //                                    DAC_IREF_1P5UA | AUD_IREF_ENABLE | // DAC_IREF_2P0UA
    //                                    AUD_VMID_50KOHM | AUD_VMID_ENABLE;

    uint32_t val = AON_CODEC->REG_AUD_R2_GLOBAL1.all;
    AON_CODEC->REG_AUD_R2_GLOBAL1.all = MB0_MILLER_EXT_CAP | (val & MB0_SET_MASK) |
                                        MB1_MILLER_EXT_CAP | (val & MB1_SET_MASK) |
                                        DAC_IREF_1P5UA | AUD_IREF_ENABLE | // DAC_IREF_2P0UA
                                        AUD_VMID_50KOHM | AUD_VMID_ENABLE;

}

static inline void CONFIG_CODEC_ANALOG2(uint8_t adc_dev_bmp) {
    // CP CODEC R2 register is pseudo one, actually those fields related to
    // VMID/Vreference/MicBias etc. are subject to AON CODEC R2 register.
    uint32_t val;
    val = (adc_dev_bmp & CH_BMP_LEFT) ? MB0_SET_2P0V : MB0_SET_DISABLE;
    val |= (adc_dev_bmp & CH_BMP_RIGHT) ? MB1_SET_2P0V : MB1_SET_DISABLE;
    AON_CODEC->REG_AUD_R2_GLOBAL1.all = MB0_MILLER_EXT_CAP | MB1_MILLER_EXT_CAP | val |
                                        DAC_IREF_1P5UA | AUD_IREF_ENABLE | // DAC_IREF_2P0UA
                                        AUD_VMID_50KOHM | AUD_VMID_ENABLE;
}
*/

static inline void ENABLE_CODEC_BASIC() {
    //FIXME: consult ANALOG guys about CODEC's external power & clock settings!!
	IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 1;
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_ADC_CLK = 1;
}

static inline void CONFIG_CODEC_ADC_ANALOG(uint8_t ch_bmp, uint8_t single_in_bmp) {
    //FIXME: consult ANALOG guys about CODEC's analog settings!!
    // i.e. VMID, VREF, MICBIAS etc.

#if (IC_BOARD != 0)
    // Differential input use 2 Pin: P & N, while Single-ended input use only 1 pin: P
//    if  (ch_bmp & CH_BMP_LEFT) {
//        IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_FSEL = 21; // MIC0_INP
//        if (!(single_in_bmp & 0x1)) // differential input
//            IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_FSEL = 21; // MIC0_INN
//    }
//    if (ch_bmp & CH_BMP_RIGHT) {
//        IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_FSEL = 21; // MIC1_INP
//        if (!(single_in_bmp & 0x2)) // differential input
//            IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_FSEL = 21; // MIC1_INN
//    }
#endif

    //uint32_t val = IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.all;
    IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.all |= R2_AUD_EN_IREF | R2_AUD_EN_VMID | R2_AUD_EN_MICBIAS;
}

static inline void CONFIG_CODEC_DAC_ANALOG() {
    //FIXME: consult ANALOG guys about CODEC's analog settings!!
    // i.e. VMID, VREF, MICBIAS etc.

    //uint32_t val = IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.all;
    IP_AUDIO_CODEC->REG_AUD_R2_GLOBAL1.all |= R2_AUD_EN_IREF | R2_AUD_EN_VMID;
}

#endif /* __AUDIO_CODEC_H */
