/**
 * @file Driver_ADC_PDM.h
 * @brief ADC/PDM (Pulse Density Modulation) Driver Header File
 * 
 * @details This header defines the interface for controlling and managing ADC (Analog-to-Digital Converter)
 * and PDM (Pulse Density Modulation) interfaces. It provides functions for initialization, configuration,
 * data transfer, and control of ADC/PDM devices.
 * 
 * The driver supports:
 * - Multiple sample rates (8KHz, 16KHz, 48KHz)
 * - Various oversampling ratios (50, 100, 125, 250, 500)
 * - High-pass filter configuration
 * - Automatic Level Control (ALC)
 * - Ping-Pong mode data transfer
 * - Volume and mute control
 * - Mixing with digital echo (DAC or I2S)
 */

#ifndef __DRIVER_ADC_PDM_H
#define __DRIVER_ADC_PDM_H

#include "Driver_Common.h"

#define MIX_WITH_DAC_DIGTAL_ECHO     1 // 0
#define MIX_WITH_I2S_DIGTAL_ECHO     1 // 0

/**
 * @def CSK_ADC_PDM_API_VERSION
 * @brief ADC/PDM driver API version (major.minor)
 */
#define CSK_ADC_PDM_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

/****** ADC_PDM Control Codes *****/

/**
 * @name ADC_PDM Sample Rate Control Codes
 * @{
 */
#define CSK_ADCPDM_SR_Pos                           0
#define CSK_ADCPDM_SR_Msk                           (0xFUL << CSK_ADCPDM_SR_Pos)
#define CSK_ADCPDM_SR_UNSET                         (0x0UL << CSK_ADCPDM_SR_Pos)
#define CSK_ADCPDM_SR_8KHZ                          (0x1UL << CSK_ADCPDM_SR_Pos)
#define CSK_ADCPDM_SR_16KHZ                         (0x2UL << CSK_ADCPDM_SR_Pos)
#define CSK_ADCPDM_SR_48KHZ                         (0x4UL << CSK_ADCPDM_SR_Pos)
/** @} */

/**
 * @name ADC_PDM Oversampling Ratio Control Codes
 * @{
 */
#define CSK_ADCPDM_OSR_Pos                          4
#define CSK_ADCPDM_OSR_Msk                          (0x7UL << CSK_ADCPDM_OSR_Pos)
#define CSK_ADCPDM_OSR_UNSET                        (0x0UL << CSK_ADCPDM_OSR_Pos)
#define CSK_ADCPDM_OSR_500                          (0x1UL << CSK_ADCPDM_OSR_Pos)
#define CSK_ADCPDM_OSR_250                          (0x2UL << CSK_ADCPDM_OSR_Pos)
#define CSK_ADCPDM_OSR_125                          (0x3UL << CSK_ADCPDM_OSR_Pos)
#define CSK_ADCPDM_OSR_100                          (0x4UL << CSK_ADCPDM_OSR_Pos)
#define CSK_ADCPDM_OSR_50                           (0x5UL << CSK_ADCPDM_OSR_Pos)
/** @} */

/**
 * @name ADC_PDM High Pass Filter Control Codes
 * @{
 */
#define CSK_ADCPDM_ARG_HPF1_EN            (1 << 0)
#define CSK_ADCPDM_ARG_HPF2_EN            (1 << 1)
#define CSK_ADCPDM_ARG_HPF2_CUT(n)        (((n) & 0x7) << 2)

#define CSK_ADCPDM_HPF_Pos                          7
#define CSK_ADCPDM_HPF_Msk                          (0x1UL << CSK_ADCPDM_HPF_Pos)
#define CSK_ADCPDM_HPF_UNSET                        (0x0UL << CSK_ADCPDM_HPF_Pos)
#define CSK_ADCPDM_HPF_SET                          (0x1UL << CSK_ADCPDM_HPF_Pos)
/** @} */

/**
 * @name ADC_PDM RX Configuration Control Codes
 * @{
 */
