/**
 * @file audio_codec.h
 * @brief Audio Codec Register Definitions and Control Functions
 *
 * @details This header file provides comprehensive register definitions, bit masks,
 * and control functions for the Venus Audio Codec. It includes:
 * - Register map definitions for ADC and DAC sections
 * - Bit field masks and position constants for all control registers
 * - Value ranges and constants for audio parameters (gain, volume, etc.)
 * - Inline configuration functions for basic codec operations
 *
 * The codec supports:
 * - Stereo ADC with programmable gain, ALC, and HPF
 * - Stereo DAC with digital gain and soft mute
 * - Multiple operating modes (4MHz/12MHz)
 * - Digital microphone interface
 * - Various power optimization features
 *
 * @note All register accesses should use the provided bit masks and macros
 * to ensure proper configuration and maintain code readability.
 *
 * @version 1.0
 * @date 2023
 *
 * @copyright Copyright (c) 2023
 */


#ifndef __AUDIO_CODEC_H
#define __AUDIO_CODEC_H

#include "venusa_ap.h"
#include "audio_codec_reg.h"

//=============================== CODEC/AON_CODEC Register Map ===============================
// REG_AUD_VMID_CFG
#define AUD_VMID_CFG_BITS       AUDIO_CODEC_REG_AUD_VMID_CFG_BITS
#define AUD_VMID_CFG            AUDIO_CODEC_REG_AUD_VMID_CFG

#define AUD_EN_IREF          (0x1 << 0)
#define AUD_EN_VMID          (0x1 << 1)

// REG_AUD_ADC_CTRL0
#define AUD_ADC_CTRL0_BITS     AUDIO_CODEC_REG_AUD_ADC_CTRL0_BITS
#define AUD_ADC_CTRL0          AUDIO_CODEC_REG_AUD_ADC_CTRL0

#define ADCCLK_EN_POS     7
#define ADCCLK_EN        (1 << 7)

// REG_AUD_ADC_CTRL1
#define AUD_ADC_CTRL1_BITS      AUDIO_CODEC_REG_AUD_ADC_CTRL1_BITS
#define AUD_ADC_CTRL1           AUDIO_CODEC_REG_AUD_ADC_CTRL1

#define ADCR_VOL_MASK        (0x7F << 0)
#define ADCL_VOL_MASK        (0x7F << 7)
#define ADCR_VOL(n)          (((n) & 0x7F) << 0) // bit[6:0]
#define ADCL_VOL(n)          (((n) & 0x7F) << 7) // bit[13:7]
#define ADC_VOL_MIN          0x0     // -83dB
#define ADC_VOL_MAX          0x7F    // +42dB

#define HPF2_EN              (1 << 14)   // bit[14]
#define HPF1_EN              (1 << 15)   // bit[15]
#define ADCR_PGA_MASK        (0x1F << 16)
#define ADCR_PGA_VOL(n)      (((n) & 0x1F) << 16)    // bit[20:16]
#define ADCL_PGA_MASK        (0x1F << 21)
#define ADCL_PGA_VOL(n)      (((n) & 0x1F) << 21)    // bit[25:21]
#define ADC_PGA_VOL_MIN      0x0     // -12dB
#define ADC_PGA_VOL_MAX      0x18    // +36dB
#define HPF_CUT(n)           (((n) & 0x7) << 26)     // bit[28:26]
#define ADC_SINGLE_CH_MODE   (1 << 29)   // bit[29]
#define HPF_OUT_SEL          (1 << 30)   // bit[30]

// REG_AUD_ADC_CTRL2
#define AUD_ADC_CTRL2_BITS      AUDIO_CODEC_REG_AUD_ADC_CTRL2_BITS
#define AUD_ADC_CTRL2           AUDIO_CODEC_REG_AUD_ADC_CTRL2

// REG_AUD_ADC_CTRL3
#define AUD_ADC_CTRL3_BITS      AUDIO_CODEC_REG_AUD_ADC_CTRL3_BITS
#define AUD_ADC_CTRL3           AUDIO_CODEC_REG_AUD_ADC_CTRL3

#define ERR_TOLERANCE_MIN                0
#define ERR_TOLERANCE_MAX                4
#define TARGET_LEVEL_MIN                 0
#define TARGET_LEVEL_MAX                 23
#define NGATE_FLOOR_MIN                  0
#define NGATE_FLOOR_MAX                  22
#define ALCMIN_MIN                       0
#define ALCMIN_MAX                       24
#define ALCMAX_MIN                       0
#define ALCMAX_MAX                       24

