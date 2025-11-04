/*
 * Driver_DAC.h
 *
 *
 */

#ifndef __DRIVER_DAC_H
#define __DRIVER_DAC_H


#include "Driver_Common.h"

#define CSK_DAC_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */

/****** DAC Control Codes *****/

/*----- DAC Control Codes: Configuration Parameters: Sample Rate -----*/
#define CSK_DAC_SR_Pos                           0
#define CSK_DAC_SR_Msk                           (0xFUL << CSK_DAC_SR_Pos) // bit[3:0], 4 bits
#define CSK_DAC_SR_UNSET                         (0x0UL << CSK_DAC_SR_Pos) // Sample Rate is kept unchanged or default
#define CSK_DAC_SR_8KHZ                          (0x1UL << CSK_DAC_SR_Pos) // Sample Rate = 8KHz
//#define CSK_DAC_SR_11P025KHZ                     (0x2UL << CSK_DAC_SR_Pos) // Sample Rate = 11.025KHz
//#define CSK_DAC_SR_12KHZ                         (0x3UL << CSK_DAC_SR_Pos) // Sample Rate = 12KHz
#define CSK_DAC_SR_16KHZ                         (0x4UL << CSK_DAC_SR_Pos) // Sample Rate = 16KHz
//#define CSK_DAC_SR_22P05KHZ                      (0x5UL << CSK_DAC_SR_Pos) // Sample Rate = 22.05KHz
#define CSK_DAC_SR_24KHZ                         (0x6UL << CSK_DAC_SR_Pos) // Sample Rate = 24KHz
#define CSK_DAC_SR_32KHZ                         (0x7UL << CSK_DAC_SR_Pos) // Sample Rate = 32KHz
//#define CSK_DAC_SR_44P1KHZ                       (0x8UL << CSK_DAC_SR_Pos) // Sample Rate = 44.1KHz
#define CSK_DAC_SR_48KHZ                         (0x9UL << CSK_DAC_SR_Pos) // Sample Rate = 48KHz
#define CSK_DAC_SR_96KHZ                         (0xAUL << CSK_DAC_SR_Pos) // Sample Rate = 96KHz

/*----- DAC Control Codes: Configuration Parameters: Over Sample Ratio -----*/
#define CSK_DAC_OSR_Pos                          4
#define CSK_DAC_OSR_Msk                          (0x3UL << CSK_DAC_OSR_Pos) // bit[5:4], 2 bits
#define CSK_DAC_OSR_UNSET                        (0x0UL << CSK_DAC_OSR_Pos) // Over Sample Ratio is kept unchanged or default
#define CSK_DAC_OSR_250                          (0x1UL << CSK_DAC_OSR_Pos) // Over Sample Ratio = 250
#define CSK_DAC_OSR_125                          (0x2UL << CSK_DAC_OSR_Pos) // Over Sample Ratio = 125

/*----- DAC Control Codes: Configuration Parameters: Soft Mute Setting -----*/
// Soft Mute Setting @ arg:
// bit[4] enable/disable soft mute, 1 = enable, 0 = disable. Soft Mute is enabled by default.
// bit[3:0] soft mute speed, 0000b means 1dB adjust / (2 LRCK cycles), 0001b means 1dB adjust / (4 LRCK cycles),
// till to 1111b means 1dB adjust / (2^16 LRCK cycles), double adjust time per step. default 0000b.
#define CSK_DAC_ARG_SOFT_MUTE_EN            (1 << 4)
#define CSK_DAC_ARG_SOFT_MUTE_SPD(n)        (((n) & 0xF) << 0)

#define CSK_DAC_SOFT_MUTE_Pos                    6
#define CSK_DAC_SOFT_MUTE_Msk                    (0x1UL << CSK_DAC_SOFT_MUTE_Pos) // bit[6], 1 bit
#define CSK_DAC_SOFT_MUTE_UNSET                  (0x0UL << CSK_DAC_SOFT_MUTE_Pos) // Soft Mute Setting is kept unchanged or default
#define CSK_DAC_SOFT_MUTE_SET                    (0x1UL << CSK_DAC_SOFT_MUTE_Pos) // Soft Mute Setting @ arg bit[4:0], see comment above