#define CSK_ADCPDM_RXCFG_Pos                        8
#define CSK_ADCPDM_RXCFG_Msk                        (0x3UL << CSK_ADCPDM_RXCFG_Pos)
#define CSK_ADCPDM_RXCFG_UNSET                      (0x0UL << CSK_ADCPDM_RXCFG_Pos)
#define CSK_ADCPDM_RXCFG_SEPA                       (0x1UL << CSK_ADCPDM_RXCFG_Pos)
#define CSK_ADCPDM_RXCFG_MIXED                      (0x2UL << CSK_ADCPDM_RXCFG_Pos)
/** @} */

/**
 * @name ADC_PDM Latch Delay Control Codes
 * @{
 */
#define CSK_ADCPDM_LATCH_DELAY_Pos                  10
#define CSK_ADCPDM_LATCH_DELAY_Msk                  (0x7UL << CSK_ADCPDM_LATCH_DELAY_Pos)
#define CSK_ADCPDM_LATCH_DELAY_UNSET                (0x0UL << CSK_ADCPDM_LATCH_DELAY_Pos)
#define CSK_ADCPDM_LATCH_DELAY_DGR0                 (0x1UL << CSK_ADCPDM_LATCH_DELAY_Pos)
#define CSK_ADCPDM_LATCH_DELAY_NO                   CSK_ADCPDM_LATCH_DELAY_DGR0
#define CSK_ADCPDM_LATCH_DELAY_DGR90                (0x2UL << CSK_ADCPDM_LATCH_DELAY_Pos)
#define CSK_ADCPDM_LATCH_DELAY_DGR180               (0x3UL << CSK_ADCPDM_LATCH_DELAY_Pos)
#define CSK_ADCPDM_LATCH_DELAY_DGR270               (0x4UL << CSK_ADCPDM_LATCH_DELAY_Pos)
/** @} */

/**
 * @name ADC_PDM PGA Input Mode Control Codes
 * @{
 */
#define CSK_ADCPDM_ARG_LPGA_INPUT_DIFFER        (0 << 5)
#define CSK_ADCPDM_ARG_LPGA_INPUT_SINGLE        (1 << 5)
#define CSK_ADCPDM_ARG_RPGA_INPUT_DIFFER        (0 << 6)
#define CSK_ADCPDM_ARG_RPGA_INPUT_SINGLE        (1 << 6)

#define CSK_ADCPDM_PGA_INPUT_Pos                11
#define CSK_ADCPDM_PGA_INPUT_Msk                (0x1UL << CSK_ADCPDM_PGA_INPUT_Pos)
#define CSK_ADCPDM_PGA_INPUT_UNSET              (0x0UL << CSK_ADCPDM_PGA_INPUT_Pos)
#define CSK_ADCPDM_PGA_INPUT_SET                (0x1UL << CSK_ADCPDM_PGA_INPUT_Pos)
/** @} */

/**
 * @name ADC_PDM Exclusive Operation Control Codes
 * @{
 */
#define CSK_ADCPDM_EXCL_OP_Pos                 29
#define CSK_ADCPDM_EXCL_OP_Msk                 (0x7UL << CSK_ADCPDM_EXCL_OP_Pos)
#define CSK_ADCPDM_EXCL_OP_UNSET               (0x0UL << CSK_ADCPDM_EXCL_OP_Pos)
#define CSK_ADCPDM_ABORT_TRANSFER              (0x1UL << CSK_ADCPDM_EXCL_OP_Pos)
#define CSK_ADCPDM_GET_SAMP_RATE               (0x2UL << CSK_ADCPDM_EXCL_OP_Pos)
#define CSK_ADCPDM_SET_ALC_PARAMS              (0x3UL << CSK_ADCPDM_EXCL_OP_Pos)
#define CSK_ADCPDM_DO_CALIBRATION              (0x4UL << CSK_ADCPDM_EXCL_OP_Pos)
/** @} */

/**
 * @defgroup ALC_FLAGS Automatic Level Control Flags
 * @brief Flags for configuring Automatic Level Control (ALC) parameters
 * @{
 */
