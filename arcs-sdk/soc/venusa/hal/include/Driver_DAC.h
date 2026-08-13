/**
 * @file Driver_DAC.h
 * @brief Digital-to-Analog Converter (DAC) Driver Interface Definitions
 * 
 * This header defines the interface for controlling DAC peripherals, including:
 * - Configuration parameters (sample rates, oversampling ratios)
 * - Control codes and error definitions
 * - Event handling and status reporting
 * - API function prototypes
 */

#ifndef __DRIVER_DAC_H
#define __DRIVER_DAC_H

#include "Driver_Common.h"

#define CSK_DAC_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  ///< API version */

/****** DAC Control Codes *****/

/**
 * @name DAC Sample Rate Control Codes
 * @{
 */
#define CSK_DAC_SR_Pos                           0
#define CSK_DAC_SR_Msk                           (0xFUL << CSK_DAC_SR_Pos) ///< Sample rate bitmask (bits 3:0)
#define CSK_DAC_SR_UNSET                         (0x0UL << CSK_DAC_SR_Pos) ///< Keep current sample rate
#define CSK_DAC_SR_8KHZ                          (0x1UL << CSK_DAC_SR_Pos) ///< 8KHz sample rate
#define CSK_DAC_SR_16KHZ                         (0x4UL << CSK_DAC_SR_Pos) ///< 16KHz sample rate
#define CSK_DAC_SR_24KHZ                         (0x6UL << CSK_DAC_SR_Pos) ///< 24KHz sample rate
#define CSK_DAC_SR_32KHZ                         (0x7UL << CSK_DAC_SR_Pos) ///< 32KHz sample rate
#define CSK_DAC_SR_48KHZ                         (0x9UL << CSK_DAC_SR_Pos) ///< 48KHz sample rate
#define CSK_DAC_SR_96KHZ                         (0xAUL << CSK_DAC_SR_Pos) ///< 96KHz sample rate
/** @} */

/**
 * @name DAC Oversampling Ratio Control Codes
 * @{
 */
#define CSK_DAC_OSR_Pos                          4
#define CSK_DAC_OSR_Msk                          (0x3UL << CSK_DAC_OSR_Pos) ///< Oversampling ratio bitmask (bits 5:4)
#define CSK_DAC_OSR_UNSET                        (0x0UL << CSK_DAC_OSR_Pos) ///< Keep current oversampling ratio
#define CSK_DAC_OSR_250                          (0x1UL << CSK_DAC_OSR_Pos) ///< 250x oversampling
#define CSK_DAC_OSR_125                          (0x2UL << CSK_DAC_OSR_Pos) ///< 125x oversampling
/** @} */

/**
 * @name DAC Soft Mute Control Parameters
 * @{
 */
#define CSK_DAC_ARG_SOFT_MUTE_EN            (1 << 4)  ///< Enable/disable soft mute
#define CSK_DAC_ARG_SOFT_MUTE_SPD(n)        (((n) & 0xF) << 0) ///< Soft mute speed setting //FIXME:

#define CSK_DAC_SOFT_MUTE_Pos                    6
#define CSK_DAC_SOFT_MUTE_Msk                    (0x1UL << CSK_DAC_SOFT_MUTE_Pos) ///< Soft mute control bit
#define CSK_DAC_SOFT_MUTE_UNSET                  (0x0UL << CSK_DAC_SOFT_MUTE_Pos) ///< Keep current soft mute setting
#define CSK_DAC_SOFT_MUTE_SET                    (0x1UL << CSK_DAC_SOFT_MUTE_Pos) ///< Configure soft mute
/** @} */

/**
 * @name DAC Auto Mute Control Parameters
 * @{
 */
#define CSK_DAC_ARG_AUTO_MUTE_EN            (1 << 8)  ///< Enable/disable auto mute
#define CSK_DAC_ARG_AUTO_MUTE_THR(n)        (((n) & 0x7) << 5) ///< Auto mute threshold setting