/*----- DAC Control Codes: Configuration Parameters: Auto Mute Setting -----*/
// Auto Mute Setting @ arg:
// bit[8] enable/disable auto mute (SDM auto bypass when all zero's input), 1 = enable, 0 = disable; Auto Mute is enabled by default.
// bit[7:5] auto mute threshold, 000b ~ 101b (more bigger, more longer, default 010b), other values are not used.
#define CSK_DAC_ARG_AUTO_MUTE_EN            (1 << 8)
#define CSK_DAC_ARG_AUTO_MUTE_THR(n)        (((n) & 0x7) << 5)

#define CSK_DAC_AUTO_MUTE_Pos                    7
#define CSK_DAC_AUTO_MUTE_Msk                    (0x1UL << CSK_DAC_AUTO_MUTE_Pos) // bit[7], 1 bit
#define CSK_DAC_AUTO_MUTE_UNSET                  (0x0UL << CSK_DAC_AUTO_MUTE_Pos) // Auto Mute Setting is kept unchanged or default
#define CSK_DAC_AUTO_MUTE_SET                    (0x1UL << CSK_DAC_AUTO_MUTE_Pos) // Auto Mute Setting @ arg[8:5], see comment above

/*----- DAC Control Codes: Configuration Parameters: TX config. (mono/stereo input, mixed output or not?) -----*/
//#define CSK_DAC_TXCFG_Pos                        8
//#define CSK_DAC_TXCFG_Msk                        (0x3UL << CSK_DAC_TXCFG_Pos) // bit[9:8], 2 bits
// // TX config (mono/stereo, mixed or not) is kept unchanged or default
//#define CSK_DAC_TXCFG_UNSET                      (0x0UL << CSK_DAC_TXCFG_Pos)
//// [default] each mono channel data is sent to DAC device group (DAC0 & DAC1) respectively
//#define CSK_DAC_TXCFG_MONO_SRC_MONO              (0x1UL << CSK_DAC_TXCFG_Pos)
//// mono channel data, duplicated and sent to both DACs (DAC0 & DAC1)
//#define CSK_DAC_TXCFG_STEREO_SRC_MONO            (0x2UL << CSK_DAC_TXCFG_Pos)
//// stereo channel data, sent to DAC device group (DAC0 & DAC1) respectively
//#define CSK_DAC_TXCFG_STEREO_SRC_STEREO          (0x3UL << CSK_DAC_TXCFG_Pos)


//TODO/FIXME: other control codes...

/*----- DAC Control Codes: Exclusive Controls -----*/
/*----- exclusive operations, CANNOT coexist with other Control Codes -----*/
#define CSK_DAC_EXCL_OP_Pos                 29
#define CSK_DAC_EXCL_OP_Msk                 (0x7UL << CSK_DAC_EXCL_OP_Pos) // bit[31:29], 3 bits
#define CSK_DAC_EXCL_OP_UNSET               (0x0UL << CSK_DAC_EXCL_OP_Pos) // NO exclusive operations
#define CSK_DAC_ABORT_TRANSFER              (0x1UL << CSK_DAC_EXCL_OP_Pos) // Abort current data transfer,
                                                                           // arg bit[1:0] = dev_bmp, arg bit[9:8] = echo_bmp
#define CSK_DAC_GET_SAMP_RATE               (0x2UL << CSK_DAC_EXCL_OP_Pos) // Get sample rate, return sample rate
#define CSK_DAC_SET_ECHO_PARAMS             (0x3UL << CSK_DAC_EXCL_OP_Pos) // Set ECHO parameters, arg = pointer to ECHO_PARAMS

//TODO: other exclusive operation code...

/****** DAC specific error codes *****/
#define CSK_DAC_ERROR_SAMP_RATE             (CSK_DRIVER_ERROR_SPECIFIC - 1) // Specified Sample Rate not supported
#define CSK_DAC_ERROR_OVER_SAMP_RATIO       (CSK_DRIVER_ERROR_SPECIFIC - 2) // Specified Over Sample Ratio not supported
#define CSK_DAC_ERROR_SR_OSR_PAIR           (CSK_DRIVER_ERROR_SPECIFIC - 3) // Specified combination of Sample Rate & Over Sample Ratio not supported
#define CSK_DAC_ERROR_TXCFG                 (CSK_DRIVER_ERROR_SPECIFIC - 4) // Specified TX Config (mono/stereo, mixed or not) not supported
//#define CSK_DAC_ERROR_MCLK_SEL              (CSK_DRIVER_ERROR_SPECIFIC - 5) // Specified Main clock not supported
#define CSK_DAC_ERROR_INITED_ALREADY        (CSK_DRIVER_ERROR_SPECIFIC - 8) // DAC has already been initialized