// REG_AUD_ADC_CTRL4
#define AUD_ADC_CTRL4_BITS      AUDIO_CODEC_REG_AUD_ADC_CTRL4_BITS
#define AUD_ADC_CTRL4           AUDIO_CODEC_REG_AUD_ADC_CTRL4

#define ALC_DECAY_MIN        0x0
#define ALC_DECAY_MAX        0xF
#define ALC_ATTACK_MIN       0x0
#define ALC_ATTACK_MAX       0xF
#define ALC_HOLD_MIN         0x0
#define ALC_HOLD_MAX         0xF

#define ALC_DECAY(n)         (((n) & 0xF) << 0)
#define ALC_ATTACK(n)        (((n) & 0xF) << 4)
#define ALC_HOLD(n)          (((n) & 0xF) << 8)
#define PEAK_FASTALC_EN      (1 << 12)

// 1: double edge on DMIC0 or DMIC1, 0: single edge on both DMIC0 & DMIC1
#define DMIC_MODE_DBL_EDGE   (1 << 16)
// 1: from DMIC1, 0: from DMIC0 only when DMIC_MODE=1
#define DMIC_SRC_DMIC1       (1 << 21)
#define DMIC_SRC_DMIC0       (0 << 21)
#define DMIC_EN              (1 << 22)

#define AUTORST_TYPE(n)      (((n) & 0x7) << 23)     //0x0~0x5, default 0x1 (256us)
#define AUTORST_EN_R         (1 << 26)
#define AUTORST_EN_L         (1 << 27)

// REG_AUD_ADC_CTRL5
#define AUD_ADC_CTRL5_BITS      AUDIO_CODEC_REG_AUD_ADC_CTRL5_BITS
#define AUD_ADC_CTRL5           AUDIO_CODEC_REG_AUD_ADC_CTRL5

#define FILGAIN_REG_MASK    (0xFFFFF << 0)  // bit[19:0]
#define FILGAIN_REG(n)      (((n) & 0xFFFFF) << 0)
#define FILGAIN_REGEN       (0x1 << 20)     // bit[20]

// REG_AUD_ADC_CTRL6
#define AUD_ADC_CTRL6_BITS      AUDIO_CODEC_REG_AUD_ADC_CTRL6_BITS
#define AUD_ADC_CTRL6           AUDIO_CODEC_REG_AUD_ADC_CTRL6

#define ADCR_EN              (0x1 << 0) // bit[0]: enable Right ADC (ADC1)
#define ADCL_EN              (0x1 << 1) // bit[1]: enable Left ADC (ADC0)
#define ADCLR_EN             (0x3 << 0) // bit[1:0]: enable both LR ADC (ADC0 & ADC1)

#define ADCR_ANA_RST        (0x1 << 2) // bit[2], [RW] Right ADC analog path reset
#define ADCL_ANA_RST        (0x1 << 3) // bit[3], [RW] Left ADC analog path reset
#define ADC_ANA_RST         (0x1 << 4) // bit[4], [RW]
#define ADC_LP_AUTORST_SPLIT (0x1 << 5) // bit[5]

#define ADC_IB_CTRL_MASK    (0x3 << 6) // bit[7:6], IB=IBIAS
#define ADC_IB_CTRL(n)      (((n) & 0x3) << 6)
#define IB_CTRL_1P5UA           0x0
#define IB_CTRL_2UA             0x1
#define IB_CTRL_2P5UA           0x2
#define IB_CTRL_3UA             0x3
#define IB_CTRL_VAL_DEF         IB_CTRL_3UA // IB_CTRL_2UA
#define IB_CTRL_VAL_12MHZ       IB_CTRL_2P5UA // make sure to work under limiting conditions
#define IB_CTRL_VAL_LP          IB_CTRL_1P5UA //FIXME:

#define ADC_IDAC_CTRL_MASK  (0x3 << 8) // bit[9:8]
#define ADC_IDAC_CTRL(n)    (((n) & 0x3) << 8)
#define IDAC_7P5UA_M5P          (0x0)
#define IDAC_7P5UA              (0x1)
#define IDAC_7P5UA_P6P          (0x2)
#define IDAC_7P5UA_P13P         (0x3)

