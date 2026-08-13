/** @file rgb.h
 *  @brief This file contains internal definitions and structures for the RGB driver module.
 *         It includes clock configuration constants, device handle structure, and private data types.
 *         &#169; 2025 ListenAI. All rights reserved.
 */

#ifndef __DRIVER_RGB_INTERNAL_H
#define __DRIVER_RGB_INTERNAL_H

#include "venusa_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "Driver_RGB.h"


/**
 *  These macros define available clock sources and divisor limits for RGB interface operation.
 */
#define RGB_CLK_OUT_SEL_100MHz  BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK ///< Base clock frequency selection: 100MHz or 200MHz
#define RGB_CLK_OUT_SEL_24MHz   24000000  ///< Alternative clock source: 24MHz
#define RGB_CLK_OUT_DIV_MAX     15       ///< Maximum allowed clock divider value
#define RGB_CLK_OUT_DIV_MIN     1        ///< Minimum allowed clock divider value
#define RGB_CLK_OUT_HZ_MAX      (RGB_CLK_OUT_SEL_100MHz / RGB_CLK_OUT_DIV_MIN) ///< Highest achievable output frequency
#define RGB_CLK_OUT_HZ_MIN      (RGB_CLK_OUT_SEL_24MHz / RGB_CLK_OUT_DIV_MAX)    ///< Lowest achievable output frequency

/**
  * @brief RGB State enumeration
  * @details Defines the possible states of the RGB controller
  */
typedef enum
{
    RGB_STATE_RESET             = 0x00U,  /*!< RGB not yet initialized or disabled  */
    RGB_STATE_READY             = 0x01U,  /*!< RGB initialized and ready for use    */
    RGB_STATE_BUSY              = 0x02U,  /*!< RGB internal processing is ongoing   */
    RGB_STATE_TIMEOUT           = 0x03U,  /*!< RGB timeout state                    */
    RGB_STATE_ERROR             = 0x04U,  /*!< RGB error state                      */
    RGB_STATE_SUSPENDED         = 0x05U,  /*!< RGB suspend state                    */
    RGB_STATE_BUTT                          /*!< Boundary value */
}RGB_emState;

/**
 * @brief RGB Device Handle Structure
 *
 * This structure maintains all necessary state information for managing an RGB peripheral instance.
 * It combines hardware register access, initialization parameters, callback mechanisms,
 * and operational state tracking.
 */
typedef struct __RGB_DEV
{
    RGB_INTERFACE_RegDef          *Instance;           /*!< @brief [RW] Base address of RGB register block */
                                                     ///< Points to memory-mapped peripheral registers

    RGB_InitTypeDef               Init;                /*!< @brief [RW] Initialization parameters */
                                                     ///< Contains timing settings and mode configurations

    RGB_SignalEvent_t             cb_event;            /*!< @brief [WE] Event callback function pointer */
                                                     ///< Called on signal events (NULL if disabled)

    volatile RGB_emState          State;               /*!< @brief [RO] Current operational state */
                                                     ///< Tracks idle/active/error states atomically
} RGB_DEV;

#endif /* __DRIVER_RGB_INTERNAL_H */