// DAC status bit definitions
typedef struct _CSK_DAC_STATUS_BIT
{
    uint32_t busy :2;       // Send busy flag, bit[0] for left channel, bit[1] for right channel
    uint32_t tx_emp :1;     // Send TX FIFO empty (cleared on start of transfer operation)
    uint32_t tx_undf :1;    // Send TX FIFO underflow (cleared on start of transfer operation)
    uint32_t l_mute :1;     // DAC0 (left channel) is mute or not, 1 means mute
    uint32_t r_mute :1;     // DAC1 (right channel) is mute or not, 1 means mute
    uint32_t ech_busy :2;   // ECHO receive busy flag, bit[0] for left channel, bit[1] for right channel
    uint32_t ech_ovf :1;    // ECHO Receiver overflow (cleared on start of ECHO Receive operation)
    uint32_t reserved :23;
} CSK_DAC_STATUS_BIT;

// DAC status
typedef union {
    uint32_t all;
    CSK_DAC_STATUS_BIT bit;
} CSK_DAC_STATUS;

///****** DAC Event *****/
#define CSK_DAC_EVENT_SEND_COMPLETE         (0x1UL << 0) ///< Data Send completed
#define CSK_DAC_EVENT_TX_FIFO_UNDERRUN      (0x1UL << 1) ///< Data TX FIFO underflow
#define CSK_DAC_EVENT_TX_FIFO_EMPTY         (0x1UL << 2) ///< Data TX FIFO empty
#define CSK_DAC_EVENT_TX_PING_DONE          (0x1UL << 3) ///< Data TX Ping Transfer Done
#define CSK_DAC_EVENT_TX_PONG_DONE          (0x1UL << 4) ///< Data TX Pong Transfer Done
#define CSK_DAC_EVENT_BLOCK_COMPLETE        (CSK_DAC_EVENT_TX_PING_DONE | CSK_DAC_EVENT_TX_PONG_DONE)

#define CSK_DAC_EVENT_ECHO_RX_COMPLETE      (0x1UL << 6) ///< ECHO Data Receive completed
#define CSK_DAC_EVENT_ECHO_RX_FIFO_OVERRUN  (0x1UL << 7) ///< ECHO Data RX FIFO overflow
#define CSK_DAC_EVENT_ECHO_RX_PING_DONE     (0x1UL << 8) ///< Echo Data RX Ping Transfer Done
#define CSK_DAC_EVENT_ECHO_RX_PONG_DONE     (0x1UL << 9) ///< Echo Data RX Pong Transfer Done
#define CSK_DAC_EVENT_ECHO_BLOCK_COMPLETE   (CSK_DAC_EVENT_ECHO_RX_PING_DONE | \
                                             CSK_DAC_EVENT_ECHO_RX_PONG_DONE)

#define CSK_DAC_EVENT_OTHER_ERROR           (0x1UL << 10) ///< Other Error

//FIXME: Add following events...


/**
 \fn          void CSK_DAC_SignalEvent_t (uint32_t event, uint32_t usr_param)
 \brief       Signal DAC Events.
  \param[in]  event_info DAC event and channel information
              bit[13:0] is event type, bit[15:14] is DAC/channel number (0=DAC0/Left, 1=DAC1/Right)
              bit[23:16] indicate APC dual_channel number
 \param[in]   usr_param     user parameter
 \return      none
*/
typedef void
(*CSK_DAC_SignalEvent_t)(uint32_t event_info, uint32_t usr_param);