#define ADC_CAP_CALI_EN_SRC_MASK    (0x1 << 10) // bit[10]
#define ADC_CAP_CALI_EN_SRC_REG     (0x1 << 10) // enable from register
#define ADC_CAP_CALI_EN_SRC_ISM     (0x0 << 10) // enable from internal state machine?
#define ADC_CAP_CALI_EN_REG_MASK    (0x1 << 11) // bit[11]
#define ADC_CAP_CALI_EN_REG_ENA     (0x1 << 11) // enable
#define ADC_CAP_CALI_EN_REG_DIS     (0x0 << 11) // disable

#define ADC_CAP_CALI_REGVAL_MASK    (0x1F << 12) // bit[16:12]
#define ADC_CAP_CALI_REGVAl(n)      (((n) & 0x1F) << 12)
#define ADC_CAP_CALI_SRC_MASK       (0x1 << 17)
#define ADC_CAP_CALI_SRC_REG        (0x1 << 17) // CALI setting from register
#define ADC_CAP_CALI_SRC_ISM        (0x0 << 17) // CALI setting from internal state machine

// REG_AUD_ADC_CTRL7
#define AUD_ADC_CTRL7_BITS      AUDIO_CODEC_REG_AUD_ADC_CTRL7_BITS
#define AUD_ADC_CTRL7           AUDIO_CODEC_REG_AUD_ADC_CTRL7

#define RPGA_VCMBUF_EN              (1 << 0) // bit[0]: Enable ADC PGA1 VCM Buffer
#define LPGA_VCMBUF_EN              (1 << 1) // bit[1]: Enable ADC PGA0 VCM Buffer

#define RPGA_EN                     (1 << 2) // bit[2]: Enable ADC PGA1
#define RPGA_MUTE                   (1 << 3) // bit[3]: Mute ADC PGA1
#define RPGA_SINGLE                 (1 << 4) // bit[4]: ADC PGA1 input mode: Single
#define RPGA_DIFF                   (0 << 4) // bit[4]: ADC PGA1 input mode: Differential
#define PGA_VCOM_SEL_VMID_75PER     (1 << 5) // bit[5]: ADC PGA VCOM Select: VMID*0.75
#define PGA_VCOM_SEL_VMID           (0 << 5) // bit[5]: ADC PGA VCOM Select: VMID (Default)
#define RPGA_ZCEN                   (1 << 6) // bit[6]: Enable ADC PGA1 Zero Crossing
#define RPGA_ZCEN_REG               (1 << 7) // bit[7]: Enable ADC PGA1 Zero Crossing REG
#define RPGA_ZCEN_FORCE             (1 << 8) // bit[8]: Enable ADC PGA1 Zero Crossing FORCE

#define LPGA_EN                     (1 << 9) // bit[9]: Enable ADC PGA0
#define LPGA_MUTE                   (1 << 10) // bit[10]: Mute ADC PGA0
#define LPGA_SINGLE                 (1 << 11) // bit[11]: ADC PGA0 input mode: Single
#define LPGA_DIFF                   (0 << 11) // bit[11]: ADC PGA0 input mode: Differential
#define PGA_LP                      (1 << 12) // bit[12]: ADC PGA power mode: Low Power
#define PGA_NORM                    (0 << 12) // bit[12]: ADC PGA power mode: Normal
#define LPGA_ZCEN                   (1 << 13) // bit[13]: Enable ADC PGA0 Zero Crossing
#define LPGA_ZCEN_REG               (1 << 14) // bit[14]: Enable ADC PGA1 Zero Crossing REG
#define LPGA_ZCEN_FORCE             (1 << 15) // bit[15]: Enable ADC PGA1 Zero Crossing FORCE

#define ADC_ATB_CTRL(n)             (((n) & 0x3) << 16) // bit[17:16], 0~2, default 0
#define ADC_SAR_TEST_EN             (0x1 << 18) // bit[18]: Enable ADC SAR TEST
#define ADC_SAR_DELAY_MASK          (0x7 << 19) // bit[21:19]: ADC SAR Delay Control
#define ADC_SAR_DELAY_CTRL(n)        (((n) & 0x7) << 19) // 0x0~0x7: 6/7/8/9.5(default)/12/14/16/19ns
#define ADC_SAR_COMP_LP             (0x1 << 22) // bit[22]: ADC SAR Comparator Low Power, 1 = Low Current
#define ADC_INT2_LP                 (0x1 << 23) // bit[23]: ADC INTegrator 2 Low Power, 1 = Low Current (default)
#define ADC_INT1_LP                 (0x1 << 24) // bit[24]: ADC INTegrator 1 Low Power, 1 = Low Current (default)

