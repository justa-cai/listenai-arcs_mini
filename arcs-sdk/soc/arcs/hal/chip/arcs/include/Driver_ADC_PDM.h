/*
 * Driver_ADC_PDM.h
 *
 *
 */

#ifndef __DRIVER_ADC_PDM_H
#define __DRIVER_ADC_PDM_H

#include "Driver_Common.h"

#define MIX_WITH_DAC_DIGTAL_ECHO     1 // 0
#define MIX_WITH_I2S_DIGTAL_ECHO     1 // 0
#define CSK_ADC_PDM_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */

/****** ADC_PDM Control Codes *****/

/*----- ADC_PDM Control Codes: Configuration Parameters: Sample Rate -----*/
#define CSK_ADCPDM_SR_Pos                           0
#define CSK_ADCPDM_SR_Msk                           (0xFUL << CSK_ADCPDM_SR_Pos) // bit[3:0], 4 bits
#define CSK_ADCPDM_SR_UNSET                         (0x0UL << CSK_ADCPDM_SR_Pos) // Sample Rate is kept unchanged or default
#define CSK_ADCPDM_SR_8KHZ                          (0x1UL << CSK_ADCPDM_SR_Pos) // Sample Rate = 8KHz
#define CSK_ADCPDM_SR_16KHZ                         (0x2UL << CSK_ADCPDM_SR_Pos) // Sample Rate = 16KHz
//#define CSK_ADCPDM_SR_44P1KHZ                       (0x3UL << CSK_ADCPDM_SR_Pos) // Sample Rate = 44.1KHz
#define CSK_ADCPDM_SR_48KHZ                         (0x4UL << CSK_ADCPDM_SR_Pos) // Sample Rate = 48KHz

/*----- ADC_PDM Control Codes: Configuration Parameters: Over Sample Ratio -----*/
#define CSK_ADCPDM_OSR_Pos                          4
#define CSK_ADCPDM_OSR_Msk                          (0x7UL << CSK_ADCPDM_OSR_Pos) // bit[6:4], 3 bits
#define CSK_ADCPDM_OSR_UNSET                        (0x0UL << CSK_ADCPDM_OSR_Pos) // Over Sample Ratio is kept unchanged or default
#define CSK_ADCPDM_OSR_500                          (0x1UL << CSK_ADCPDM_OSR_Pos) // Over Sample Ratio = 500
#define CSK_ADCPDM_OSR_250                          (0x2UL << CSK_ADCPDM_OSR_Pos) // Over Sample Ratio = 250
#define CSK_ADCPDM_OSR_125                          (0x3UL << CSK_ADCPDM_OSR_Pos) // Over Sample Ratio = 125
#define CSK_ADCPDM_OSR_100                          (0x4UL << CSK_ADCPDM_OSR_Pos) // Over Sample Ratio = 100
#define CSK_ADCPDM_OSR_50                           (0x5UL << CSK_ADCPDM_OSR_Pos) // Over Sample Ratio = 50

/*----- ADC_PDM Control Codes: Configuration Parameters: High Pass Filter -----*/
// HPF1 & HPF2 Settings @ arg:
// bit[0] enable/disable HPF1, 1 = enable, 0 = disable;
// bit[1] enable/disable HPF2, 1 = enable, 0 = disable;
// bit[4:2] HPF2 cutoff frequency, and following options are listed below:
//  48Khz Fs(Sample Rate):
//        000: 122Hz                    001: 153Hz
//        010: 156Hz                    011: 245Hz
//        100: 306Hz                    101: 392Hz
//        110: 490Hz                    111: 612Hz
//  44.1KHz Fs(Sample Rate):
//        000: 112Hz                    001: 140Hz
//        010: 143Hz                    011: 225Hz
//        100: 281Hz                    101: 360Hz
//        110: 450Hz                    111: 562Hz
//  16KHz Fs(Sample Rate):
//        000: 41Hz                       001: 51Hz
//        010: 52Hz                       011: 82Hz
//        100: 102Hz                    101: 131Hz
//        110: 186Hz                    111: 204Hz
//  8KHz Fs(Sample Rate):
//        000: 21Hz                       001: 25Hz
//        010: 26Hz                       011: 41Hz
//        100: 51Hz                       101: 66Hz
//        110: 93Hz                       111: 102Hz
#define CSK_ADCPDM_ARG_HPF1_EN            (1 << 0)
#define CSK_ADCPDM_ARG_HPF2_EN            (1 << 1)
#define CSK_ADCPDM_ARG_HPF2_CUT(n)        (((n) & 0x7) << 2)

