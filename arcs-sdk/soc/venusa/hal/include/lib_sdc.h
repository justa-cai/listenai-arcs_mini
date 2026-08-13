/**
 * @file    lib_sdc.h
 * @brief   Secure Digital Card (SDC) Driver API Header File
 * @details This file contains the data types, macro definitions, and function prototypes
 *          required for interfacing with SDC/SDIO devices. It includes configuration options,
 *          error codes, hardware control registers, and core driver functionality.
 * @copyright (c) 2015 Your Company Name. All rights reserved.
 * @license   Apache License Version 2.0 (see LICENSE file for details)
 */

#ifndef _LIB_SDC_API_H_
#define _LIB_SDC_API_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup SDIO
  * @brief SDIO HAL module driver
  * @{
  */

/** @defgroup SDIO_Exported_Constants SDIO Exported Constants
  * @{
  */

/** @defgroup type_definitions Basic Data Type Definitions
 *  @{
 */
#ifndef s8
#define s8                      char        ///< Signed 8-bit integer
#endif
#ifndef u8
#define u8                     uint8_t     ///< Unsigned 8-bit integer
#endif
#ifndef s16
#define s16                    int16_t    ///< Signed 16-bit integer
#endif
#ifndef u16
#define u16                    uint16_t   ///< Unsigned 16-bit integer
#endif
#ifndef s32
#define s32                    int32_t    ///< Signed 32-bit integer
#endif
#ifndef u32
#define u32                    uint32_t   ///< Unsigned 32-bit integer
#endif
#ifndef s64
#define s64                    int64_t    ///< Signed 64-bit integer
#endif
#ifndef u64
#define u64                    uint64_t   ///< Unsigned 64-bit integer
#endif
/** @} */ /* End of group type_definitions */

/** @defgroup card_state Card State Codes
 *  @{
 */
#define SD_0 0                 ///< Card absent
#define SD_1 1                ///< Card present
#define SD_HOST_NUM 2         ///< Number of supported hosts
/** @} */ /* End of group card_state */

/** @defgroup card_types Supported Card Types
 *  @{
 */
#define CARD_TYPE_UNKNOWN     0           ///< Unknown card type
#define MEMORY_CARD_TYPE_SD   1           ///< Standard SD card
#define MEMORY_CARD_TYPE_MMC  2           ///< MultiMediaCard
#define SDIO_TYPE_CARD        3           ///< SDIO card
#define MEMORY_SDIO_COMBO     4           ///< Combo SDIO/memory card
#define MEMORY_eMMC           5           ///< eMMC embedded flash
/** @} */ /* End of group card_types */

/** @defgroup sdio_options Driver Configuration Options
 *  @{
 */
#define SDC_OPTION_ENABLE       0x00000001 ///< Enable driver features
#define SDC_OPTION_CD_INVERT    0x00000002 ///< Invert card detect signal
#define SDC_OPTION_FIXED        0x00000004 ///< Fixed clock selection mode
#define SDC_OPTION_SDIO_STD_FUNC    0x00000008 ///< Use standard SDIO functions
#define SDC_OPTION_SDIO_FORCE_3_3_V  0x00000010 ///< Force 3.3V signaling for SDIO
/** @} */ /* End of group sdio_options */

/** @defgroup soft_reset Soft Reset Control Bits
 *  @{
 */
/* 0x2F: SoftRst */
#define SD_SOFTRST_ALL      (1 << 0)    ///< Reset all modules
#define SD_SOFTRST_CMD      (1 << 1)    ///< Reset command module
#define SD_SOFTRST_DAT      (1 << 2)    ///< Reset data path module
/** @} */ /* End of group soft_reset */

#define FTSDC021_READ_CIS   ///< Enable CIS reading capability
#ifndef SDHCI_INTR_STS_CARD_INTR
#define SDHCI_INTR_STS_CARD_INTR    (1 << 8) ///< Card interrupt status bit
#endif
/** @} */ /* End of group SDC_Exported_Constants */

/** @defgroup SDIO_Exported_Types SDIO Exported Types
  * @{
  */
/** @defgroup sdio_tuple SDIO Function Tuple Structure
 *  @{
 */
#ifdef FTSDC021_READ_CIS
struct sdio_func_tuple
{
    struct sdio_func_tuple* next; ///< Next function in chain
    u8 code;                       ///< Function code
    u8 size;                       ///< Data segment size
    u8 data[256];                  ///< Function-specific data
};
#endif
/** @} */ /* End of group sdio_tuple */

/** @defgroup sdio_func SDIO Function Information Block
 *  @{
 */
typedef struct
{
    u32     num;        /*!< Function number assigned by host */
    u16     vendor;     /*!< Vendor ID (JEDEC assignment) */
    u16     device;     /*!< Device ID (manufacturer specific) */

    u32     max_blksize;    /*!< Maximum supported block size */
    u32     cur_blksize;    /*!< Currently configured block size */

    u32     enable_timeout; /*!< Maximum time to enable function (ms) */

    u32     state;      /*!< Current functional state */

    u32     num_info;   /*!< Number of information strings available */
    const s8**        info;     /*!< Array of information strings */
#ifdef FTSDC021_READ_CIS
    struct sdio_func_tuple* tuples; ///< Associated function tuples
#endif
    u8      class;      /*!< Standard interface class code */
    u8      enable;     /*!< Function enable flag */
} SDIO_FUNC;
/** @} */ /* End of group sdio_func */

/** @defgroup callback_types Driver Event Callback Types
 *  @{
 */
typedef void (*SDIO_ISR_handler)(u16);    ///< SDIO interrupt service routine
typedef void (*SDC_device_handler)(u8);   ///< Device event handler
typedef u32(*SDC_driver_func)(u8);       ///< Driver operation function
/** @} */ /* End of group callback_types */

/** @defgroup sd_result SD Operation Result Codes
 *  @{
 */
typedef enum _SD_SD_RESULT
{
    ERR_SD_NO_ERROR = 0,              ///< Successful operation
    ERR_SD_CARD_ERROR,               ///< General card error occurred
    ERR_SD_CARD_NOT_EXIST,           ///< No card detected
    ERR_SD_CARD_LOCK,                ///< Card is write protected
    ERR_SD_IP_IDX_ERROR,             ///< Invalid host index
    ERR_SD_CLK_IN_ERROR,             ///< Clock input failure
    ERR_SD_HOST_RESET_TIMEOUT,       ///< Host reset timed out
    ERR_SD_CARD_STATUS_ERROR,        ///< Card returned error status
    ERR_SD_SEND_COMMAND_TIMEOUT,     ///< Command transmission timed out
    ERR_SD_DATA_CRC_ERROR,           ///< Data CRC check failed
    ERR_SD_RSP_CRC_ERROR,            ///< Response CRC check failed
    ERR_SD_DATA_TIMEOUT,             ///< Data transfer timed out
    ERR_SD_RSP_TIMEOUT,             ///< Response timed out
    ERR_SD_CMD_RSP_ARG_ERROR,        ///< Command argument error in response
    ERR_SD_DATA_LENGTH_TOO_LONG,     ///< Data length exceeds maximum
    ERR_SD_WAIT_OVERRUN_TIMEOUT,     ///< Overrun condition during wait
    ERR_SD_WAIT_UNDERRUN_TIMEOUT,    ///< Underrun condition during wait
    ERR_SD_WAIT_DATA_CRC_TIMEOUT,    ///< Data CRC error during wait
    ERR_SD_WAIT_TRANSFER_END_TIMEOUT,///< Transfer completion timeout
    ERR_SD_WAIT_OPERATION_COMPLETE_TIMEOUT, ///< Operation completion timeout
    ERR_SD_WAIT_TRANSFER_STATE_TIMEOUT, ///< Transfer state change timeout
    ERR_SD_STATE_CHANGE_TIMEOUT,     ///< State transition timeout
    ERR_SD_CARD_IS_BUSY,            ///< Card busy with previous operation
    ERR_SD_CID_REGISTER_ERROR,      ///< CID register read error
    ERR_SD_CSD_REGISTER_ERROR,      ///< CSD register read error
    ERR_SD_OUT_OF_VOLTAGE_RANGE,    ///< Voltage out of operational range
    ERR_SD_OUT_OF_ADDRESS_RANGE,    ///< Address out of valid range
    ERR_SD_INIT_ERROR,             ///< Initialization sequence failed
    ERR_SD_CARD_INITIAL_NOT_COMPLETE, ///< Card initialization incomplete
    ERR_SD_OTHER_ERROR             ///< Other unspecified error
} SD_RESULT;
/** @} */ /* End of group sd_result */

/** @defgroup platform_setting Platform Configuration Typedef
 *  @{
 */
typedef void(*sdc_platform_setting_t)(void);
/** @} */ /* End of group platform_setting */
/**
  * @}
  */ /* End of group SDIO_Exported_Types */

/** @defgroup SDIO_Exported_Macro SDIO Exported Macros
  * @{
  */
#define GM_SDC_ACTION_INIT                  1  // SD host initialization
#define GM_SDC_ACTION_CARD_SCAN             2  // Scan for card presence (input: SD speed)
#define GM_SDC_ACTION_GET_CARD_TYPE         3  // Get detected card type (output)
#define GM_SDC_ACTION_SET_BUS_WIDTH         4  // Set bus width configuration (input)
#define GM_SDC_ACTION_DET_INIT              5  // Initialize detection parameters
#define GM_SDC_ACTION_DET_HANDLE            6  // Process detection events
#define GM_SDC_ACTION_SOFT_RESET            7  // Perform soft reset (input: reset flags)
#define GM_SDC_ACTION_IS_CARD_EXIST         8  // Check card presence (returns status)
#define GM_SDC_ACTION_IS_CARD_WRITABLE      9  // Check write protection status
#define GM_SDC_ACTION_IS_CARD_INSERT        10 // Check physical insertion status
#define GM_SDC_ACTION_GET_BLK_LEN           11 // Get current block length (output)
#define GM_SDC_ACTION_GET_BLK_NUM           12 // Get total block count (output)
#define GM_SDC_ACTION_GET_ERASE_SIZE        13 // Get erase block size (output)
#define GM_SDC_ACTION_ENABLE_IRQ            14 // Enable specific interrupts (input: bitmask)
#define GM_SDC_ACTION_DISABLE_IRQ           15 // Disable specific interrupts (input: bitmask)
#define GM_SDC_ACTION_SDIO_REG_IRQ          16 // Register SDIO IRQ handler (input: SDIO_ISR_handler)
#define GM_SDC_ACTION_SDIO_REMOVE_IRQ       17 // Remove SDIO IRQ registration
#define GM_SDC_ACTION_IRQ_SET_INIT          18 // Set initial IRQ configuration
#define GM_SDC_ACTION_IRQ_SET_NORMAL        19 // Set normal operating IRQ configuration
#define GM_SDC_ACTION_IS_APP_INIT_DONE      20 // Check application initialization status
#define GM_SDC_ACTION_SET_APP_INIT_DONE     21 // Mark application initialization complete
#define GM_SDC_ACTION_SET_ADMA_BUFER        22  // Set ADMA buffer address (input)
#define GM_SDC_ACTION_IS_HOST_INIT_DONE     23 // Check host initialization status
#define GM_SDC_ACTION_ENTER_IDLE_STATE      24 // Enter low-power idle state
#define GM_SDC_ACTION_CARD_DETECTION        25 // Run card detection sequence
#define GM_SDC_ACTION_REG_SDCARD_APP_INIT   26 // Register SD card app init handler (input: SDC_device_handler)
#define GM_SDC_ACTION_REG_SDIO_APP_INIT     27 // Register SDIO app init handler (input: SDC_device_handler)
#define GM_SDC_ACTION_SDIO_FUNC_SET_BLOCK_SIZE  28 // Set SDIO function block size (updates cur_blksize)
#define GM_SDC_ACTION_SDIO_FUNC_ENABLE          29 // Enable SDIO function
#define GM_SDC_ACTION_SDIO_FUNC_DISABLE         30 // Disable SDIO function
#define GM_SDC_ACTION_SDIO_FUNC_CLAIM_IRQ       31 // Claim IRQ ownership for SDIO function
#define GM_SDC_ACTION_SDIO_FUNC_RELEASE_IRQ     32 // Release IRQ ownership for SDIO function
#define GM_SDC_ACTION_SDIO_GET_FUNC             33 // Retrieve specific function structure
#define GM_SDC_ACTION_SDIO_GET_FUNC_NUM         34 // Get number of supported functions
#define GM_SDC_ACTION_PS_SET_INIT_FUNC          35 // Set power save initialization function
#define GM_SDC_ACTION_PS_SET_DETECTION_FUNC     36 // Set power save detection function
/**
  * @}
  */ /* End of group SDIO_Exported_Macro */

/** @defgroup SDIO_Exported_Functions SDIO Exported Functions
 *  @{ */

/**
 * @fn u32 gm_sdc_api_action(u8 ip_idx, u32 type, void* in, void* out);
 * @brief Generic SDIO action dispatcher
 * @param[in] ip_idx Host interface index
 * @param[in] type Action code from @ref action_codes
 * @param[in] in Input data pointer (varies by action)
 * @param[out] out Output data pointer (varies by action)
 * @return Error code from @ref sd_result
 */
u32 gm_sdc_api_action(u8 ip_idx, u32 type, void* in, void* out);

/**
 * @fn u32 gm_sdc_api_erase(u8 ip_idx, u32 addr, u32 cnt);
 * @brief Erase sectors on storage device
 * @param[in] ip_idx Host interface index
 * @param[in] addr Starting address
 * @param[in] cnt Number of sectors to erase
 * @return Error code from @ref sd_result
 */
u32 gm_sdc_api_erase(u8 ip_idx, u32 addr, u32 cnt);

/**
 * @fn u32 gm_sdc_api_sdcard_sector_read(u8 ip_idx, u32 sector, u32 cnt, void* buff);
 * @brief Read sectors from SD card
 * @param[in] ip_idx Host interface index
 * @param[in] sector Starting sector number
 * @param[in] cnt Number of sectors to read
 * @param[out] buff Buffer for read data
 * @return Error code from @ref sd_result
 */
u32 gm_sdc_api_sdcard_sector_read(u8 ip_idx, u32 sector, u32 cnt, void* buff);

/**
 * @fn u32 gm_sdc_api_sdcard_sector_write(u8 ip_idx, u32 sector, u32 cnt, void* buff);
 * @brief Write sectors to SD card
 * @param[in] ip_idx Host interface index
 * @param[in] sector Starting sector number
 * @param[in] cnt Number of sectors to write
 * @param[in] buff Buffer containing data to write
 * @return Error code from @ref sd_result
 */
u32 gm_sdc_api_sdcard_sector_write(u8 ip_idx, u32 sector, u32 cnt, void* buff);

/**
 * @fn u32 gm_sdc_api_sdio_cmd53(u8 ip_idx, u32 write, u8 fn, u32 addr, u32 incr_addr, u32* buf, u32 blocks, u32 blksz);
 * @brief Execute SDIO command 53 (General Purpose Command)
 * @param[in] ip_idx Host interface index
 * @param[in] write Write direction flag
 * @param[in] fn Function number
 * @param[in] addr Base address
 * @param[in] incr_addr Address increment flag
 * @param[in] buf Data buffer
 * @param[in] blocks Number of blocks
 * @param[in] blksz Block size
 * @return Error code from @ref sd_result
 */
u32 gm_sdc_api_sdio_cmd53(u8 ip_idx, u32 write, u8 fn, u32 addr, u32 incr_addr, u32* buf, u32 blocks, u32 blksz);

/**
 * @fn u32 gm_sdc_api_sdio_cmd52(u8 ip_idx, u32 write, u8 fn, u32 addr, u8 in, u8* out);
 * @brief Execute SDIO command 52 (Send Relative Address)
 * @param[in] ip_idx Host interface index
 * @param[in] write Write direction flag
 * @param[in] fn Function number
 * @param[in] addr Address parameter
 * @param[in] in Input data byte
 * @param[out] out Output data pointer
 * @return Error code from @ref sd_result
 */
u32 gm_sdc_api_sdio_cmd52(u8 ip_idx, u32 write, u8 fn, u32 addr, u8 in, u8* out);

/**
 * @fn u32 gm_api_sdc_platform_init(u32 sdc0_option, u32 sdc1_option, sdc_platform_setting_t setting, u32 card_buffer);
 * @brief Initialize SDC platform drivers
 * @param[in] sdc0_option Configuration options for SDC0
 * @param[in] sdc1_option Configuration options for SDC1
 * @param[in] setting Platform setup function
 * @param[in] card_buffer Card buffer address
 * @return Error code from @ref sd_result
 */
u32 gm_api_sdc_platform_init(u32 sdc0_option, u32 sdc1_option, sdc_platform_setting_t setting, u32 card_buffer);
/** @} */ /* End of group SDIO_Exported_Functions */
/** @} */ /* End of SDIO group */
#ifdef __cplusplus
}
#endif

#endif /* _LIB_SDC_API_H_ */
