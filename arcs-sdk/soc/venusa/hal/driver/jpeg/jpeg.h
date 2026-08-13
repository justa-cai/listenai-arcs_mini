/**
 * @file jpeg.h
 * @author USER
 * @date   Generated on 2025.06.01
 * @brief  Private header file containing internal data structures and definitions for JPEG driver implementation.
 *         Contains the core device control structure (@ref Jpeg_DEV) used by the driver stack.
 *         This file should not be directly included by application code.
 * @note   Do not modify this file unless you are developing new driver features.
 */

#ifndef __DRIVER_JPEG_INTERNAL_H__
#define __DRIVER_JPEG_INTERNAL_H__

#include "venusa_ap.h"
#include "ClockManager.h"
#include "assert.h"

#include "jpeg_reg.h"
#include "Driver_JPEG.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief  JPEG Device Control Block (DCB) structure definition
 *         Central container managing hardware resources and operational state for a single JPEG peripheral instance.
 *         Aggregates register mapping, initialization parameters, and event callback mechanism.
 *
 * @details The DCB maintains all necessary context information between driver operations. It provides:
 *          - Direct access to physical register block through #JPEG_RegDef pointer
 *          - Persisted initialization configuration (@ref Jpeg_InitTypeDef)
 *          - Event notification interface via function pointer
 *
 * @warning Must be properly initialized before use. Use @ref Driver_JPEG_CreateHandle() API.
 */
typedef struct __Jpeg_DEV
{
    /**
     * @brief Base address of JPEG peripheral register block
     * @details Mapped memory region containing all configurable registers for this specific hardware instance.
     *         Derived from #JPEG_RegDef type defined in jpeg_reg.h.
     */
    JPEG_RegDef            *Instance;           /*!< jpeg Register base address  */

    /**
     * @brief Complete set of driver initialization parameters
     * @details Stores configured operating mode, resolution, quality settings, etc. Retained across operations.
     *         Populated during handle creation and updated by configuration functions.
     */
    Jpeg_InitTypeDef       Init;                /*!< jpeg parameters             */

    /**
     * @brief Event callback function pointer
     * @details Called when significant events occur (completion/error). Allows async operation flow.
     *         Client must implement this callback per @ref Jpeg_SignalEvent_t signature.
     * @sa Driver_JPEG_RegisterCallback()
     */
    Jpeg_SignalEvent_t     cb_event;            /*!< jpegDVP callback               */
} Jpeg_DEV;

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_JPEG_INTERNAL_H__ */