//For other Sample Rate setting,  Please zoom in/out linearly and calculate their cut-off frequency;
//NOTE: The cutoff frequency of HPF1 is fixed, and around 3.7Hz (Fs=48KHz).
#define CSK_ADCPDM_HPF_Pos                          7
#define CSK_ADCPDM_HPF_Msk                          (0x1UL << CSK_ADCPDM_HPF_Pos) // bit[7], 1 bit
#define CSK_ADCPDM_HPF_UNSET                        (0x0UL << CSK_ADCPDM_HPF_Pos) // HPF1 & HPF2 settings are kept unchanged or default
#define CSK_ADCPDM_HPF_SET                          (0x1UL << CSK_ADCPDM_HPF_Pos) // Set HPF1 & HPF2, detailed settings @ arg[4:0] (see above)

/*----- ADC_PDM Control Codes: Configuration Parameters: RX config. (mixed data input or not?) -----*/
#define CSK_ADCPDM_RXCFG_Pos                        8
#define CSK_ADCPDM_RXCFG_Msk                        (0x3UL << CSK_ADCPDM_RXCFG_Pos) // bit[9:8], 2 bits
 // RX config (Mix Mode) is kept unchanged or default
#define CSK_ADCPDM_RXCFG_UNSET                      (0x0UL << CSK_ADCPDM_RXCFG_Pos)
// [default] IN data from ADC device group(i.e. ADC01 or ADC23) are separated
#define CSK_ADCPDM_RXCFG_SEPA                       (0x1UL << CSK_ADCPDM_RXCFG_Pos)
// IN data  from ADC device group(i.e. ADC01 or ADC23) are mixed together
#define CSK_ADCPDM_RXCFG_MIXED                      (0x2UL << CSK_ADCPDM_RXCFG_Pos)

/*----- ADC_PDM Control Codes: Configuration Parameters: Latch delay (DMIC ONLY, input data timing adjust) -----*/
#define CSK_ADCPDM_LATCH_DELAY_Pos                  10
#define CSK_ADCPDM_LATCH_DELAY_Msk                  (0x7UL << CSK_ADCPDM_LATCH_DELAY_Pos) // bit[12:10], 3 bits
 // Latch delay is kept unchanged or default
#define CSK_ADCPDM_LATCH_DELAY_UNSET                (0x0UL << CSK_ADCPDM_LATCH_DELAY_Pos)
// [default] 0 degree delay, i.e. NO Latch delay
#define CSK_ADCPDM_LATCH_DELAY_DGR0                 (0x1UL << CSK_ADCPDM_LATCH_DELAY_Pos)
#define CSK_ADCPDM_LATCH_DELAY_NO                   CSK_ADCPDM_LATCH_DELAY_DGR0
// 90 degree delay, i.e. 1/4 DMIC clock cycle delay
#define CSK_ADCPDM_LATCH_DELAY_DGR90                (0x2UL << CSK_ADCPDM_LATCH_DELAY_Pos)
// 180 degree delay, i.e. 1/2 DMIC clock cycle delay
#define CSK_ADCPDM_LATCH_DELAY_DGR180               (0x3UL << CSK_ADCPDM_LATCH_DELAY_Pos)
// 270 degree delay, i.e. 3/4 DMIC clock cycle delay
#define CSK_ADCPDM_LATCH_DELAY_DGR270               (0x4UL << CSK_ADCPDM_LATCH_DELAY_Pos)