#define CSK_DAC_AUTO_MUTE_Pos                    7
#define CSK_DAC_AUTO_MUTE_Msk                    (0x1UL << CSK_DAC_AUTO_MUTE_Pos) ///< Auto mute control bit
#define CSK_DAC_AUTO_MUTE_UNSET                  (0x0UL << CSK_DAC_AUTO_MUTE_Pos) ///< Keep current auto mute setting
#define CSK_DAC_AUTO_MUTE_SET                    (0x1UL << CSK_DAC_AUTO_MUTE_Pos) ///< Configure auto mute
/** @} */

/**
 * @name DAC Exclusive Control Codes
 * @brief These operations cannot coexist with other control codes
 * @{
 */
#define CSK_DAC_EXCL_OP_Pos                 29
#define CSK_DAC_EXCL_OP_Msk                 (0x7UL << CSK_DAC_EXCL_OP_Pos) ///< Exclusive ops bitmask (bits 31:29)
#define CSK_DAC_EXCL_OP_UNSET               (0x0UL << CSK_DAC_EXCL_OP_Pos) ///< No exclusive operation
#define CSK_DAC_ABORT_TRANSFER              (0x1UL << CSK_DAC_EXCL_OP_Pos) ///< Abort current transfer
#define CSK_DAC_GET_SAMP_RATE               (0x2UL << CSK_DAC_EXCL_OP_Pos) ///< Get current sample rate
#define CSK_DAC_SET_ECHO_PARAMS             (0x3UL << CSK_DAC_EXCL_OP_Pos) ///< Set echo parameters
/** @} */

/****** DAC specific error codes *****/
#define CSK_DAC_ERROR_SAMP_RATE             (CSK_DRIVER_ERROR_SPECIFIC - 1) ///< Unsupported sample rate
#define CSK_DAC_ERROR_OVER_SAMP_RATIO       (CSK_DRIVER_ERROR_SPECIFIC - 2) ///< Unsupported oversampling ratio
#define CSK_DAC_ERROR_SR_OSR_PAIR           (CSK_DRIVER_ERROR_SPECIFIC - 3) ///< Unsupported SR/OSR combination
#define CSK_DAC_ERROR_TXCFG                 (CSK_DRIVER_ERROR_SPECIFIC - 4) ///< Invalid TX configuration
#define CSK_DAC_ERROR_INITED_ALREADY        (CSK_DRIVER_ERROR_SPECIFIC - 8) ///< DAC already initialized

/**
 * @brief DAC Status Bit Definitions
 * 
 * This structure defines the bitfields for the DAC status register,
 * indicating various operational states and error conditions.
 */
typedef struct _CSK_DAC_STATUS_BIT {
    uint32_t busy :2;       ///< Send busy flags (bit0: left, bit1: right)
    uint32_t tx_emp :1;     ///< TX FIFO empty flag
    uint32_t tx_undf :1;    ///< TX FIFO underflow flag
    uint32_t l_mute :1;     ///< Left channel mute status
    uint32_t r_mute :1;     ///< Right channel mute status
    uint32_t ech_busy :2;   ///< Echo receive busy flags (bit0: left, bit1: right)
    uint32_t ech_ovf :1;    ///< Echo receiver overflow flag
    uint32_t reserved :23;  ///< Reserved bits
} CSK_DAC_STATUS_BIT;

/**
 * @brief DAC Status Register
 * 
 * Union allowing access to status bits either individually or as a whole word.
 */
typedef union {
    uint32_t all;           ///< Complete status word
    CSK_DAC_STATUS_BIT bit; ///< Individual status bits
} CSK_DAC_STATUS;