#define ADC_IDAC_OS_MASK            (0x3 << 25) // bit[26:25]
#define ADC_IDAC_OS_CTRL(n)         (((n) & 0x3) << 25)
#define IDAC_OS_0MV                 (0x0) //0MV
#define IDAC_OS_20MV                (0x1) //20MV
#define IDAC_OS_40MV                (0x2) //40MV
#define IDAC_OS_80MV                (0x3) //80MV

#define IDAC_BIAS_SRC               (0x1 << 27) // bit[27], 1=from register value?
#define IDAC_BIAS_REG               (0x1 << 28) // bit[28], register value?

#define ADCL_VREF_EN                (0x1 << 29) // bit[29], Enable ADC0 (Left) VREF
#define ADCR_VREF_EN                (0x1 << 30) // bit[30], Enable ADC1 (Right) VREF
#define ADC_MODE_12MHZ              (0x1 << 31) // bit[31], 12MHz mode
#define ADC_MODE_4MHZ               (0x0 << 31) // bit[31], 4MHz mode


// REG_AUD_ADC_CALI_STATUS
#define AUD_ADC_CALI_STATUS_BITS        AUDIO_CODEC_REG_AUD_ADC_CALI_STATUS_BITS
#define AUD_ADC_CALI_STATUS             AUDIO_CODEC_REG_AUD_ADC_CALI_STATUS

// REG_AUD_ADC0_OUT
#define AUD_ADC0_OUT_BITS               AUDIO_CODEC_REG_AUD_ADC0_OUT_BITS
#define AUD_ADC0_OUT                    AUDIO_CODEC_REG_AUD_ADC0_OUT

// REG_AUD_ADC1_OUT
#define AUD_ADC1_OUT_BITS               AUDIO_CODEC_REG_AUD_ADC1_OUT_BITS
#define AUD_ADC1_OUT                    AUDIO_CODEC_REG_AUD_ADC1_OUT

// REG_AUD_DEBUG_CFG
#define AUD_DEBUG_CFG_BITS               AUDIO_CODEC_REG_AUD_DEBUG_CFG_BITS
#define AUD_DEBUG_CFG                    AUDIO_CODEC_REG_AUD_DEBUG_CFG

//----------------------------------------------------------------------
typedef struct
{
    union AUD_VMID_CFG                    REG_AUD_VMID_CFG; // 0x000
    union AUD_ADC_CTRL0                   REG_AUD_ADC_CTRL0; // 0x004
    union AUD_ADC_CTRL1                   REG_AUD_ADC_CTRL1; // 0x008
    union AUD_ADC_CTRL2                   REG_AUD_ADC_CTRL2; // 0x00c
    union AUD_ADC_CTRL3                   REG_AUD_ADC_CTRL3; // 0x010
    union AUD_ADC_CTRL4                   REG_AUD_ADC_CTRL4; // 0x014
    union AUD_ADC_CTRL5                   REG_AUD_ADC_CTRL5; // 0x018
    union AUD_ADC_CTRL6                   REG_AUD_ADC_CTRL6; // 0x01c
    union AUD_ADC_CTRL7                   REG_AUD_ADC_CTRL7; // 0x020
    volatile uint32_t                     REG_RSVD1[9];
    union AUD_ADC_CALI_STATUS             REG_AUD_R25_STATUS0; // 0x048
    volatile uint32_t                     REG_RSVD2[1];
    union AUD_ADC0_OUT                    REG_AUD_ADC0_OUT; //0x50
    union AUD_ADC1_OUT                    REG_AUD_ADC1_OUT; //0x54
    volatile uint32_t                     REG_RSVD3[2];
    union AUD_DEBUG_CFG                   REG_AUD_DEBUG_CFG; //0x60
} CSK_ADC_PDM_RegDef;

#define CSK_ADC01               ((CSK_ADC_PDM_RegDef *)CODEC_BASE)