/*----- ADC_PDM Control Codes: Configuration Parameters: PGA input mode (ADC ONLY, differential or single-ended) -----*/
// Left(ADC0) & Right(ADC1) PGA Input mode settings @ arg:
// bit[5] Left(ADC0) PGA Input mode, 0 = differential (default), 1 = single-ended;
// bit[6] Right(ADC1) PGA Input mode, 0 = differential (default), 1 = single-ended;
#define CSK_ADCPDM_ARG_LPGA_INPUT_DIFFER        (0 << 5)
#define CSK_ADCPDM_ARG_LPGA_INPUT_SINGLE        (1 << 5)
#define CSK_ADCPDM_ARG_RPGA_INPUT_DIFFER        (0 << 6)
#define CSK_ADCPDM_ARG_RPGA_INPUT_SINGLE        (1 << 6)

#define CSK_ADCPDM_PGA_INPUT_Pos                11
#define CSK_ADCPDM_PGA_INPUT_Msk                (0x1UL << CSK_ADCPDM_PGA_INPUT_Pos) // bit[11], 1 bit
#define CSK_ADCPDM_PGA_INPUT_UNSET              (0x0UL << CSK_ADCPDM_PGA_INPUT_Pos) // L&R PGA Input mode settings are kept unchanged or default
#define CSK_ADCPDM_PGA_INPUT_SET                (0x1UL << CSK_ADCPDM_PGA_INPUT_Pos) // Set L&R PGA Input mode, detailed @ arg[6:5] (see above)

//TODO: other control codes...

/*----- ADCPDM Control Codes: Exclusive Controls -----*/
/*----- exclusive operations, CANNOT coexist with other Control Codes -----*/
#define CSK_ADCPDM_EXCL_OP_Pos                 29
#define CSK_ADCPDM_EXCL_OP_Msk                 (0x7UL << CSK_ADCPDM_EXCL_OP_Pos) // bit[31:29], 3 bits
#define CSK_ADCPDM_EXCL_OP_UNSET               (0x0UL << CSK_ADCPDM_EXCL_OP_Pos) // NO exclusive operations
#define CSK_ADCPDM_ABORT_TRANSFER              (0x1UL << CSK_ADCPDM_EXCL_OP_Pos) // Abort current data transfer, arg bit[1:0] = dev_bmp
#define CSK_ADCPDM_GET_SAMP_RATE               (0x2UL << CSK_ADCPDM_EXCL_OP_Pos) // Get sample rate, return sample rate
#define CSK_ADCPDM_SET_ALC_PARAMS              (0x3UL << CSK_ADCPDM_EXCL_OP_Pos) // Set ALC parameters, arg = pointer to ALC_PARAMS
#define CSK_ADCPDM_DO_CALIBRATION              (0x4UL << CSK_ADCPDM_EXCL_OP_Pos) // Start ADC CAP calibration process, only for ADC

//TODO: other exclusive operation code...

//----------------------------------------
//ALC = Automatic Level(or Gain) Control
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

typedef struct {
    uint32_t alc_flags;     // indicate which following fields are specified

    uint32_t alc_sel_l      :1; // ADC Left ALC function enable
    uint32_t alc_sel_r      :1; // ADC Right ALC function enable
    uint32_t err_tolerance  :3; // ADC ALC target error tolerance setting
    uint32_t target_l       :5; // ADC Left channel ALC target level
    uint32_t target_r       :5; // ADC Right channel ALC target level
    uint32_t alc_mode       :1; // 1: limiter mode, 0: normal mode

    uint32_t ngate_en       :1; // ADC ALC Noise Gate enable
    uint32_t ngate_floor    :5; // ADC ALC noise floor level setting
    uint32_t alc_min        :5; // Min ADC PGA gain used in ALC mode
    uint32_t alc_max        :5; // Max ADC PGA gain used in ALC mode

    uint32_t alc_hold       :4; // ADC ALC hold time before gain is increased
    uint32_t alc_attack     :4; // ADC ALC attack (gain ramp-down) time
    uint32_t alc_decay      :4; // ADC Decay (gain ramp-up) time
    uint32_t reserved       :20;

} ALC_PARAMS;
//----------------------------------------