#define ALC_FLAG_ALC_SEL_L      (1 << 0)
#define ALC_FLAG_ALC_SEL_R      (1 << 1)
#define ALC_FLAG_ERR_TOLERANCE  (1 << 2)
#define ALC_FLAG_TARGET_L       (1 << 3)
#define ALC_FLAG_TARGET_R       (1 << 4)
#define ALC_FLAG_ALC_MODE       (1 << 5)
#define ALC_FLAG_NGATE_EN       (1 << 6)
#define ALC_FLAG_NGATE_FLOOR    (1 << 7)
#define ALC_FLAG_ALC_MIN        (1 << 8)
#define ALC_FLAG_ALC_MAX        (1 << 9)
#define ALC_FLAG_ALC_HOLD       (1 << 10)
#define ALC_FLAG_ALC_ATTACK     (1 << 11)
#define ALC_FLAG_ALC_DECAY      (1 << 12)
/** @} */

/**
 * @struct ALC_PARAMS
 * @brief Automatic Level Control (ALC) parameters structure
 */
typedef struct {
    uint32_t alc_flags;     /**< Bitmask indicating which fields are specified */

    uint32_t alc_sel_l      :1; /**< ADC Left ALC function enable */
    uint32_t alc_sel_r      :1; /**< ADC Right ALC function enable */
    uint32_t err_tolerance  :3; /**< ADC ALC target error tolerance setting */
    uint32_t target_l       :5; /**< ADC Left channel ALC target level */
    uint32_t target_r       :5; /**< ADC Right channel ALC target level */
    uint32_t alc_mode       :1; /**< 1: limiter mode, 0: normal mode */

    uint32_t ngate_en       :1; /**< ADC ALC Noise Gate enable */
    uint32_t ngate_floor    :5; /**< ADC ALC noise floor level setting */
    uint32_t alc_min        :5; /**< Min ADC PGA gain used in ALC mode */
    uint32_t alc_max        :5; /**< Max ADC PGA gain used in ALC mode */

    uint32_t alc_hold       :4; /**< ADC ALC hold time before gain is increased */
    uint32_t alc_attack     :4; /**< ADC ALC attack (gain ramp-down) time */
    uint32_t alc_decay      :4; /**< ADC Decay (gain ramp-up) time */
    uint32_t reserved       :20; /**< Reserved for future use */
} ALC_PARAMS;

/****** ADCPDM specific error codes *****/
/**
 * @name ADC_PDM Specific Error Codes
 * @{
 */
#define CSK_ADCPDM_ERROR_SAMP_RATE              (CSK_DRIVER_ERROR_SPECIFIC - 1) /**< Specified Sample Rate not supported */
#define CSK_ADCPDM_ERROR_OVER_SAMP_RATIO        (CSK_DRIVER_ERROR_SPECIFIC - 2) /**< Specified Over Sample Ratio not supported */
#define CSK_ADCPDM_ERROR_SR_OSR_PAIR            (CSK_DRIVER_ERROR_SPECIFIC - 3) /**< Specified combination of Sample Rate & Over Sample Ratio not supported */
#define CSK_ADCPDM_ERROR_RXCFG                  (CSK_DRIVER_ERROR_SPECIFIC - 4) /**< Specified RX Config (Mix or not) not supported */
#define CSK_ADCPDM_ERROR_ALC_PARAMS             (CSK_DRIVER_ERROR_SPECIFIC - 5) /**< Specified ALC parameters are invalid */
#define CSK_ADCPDM_ERROR_LATCH_DELAY            (CSK_DRIVER_ERROR_SPECIFIC - 6) /**< Specified Latch delay is invalid */
#define CSK_ADCPDM_ERROR_INITED_ALREADY         (CSK_DRIVER_ERROR_SPECIFIC - 8) /**< ADC_PDM has already been initialized */
/** @} */

/**
 * @struct CSK_ADCPDM_STATUS_BIT
 * @brief ADC/PDM status bit field structure
 */
typedef struct _CSK_ADCPDM_STATUS_BIT {
    uint32_t busy :2;       /**< Receive busy flag, bit[0] for left channel, bit[1] for right channel */
    uint32_t rx_full :1;    /**< Receive RX FIFO full (cleared on start of transfer operation) */
    uint32_t rx_ovf :1;     /**< Receive RX FIFO overflow (cleared on start of transfer operation) */
    uint32_t l_mute :1;     /**< ADC0/2 (left channel) is mute or not, 1 means mute */
    uint32_t r_mute :1;     /**< ADC1/3 (right channel) is mute or not, 1 means mute */
    uint32_t reserved :26;  /**< Reserved for future use */
} CSK_ADCPDM_STATUS_BIT;

