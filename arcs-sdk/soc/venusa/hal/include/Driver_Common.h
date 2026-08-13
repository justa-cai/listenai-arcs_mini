/*!
 * @file Driver_Common.h
 * @brief Common driver definitions and data structures for CSK drivers
 *        This file contains shared type definitions, version information,
 *      power state enumerations, and audio-related data structures used across drivers.
 *        Project: Common Driver definitions
 */

#ifndef __DRIVER_COMMON_H
#define __DRIVER_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Create a combined major/minor version number from components
 * @param[in] major Major version component (upper 8 bits)
 * @param[in] minor Minor version component (lower 8 bits)
 * @return Combined 16-bit version number with major in MSB and minor in LSB
 */
#define CSK_DRIVER_VERSION_MAJOR_MINOR(major,minor) (((major) << 8) | (minor))

/**
 * @struct CSK_DRIVER_VERSION
 * @brief Version information structure containing API and driver versions
 * @var api API version number (typically matches driver capabilities)
 * @var drv Driver implementation version number
 */
typedef struct _CSK_DRIVER_VERSION
{
    uint16_t api;                        ///< API version number
    uint16_t drv;                        ///< Driver implementation version number
} CSK_DRIVER_VERSION;

/*================================================================*/
/*                     Return Code Definitions                 */
/*================================================================*/

#define CSK_DRIVER_OK                 0 ///< Operation completed successfully
#define CSK_DRIVER_ERROR             -1 ///< Unspecified general error occurred
#define CSK_DRIVER_ERROR_BUSY        -2 ///< Driver resource is currently busy
#define CSK_DRIVER_ERROR_TIMEOUT     -3 ///< Operation timed out waiting for condition
#define CSK_DRIVER_ERROR_UNSUPPORTED -4 ///< Requested operation not supported by hardware
#define CSK_DRIVER_ERROR_PARAMETER   -5 ///< Invalid parameter passed to function
#define CSK_DRIVER_ERROR_SPECIFIC    -6 ///< Start of driver-specific error codes

/**
 * @enum CSK_POWER_STATE
 * @brief Power management states for peripheral devices
 * @var CSK_POWER_OFF Completely powered down (no operation possible)
 * @var CSK_POWER_LOW Low power mode (preserves state, wakes on events)
 * @var CSK_POWER_FULL Fully powered on (maximum performance operation)
 */
typedef enum _CSK_POWER_STATE
{
    CSK_POWER_OFF,                        ///< Power off - no operation possible
    CSK_POWER_LOW,                        ///< Low power mode - retains state, wakes on events
    CSK_POWER_FULL                        ///< Full power mode - maximum performance operation
} CSK_POWER_STATE;

// For PingPong block transfer
typedef struct {
    uint32_t reserved; // SHOULD set to 0
    void *sample_data;
    uint32_t sample_cnt; // SHOULD be EVEN when 16-bit sample!!
    uint32_t flags; //1: Stop PingPing after this block transfer
} PIPO_IN_BLOCK;

typedef struct {
    void *sample_data;
    uint32_t reserved; // SHOULD set to 0
    uint32_t sample_cnt; // SHOULD be EVEN when 16-bit sample!!
    uint32_t flags; // 1: indicates stop PingPing after the block transfer
} PIPO_OUT_BLOCK;

typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t cnt;
    uint32_t flags; //1: Stop PingPing after this block transfer
} PIPO_XFER_BLOCK;

typedef union {
    PIPO_IN_BLOCK in;
    PIPO_OUT_BLOCK out;
    PIPO_XFER_BLOCK xfer;
} PIPO_IO_BLOCK;

/**
 * @struct DMA_DESC
 * @brief Direct Memory Access descriptor structure for multi-buffer transfers
 * @details Used for managing non-continuous audio buffers in DMA operations
 * @note Contents should NOT be modified directly by application code
 * @var dma_lli_words Linked list pointer words for DMA engine control
 */
typedef struct {
    uint32_t dma_lli_words[12];          ///< DMA linked list instruction words
} DMA_DESC;

/**
 * @struct AUDIO_BUFFER_USER
 * @brief User-accessible audio buffer structure for DMA operations
 * @details Manages audio sample data and associated DMA descriptors
 * @warning The 'dma_desc' field MUST NOT be modified by application code
 * @warning Buffer memory MUST remain valid until transfer completes or aborts
 * @var sample_data Pointer to audio sample data array
 * @var sample_cnt Number of samples (MUST BE EVEN for 16-bit samples!)
 * @var dma_desc Reserved for DMA driver use (DO NOT TOUCH)
 */
typedef struct {
    uint32_t* sample_data;               ///< Pointer to audio sample data
    uint32_t  sample_cnt;               ///< Number of samples (even for 16-bit format)
    DMA_DESC  dma_desc;                  ///< DMA descriptor (driver only)
} AUDIO_BUFFER_USER;

/**
 * @struct ECHO_PARAMS
 * @brief Echo channel configuration parameters for APC processing
 * @details Bitfield layout optimizes memory usage while maintaining readability
 * @var samp_rate Echo sample rate (matches recording sample rate)
 * @var trim_16bits Trim low 16 bits during recording/playback for 24MSB/32ch modes
 * @var echo_mixed Mix left/right channels when both echo channels are active
 * @var reserved Reserved bits for future expansion
 */
typedef struct {
    uint32_t    samp_rate : 24;         ///< Echo sample rate (Hz)
    uint32_t    trim_16bits : 1;        ///< Trim low 16 bits flag (for 24MSB/32ch modes)
    uint32_t    echo_mixed : 1;         ///< Mix stereo channels flag
    uint32_t    reserved : 6;           ///< Reserved for future use
} ECHO_PARAMS;


#define CH_BMP_LEFT    (0x1 << 0)      ///< Left channel mask
#define CH_BMP_RIGHT   (0x1 << 1)      ///< Right channel mask
#define CH_BMP_STEREO  (CH_BMP_LEFT | CH_BMP_RIGHT) ///< Stereo channel mask (both channels)

#endif /* __DRIVER_COMMON_H */