/****** ADCPDM specific error codes *****/
#define CSK_ADCPDM_ERROR_SAMP_RATE              (CSK_DRIVER_ERROR_SPECIFIC - 1) // Specified Sample Rate not supported
#define CSK_ADCPDM_ERROR_OVER_SAMP_RATIO        (CSK_DRIVER_ERROR_SPECIFIC - 2) // Specified Over Sample Ratio not supported
#define CSK_ADCPDM_ERROR_SR_OSR_PAIR            (CSK_DRIVER_ERROR_SPECIFIC - 3) // Specified combination of Sample Rate & Over Sample Ratio not supported
#define CSK_ADCPDM_ERROR_RXCFG                  (CSK_DRIVER_ERROR_SPECIFIC - 4) // Specified RX Config (Mix or not) not supported
#define CSK_ADCPDM_ERROR_ALC_PARAMS             (CSK_DRIVER_ERROR_SPECIFIC - 5) // Specified ALC parameters are invalid
#define CSK_ADCPDM_ERROR_LATCH_DELAY            (CSK_DRIVER_ERROR_SPECIFIC - 6) // Specified Latch delay is invalid
#define CSK_ADCPDM_ERROR_INITED_ALREADY         (CSK_DRIVER_ERROR_SPECIFIC - 8) // ADC_PDM has already been initialized


// ADCPDM status bit definitions
typedef struct _CSK_ADCPDM_STATUS_BIT
{
    uint32_t busy :2;       // Receive busy flag, bit[0] for left channel, bit[1] for right channel
    uint32_t rx_full :1;    // Receive RX FIFO full (cleared on start of transfer operation)
    uint32_t rx_ovf :1;     // Receive RX FIFO overflow (cleared on start of transfer operation)
    uint32_t l_mute :1;     // ADC0/2 (left channel) is mute or not, 1 means mute
    uint32_t r_mute :1;     // ADC1/3 (right channel) is mute or not, 1 means mute

    uint32_t reserved :26;
} CSK_ADCPDM_STATUS_BIT;

// ADCPDM status
typedef union {
    uint32_t all;
    CSK_ADCPDM_STATUS_BIT bit;
} CSK_ADCPDM_STATUS;

///****** ADCPDM Event *****/
#define CSK_ADCPDM_EVENT_RECEIVE_COMPLETE       (0x1UL << 0) ///< Data Receive completed
#define CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN        (0x1UL << 1) ///< Data Receive overflow
#define CSK_ADCPDM_EVENT_RX_FIFO_FULL           (0x1UL << 2) ///< Data Receive full
#define CSK_ADCPDM_EVENT_RX_PING_DONE           (0x1UL << 3) ///< Data RX Ping Transfer Done
#define CSK_ADCPDM_EVENT_RX_PONG_DONE           (0x1UL << 4) ///< Data RX Pong Transfer Done
#define CSK_ADCPDM_EVENT_OTHER_ERROR            (0x1UL << 7) ///< Other Error
#define CSK_ADCPDM_EVENT_BLOCK_COMPLETE         (CSK_ADCPDM_EVENT_RX_PING_DONE | CSK_ADCPDM_EVENT_RX_PONG_DONE)