#define CSK_DAC_EVENT_HIGHEST_POS       13
#define CSK_DAC_EVENT_MASK              ((0x1UL << (CSK_DAC_EVENT_HIGHEST_POS + 1)) - 1)
#define CSK_DAC_EVENT_CH_POS            (CSK_DAC_EVENT_HIGHEST_POS + 1)
#define CSK_DAC_EVENT_APC_DCH_POS       (CSK_DAC_EVENT_HIGHEST_POS + 3)
#define CSK_DAC_EVENT_CH_MASK           (0x3)
#define CSK_DAC_EVENT_APC_DCH_MASK      (0xFF)

/* NOTE: ONLY 24 bits of sample is supported with PDM_DMIC, ADC and DAC !! */
#define CSK_DAC_SAMPLE_BITS         24

//------------------------------------------------------------------------------------------
/**
 \fn          CSK_DRIVER_VERSION DAC_GetVersion (void)
 \brief       Get driver version.
 \return      \ref CSK_DRIVER_VERSION
*/
CSK_DRIVER_VERSION
DAC_GetVersion();

#define DAC_BMP_LEFT        CH_BMP_LEFT     // (0x1 << 0)
#define DAC_BMP_RIGHT       CH_BMP_RIGHT    // (0x1 << 1)
#define DAC_BMP_STEREO      CH_BMP_STEREO   // (0x3 << 0)

/**
 \fn          int32_t DAC_Initialize(void *dac_grp, ...)
 \brief       Initialize DAC device group. A DAC device group includes at most two DAC devices,
              i.e. DAC01 includes DAC0 (Left Channel) and DAC1 (Right Channel) two devices.
              A DAC device must belong to some DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   cb_event  Pointer to \ref CSK_DAC_SignalEvent_t
 \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
 \param[in]   dev_bmp  which DAC devices (similar to "I2S channels") are used,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   echo_bmp  which DAC devices' echo channels are used,
              DAC0 echo (Left Channel, @bit[0]) and DAC1 echo (Right Channel, @bit[1]),
              and echo_bmp = 0x3 indicates both echo channels of DAC0 & DAC1
 \param[in]   init_flags  see below
 \return      \ref execution_status
*/

//#define DAC_USE_16BITS      (0x1 << 0) // bit[0] = 1 indicates 16bits sample, else 32bits
//
//int32_t
//DAC_Initialize(void *dac_grp, CSK_DAC_SignalEvent_t cb_event, uint32_t usr_param,
//                uint8_t dev_bmp, uint8_t echo_bmp, uint8_t init_flags);

// bit flag of the parameter 'dev_bmp_flag'
#define DAC_BMP_FLAG_OUT_LEFT        (0x1 << 0) // bit[0] for DAC Left Channel (ONLY DAC0 on ARCS!)
#define DAC_BMP_FLAG_OUT_RIGHT       (0x1 << 1) // bit[1] for DAC Channel (NO DAC1)
#define DAC_BMP_FLAG_OUT_STEREO      (0x3 << 0) // bit[1:0] for DAC Left Channel (DAC0) & Right Channel (DAC1)

// ECHO channels are handled in the ADC_PDM driver or DAC driver?
#define DAC_BMP_FLAG_ECHO_LEFT      (0x1 << 2) // bit[2] for ECHO Left Channel
#define DAC_BMP_FLAG_ECHO_RIGHT     (0x1 << 3) // bit[3] for ECHO Right Channel
#define DAC_BMP_FLAG_ECHO_STEREO    (0x3 << 2) // bit[3:2] for ECHO Left & Right Channels

#define DAC_BMP_FLAG_USE_16BITS     (0x1 << 4) // bit[4] = 1 indicates 16bits sample, else 32bits

#define DAC_BMP_FLAG_OUT_POS        0 // bit[1:0] for DAC
#define DAC_BMP_FLAG_ECHO_POS       2 // bit[3:2] for ECHO

// DMA channels specified by user, set to 0xFF if ignored or NOT used.
typedef struct {
    uint32_t dma_ch_out_left : 8; // DMA channel no. for DAC OUT Left channel
    //uint32_t dma_ch_in_right : 8; // DMA channel no. for DAC OUT Right channel
    uint32_t rsvd0 : 8;
    uint32_t dma_ch_echo_left : 8; // DMA channel no. for ECHO Left channel //TODO/FIXME:
    //uint32_t dma_ch_echo_right : 8; // DMA channel no. for ECHO Right channel
    uint32_t rsvd1 : 8;
} DAC_DMA_CHS;