// REG_AUD_DAC_CTRL0
#define AUD_DAC_CTRL0_BITS      AUDIO_CODEC_REG_AUD_DAC_CTRL0_BITS
#define AUD_DAC_CTRL0           AUDIO_CODEC_REG_AUD_DAC_CTRL0

#define DACCLK_EN           (1 << 5)
#define DAC_RELEASE         (1 << 6)

//#define DAC_MCLK_24MHZ      (0 << 8)
//#define DAC_MCLK_22P05MHZ   (1 << 8)

#define DAC_IB_CTRL_MASK    (0x3 << 9) // bit[10:9], IB=IBIAS
#define DAC_IB_CTRL(n)      (((n) & 0x3) << 9)
#define DAC_IB_CTRL_1P5UA       0x0
#define DAC_IB_CTRL_2UA         0x1
#define DAC_IB_CTRL_2P5UA       0x2
#define DAC_IB_CTRL_3UA         0x3
#define DAC_IB_CTRL_VAL_DEF     DAC_IB_CTRL_2UA
//#define DAC_IB_CTRL_VAL_12MHZ   DAC_IB_CTRL_2P5UA
#define DAC_IB_CTRL_VAL_LP      DAC_IB_CTRL_1P5UA


// REG_AUD_DAC_CTRL1
#define AUD_DAC_CTRL1_BITS      AUDIO_CODEC_REG_AUD_DAC_CTRL1_BITS
#define AUD_DAC_CTRL1           AUDIO_CODEC_REG_AUD_DAC_CTRL1

// 0x70:-113dB, 0xe1: 0dB, 0xFF:+30dB, 1dB each step
#define DAC_GAIN_MASK       (0xFF << 0) // bit[7:0] for digital gain
#define DAC_GAIN(n)         (((n) & 0xFF) << 0)
#define DAC_GAIN_MIN        0x70
#define DAC_GAIN_MAX        0xFF

// REG_AUD_DAC_CTRL2
#define AUD_DAC_CTRL2_BITS      AUDIO_CODEC_REG_AUD_DAC_CTRL2_BITS
#define AUD_DAC_CTRL2           AUDIO_CODEC_REG_AUD_DAC_CTRL2

#define SOFT_MUTE_SPEED(n)      (((n) & 0xF) << 0)  // bit[3:0] more bigger, more slower
#define SOFT_MUTE_DISABLE       (0 << 4)
#define SOFT_MUTE_ENABLE        (1 << 4)            // bit[4]
#define DAC_DWA_DISABLE         (0 << 5)
#define DAC_DWA_ENABLE          (1 << 5)            // bit[5]
#define DAC_UNMUTE              (0 << 6)
#define DAC_MUTE                (1 << 6)            // bit[6]
#define DAC_DATA_INV            (1 << 7)            // bit[7]

// REG_AUD_DAC_CTRL3
#define AUD_DAC_CTRL3_BITS      AUDIO_CODEC_REG_AUD_DAC_CTRL3_BITS
#define AUD_DAC_CTRL3           AUDIO_CODEC_REG_AUD_DAC_CTRL3

#define DAC_DITH_BYPASS             (1 << 2)    // bit[2]
#define DAC_SDM_RESET               (0 << 4)    // bit[4]
#define DAC_SDM_RELEASE             (1 << 4)
//#define DAC_SDM_AMUTE_THRESHOLD(n)  (((n) & 0x7) << 5)  // bit[7:5], 0 =< n < 6
#define DAC_SDM_AMUTE_TYPE(n)  (((n) & 0x7) << 5)  // bit[7:5], 0 =< n < 6
#define DAC_SDM_AMUTE_MAX           5
#define DAC_SDM_AMUTE_EN            (1 << 8)    // bit[8]
#define DAC_DITH_NTF_EN             (1 << 10)   // bit[10]
#define DAC_DWA_TYPE_MASK           (0x7 << 11)   // bit[13:11]
#define DAC_DWA_TYPE(n)             (((n) & 0x7) << 11)
#define DAC_DWA_TYPE_MAX                0x5
#define DAC_DWA_TYPE_DEF                0x1

// REG_AUD_DAC_CTRL4
#define AUD_DAC_CTRL4_BITS      AUDIO_CODEC_REG_AUD_DAC_CTRL4_BITS
#define AUD_DAC_CTRL4           AUDIO_CODEC_REG_AUD_DAC_CTRL4