/****** DAC Event Definitions *****/
#define CSK_DAC_EVENT_SEND_COMPLETE         (0x1UL << 0) ///< Data send completed
#define CSK_DAC_EVENT_TX_FIFO_UNDERRUN      (0x1UL << 1) ///< TX FIFO underflow occurred
#define CSK_DAC_EVENT_TX_FIFO_EMPTY         (0x1UL << 2) ///< TX FIFO is empty
//#define CSK_DAC_EVENT_TX_PING_DONE          (0x1UL << 3) ///< Ping transfer complete
//#define CSK_DAC_EVENT_TX_PONG_DONE          (0x1UL << 4) ///< Pong transfer complete
//#define CSK_DAC_EVENT_BLOCK_COMPLETE        (CSK_DAC_EVENT_TX_PING_DONE | CSK_DAC_EVENT_TX_PONG_DONE)
#define CSK_DAC_EVENT_BLOCK_COMPLETE        (0x1UL << 3) ///< Block transfer complete

//#define CSK_DAC_EVENT_ECHO_RX_COMPLETE      (0x1UL << 6) ///< Echo receive complete
//#define CSK_DAC_EVENT_ECHO_RX_FIFO_OVERRUN  (0x1UL << 7) ///< Echo RX FIFO overflow
//#define CSK_DAC_EVENT_ECHO_RX_PING_DONE     (0x1UL << 8) ///< Echo ping transfer complete
//#define CSK_DAC_EVENT_ECHO_RX_PONG_DONE     (0x1UL << 9) ///< Echo pong transfer complete
//#define CSK_DAC_EVENT_ECHO_BLOCK_COMPLETE   (CSK_DAC_EVENT_ECHO_RX_PING_DONE | \
//                                             CSK_DAC_EVENT_ECHO_RX_PONG_DONE)

#define CSK_DAC_EVENT_ECHO_RX_COMPLETE      (0x1UL << 4) ///< Echo receive complete
#define CSK_DAC_EVENT_ECHO_RX_FIFO_OVERRUN  (0x1UL << 5) ///< Echo RX FIFO overflow
#define CSK_DAC_EVENT_ECHO_RX_FIFO_FULL     (0x1UL << 6) ///< Echo RX FIFO full
#define CSK_DAC_EVENT_ECHO_BLOCK_COMPLETE   (0x1UL << 7) ///< Block transfer complete

//#define CSK_DAC_EVENT_OTHER_ERROR           (0x1UL << 10) ///< Other error occurred
#define CSK_DAC_EVENT_OTHER_ERROR           (0x1UL << 8) ///< Other error occurred

/**
 * @brief DAC Event Callback Function Type
 * @param event_info Event and channel information:
 *        - bits 13:0: Event type
 *        - bits 15:14: DAC/channel number (0=Left, 1=Right)
 *        - bits 23:16: APC dual_channel number
 * @param usr_param User-defined parameter passed to callback
 */
typedef void (*CSK_DAC_SignalEvent_t)(uint32_t event_info, uint32_t usr_param);

/* Event bitfield definitions */
#define CSK_DAC_EVENT_HIGHEST_POS       13
#define CSK_DAC_EVENT_MASK              ((0x1UL << (CSK_DAC_EVENT_HIGHEST_POS + 1)) - 1)
#define CSK_DAC_EVENT_CH_POS            (CSK_DAC_EVENT_HIGHEST_POS + 1)
#define CSK_DAC_EVENT_APC_DCH_POS       (CSK_DAC_EVENT_HIGHEST_POS + 3)
#define CSK_DAC_EVENT_CH_MASK           (0x3)
#define CSK_DAC_EVENT_APC_DCH_MASK      (0xFF)

#define CSK_DAC_SAMPLE_BITS         24  ///< Supported sample resolution (24-bit)

/* Channel bitmap definitions */
#define DAC_BMP_LEFT        CH_BMP_LEFT     ///< Left channel bitmap (0x1 << 0)
#define DAC_BMP_RIGHT       CH_BMP_RIGHT    ///< Right channel bitmap (0x1 << 1)
#define DAC_BMP_STEREO      CH_BMP_STEREO   ///< Stereo channels bitmap (0x3 << 0)