/**
 * @union CSK_ADCPDM_STATUS
 * @brief ADC/PDM status union (all bits or individual fields)
 */
typedef union {
    uint32_t all;                   /**< Complete status as 32-bit value */
    CSK_ADCPDM_STATUS_BIT bit;      /**< Status accessed by bit fields */
} CSK_ADCPDM_STATUS;

/****** ADCPDM Event *****/
/**
 * @name ADC_PDM Event Definitions
 * @{
 */
#define CSK_ADCPDM_EVENT_RECEIVE_COMPLETE       (0x1UL << 0) /**< Data Receive completed */
#define CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN        (0x1UL << 1) /**< Data Receive overflow */
#define CSK_ADCPDM_EVENT_RX_FIFO_FULL           (0x1UL << 2) /**< Data Receive full */
//#define CSK_ADCPDM_EVENT_RX_PING_DONE           (0x1UL << 3) /**< Data RX Ping Transfer Done */
//#define CSK_ADCPDM_EVENT_RX_PONG_DONE           (0x1UL << 4) /**< Data RX Pong Transfer Done */
#define CSK_ADCPDM_EVENT_BLOCK_COMPLETE         (0x1UL << 3) /**< Data RX Block Transfer Done */
#define CSK_ADCPDM_EVENT_OTHER_ERROR            (0x1UL << 7) /**< Other Error */
//#define CSK_ADCPDM_EVENT_BLOCK_COMPLETE         (CSK_ADCPDM_EVENT_RX_PING_DONE | CSK_ADCPDM_EVENT_RX_PONG_DONE)
/** @} */

/**
 * @typedef CSK_ADC_PDM_SignalEvent_t
 * @brief Callback function type for signaling ADC/PDM events
 * @param event_info ADC/PDM event and channel information
 *        bit[7:0] is event type, bit[15:8] is ADC/PDM channel number
 *        bit[23:16] indicate APC dual_channel number
 * @param usr_param User parameter passed to callback
 */
typedef void (*CSK_ADC_PDM_SignalEvent_t)(uint32_t event_info, uint32_t usr_param);

/**
 * @def CSK_ADC_PDM_SAMPLE_BITS
 * @brief Number of bits per sample (24 bits supported with PDM_DMIC, ADC and DAC)
 */
#define CSK_ADC_PDM_SAMPLE_BITS         24

/* Function Documentation */

/**
 * @brief Get driver version
 * @return Driver version information
 */
CSK_DRIVER_VERSION ADC_PDM_GetVersion();

/**
 * @name ADC_PDM Device Bitmap Flags
 * @{
 */
#define ADC_PDM_BMP_LEFT        CH_BMP_LEFT     /**< (0x1 << 0) Left channel bitmap */
#define ADC_PDM_BMP_RIGHT       CH_BMP_RIGHT    /**< (0x1 << 1) Right channel bitmap */
#define ADC_PDM_BMP_STEREO      CH_BMP_STEREO   /**< (0x3 << 0) Stereo channel bitmap */
/** @} */

/**
 * @name ADC_PDM Bitmap Configuration Flags
 * @{
 */
#define ADC_PDM_BMP_FLAG_IN_LEFT        (0x1 << 0) /**< bit[0] for ADC/PDM Left Channel (ADC0/DMIC0) */
#define ADC_PDM_BMP_FLAG_IN_RIGHT       (0x1 << 1) /**< bit[1] for ADC/PDM Right Channel (ADC1/DMIC1) */
#define ADC_PDM_BMP_FLAG_IN_STEREO      (0x3 << 0) /**< bit[1:0] for ADC/PDM Left & Right Channels */

#define ADC_PDM_BMP_FLAG_USE_PDM        (0x1 << 4) /**< bit[4] = 1 indicates PDM/DMIC, else AMIC */
#define ADC_PDM_BMP_FLAG_USE_16BITS     (0x1 << 5) /**< bit[5] = 1 indicates 16bits sample, else 32bits */

