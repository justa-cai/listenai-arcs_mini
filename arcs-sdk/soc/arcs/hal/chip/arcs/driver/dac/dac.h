/*
 * dac.h
 *
 *
 */

#ifndef __DAC_AUDIO_H
#define __DAC_AUDIO_H

#include "arcs_ap.h"
#include "apc.h"
#include "apc_reg.h"
#include "Driver_DAC.h"
//#include "pmuctrl_reg.h" // for pmuctrl regiger (power)

#include "audio_codec.h" // CODEC Registers etc.

//====================== Register-related Macros & Functions =================

// Sample Rate: refer to CODEC registers
static const REG_VAL_MAP sc_DAC_SR_val_map[] = {
        { 0x0, 8000 },
//        { 0x1, 11025 },
//        { 0x2, 12000 },
        { 0x3, 16000 },
//        { 0x4, 22050 },
        { 0x5, 24000 },
        { 0x6, 32000 },
//        { 0x7, 44100 },
        { 0x8, 48000 },
        { 0x9, 96000 }
};

// Over Sample Ratio: refer to CODEC registers
static const REG_VAL_MAP sc_DAC_OSR_val_map[] = {
        { 0x0, 500 },
        { 0x1, 250 },
};

//DAC Sample Rate : Over Sample Ratio
static const VAL_PAIR sc_DAC_SR_OSR[] = {
        { 96000, 125 },
        { 48000, 125 },
        { 32000, 125 },
        { 24000, 250 },
        { 16000, 250 },
        { 8000, 250 },
//        { 88200, 125 },
//        { 44100, 125 },
//        { 22050, 250 },
//        { 11025, 250 }
};


// APC register offset 0x134
//#define DAC_DATA_DUP_REG        (APC_BASE + 0x134)
//#define DAC_DATA_DUP_MASK       (0x3 << 0)
//#define DAC_DATA_DUP_NONE       (0x0 << 0)
//#define DAC_DATA_DUP_LEFT       (0x1 << 0)
//#define DAC_DATA_DUP_RIGHT      (0x2 << 0)

//====================== Register-irrelevant definitions =====================

// DAC device flags
#define DAC_FLAG_INITIALIZED          (1UL << 0)     // DAC initialized
#define DAC_FLAG_POWERED              (1UL << 1)     // DAC powered on
#define DAC_FLAG_CONFIGURED           (1UL << 2)     // DAC configured

// DAC information (Run-time)
typedef struct _DAC_INFO
{
    CSK_DAC_SignalEvent_t cb_event; // event callback
    uint32_t usr_param; // user parameter of event callback

    uint32_t flags: 6; // DAC driver flags
    uint32_t use_16bits: 1; // 0: use 32bits sample, 1: 16bits sample
    uint32_t ch_bmp: 2; // current acquired OUT channel bitmap (Left@bit0, Right@bit1)
    uint32_t ch_mix: 1; // mix Left & Right OUT channel data if both is configured, 1: mixed
    uint32_t src_stereo: 1; // audio source is stereo or mono channel, 1: stereo
    //uint32_t tx_cfg: 2; // tx channel configuration

    uint32_t echo_bmp: 2; // current acquired ECHO channel bitmap (Left@bit0, Right@bit1)
    uint32_t mix_echo: 1; // mix Left & Right OUT channel data? 0: Not mixed, 1: mixed

    uint32_t nsynca_l: 1; // set NOT_SYNC_CACHE for left channel if 1
    uint32_t nsynca_r: 1; // set NOT_SYNC_CACHE for right channel if 1

    uint32_t over_samp_ratio: 16;

    uint32_t samp_freq; // current sample rate
    CSK_DAC_STATUS status; // DAC status flags

} DAC_INFO;


typedef struct {
    uint8_t dev_idx; // 0=DAC01, only 0 is valid
    uint8_t apc_dch; // which APC OUT dual_channel?
    uint8_t dch_echo; // available APC dual_channel of ECHO (may be NOT used)
    uint8_t data_len; // channel data length (in bit)

    CSK_DAC_RegDef *reg;  // pointer to CODEC(DAC) registers
    DAC_INFO *info;  // DAC run-time information
} DAC_GRP;


DAC_GRP * safe_dac_grp(void *dac_grp);
int32_t dac_enable_tx_channels(DAC_GRP *dac, uint8_t dev_bmp);
int32_t dac_enable_echo_channels(DAC_GRP *dac, uint8_t echo_bmp);
int32_t dac_disable_echo_channels(DAC_GRP *dac, uint8_t echo_bmp);

#endif /* __DAC_AUDIO_H */