/**
 * @brief Get DAC driver version
 * @return Driver version structure
 */
CSK_DRIVER_VERSION DAC_GetVersion();

/**
 * @name DAC Initialization Flags
 * @{
 */
#define DAC_BMP_FLAG_OUT_LEFT        (0x1 << 0) ///< Enable left output channel
#define DAC_BMP_FLAG_OUT_RIGHT       (0x1 << 1) ///< Enable right output channel
#define DAC_BMP_FLAG_OUT_STEREO      (0x3 << 0) ///< Enable stereo output

#define DAC_BMP_FLAG_ECHO_LEFT      (0x1 << 2) ///< Enable left echo channel
#define DAC_BMP_FLAG_ECHO_RIGHT     (0x1 << 3) ///< Enable right echo channel
#define DAC_BMP_FLAG_ECHO_STEREO    (0x3 << 2) ///< Enable stereo echo

#define DAC_BMP_FLAG_USE_16BITS     (0x1 << 4) ///< Use 16-bit samples (0=32-bit)

#define DAC_BMP_FLAG_OUT_POS        0 ///< Output channel bit position
#define DAC_BMP_FLAG_ECHO_POS       2 ///< Echo channel bit position
/** @} */

/**
 * @brief DMA Channel Configuration Structure
 * 
 * Specifies DMA channels for DAC operations. Set to 0xFF if unused.
 */
typedef struct {
    uint32_t dma_ch_out_left : 8; ///< DMA channel for left output
    uint32_t rsvd0 : 8;
    uint32_t dma_ch_echo_left : 8; ///< DMA channel for left echo
    uint32_t rsvd1 : 8;
} DAC_DMA_CHS;

/**
 * @brief Initialize DAC device group
 * @param dac_grp Pointer to DAC device group instance
 * @param cb_event Event callback function
 * @param usr_param User parameter for callback
 * @param dev_bmp_flag Device bitmap flags (output and echo channels)
 * @param dma_chs_p DMA channel configuration
 * @return Execution status
 */
int32_t DAC_Initialize(void *dac_grp, CSK_DAC_SignalEvent_t cb_event, uint32_t usr_param,
                      uint8_t dev_bmp_flag, DAC_DMA_CHS *dma_chs_p);

/**
 * @brief De-initialize DAC device group
 * @param dac_grp Pointer to DAC device group instance
 * @return Execution status
 */
int32_t DAC_Uninitialize(void *dac_grp);

/**
 * @brief Control DAC power state
 * @param dac_grp Pointer to DAC device group instance
 * @param state Power state to set
 * @return Execution status
 */
int32_t DAC_PowerControl(void *dac_grp, CSK_POWER_STATE state);

/**
 * @name DAC Transfer Flags
 * @{
 */
#define DAC_TX_FLAG_START_NOW   (0x1 << 0) ///< Start transfer immediately
#define DAC_TX_FLAG_NSYNCA      (0x1 << 1) ///< Don't sync cache internally
/** @} */

/**
 * @brief Send data to DAC
 * @param dac_grp Pointer to DAC device group instance
 * @param data Data buffer to send
 * @param num Number of data items
 * @param dev_bmp Target channels bitmap
 * @param tx_flag Transfer control flags
 * @return Execution status
 */
int32_t DAC_Send(void *dac_grp, const uint32_t *data, uint32_t num,
                uint8_t dev_bmp, uint8_t tx_flag);

/**
 * @brief Send data in Ping/Pong mode
 * @param dac_grp Pointer to DAC device group instance
 * @param blks Ping/Pong buffer blocks
 * @param blk_cnt_p Pointer to block count
 * @param dev_bmp Target channels bitmap
 * @param tx_flag Transfer control flags
 * @return Execution status
 */
int32_t DAC_Send_PiPo(void *dac_grp, PIPO_OUT_BLOCK *blks, uint8_t *blk_cnt_p,
                     uint8_t dev_bmp, uint8_t tx_flag);

