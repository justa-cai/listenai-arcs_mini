/**
 * @file dvp.h
 * @brief Digital Video Port (DVP) driver internal implementation header file.
 *        This file contains private definitions, data structures and configuration
 *        parameters used by the DVP driver module. It includes register mappings,
 *        format specifications, clock settings and device control structures.
 *
 * @details
 *   Contains hardware-specific configurations for DVP interface including:
 *     - Clock frequency definitions based on target platform
 *     - Image format encoding schemes (YUV/RGB variants)
 *     - Timing polarity control options
 *     - Device state management structure
 *     - Interrupt event callback mechanism
 *
 * @attention
 *   This is an internal header file intended for driver implementation use only.
 *   Application code should not directly include this file.
 */

#ifndef __DRIVER_DVP_INTERNAL_H
#define __DRIVER_DVP_INTERNAL_H

#include "venusa_ap.h"          ///< Chip-specific peripheral definitions
#include "ClockManager.h"  ///< System clock management utilities
#include "log_print.h"     ///< Logging functionality interfaces

#include "image_vic_reg.h" ///< Video input capture register definitions
#include "Driver_DVP.h"    ///< Public DVP driver interface declarations


/**
 *  These macros define available clock sources and divisor limits for DVP interface operation.
 */
#define DVP_CLK_OUT_SEL_100MHz  BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK ///< Base clock frequency selection: 100MHz or 200MHz
#define DVP_CLK_OUT_SEL_24MHz   24000000  ///< Alternative clock source: 24MHz
#define DVP_CLK_OUT_DIV_MAX     0x1FF     ///< Maximum allowed clock divider value
#define DVP_CLK_OUT_DIV_MIN     1         ///< Minimum allowed clock divider value
#define DVP_CLK_OUT_HZ_MAX      (DVP_CLK_OUT_SEL_100MHz / DVP_CLK_OUT_DIV_MIN) ///< Highest achievable output frequency
#define DVP_CLK_OUT_HZ_MIN      (DVP_CLK_OUT_SEL_24MHz / DVP_CLK_OUT_DIV_MAX)    ///< Lowest achievable output frequency =46966Hz

/**
 * @brief Receiver FIFO depth in words
 * @details Determines amount of buffering available before potential overrun conditions
 */
#define CSK_DVP_RXFIFO_DEPTH    64  // words

/********************  Bits definition for DVP_INPUT_FORM register  *******************/
/**
 * Image format selection codes for DVP_INPUT_FORM register
 */
/** @brief YUV422 planar format with Y0/Cb/Y1/Cr ordering */
#define CSK_DVP_INPUT_FORM_YUV422_Y0CBY1CR        ((0 << 2) | 0x0U)
/** @brief YUV422 interleaved format with Cb/Y0/Cr/Y1 ordering */
#define CSK_DVP_INPUT_FORM_YUV422_CBY0CRY1        ((0 << 2) | 0x1U)
/** @brief YUV422 alternative plane arrangement */
#define CSK_DVP_INPUT_FORM_YUV422_Y0CRY1CB        ((0 << 2) | 0x2U)
/** @brief YUV422 reversed interleaved sequence */
#define CSK_DVP_INPUT_FORM_YUV422_CRY0CBY1        ((0 << 2) | 0x3U)
/** @brief Full YUV444 chroma resolution format */
#define CSK_DVP_INPUT_FORM_YUV444_Y0CBCR          ((1 << 2) | 0x0U)
/** @brief 5-bit per component RGB format */
#define CSK_DVP_INPUT_FORM_RGB555                 ((2 << 2) | 0x0U)
/** @brief 5-6-5 bit distribution RGB format */
#define CSK_DVP_INPUT_FORM_RGB565                 ((3 << 2) | 0x0U)
/** @brief True color 8-bit per component RGB format */
#define CSK_DVP_INPUT_FORM_RGB888                 ((4 << 2) | 0x0U)
/** @brief 8-bit monochrome luminosity channel */
#define CSK_DVP_INPUT_FORM_LUMINA_8BIT            ((5 << 2) | 0x0U)


/********************  Bits definition for DVP_POL_CNTL register  *********************/
/**
 * Timing polarity configuration options for video synchronization signals
 */
/** @brief Data sampling occurs on rising edge of DVP clock */
#define CSK_DVP_POL_CNTL_CLOCK_RISING       (0x0UL)
/** @brief Data sampling occurs on falling edge of DVP clock */
#define CSK_DVP_POL_CNTL_CLOCK_FALLING      (0x1UL)

/** @brief Horizontal sync pulse active high */
#define CSK_DVP_POL_CNTL_HSYNC_RISING       (0x0UL)
/** @brief Horizontal sync pulse active low */
#define CSK_DVP_POL_CNTL_HSYNC_FALLING      (0x1UL)

/** @brief Vertical sync pulse active high */
#define CSK_DVP_POL_CNTL_VSYNC_RISING       (0x0UL)
/** @brief Vertical sync pulse active low */
#define CSK_DVP_POL_CNTL_VSYNC_FALLING      (0x1UL)

/** @brief Least Significant Bit first in data bus transfer */
#define CSK_DVP_DATA_BUS_ALIGN_LSB          (0x0UL)
/** @brief Most Significant Bit first in data bus transfer */
#define CSK_DVP_DATA_BUS_ALIGN_MSB          (0x1UL)

/**
 * @enum DVP_emState
 * @brief Operational states of the DVP module
 * @var DVP_STATE_RESET Uninitialized or disabled state
 * @var DVP_STATE_READY Ready for operation
 * @var DVP_STATE_BUSY Processing internal operations
 * @var DVP_STATE_TIMEOUT Operation timed out
 * @var DVP_STATE_ERROR Error condition detected
 * @var DVP_STATE_SUSPENDED Suspended state
 */
typedef enum
{
    DVP_STATE_RESET             = 0x00U,  /*!< DVP not yet initialized or disabled  */
    DVP_STATE_READY             = 0x01U,  /*!< DVP initialized and ready for use    */
    DVP_STATE_BUSY              = 0x02U,  /*!< DVP internal processing is ongoing   */
    DVP_STATE_TIMEOUT           = 0x03U,  /*!< DVP timeout state                    */
    DVP_STATE_ERROR             = 0x04U,  /*!< DVP error state                      */
    DVP_STATE_SUSPENDED         = 0x05U,  /*!< DVP suspend state                    */
    DVP_STATE_BUTT
} DVP_emState;

/**
 * @brief DVP Device Control Block Structure
 * @details Maintains all necessary state information for DVP peripheral instance
 * @var DVP_DEV
 *
 * This structure encapsulates the complete state machine and configuration
 * parameters required to manage a DVP hardware instance. All members are
 * accessed exclusively by the DVP driver core logic.
 */
typedef struct __DVP_DEV
{
    IMAGE_VIC_RegDef              *Instance;           /*!< Base address of DVP register block */
    DVP_InitTypeDef               Init;                /*!< Stored initialization parameters */
    DVP_SignalEvent_t             cb_event;            /*!< Registered event notification callback */
    volatile DVP_emState          State;               /*!< Current operational state of DVP controller */
}DVP_DEV;

/** @endcond */
#endif /* __DRIVER_DVP_INTERNAL_H */