/**
 \fn          void CSK_ADC_PDM_SignalEvent_t (uint32_t event, uint32_t usr_param)
 \brief       Signal ADC/PDM Events.
  \param[in]  event_info ADC/PDM event and channel information
              bit[7:0] is event type, bit[15:8] is ADC/PDM channel number
              bit[23:16] indicate APC dual_channel number
 \param[in]   usr_param     user parameter
 \return      none
*/
typedef void
(*CSK_ADC_PDM_SignalEvent_t)(uint32_t event_info, uint32_t usr_param);

/* NOTE: ONLY 24 bits of sample is supported with PDM_DMIC, ADC and DAC !! */
#define CSK_ADC_PDM_SAMPLE_BITS         24

//------------------------------------------------------------------------------------------
/**
 \fn          CSK_DRIVER_VERSION ADC_PDM_GetVersion (void)
 \brief       Get driver version.
 \return      \ref CSK_DRIVER_VERSION
*/
CSK_DRIVER_VERSION
ADC_PDM_GetVersion();

#define ADC_PDM_BMP_LEFT        CH_BMP_LEFT     // (0x1 << 0)
#define ADC_PDM_BMP_RIGHT       CH_BMP_RIGHT    // (0x1 << 1)
#define ADC_PDM_BMP_STEREO      CH_BMP_STEREO   // (0x3 << 0)

/**
 \fn          int32_t ADC_PDM_Initialize(void *adc_pdm_grp, ...)
 \brief       Initialize ADC/PDM device group interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   cb_event  Pointer to \ref CSK_ADC_PDM_SignalEvent_t
 \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \param[in]   use_flags  see below
 \return      \ref execution_status
*/

//#define ADC_PDM_USE_PDM         (0x1 << 0) // bit[0] = 1 indicated PDM/DMIC, else AMIC
//#define ADC_PDM_USE_16BITS      (0x1 << 1) // bit[1] = 1 indicates 16bits sample, else 32bits
//
//int32_t
//ADC_PDM_Initialize(void *adc_pdm_grp, CSK_ADC_PDM_SignalEvent_t cb_event, uint32_t usr_param,
//                uint8_t dev_bmp, uint8_t use_flags);


// bit flag of the parameter 'dev_bmp_flag'
#define ADC_PDM_BMP_FLAG_IN_LEFT        (0x1 << 0) // bit[0] for ADC/PDM Left Channel (ADC0/DMIC0)
#define ADC_PDM_BMP_FLAG_IN_RIGHT       (0x1 << 1) // bit[1] for ADC/PDM Right Channel (ADC1/DMIC1)
#define ADC_PDM_BMP_FLAG_IN_STEREO      (0x3 << 0) // bit[1:0] for ADC/PDM Left Channel (ADC0/DMIC0) & Right Channel (ADC1/DMIC1)

#define ADC_PDM_BMP_FLAG_USE_PDM        (0x1 << 4) // bit[4] = 1 indicated PDM/DMIC, else AMIC
#define ADC_PDM_BMP_FLAG_USE_16BITS     (0x1 << 5) // bit[5] = 1 indicates 16bits sample, else 32bits

#define ADC_PDM_BMP_FLAG_IN_POS         0 // bit[1:0] for ADC/PDM

// DMA channels specified by user, set to 0xFF if ignored or NOT used.
typedef struct {
    uint32_t dma_ch_in_left : 8; // DMA channel no. for ADC/PDM IN Left channel
    uint32_t dma_ch_in_right : 8; // DMA channel no. for ADC/PDM IN Right channel
    uint32_t reserved : 16; //8;
} ADC_PDM_DMA_CHS;

int32_t
ADC_PDM_Initialize(void *adc_pdm_grp, CSK_ADC_PDM_SignalEvent_t cb_event, uint32_t usr_param,
                uint32_t dev_bmp_flag, ADC_PDM_DMA_CHS *dma_chs_p);

/**
 \fn          int32_t ADC_PDM_Uninitialize(void *adc_pdm_grp)
 \brief       De-initialize ADC/PDM device group interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \return      \ref execution_status
*/
int32_t
ADC_PDM_Uninitialize(void *adc_pdm_grp);