/**
 * @brief Get transferred blocks count in Ping/Pong mode
 * @param dac_grp Pointer to DAC device group instance
 * @param blks Buffer for block descriptors
 * @param blk_cnt Number of blocks in array
 * @param dev_bmp Target channels bitmap
 * @return Number of transferred blocks (>=0) or error code (<0)
 */
int32_t DAC_PiPo_Xferred_Blocks(void *dac_grp, PIPO_OUT_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp);

/**
 * @brief Receive echo data from DAC
 * @param dac_grp Pointer to DAC device group instance
 * @param data Buffer for received data
 * @param num Number of data items to receive
 * @param echo_bmp Echo channels bitmap
 * @return Execution status
 */
int32_t DAC_Echo_Receive(void *dac_grp, uint32_t *data, uint32_t num, uint8_t echo_bmp);

/**
 * @brief Receive echo data in Ping/Pong mode
 * @param dac_grp Pointer to DAC device group instance
 * @param blks Ping/Pong buffer blocks
 * @param blk_cnt_p Pointer to block count
 * @param echo_bmp Echo channels bitmap
 * @return Execution status
 */
int32_t DAC_Echo_Receive_PiPo(void *dac_grp, PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p, uint8_t echo_bmp);

/**
 * @brief Get transferred echo blocks count in Ping/Pong mode
 * @param dac_grp Pointer to DAC device group instance
 * @param blks Buffer for block descriptors
 * @param blk_cnt Number of blocks in array
 * @param echo_bmp Echo channels bitmap
 * @return Number of transferred blocks (>=0) or error code (<0)
 */
int32_t DAC_Echo_PiPo_Xferred_Blocks(void *dac_grp, PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t echo_bmp);

/**
 * @brief Enable DAC channels
 * @param dac_grp Pointer to DAC device group instance
 * @param dev_bmp Output channels bitmap
 * @param echo_bmp Echo channels bitmap
 * @return Execution status
 */
int32_t DAC_Enable(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp);

/**
 * @brief Disable DAC channels
 * @param dac_grp Pointer to DAC device group instance
 * @param dev_bmp Output channels bitmap
 * @param echo_bmp Echo channels bitmap
 * @return Execution status
 */
int32_t DAC_Disable(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp);

/**
 * @brief Abort DAC transfers
 * @param dac_grp Pointer to DAC device group instance
 * @param dev_bmp Output channels bitmap
 * @param echo_bmp Echo channels bitmap
 * @return Execution status
 */
int32_t DAC_Abort(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp);

/**
 * @brief Get transmitted data count
 * @param dac_grp Pointer to DAC device group instance
 * @param dev_bmp Output channels bitmap
 * @return Number of samples transmitted (>=0) or error code (<0)
 */
int32_t DAC_GetTxCount(void *dac_grp, uint8_t dev_bmp);

/**
 * @brief Get received echo data count
 * @param dac_grp Pointer to DAC device group instance
 * @param echo_bmp Echo channels bitmap
 * @return Number of samples received (>=0) or error code (<0)
 */
int32_t DAC_GetEchoCount(void *dac_grp, uint8_t echo_bmp);

/**
 * @brief Control DAC operation
 * @param dac_grp Pointer to DAC device group instance
 * @param control Control operation
 * @param arg Operation argument
 * @return Execution status
 */
int32_t DAC_Control(void *dac_grp, uint32_t control, uint32_t arg);

/**
 * @name DAC Volume Control Flags
 * @{
 */
#define DAC_VOL_FLAG_A_LEFT     0x1UL ///< Set left analog gain
#define DAC_VOL_FLAG_A_RIGHT    0x2UL ///< Set right analog gain
#define DAC_VOL_FLAG_D_LEFT     0x4UL ///< Set left digital gain
#define DAC_VOL_FLAG_D_RIGHT    0x8UL ///< Set right digital gain
/** @} */