int32_t
DAC_Initialize(void *dac_grp, CSK_DAC_SignalEvent_t cb_event, uint32_t usr_param,
                uint8_t dev_bmp_flag, DAC_DMA_CHS *dma_chs_p);

/**
 \fn          int32_t DAC_Uninitialize(void *dac_grp)
 \brief       De-initialize DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \return      \ref execution_status
*/
int32_t
DAC_Uninitialize(void *dac_grp);

/**
 \fn          int32_t DAC_PowerControl(void *dac_grp, ...)
 \brief       Control DAC device group's Power.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   state  Power state
 \return      \ref execution_status
*/
int32_t
DAC_PowerControl(void *dac_grp, CSK_POWER_STATE state);

/**
 \fn          int32_t DAC_Send(void *dac_grp, ...)
 \brief       Send data within the buffer
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   data  Pointer to buffer of data to send
 \param[in]   num   Number of data items to send
 \param[in]   dev_bmp  send data from which DAC devices in the device group,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   tx_flag   bit flags of send operation
 \return      \ref execution_status
*/

// bit[0]=1 indicates Start sending immediately, or else
//  it will NOT transfer data until DAC_Enable is called
#define DAC_TX_FLAG_START_NOW   (0x1 << 0)

// bit[1]=1 indicates don't sync cache internally for SEND
#define DAC_TX_FLAG_NSYNCA      (0x1 << 1)

int32_t
DAC_Send(void *dac_grp, const uint32_t *data, uint32_t num,
                uint8_t dev_bmp, uint8_t tx_flag);

// bit[7]=1 indicates parameters are fully checked
//#define DAC_TX_FLAG_QUICK_CHECK  (0x1 << 7)

// Send data via DAC interface in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_OUT_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
int32_t
DAC_Send_PiPo(void *dac_grp, PIPO_OUT_BLOCK *blks, uint8_t *blk_cnt_p,
              uint8_t dev_bmp, uint8_t tx_flag);


//[OUT]  blks  Pointer to array of PIPO_OUT_BLOCK to hold transferred block descriptors
//[IN]   blk_cnt  Number of PIPO_OUT_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
int32_t
DAC_PiPo_Xferred_Blocks(void *dac_grp, PIPO_OUT_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp);


/**
 \fn          int32_t DAC_Send_LLP(void *dac_grp, ...)
 \brief       Send data within several non-continuous buffers
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   bufs  Pointer to list of buffers of data to send
 \param[in]   buf_cnt  Number of data buffers in the list
 \param[in]   dev_bmp  send data from which DAC devices in the device group,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   tx_flag   bit flags of send operation
 \return      \ref execution_status
*/
//int32_t
//DAC_Send_LLP(void *dac_grp, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt,
//                uint8_t dev_bmp, uint8_t tx_flag);

/**
 \fn          int32_t DAC_Echo_Receive(void *dac_grp, ...)
 \brief       Receive echo data into the buffer
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   data  Pointer to buffer to receive echo data
 \param[in]   num   Number of data items to receive
 \param[in]   echo_bmp  receive data from which echo channels of DAC device group,
              Left Echo Channel @bit[0] and Right Echo Channel @bit[1],
              and echo_bmp = 0x3 indicates both echo channels of DAC device group
 \return      \ref execution_status
*/
int32_t
DAC_Echo_Receive(void *dac_grp, uint32_t *data, uint32_t num, uint8_t echo_bmp);

// Receive echo data in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_IN_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
int32_t
DAC_Echo_Receive_PiPo(void *dac_grp, PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p, uint8_t echo_bmp);


//[OUT]  blks  Pointer to array of PIPO_IN_BLOCK to hold transferred block descriptors
//[IN]   blk_cnt  Number of PIPO_IN_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
int32_t
DAC_Echo_PiPo_Xferred_Blocks(void *dac_grp, PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t echo_bmp);