/**
 \fn          int32_t ADC_PDM_PowerControl(void *adc_pdm_grp, ...)
 \brief       Control ADC/PDM interface's Power.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   state  Power state
 \return      \ref execution_status
*/
int32_t
ADC_PDM_PowerControl(void *adc_pdm_grp, CSK_POWER_STATE state);

/**
 \fn          int32_t ADC_PDM_Receive(void *adc_pdm_grp, ...)
 \brief       Receive data from ADC/PDM interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[out]  data  Pointer to buffer to receive data from ADC/PDM interface
 \param[in]   num   Number of data items to receive
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \param[in]   rx_flag   bit flags of receive operation
 \return      \ref execution_status
*/

// bit[0]=1 indicates Start sending immediately, or else
//  it will NOT transfer data until ADC_PDM_Enable is called
// bit[1]=1 indicates Do quick check for parameters & status (DON'T set the flag for first call!)
#define ADC_PDM_RX_FLAG_START_NOW   (0x1 << 0)
#define ADC_PDM_RX_FLAG_QUICK_CHK   (0x1 << 1)

int32_t
ADC_PDM_Receive(void *adc_pdm_grp, uint32_t *data, uint32_t num,
                uint8_t dev_bmp, uint8_t rx_flag);

// bit[7]=1 indicates parameters are fully checked
//#define ADC_PDM_RX_FLAG_QUICK_CHECK  (0x1 << 7)

// Receive data via ADC/DMIC interface in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_IN_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
int32_t
ADC_PDM_Receive_PiPo(void *adc_pdm_grp, PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p,
              uint8_t dev_bmp, uint8_t rx_flag);

//[OUT]  blks  Pointer to array of PIPO_IN_BLOCK to hold transferred block descriptors
//[IN]   blk_cnt  Number of PIPO_IN_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
int32_t
ADC_PDM_PiPo_Xferred_Blocks(void *adc_pdm_grp, PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp);

/**
 \fn          int32_t ADC_PDM_Receive_LLP(void *adc_pdm_grp, ...)
 \brief       Receive data from ADC/PDM interface into several non-continuous buffers.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   bufs  Pointer to list of buffers to receive data from ADC/PDM interface
 \param[in]   buf_cnt  Number of data buffers in the list
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \param[in]   rx_flag   bit flags of receive operation
 \return      \ref execution_status
*/
//NOTE: DON'T support _LLP API on ARCS_C0 for multi-block is NOT supported by GPDMAC (ONLY PingPong)
//int32_t
//ADC_PDM_Receive_LLP(void *adc_pdm_grp, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt,
//                uint8_t dev_bmp, uint8_t rx_flag);

/**
 \fn          int32_t ADC_PDM_Enable(void *adc_pdm_grp, ...)
 \brief       Enable ADC/PDM interface and data receiving is started from now on
              if ADC_PDM_Receive_XXX is called before.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \return      \ref execution_status
*/
int32_t
ADC_PDM_Enable(void *adc_pdm_grp, uint8_t dev_bmp);

/**
 \fn          int32_t ADC_PDM_Disable(void *adc_pdm_grp, ...)
 \brief       Disable ADC/PDM interface (and data receiving is suspended if any).
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \return      \ref execution_status
*/
int32_t
ADC_PDM_Disable(void *adc_pdm_grp, uint8_t dev_bmp);

/**
 \fn          int32_t ADC_PDM_Abort(void *adc_pdm_grp, ...)
 \brief       Abort ADC/PDM data transfer if any.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   dev_bmp  ADC/PDM device bitmap (bit0 for ADC0 or ADC2, bit1 for ADC1 or ADC3)
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \return      \ref execution_status
*/
int32_t
ADC_PDM_Abort(void *adc_pdm_grp, uint8_t dev_bmp);

