/*
 * Project:      Common Driver definitions
 */

#ifndef __DRIVER_COMMON_H
#define __DRIVER_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define CSK_DRIVER_VERSION_MAJOR_MINOR(major,minor) (((major) << 8) | (minor))

/**
 \brief Driver Version
 */
typedef struct _CSK_DRIVER_VERSION
{
    uint16_t api;                         ///< API version
    uint16_t drv;                         ///< Driver version
} CSK_DRIVER_VERSION;

/* General return codes */
#define CSK_DRIVER_OK                 0 ///< Operation succeeded
#define CSK_DRIVER_ERROR             -1 ///< Unspecified error
#define CSK_DRIVER_ERROR_BUSY        -2 ///< Driver is busy
#define CSK_DRIVER_ERROR_TIMEOUT     -3 ///< Timeout occurred
#define CSK_DRIVER_ERROR_UNSUPPORTED -4 ///< Operation not supported
#define CSK_DRIVER_ERROR_PARAMETER   -5 ///< Parameter error
#define CSK_DRIVER_ERROR_SPECIFIC    -6 ///< Start of driver specific errors

/**
 \brief General power states
 */
typedef enum _CSK_POWER_STATE
{
    CSK_POWER_OFF,                        ///< Power off: no operation possible
    CSK_POWER_LOW,                        ///< Low Power mode: retain state, detect and signal wake-up events
    CSK_POWER_FULL                        ///< Power on: full operation at maximum performance
} CSK_POWER_STATE;


//-------------------------------------------------------------
// For PingPong block transfer
typedef struct {
    uint32_t reserved; // SHOULD set to 0
    uint32_t *sample_data;
    uint32_t sample_cnt; // SHOULD be EVEN when 16-bit sample!!
    // 1: Stop PingPing after this block transfer
    uint32_t flags;
} PIPO_IN_BLOCK;

typedef struct {
    uint32_t *sample_data;
    uint32_t reserved; // SHOULD set to 0
    uint32_t sample_cnt; // SHOULD be EVEN when 16-bit sample!!
    // 1: Stop PingPing after this block transfer
    uint32_t flags;
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


//-------------------------------------------------------------
// Audio buffer used for DMA transfer of multiple non-continuous buffers
// NOTE: AUDIO_BUFFER_USER (array) SHOULD NOT be in the stack, and SHOULD NOT
//      be released until data transfer is completed or aborted!
typedef struct {
    //uint32_t dma_lli_words[12];
    uint32_t dma_lli_words[6];
} DMA_DESC;

typedef struct {
    uint32_t* sample_data;
    uint32_t  sample_cnt; // SHOULD be EVEN when 16-bit sample!!
    DMA_DESC  dma_desc; // DON'T TOUCH IT! reserved for DMA driver only
} AUDIO_BUFFER_USER;

// parameters setting of APC ECHO channels
/*
typedef struct {
    uint32_t    samp_rate; // echo sample rate, generally equals to recording sample rate
    uint8_t     echo_mixed; // if both L&R echo channels are used, whether L&R data are mixed
                           // 1: mixed, 0: NOT mixed
    uint8_t     trim_16bits;// only valid for 24 MSB and 32 channel mode, when set to 1:
                            // record: trim low 16bits, get 16bits audio data
                            // playback: 16bits audio data is placed at high 16bits of WORD
    uint8_t     reserved[2];
} ECHO_PARAMS;
*/

typedef struct {
    uint32_t    samp_rate : 24; // echo sample rate, generally equals to recording sample rate
    uint32_t    trim_16bits : 1;// only valid for 24 MSB and 32 channel mode, when set to 1:
                            // record: trim low 16bits, get 16bits audio data
                            // playback: 16bits audio data is placed at high 16bits of WORD
    uint32_t    echo_mixed : 1; // if both L&R echo channels are used, whether L&R data are mixed
                            // 1: mixed, 0: NOT mixed
    uint32_t    reserved : 6;
} ECHO_PARAMS;

// Channel definition in audio interface, i.e. I2S, ADC_PDM couple, DAC couple etc.
#define CH_BMP_LEFT    (0x1 << 0)
#define CH_BMP_RIGHT   (0x1 << 1)
#define CH_BMP_STEREO  (CH_BMP_LEFT | CH_BMP_RIGHT)


#endif /* __DRIVER_COMMON_H */