/**
 \fn          int32_t DAC_Echo_Receive_LLP(void *dac_grp, ...)
 \brief       Receive echo data into several non-continuous buffers
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   bufs  Pointer to list of buffer to receive echo data
 \param[in]   buf_cnt  Number of data buffers in the list
 \param[in]   echo_bmp  receive data from which echo channels of DAC device group,
              Left Echo Channel @bit[0] and Right Echo Channel @bit[1],
              and echo_bmp = 0x3 indicates both echo channels of DAC device group
 \return      \ref execution_status
*/
//int32_t
//DAC_Echo_Receive_LLP(void *dac_grp, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt, uint8_t echo_bmp);

/**
 \fn          int32_t DAC_Enable(void *dac_grp, ...)
 \brief       Enable DAC device(s) and data send is started
              if DAC_Send_XXX is called before and START_NOW NOT specified.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   dev_bmp  specify which DAC devices in the device group,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   echo_bmp  receive data from which echo channels of DAC device group,
              Left Echo Channel @bit[0] and Right Echo Channel @bit[1],
              and echo_bmp = 0x3 indicates both echo channels of DAC device group
 \return      \ref execution_status
*/
int32_t
DAC_Enable(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp);

/**
 \fn          int32_t DAC_Disable(void *dac_grp, ...)
 \brief       Disable DAC device(s) (and data send is suspended if any).
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   dev_bmp  specify which DAC devices in the device group,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   echo_bmp  receive data from which echo channels of DAC device group,
              Left Echo Channel @bit[0] and Right Echo Channel @bit[1],
              and echo_bmp = 0x3 indicates both echo channels of DAC device group
 \return      \ref execution_status
*/
int32_t
DAC_Disable(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp);

/**
 \fn          int32_t DAC_Abort(void *dac_grp, ...)
 \brief       Abort DAC data transfer if any.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   dev_bmp  specify which DAC devices in the device group
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   echo_bmp  which DAC devices' echo data channels are used,
              DAC0 echo (Left Channel, @bit[0]) and DAC1 echo (Right Channel, @bit[1]),
              and echo_bmp = 0x3 indicates both echo data channels of DAC0 & DAC1
 \return      \ref execution_status
*/
int32_t
DAC_Abort(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp);

/**
 \fn          uint32_t DAC_GetTxCount(void *dac_grp, ...)
 \brief       Get count of data sent from DAC device(s).
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   dev_bmp  specify which DAC devices in the device group
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \return      number of data items (24-bit samples)transferred if positive, error value if negative.
*/
int32_t
DAC_GetTxCount(void *dac_grp, uint8_t dev_bmp);

/**
 \fn          uint32_t DAC_GetEchoCount(void *dac_grp, ...)
 \brief       Get transferred echo data count.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   echo_bmp  which DAC devices' echo data channels are used,
              DAC0 echo (Left Channel, @bit[0]) and DAC1 echo (Right Channel, @bit[1]),
              and echo_bmp = 0x3 indicates both echo data channels of DAC0 & DAC1
 \return      number of data items transferred if positive,
              error value if negative.
*/
int32_t
DAC_GetEchoCount(void *dac_grp, uint8_t echo_bmp);

/**
 \fn          int32_t DAC_Control(void *dac_grp, ...)
 \brief       Control DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   control  Operation
 \param[in]   arg  Argument of operation (optional), i.e. sample rate
 \return      common \ref execution_status and driver specific \ref DAC execution_status
*/
int32_t
DAC_Control(void *dac_grp, uint32_t control, uint32_t arg);

/**
 \fn          int32_t DAC_SetVolume(void *dac_grp, ...)
 \brief       Set analog (line out) and/or digital gain of DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   a_gain    Analog (Line out) Gain of Left Channel (DAC0, @ a_gain[15:0])
                        and Right Channel (DAC1, @ a_gain[31:16]),
                        Analog (Line Out) Gain value: 0x0 (-24dB) ~ 0xF (6dB), 2dB each step, default 0x9 (-6dB)
 \param[in]   d_gain    Digital gain of Left Channel (DAC0, @ d_gain[15:0])
                        and Right Channel (DAC1, @ d_gain[31:16]),
                        Digital Gain value: 0x70 (-113dB) ~ 0xFF (30dB), 1dB each step, default 0xe1 (0dB)
 \param[in]   vol_flag  volume flag which indicates which volume items are specified
 \return      common \ref execution_status and driver specific \ref DAC execution_status
*/