/**
 \fn          uint32_t ADC_PDM_GetRxCount(void *adc_pdm_grp, ...)
 \brief       Get received data count from ADC/PDM instance.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \return      number of data items (24-bit samples)transferred if positive, error value if negative.
*/
int32_t
ADC_PDM_GetRxCount(void *adc_pdm_grp, uint8_t dev_bmp);

/**
 \fn          int32_t ADC_PDM_Control(void *adc_pdm_grp, ...)
 \brief       Control ADC/PDM interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   control  Operation
 \param[in]   arg  Argument of operation (optional), i.e. sample rate
 \return      common \ref execution_status and driver specific \ref ADC_PDM execution_status
*/
int32_t
ADC_PDM_Control(void *adc_pdm_grp, uint32_t control, uint32_t arg);

/**
 \fn          int32_t ADC_PDM_SetVolume(void *adc_pdm_grp, ...)
 \brief       Set analog and/or digital of ADC/PDM interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   a_gain    Analog Gain of Left Channel (ADC0/2, @ a_gain[15:0])
                        and Right Channel (ADC1/3, @ a_gain[31:16]), for ADC only (NOT for PDM/DMIC)
                        Analog Gain value: 0x0 (-12dB) ~ 0x18 (+36dB), 2dB each step, default 0x6 (0dB)
 \param[in]   d_gain    Digital gain of Left Channel (ADC0/2, @ d_gain[15:0])
                        and Right Channel (ADC1/3, @ d_gain[31:16]),
                        Digital Gain value: 0x2 (-83dB) ~ 0x7F (+42dB), 1dB each step, default 0x55 (0dB)
 \param[in]   vol_flag  volume flag which indicates which volume items are specified
 \return      common \ref execution_status and driver specific \ref ADC/PDM execution_status
*/

#define ADC_PDM_VOL_FLAG_A_LEFT     0x1UL // Line out Gain of Left Channel (ADC0 or ADC2, @ a_gain[15:0])
#define ADC_PDM_VOL_FLAG_A_RIGHT    0x2UL // Line out Gain of Right Channel (ADC1 or ADC3, @ a_gain[31:16])
#define ADC_PDM_VOL_FLAG_D_LEFT     0x4UL // Digital Gain of Left Channel (ADC0 or ADC2, @ d_gain[15:0])
#define ADC_PDM_VOL_FLAG_D_RIGHT    0x8UL // Digital Gain of Right Channel (ADC1 or ADC3, @ d_gain[31:16])

// analog gain: dB => register value
#define ADC_PDM_GAIN_A_MAX_DB       36
#define ADC_PDM_GAIN_A_MIN_DB       -12
#define ADC_PDM_GAIN_A_DEF_DB       0
#define ADC_PDM_GAIN_A_DB_STEP      2

#define ADC_PDM_GAIN_A_MAX_VAL      0x18
#define ADC_PDM_GAIN_A_MIN_VAL      0x0
#define ADC_PDM_GAIN_A_DEF_VAL      0x6
#define ADC_PDM_GAIN_A_VAL(dB)      ((ADC_PDM_GAIN_A_MIN_VAL + (dB - ADC_PDM_GAIN_A_MIN_DB + \
                                    ADC_PDM_GAIN_A_DB_STEP - 1) / ADC_PDM_GAIN_A_DB_STEP) & 0x1F)

// digital gain: dB => register value
#define ADC_PDM_GAIN_D_MAX_DB       42
#define ADC_PDM_GAIN_D_MIN_DB       -83
#define ADC_PDM_GAIN_D_DEF_DB       0
#define ADC_PDM_GAIN_D_DB_STEP      1

#define ADC_PDM_GAIN_D_MAX_VAL      0x7F
#define ADC_PDM_GAIN_D_MIN_VAL      0x2
#define ADC_PDM_GAIN_D_DEF_VAL      0x55
#define ADC_PDM_GAIN_D_VAL(dB)      ((ADC_PDM_GAIN_D_MIN_VAL + (dB - ADC_PDM_GAIN_D_MIN_DB + \
                                    ADC_PDM_GAIN_D_DB_STEP - 1) / ADC_PDM_GAIN_D_DB_STEP) & 0x7F)