#define ADC_PDM_BMP_FLAG_IN_POS         0 /**< bit[1:0] for ADC/PDM */
/** @} */

/**
 * @struct ADC_PDM_DMA_CHS
 * @brief DMA channel configuration structure for ADC/PDM
 */
typedef struct {
    uint32_t dma_ch_in_left : 8;  /**< DMA channel number for ADC/PDM IN Left channel */
    uint32_t dma_ch_in_right : 8; /**< DMA channel number for ADC/PDM IN Right channel */
    uint32_t reserved : 16;       /**< Reserved for future use */
} ADC_PDM_DMA_CHS;

/**
 * @brief Initialize ADC/PDM device group interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param cb_event Pointer to event callback function
 * @param usr_param User parameter passed to callback
 * @param dev_bmp_flag Device bitmap and configuration flags
 * @param dma_chs_p Pointer to DMA channel configuration (NULL if not used)
 * @return Execution status
 */
int32_t ADC_PDM_Initialize(void *adc_pdm_grp, CSK_ADC_PDM_SignalEvent_t cb_event, uint32_t usr_param,
                        uint32_t dev_bmp_flag, ADC_PDM_DMA_CHS *dma_chs_p);

/**
 * @brief De-initialize ADC/PDM device group interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @return Execution status
 */
int32_t ADC_PDM_Uninitialize(void *adc_pdm_grp);

/**
 * @brief Control ADC/PDM interface power state
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param state Power state to set
 * @return Execution status
 */
int32_t ADC_PDM_PowerControl(void *adc_pdm_grp, CSK_POWER_STATE state);

/**
 * @name ADC_PDM Receive Flags
 * @{
 */
#define ADC_PDM_RX_FLAG_START_NOW   (0x1 << 0) /**< Start sending immediately */
#define ADC_PDM_RX_FLAG_QUICK_CHK   (0x1 << 1) /**< Do quick check for parameters & status */
/** @} */

/**
 * @brief Receive data from ADC/PDM interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param data Pointer to buffer for received data
 * @param num Number of data items to receive
 * @param dev_bmp Device bitmap (which channels to receive)
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t ADC_PDM_Receive(void *adc_pdm_grp, uint32_t *data, uint32_t num,
                    uint8_t dev_bmp, uint8_t rx_flag);

/**
 * @brief Receive data in Ping-Pong mode
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param blks Pointer to Ping-Pong block array
 * @param blk_cnt_p Pointer to block count (input/output)
 * @param dev_bmp Device bitmap (which channels to receive)
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t ADC_PDM_Receive_PiPo(void *adc_pdm_grp, PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p,
                      uint8_t dev_bmp, uint8_t rx_flag);

/**
 * @brief Get transferred blocks in Ping-Pong mode
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param blks Pointer to Ping-Pong block array
 * @param blk_cnt Number of blocks in array
 * @param dev_bmp Device bitmap
 * @return Number of transferred blocks or error code
 */
int32_t ADC_PDM_PiPo_Xferred_Blocks(void *adc_pdm_grp, PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp);

/**
 * @brief Enable ADC/PDM interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dev_bmp Device bitmap (which channels to enable)
 * @return Execution status
 */
int32_t ADC_PDM_Enable(void *adc_pdm_grp, uint8_t dev_bmp);

/**
 * @brief Disable ADC/PDM interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dev_bmp Device bitmap (which channels to disable)
 * @return Execution status
 */
int32_t ADC_PDM_Disable(void *adc_pdm_grp, uint8_t dev_bmp);

/**
 * @brief Abort ADC/PDM data transfer
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dev_bmp Device bitmap (which channels to abort)
 * @return Execution status
 */
int32_t ADC_PDM_Abort(void *adc_pdm_grp, uint8_t dev_bmp);

/**
 * @brief Get received data count
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dev_bmp Device bitmap (which channels to query)
 * @return Number of data items transferred or error code
 */
int32_t ADC_PDM_GetRxCount(void *adc_pdm_grp, uint8_t dev_bmp);

