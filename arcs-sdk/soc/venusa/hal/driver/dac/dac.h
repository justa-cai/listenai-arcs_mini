/**
 * @file dac.h
 * @brief Digital-to-Analog Converter (DAC) Audio Interface
 *
 * This header file defines the structures, macros, and functions for controlling
 * and managing DAC audio functionality. It includes register mappings, configuration
 * options, and runtime information structures for DAC operations.
 */

#ifndef __DAC_AUDIO_H
#define __DAC_AUDIO_H

#include "apc.h"
#include "apc_reg.h"
#include "Driver_DAC.h"
#include "audio_codec.h" // CODEC Registers etc.

//====================== Register-related Macros & Functions =================

/**
 * @brief Sample Rate value mapping structure
 * 
 * Maps register values to actual sample rates in Hz.
 */
static const REG_VAL_MAP sc_DAC_SR_val_map[] = {
        { 0x0, 8000 },    ///< 8 kHz sample rate
        { 0x3, 16000 },   ///< 16 kHz sample rate
        { 0x5, 24000 },   ///< 24 kHz sample rate
        { 0x6, 32000 },   ///< 32 kHz sample rate
        { 0x8, 48000 },   ///< 48 kHz sample rate
        { 0x9, 96000 }    ///< 96 kHz sample rate
};

/**
 * @brief Over Sample Ratio value mapping structure
 * 
 * Maps register values to oversampling ratios.
 */
static const REG_VAL_MAP sc_DAC_OSR_val_map[] = {
        { 0x0, 500 },     ///< 500x oversampling
        { 0x1, 250 },     ///< 250x oversampling
};

/**
 * @brief DAC Sample Rate to Over Sample Ratio mapping
 * 
 * Defines the optimal oversampling ratio for each sample rate.
 */
static const VAL_PAIR sc_DAC_SR_OSR[] = {
        { 96000, 125 },   ///< 96 kHz with 125x OSR
        { 48000, 125 },   ///< 48 kHz with 125x OSR
        { 32000, 125 },   ///< 32 kHz with 125x OSR
        { 24000, 250 },   ///< 24 kHz with 250x OSR
        { 16000, 250 },   ///< 16 kHz with 250x OSR
        { 8000, 250 },    ///< 8 kHz with 250x OSR
};

//====================== Register-irrelevant definitions =====================

/**
 * @name DAC Device Flags
 * @brief Bitmask flags for DAC status and configuration
 * @{
 */
#define DAC_FLAG_INITIALIZED          (1UL << 0)     ///< DAC initialized
#define DAC_FLAG_POWERED              (1UL << 1)     ///< DAC powered on
#define DAC_FLAG_CONFIGURED           (1UL << 2)     ///< DAC configured
/** @} */

/**
 * @brief DAC runtime information structure
 * 
 * Contains all the runtime configuration and status information for the DAC.
 */
typedef struct _DAC_INFO {
    CSK_DAC_SignalEvent_t cb_event; ///< Event callback function pointer
    uint32_t usr_param;             ///< User parameter for event callback

    uint32_t flags: 6;              ///< DAC driver flags
    uint32_t use_16bits: 1;         ///< Sample format (0=32bit, 1=16bit)
    uint32_t ch_bmp: 2;             ///< Active output channels bitmap (Left=bit0, Right=bit1)
    uint32_t ch_mix: 1;             ///< Channel mixing enable (1=mix Left & Right)
    uint32_t src_stereo: 1;         ///< Source stereo/mono (1=stereo)

    uint32_t echo_bmp: 2;           ///< Active echo channels bitmap (Left=bit0, Right=bit1)
    uint32_t mix_echo: 1;           ///< Echo channel mixing enable (1=mixed)

    uint32_t nsynca_l: 1;           ///< Cache sync control for left channel (1=not sync)
    uint32_t nsynca_r: 1;           ///< Cache sync control for right channel (1=not sync)

    uint32_t over_samp_ratio: 16;   ///< Current oversampling ratio

    uint32_t samp_freq;             ///< Current sample rate in Hz
    CSK_DAC_STATUS status;          ///< DAC status flags
} DAC_INFO;

/**
 * @brief DAC group control structure
 * 
 * Contains hardware-specific configuration and control information for a DAC group.
 */
typedef struct {
    uint8_t dev_idx;                ///< Device index (0=DAC01)
    uint8_t apc_dch;                ///< APC OUT dual channel number
    uint8_t dch_echo;               ///< APC dual channel for ECHO (may be unused)
    uint8_t data_len;               ///< Channel data length in bits

    CSK_DAC_RegDef *reg;            ///< Pointer to CODEC(DAC) registers
    DAC_INFO *info;                 ///< Pointer to DAC runtime information
} DAC_GRP;

/**
 * @brief Safely access DAC group structure
 * @param dac_grp Pointer to DAC group structure
 * @return Valid DAC_GRP pointer or NULL if invalid
 */
DAC_GRP * safe_dac_grp(void *dac_grp);

/**
 * @brief Enable transmit channels
 * @param dac Pointer to DAC group structure
 * @param dev_bmp Bitmap of channels to enable
 * @return Execution status (0=success)
 */
int32_t dac_enable_tx_channels(DAC_GRP *dac, uint8_t dev_bmp);

/**
 * @brief Enable echo channels
 * @param dac Pointer to DAC group structure
 * @param echo_bmp Bitmap of echo channels to enable
 * @return Execution status (0=success)
 */
int32_t dac_enable_echo_channels(DAC_GRP *dac, uint8_t echo_bmp);

/**
 * @brief Disable echo channels
 * @param dac Pointer to DAC group structure
 * @param echo_bmp Bitmap of echo channels to disable
 * @return Execution status (0=success)
 */
int32_t dac_disable_echo_channels(DAC_GRP *dac, uint8_t echo_bmp);

#endif /* __DAC_AUDIO_H */