int32_t
ADC_PDM_SetVolume(void *adc_pdm_grp, uint32_t a_gain, uint32_t d_gain, uint32_t vol_flag);

/**
 \fn          int32_t ADC_PDM_SetMute(void *adc_pdm_grp, ...)
 \brief       Set mute/unmute value of ADC/PDM interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   mute_val  Mute/UnMute bit of Left Channel (ADC0/2, @ mute_val[0])
                        and Right Channel (ADC1/3, @ mute_val[1]),
 \param[in]   dev_bmp  specify which ADC/PDM devices in the device group
                      ADC0/2 (Left Channel, @bit[0]) and ADC1/3 (Right Channel, @bit[1]),
                      and dev_bmp = 0x3 indicates both ADC0/2 & ADC1/3
 \return      common \ref execution_status and driver specific \ref ADC/PDM execution_status
*/
int32_t
ADC_PDM_SetMute(void *adc_pdm_grp, uint8_t mute_val, uint8_t dev_bmp);

/**
 \fn          int32_t ADC_PDM_GetStatus(void *adc_pdm_grp, ...)
 \brief       Get ADC/PDM status.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[out]  status  Pointer to CSK_ADC_PDM_STATUS buffer
 \return      \ref execution_status
 */
int32_t
ADC_PDM_GetStatus(void *adc_pdm_grp, CSK_ADCPDM_STATUS *status);

//------------------------------------------------------------------------------------------
/**
 \fn          void* ADC_PDM01()
 \brief       Get ADC/PDM01 device group instance (in Always ON power domain)
              including ADC/PDM0 (abbr. ADC0) and ADC/PDM1 (abbr. ADC1)
 \return      ADC/PDM01 device group instance
 */
void* ADC_PDM01();

//------------------------------------------------------------------------------------------

#if MIX_WITH_DAC_DIGTAL_ECHO
// 2ch ADC + 1ch ECHO
int32_t
ADC_PDM_ECHO_TriReceive(void *adc_pdm_grp, void *dac_grp,
                    uint32_t *data, uint32_t num, uint8_t rx_flag);

// Receive data via ADC/DMIC interface in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_IN_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
int32_t
ADC_PDM_ECHO_TriReceive_PiPo(void *adc_pdm_grp, void *dac_grp,
                     PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p, uint8_t rx_flag);

#endif // MIX_WITH_DAC_DIGTAL_ECHO

#if MIX_WITH_I2S_DIGTAL_ECHO
// 2ch ADC + 2ch ECHO
int32_t
ADC_PDM_ECHO_QuadReceive(void *adc_pdm_grp, void *i2s_out,
                    uint32_t *data, uint32_t num, uint8_t rx_flag);

// Receive data via ADC/DMIC interface in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_IN_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
int32_t
ADC_PDM_ECHO_QuadReceive_PiPo(void *adc_pdm_grp, void *i2s_out,
                     PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p, uint8_t rx_flag);

#endif // MIX_WITH_I2S_DIGTAL_ECHO

#if (MIX_WITH_DAC_DIGTAL_ECHO || MIX_WITH_I2S_DIGTAL_ECHO)
// (2ch ADC + 1ch ECHO) OR (2ch ADC + 2ch ECHO)
//[OUT]  blks  Pointer to array of PIPO_IN_BLOCK to hold transferred block descriptors
//[IN]   blk_cnt  Number of PIPO_IN_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
int32_t
ADC_PDM_ECHO_PiPo_Xferred_Blocks(void *adc_pdm_grp, void *out_dev,
                    PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp);
#endif // (MIX_WITH_DAC_DIGTAL_ECHO || MIX_WITH_I2S_DIGTAL_ECHO)

#endif /* __DRIVER_ADC_PDM_H */
