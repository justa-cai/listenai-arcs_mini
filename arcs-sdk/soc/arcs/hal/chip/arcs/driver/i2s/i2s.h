/*
 * i2s.h
 *
 *
 */

#ifndef __I2S_ARCS_H
#define __I2S_ARCS_H

#include "arcs_ap.h"
#include "apc.h"
#include "apc_reg.h"
#include "Driver_I2S.h"

//=============================== I2S Register Map ===============================
// I2S registers are part of APC registers according to APC IP design...
struct I2S_CFG0_BITS
{
    //volatile uint32_t SWAP_CHLR                : 2; // bit 0~1
    volatile uint32_t SWAP_CHLR_OUT            : 1; // bit 0
    volatile uint32_t SWAP_CHLR_IN             : 1; // bit 1
    volatile uint32_t LOOP_BACK                : 1; // bit 2~2
    volatile uint32_t LSB                      : 1; // bit 3~3
    volatile uint32_t RIGHT_JUSTIFIED          : 1; // bit 4~4
    volatile uint32_t BCK_LRCK                 : 5; // bit 5~9
    volatile uint32_t WLEN                     : 2; // bit 10~11
    volatile uint32_t LRCK_POL                 : 1; // bit 12~12
    volatile uint32_t BCK_POL                  : 1; // bit 13~13
    volatile uint32_t BCKOUT_GATE              : 1; // bit 14~14
    volatile uint32_t RX_HALF_CYCLE_DLY        : 1; // bit 15~15
    volatile uint32_t TX_HALF_CYCLE_DLY        : 1; // bit 16~16
    volatile uint32_t RX_DLY                   : 2; // bit 17~18
    //ONLY for slave mode: configure 1 cycle supplementary Tx/RX delay.
    //In slave mode, The fields RX_DLY / TX_DLY may work abnormally and
    //had better be set to 0, and it's recommended to use TXRX_DLY_S instead!
    volatile uint32_t TXRX_DLY_S               : 1; // bit 19~19
    volatile uint32_t TX_DLY                   : 1; // bit 20~20
    volatile uint32_t MASTER_MODE              : 1; // bit 21~21
    volatile uint32_t SERIAL_MODE              : 2; // bit 22~23
    volatile uint32_t TX_MODE                  : 2; // bit 24~25
    volatile uint32_t BYPASS_FIFOVLD           : 1; // bit 26~26
    volatile uint32_t BCK_FORCE_ON             : 1; // bit 27~27
    volatile uint32_t EN_FORCE_ON              : 1; // bit 28~28
    volatile uint32_t RSTN_BYPASS              : 1; // bit 29~29
    volatile uint32_t SW_RESET                 : 1; // bit 30~30
    volatile uint32_t ENABLE                   : 1; // bit 31~31
};

#define BIT_INDEX_SWAP_CHLR_OUT         0
#define BIT_MASK_SWAP_CHLR_OUT          (0x1 << 0)
#define BIT_VALUE_SWAP_CHLR_OUT         (0x1 << 0)

#define BIT_INDEX_TX_MODE               24
#define BIT_MASK_TX_MODE                (0x3 << 24)
#define BIT_VALUE_TX_MODE(n)            (((n) & 0x3) << 24)

typedef union {
    volatile uint32_t                  all;
    struct I2S_CFG0_BITS               bit;
} I2S_CFG0;

struct I2S_CFG1_BITS
{
    volatile uint32_t SLOTNUM                  : 8; // bit 0~7
    volatile uint32_t LONGSYNC                 : 1; // bit 8~8
    volatile uint32_t SLOT_LRCK                : 7; // bit 9~15
    volatile uint32_t BCK_DIV                  : 10; // bit 16~25
    volatile uint32_t BCK_DIV_LD               : 1; // bit 26~26
    volatile uint32_t BCK_SAME_EDGE            : 1; // bit 27~27
    volatile uint32_t RESV_28_31               : 4; // bit 28~31
};