#define DAC_VOL_FLAG_A_LEFT     0x1UL // Analog (Line out) Gain of Left Channel (DAC0, @ a_gain[15:0])
#define DAC_VOL_FLAG_A_RIGHT    0x2UL // Analog (Line out) Gain of Right Channel (DAC1, @ a_gain[31:16])
#define DAC_VOL_FLAG_D_LEFT     0x4UL // Digital Gain of Left Channel (DAC0, @ d_gain[15:0])
#define DAC_VOL_FLAG_D_RIGHT    0x8UL // Digital Gain of Right Channel (DAC1, @ d_gain[31:16])

// analog gain: dB => register value
#define DAC_GAIN_A_MAX_DB       6
#define DAC_GAIN_A_MIN_DB       -24
#define DAC_GAIN_A_DEF_DB       -6
#define DAC_GAIN_A_DB_STEP      2

#define DAC_GAIN_A_MAX_VAL      0xF
#define DAC_GAIN_A_MIN_VAL      0x0
#define DAC_GAIN_A_DEF_VAL      0x9
#define DAC_GAIN_A_VAL(dB)      ((DAC_GAIN_A_MIN_VAL + \
                                (dB - DAC_GAIN_A_MIN_DB + DAC_GAIN_A_DB_STEP - 1) / DAC_GAIN_A_DB_STEP) & 0xF)

// digital gain: dB => register value
#define DAC_GAIN_D_MAX_DB       30
#define DAC_GAIN_D_MIN_DB       -113
#define DAC_GAIN_D_DEF_DB       0
#define DAC_GAIN_D_DB_STEP      1

#define DAC_GAIN_D_MAX_VAL      0xFF
#define DAC_GAIN_D_MIN_VAL      0x70
#define DAC_GAIN_D_DEF_VAL      0xE1
#define DAC_GAIN_D_VAL(dB)      ((DAC_GAIN_D_MIN_VAL + \
                                (dB - DAC_GAIN_D_MIN_DB + DAC_GAIN_D_DB_STEP - 1) / DAC_GAIN_D_DB_STEP) & 0xFF)

//NOTE: ONLY 1 DAC (Left channel) on ARCS!
int32_t
DAC_SetVolume(void *dac_grp, uint32_t a_gain, uint32_t d_gain, uint32_t vol_flag);

/**
 \fn          int32_t DAC_SetMute(void *dac_grp, ...)
 \brief       Set mute/unmute value of DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   mute_val  Mute/UnMute bit of Left Channel (DAC0, @ mute_val[0])
                        and Right Channel (DAC1, @ mute_val[1]),
 \param[in]   dev_bmp  specify which DAC devices in the device group
                      DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
                      and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \return      common \ref execution_status and driver specific \ref DAC execution_status
*/
//NOTE: ONLY 1 DAC (Left channel) on ARCS!
int32_t
DAC_SetMute(void *dac_grp, uint8_t mute_val, uint8_t dev_bmp);

/**
 \fn          int32_t DAC_GetStatus(void *dac_grp, ...)
 \brief       Get DAC device group's status.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[out]  status  Pointer to CSK_DAC_STATUS buffer
 \return      \ref execution_status
 */
int32_t
DAC_GetStatus(void *dac_grp, CSK_DAC_STATUS *status);

// EQ API
// Set a series of EQ coefficient(s)
int32_t DAC_EQ_Set_Coef_Array(void *dac_grp, uint32_t *eqcoefs, uint32_t num);
// Set some EQ coefficient of specified index
int32_t DAC_EQ_Set_Coef(void *dac_grp, uint32_t index, uint32_t eqcoef);
// Enable EQ functions
int32_t DAC_EQ_Enable(void *dac_grp, uint32_t stages);
// Disable EQ functions
int32_t DAC_EQ_Disble(void *dac_grp);
// Clear EQ coefficients
int32_t DAC_EQ_Clear(void *dac_grp, uint8_t wait_done);

//------------------------------------------------------------------------------------------
/**
 \fn          void* DAC01()
 \brief       Get DAC01 device group instance
 \return      DAC01 device instance
 */
void* DAC01();


#endif /* __DRIVER_DAC_H */