/**
 * @brief DAC Gain Range Definitions
 * @{
 */

#define DAC_GAIN_D_MAX_DB       30    ///< Maximum digital gain (dB)
#define DAC_GAIN_D_MIN_DB       -113  ///< Minimum digital gain (dB)
#define DAC_GAIN_D_DEF_DB       0     ///< Default digital gain (dB)
#define DAC_GAIN_D_DB_STEP      1     ///< Digital gain step size (dB)

#define DAC_GAIN_D_MAX_VAL      0xFF  ///< Maximum digital gain register value
#define DAC_GAIN_D_MIN_VAL      0x70  ///< Minimum digital gain register value
#define DAC_GAIN_D_DEF_VAL      0xE1  ///< Default digital gain register value
#define DAC_GAIN_D_VAL(dB)      ((DAC_GAIN_D_MIN_VAL + \
                                (dB - DAC_GAIN_D_MIN_DB + DAC_GAIN_D_DB_STEP - 1) / DAC_GAIN_D_DB_STEP) & 0xFF)
/** @} */

/**
 * @brief Set DAC volume levels
 * @param dac_grp Pointer to DAC device group instance
 * @param a_gain Analog gains (left in bits 15:0, right in bits 31:16)
 * @param d_gain Digital gains (left in bits 15:0, right in bits 31:16)
 * @param vol_flag Volume control flags
 * @return Execution status
 */
int32_t DAC_SetVolume(void *dac_grp, /*uint32_t a_gain,*/ uint32_t d_gain, uint32_t vol_flag);

/**
 * @brief Set DAC mute state
 * @param dac_grp Pointer to DAC device group instance
 * @param mute_val Mute control (bit0: left, bit1: right)
 * @param dev_bmp Target channels bitmap
 * @return Execution status
 */
int32_t DAC_SetMute(void *dac_grp, uint8_t mute_val, uint8_t dev_bmp);

/**
 * @brief Get DAC status
 * @param dac_grp Pointer to DAC device group instance
 * @param status Pointer to status structure
 * @return Execution status
 */
int32_t DAC_GetStatus(void *dac_grp, CSK_DAC_STATUS *status);

///**
// * @name Equalizer Control Functions
// * @{
// */
///**
// * @brief Set array of EQ coefficients
// * @param dac_grp Pointer to DAC device group instance
// * @param eqcoefs Array of EQ coefficients
// * @param num Number of coefficients
// * @return Execution status
// */
//int32_t DAC_EQ_Set_Coef_Array(void *dac_grp, uint32_t *eqcoefs, uint32_t num);
//
///**
// * @brief Set single EQ coefficient
// * @param dac_grp Pointer to DAC device group instance
// * @param index Coefficient index
// * @param eqcoef Coefficient value
// * @return Execution status
// */
//int32_t DAC_EQ_Set_Coef(void *dac_grp, uint32_t index, uint32_t eqcoef);
//
///**
// * @brief Enable EQ with specified number of stages
// * @param dac_grp Pointer to DAC device group instance
// * @param stages Number of EQ stages to enable
// * @return Execution status
// */
//int32_t DAC_EQ_Enable(void *dac_grp, uint32_t stages);
//
///**
// * @brief Disable EQ
// * @param dac_grp Pointer to DAC device group instance
// * @return Execution status
// */
//int32_t DAC_EQ_Disble(void *dac_grp);
//
///**
// * @brief Clear EQ coefficients
// * @param dac_grp Pointer to DAC device group instance
// * @param wait_done Wait for operation to complete
// * @return Execution status
// */
//int32_t DAC_EQ_Clear(void *dac_grp, uint8_t wait_done);
///** @} */

/**
 * @brief Get DAC01 device group instance
 * @return Pointer to DAC01 device group structure
 */
void* DAC01();

#endif /* __DRIVER_DAC_H */