/**
 * @brief Control ADC/PDM interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param control Control operation code
 * @param arg Operation argument
 * @return Execution status
 */
int32_t ADC_PDM_Control(void *adc_pdm_grp, uint32_t control, uint32_t arg);

/**
 * @name ADC_PDM Volume Control Flags
 * @{
 */
#define ADC_PDM_VOL_FLAG_A_LEFT     0x1UL /**< Analog Gain of Left Channel */
#define ADC_PDM_VOL_FLAG_A_RIGHT    0x2UL /**< Analog Gain of Right Channel */
#define ADC_PDM_VOL_FLAG_D_LEFT     0x4UL /**< Digital Gain of Left Channel */
#define ADC_PDM_VOL_FLAG_D_RIGHT    0x8UL /**< Digital Gain of Right Channel */
/** @} */

/**
 * @name ADC_PDM Analog Gain Definitions
 * @{
 */
#define ADC_PDM_GAIN_A_MAX_DB       36    /**< Maximum analog gain in dB */
#define ADC_PDM_GAIN_A_MIN_DB       -12   /**< Minimum analog gain in dB */
#define ADC_PDM_GAIN_A_DEF_DB       0     /**< Default analog gain in dB */
#define ADC_PDM_GAIN_A_DB_STEP      2     /**< Analog gain step size in dB */

#define ADC_PDM_GAIN_A_MAX_VAL      0x18  /**< Maximum analog gain register value */
#define ADC_PDM_GAIN_A_MIN_VAL      0x0   /**< Minimum analog gain register value */
#define ADC_PDM_GAIN_A_DEF_VAL      0x6   /**< Default analog gain register value */
#define ADC_PDM_GAIN_A_VAL(dB)      ((ADC_PDM_GAIN_A_MIN_VAL + (dB - ADC_PDM_GAIN_A_MIN_DB + \
                                    ADC_PDM_GAIN_A_DB_STEP - 1) / ADC_PDM_GAIN_A_DB_STEP) & 0x1F)
/** @} */

/**
 * @name ADC_PDM Digital Gain Definitions
 * @{
 */
#define ADC_PDM_GAIN_D_MAX_DB       42    /**< Maximum digital gain in dB */
#define ADC_PDM_GAIN_D_MIN_DB       -83   /**< Minimum digital gain in dB */
#define ADC_PDM_GAIN_D_DEF_DB       0     /**< Default digital gain in dB */
#define ADC_PDM_GAIN_D_DB_STEP      1     /**< Digital gain step size in dB */

#define ADC_PDM_GAIN_D_MAX_VAL      0x7F  /**< Maximum digital gain register value */
#define ADC_PDM_GAIN_D_MIN_VAL      0x2   /**< Minimum digital gain register value */
#define ADC_PDM_GAIN_D_DEF_VAL      0x55  /**< Default digital gain register value */
#define ADC_PDM_GAIN_D_VAL(dB)      ((ADC_PDM_GAIN_D_MIN_VAL + (dB - ADC_PDM_GAIN_D_MIN_DB + \
                                    ADC_PDM_GAIN_D_DB_STEP - 1) / ADC_PDM_GAIN_D_DB_STEP) & 0x7F)
/** @} */

/**
 * @brief Set ADC/PDM volume (analog and/or digital gain)
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param a_gain Analog gain (left in [15:0], right in [31:16])
 * @param d_gain Digital gain (left in [15:0], right in [31:16])
 * @param vol_flag Flags indicating which gains to set
 * @return Execution status
 */
int32_t ADC_PDM_SetVolume(void *adc_pdm_grp, uint32_t a_gain, uint32_t d_gain, uint32_t vol_flag);

/**
 * @brief Set ADC/PDM mute state
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param mute_val Mute value (bit0: left, bit1: right)
 * @param dev_bmp Device bitmap (which channels to mute)
 * @return Execution status
 */
int32_t ADC_PDM_SetMute(void *adc_pdm_grp, uint8_t mute_val, uint8_t dev_bmp);

/**
 * @brief Get ADC/PDM status
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param status Pointer to status structure
 * @return Execution status
 */