typedef union {
    volatile uint32_t                  all;
    struct I2S_CFG1_BITS               bit;
} I2S_CFG1;

// each I2S register map of APC
typedef struct {
    I2S_CFG0                     REG_I2S_CFG0; // I2S0@0x0E4, I2S1@0x0EC
    I2S_CFG1                     REG_I2S_CFG1; // I2S0@0x0E8, I2S1@0x0F0
} CSK_I2S_RegDef;

#define CSK_I2S0                ((CSK_I2S_RegDef *)(APC_BASE + 0xE4))
#define CSK_I2S1                ((CSK_I2S_RegDef *)(APC_BASE + 0xEC))

// Serial Mode
#define SERMODE_I2S             0 // I2S
#define SERMODE_VOICE           1 // = DSP/PCM
#define SERMODE_TDM             SERMODE_VOICE // TDM supported only for VOICE
#define SERMODE_DAI             2 // NOT SUPPORTED
#define SERMODE_DSD             3 // NOT SUPPORTED

// Bit clock polarity
#define BCK_POL_NORMAL          0 // NORMAL
#define BCK_POL_LOW_IDLE        BCK_POL_NORMAL
#define BCK_POL_INVERT          1 // INVERT
#define BCK_POL_HIGH_IDLE       BCK_POL_INVERT

// LR clock (Word select) polarity
#define LRCK_POL_LEFT_H         0
#define LRCK_POL_LEFT_L         1

// I2S Data Length
#define WLEN_16BIT              0
#define WLEN_20BIT              1
#define WLEN_24BIT              2
#define WLEN_32BIT              3

// TX Mode
#define TXMODE_STEREO_STEREO    0
#define TXMODE_MONOL_STEREO     1
#define TXMODE_MONOR_STEREO     2
#define TXMODE_MONO_MONO        3

//====================== Register-related Macros & Functions =====================


//====================== Driver internal definitions =====================

// I2S device flags
#define I2S_FLAG_INITIALIZED          (1UL << 0)     // I2S initialized
#define I2S_FLAG_POWERED              (1UL << 1)     // I2S powered on
#define I2S_FLAG_CONFIGURED           (1UL << 2)     // I2S configured


// APC channels etc. resource (fixed)
typedef struct _I2S_APC_RES
{
    uint8_t dch_in_cnt : 1; // how many APC IN dual_channels are used for I2S IN? 0 or 1?
//    uint8_t support_alt_in : 1; // whether ALTERNATE APC IN dual_channel is supported?
    uint8_t dch_out_cnt : 1; // how many APC OUT dual_channels are used for I2S OUT? 0 or 1?
    //uint8_t support_alt_out : 1; // whether ALTERNATE APC OUT dual_channels are supported? ALWAYS 0 NOW!
    uint8_t support_echo : 1; // whether APC ECHO (IN) dual_channel is supported?
    uint8_t reseved : 1;

    uint8_t dch_in : 4; // which APC IN dual_channel?
//    uint8_t dch_alt_in : 4; // which APC alternate IN dual_channel?
    uint8_t dch_out : 4; // which APC OUT dual_channel?
    //uint8_t dch_alt_out : 4; // which APC alternate OUT dual_channel?
    uint8_t dch_echo : 4; // which APC ECHO (IN) dual_channel?

} I2S_APC_RES;