// REG_AUD_DAC_CTRL5
#define AUD_DAC_CTRL5_BITS      AUDIO_CODEC_REG_AUD_DAC_CTRL5_BITS
#define AUD_DAC_CTRL5           AUDIO_CODEC_REG_AUD_DAC_CTRL5

// REG_AUD_DAC_CTRL6
#define AUD_DAC_CTRL6_BITS      AUDIO_CODEC_REG_AUD_DAC_CTRL6_BITS
#define AUD_DAC_CTRL6           AUDIO_CODEC_REG_AUD_DAC_CTRL6

#define DAC_EN          (1 << 0)
#define DAC_ZC_EN       (1 << 1)

// REG_AUD_DAC_IN_REG
#define AUD_DAC_IN_REG_BITS  AUDIO_CODEC_REG_AUD_DAC_IN_REG_BITS
#define AUD_DAC_IN_REG       AUDIO_CODEC_REG_AUD_DAC_IN_REG


// REG_AUD_DAC_IN_REG
#define AUD_DAC_DATA_TC_REG_BITS  AUDIO_CODEC_REG_DAC_DATA_TC_REG_BITS
#define AUD_DAC_DATA_TC_REG       AUDIO_CODEC_REG_DAC_DATA_TC_REG

// DAC no PGA REG with externel pga
//----------------------------------------------------------------------
typedef struct
{
	union AUD_VMID_CFG                    REG_AUD_VMID_CFG; // 0x000
	volatile uint32_t                     REG_RSVD[10];
    union AUD_DAC_CTRL0                   REG_AUD_DAC_CTRL0; // 0x02C
    union AUD_DAC_CTRL1                   REG_AUD_DAC_CTRL1; // 0x030
    union AUD_DAC_CTRL2                   REG_AUD_DAC_CTRL2; // 0x034
    union AUD_DAC_CTRL3                   REG_AUD_DAC_CTRL3; // 0x038
    union AUD_DAC_CTRL4                   REG_AUD_DAC_CTRL4; // 0x03C
    union AUD_DAC_CTRL5                   REG_AUD_DAC_CTRL5; // 0x040
    union AUD_DAC_CTRL6                   REG_AUD_DAC_CTRL6; // 0x044
    volatile uint32_t                     REG_RSVD1[4];
    union AUD_DAC_IN_REG                  REG_AUD_DAC_IN_REG; // 0x058
    union AUD_DAC_DATA_TC_REG             REG_AUD_DAC_DATA_TC_REG; // 0x05C
    union AUD_DEBUG_CFG                   REG_AUD_DEBUG_CFG; //0x60
} CSK_DAC_RegDef;

#define CSK_DAC01               ((CSK_DAC_RegDef *)CODEC_BASE)


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

/**
 * @brief Enable basic CODEC functionality including power and clock
 *
 * This function enables the essential components for CODEC operation:
 * - LDO for analog power supply
 * - CODEC clock from system controller
 *
 * @note Must be called before any other CODEC configuration
 */
static inline void ENABLE_CODEC_BASIC() {
    // about CODEC's  power & clock settings!!
	IP_AON_CTRL->REG_AON_TUNE0.bit.EN_LDO_VA = 1;
	IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_CODEC_CLK = 1;
}

/**
 * @brief Configure CODEC ADC analog section
 *
 * @param ch_bmp Channel bitmap for ADC enable
 * @param single_in_bmp Single-ended input bitmap configuration
 *
 * This function enables the necessary analog references (IREF and VMID)
 * for proper ADC operation.
 */
static inline void CONFIG_CODEC_ADC_ANALOG(uint8_t ch_bmp, uint8_t single_in_bmp) {
    IP_CODEC->REG_AUD_VMID_CFG.all |= AUD_EN_IREF | AUD_EN_VMID;
}

/**
 * @brief Configure CODEC DAC analog section
 *
 * This function enables the necessary analog references (IREF and VMID)
 * for proper DAC operation.
 */
static inline void CONFIG_CODEC_DAC_ANALOG() {
	IP_CODEC->REG_AUD_VMID_CFG.all |= AUD_EN_IREF | AUD_EN_VMID;
}

#endif /* __AUDIO_CODEC_H */