int32_t ADC_PDM_GetStatus(void *adc_pdm_grp, CSK_ADCPDM_STATUS *status);

/**
 * @brief Get ADC/PDM01 device group instance (Always ON power domain)
 * @return Pointer to ADC/PDM01 device group instance
 */
void* ADC_PDM01();

/* Digital Echo Mixing Functions */
#if MIX_WITH_DAC_DIGTAL_ECHO
/**
 * @brief Receive data from ADC with DAC digital echo (2 channels: 1ch ADC + 1ch ECHO)
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dac_grp Pointer to DAC device group instance
 * @param data Pointer to data buffer
 * @param num Number of data items
 * @param dev_bmp Device bitmap (which channel to RX)
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t ADC_PDM_ECHO_TwoReceive(void *adc_pdm_grp, void *dac_grp,
                            uint32_t *data, uint32_t num, uint8_t dev_bmp, uint8_t rx_flag);

/**
 * @brief Receive data in Ping-Pong mode with DAC digital echo (2 channels: 1ch ADC + 1ch ECHO)
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dac_grp Pointer to DAC device group instance
 * @param blks Pointer to Ping-Pong block array
 * @param blk_cnt_p Pointer to block count (input/output)
 * @param dev_bmp Device bitmap (which channel to RX)
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t ADC_PDM_ECHO_TwoReceive_PiPo(void *adc_pdm_grp, void *dac_grp,
                         PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p, uint8_t dev_bmp, uint8_t rx_flag);

/**
 * @brief Receive data from ADC with DAC digital echo (3 channels: 2ch ADC + 1ch ECHO)
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dac_grp Pointer to DAC device group instance
 * @param data Pointer to data buffer
 * @param num Number of data items
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t ADC_PDM_ECHO_TriReceive(void *adc_pdm_grp, void *dac_grp,
                            uint32_t *data, uint32_t num, uint8_t rx_flag);

/**
 * @brief Receive data in Ping-Pong mode with DAC digital echo (3 channels: 2ch ADC + 1ch ECHO)
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dac_grp Pointer to DAC device group instance
 * @param blks Pointer to Ping-Pong block array
 * @param blk_cnt_p Pointer to block count (input/output)
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t ADC_PDM_ECHO_TriReceive_PiPo(void *adc_pdm_grp, void *dac_grp,
                         PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p, uint8_t rx_flag);
#endif // MIX_WITH_DAC_DIGTAL_ECHO

#if MIX_WITH_I2S_DIGTAL_ECHO
/**
 * @brief Receive data from ADC with I2S digital echo (4 channels: 2ch ADC + 2ch ECHO)
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param i2s_out Pointer to I2S output device instance
 * @param data Pointer to data buffer
 * @param num Number of data items
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t ADC_PDM_ECHO_QuadReceive(void *adc_pdm_grp, void *i2s_out,
                            uint32_t *data, uint32_t num, uint8_t rx_flag);

/**
 * @brief Receive data in Ping-Pong mode with I2S digital echo
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param i2s_out Pointer to I2S output device instance
 * @param blks Pointer to Ping-Pong block array
 * @param blk_cnt_p Pointer to block count (input/output)
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t ADC_PDM_ECHO_QuadReceive_PiPo(void *adc_pdm_grp, void *i2s_out,
                         PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p, uint8_t rx_flag);
#endif // MIX_WITH_I2S_DIGTAL_ECHO

#if (MIX_WITH_DAC_DIGTAL_ECHO || MIX_WITH_I2S_DIGTAL_ECHO)
/**
 * @brief Get transferred blocks in Ping-Pong mode with digital echo
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param out_dev Pointer to output device instance (DAC or I2S)
 * @param blks Pointer to Ping-Pong block array
 * @param blk_cnt Number of blocks in array
 * @param dev_bmp Device bitmap
 * @return Number of transferred blocks or error code
 */
int32_t ADC_PDM_ECHO_PiPo_Xferred_Blocks(void *adc_pdm_grp, void *out_dev,
                        PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp);
#endif // (MIX_WITH_DAC_DIGTAL_ECHO || MIX_WITH_I2S_DIGTAL_ECHO)

#endif /* __DRIVER_ADC_PDM_H */