// I2S channels configuration
typedef struct _I2S_CH_CFG
{
    uint32_t chbmp_in: 2; // current acquired IN channel bitmap (Left@bit0, Right@bit1)
//    uint32_t mix_in: 2; // mix Left & Right IN channel data if both is configured,
//                        // 0: Not mixed, 1: mixed @ left channel, 2: mixed @ right channel (UNUSED)
    uint32_t mix_in: 1; // mix Left & Right IN channel data if both is configured,
                        // 0: Not mixed, 1: mixed @ left channel
//    uint32_t use_alt_in: 1; // use APC alternate IN dual_channel?
    uint32_t trim16_in: 1; // trim off low 16bit if IN sample bit width > 16

    uint32_t chbmp_out: 2; // current acquired OUT channel bitmap (Left@bit0, Right@bit1)
//    uint32_t mix_out: 2; // mix Left & Right OUT channel data if both is configured
//                         // 0: Not mixed, 1: mixed @ left channel, 2: mixed @ right channel
    uint32_t mix_out: 1; // mix Left & Right OUT channel data if both is configured
                         // 0: Not mixed, 1: mixed @ left channel
    uint32_t expd16_out: 1; // expand 16bit to 24/32 bits if OUT sample bit width > 16

    uint32_t tx_mode: 2; // MONO/STEREO IN, MONO/STEREO OUT
    uint32_t reserved : 3;
    uint32_t chbmp_echo: 2; // current acquired ECHO channel bitmap (Left@bit0, Right@bit1)
    uint32_t mix_echo: 1; // mix Left & Right OUT channel data? 0: Not mixed, 1: mixed

    uint32_t tdm_cnt: 8; // channel count in TDM mode (NOT support TDM mode if 0)
    uint32_t data_len: 8; // channel data length (in bit)

} I2S_CH_CFG;


// I2S information (Run-time)
typedef struct _I2S_INFO
{
    CSK_I2S_SignalEvent_t cb_event; // event callback
    uint32_t usr_param; // user parameter of event callback

    uint32_t flags: 7; // I2S driver flags
    uint32_t master: 1; // 0 = slave, 1 = master
    uint32_t clkin_idx :2; // input clock, 0: 24Mhz (ONLY OPTION)
    uint32_t protocol: 4;
    uint32_t apc_chmode: 2; // shared by IN & OUT channels

    // For I2S Philips/Left/Right justified, it is padding BCK count for a I2S L/R channel
    // For PCM mode 0/1 (TDM mode), it is padding BCK count for all TDM channels
    uint32_t pad_bcks: 8; // how many bit clocks for or TDM slot?

    uint32_t tx_nsynca_l: 1; // set NOT_SYNC_CACHE for left OUT channel if 1
    uint32_t tx_nsynca_r: 1; // set NOT_SYNC_CACHE for right OUTchannel if 1
    uint32_t rsvd2: 6;
    //uint32_t dch_in : 4; // which APC IN dual_channel?

    uint32_t samp_freq; // current sample rate
    CSK_I2S_STATUS status; // I2S status flags
} I2S_INFO;

typedef struct {
    uint8_t dev_idx; // 0=I2S0, 1=I2S1, 2=I2S2
    uint8_t rsvd1[3];

    CSK_I2S_RegDef *reg;  // pointer to I2S registers
    //uint32_t irq_num;      // I2S IRQ number
    //ISR irq_handler;

    I2S_APC_RES apc_res; // I2S apc channels resource (fixed)
    I2S_CH_CFG ch_cfg; // I2S channels configuration

    //TODO:

    I2S_INFO *info;  // I2S run-time information
} I2S_DEV;

I2S_DEV * safe_i2s_dev(void *i2s_dev);
int32_t i2s_enable_tx_channels(I2S_DEV *i2s, uint8_t ch_bmp_out);
int32_t i2s_enable_echo_channels(I2S_DEV *i2s, uint8_t ch_bmp_echo);
int32_t i2s_disable_tx_channels(I2S_DEV *i2s, uint8_t ch_bmp_out);
int32_t i2s_disable_echo_channels(I2S_DEV *i2s, uint8_t ch_bmp_echo);

static inline void enable_i2s(I2S_DEV *i2s) {
    //assert(i2s != NULL);
    if (i2s->reg->REG_I2S_CFG0.bit.ENABLE == 0)
        i2s->reg->REG_I2S_CFG0.bit.ENABLE = 1;
}

static inline void disable_i2s(I2S_DEV *i2s) {
    //assert(i2s != NULL);
    if (i2s->reg->REG_I2S_CFG0.bit.ENABLE == 1)
        i2s->reg->REG_I2S_CFG0.bit.ENABLE = 0;
}

#endif /* __I2S_ARCS_H */
