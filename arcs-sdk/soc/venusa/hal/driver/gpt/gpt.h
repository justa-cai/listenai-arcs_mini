/**
 * @file gpt.h
 * @brief General Purpose Timer (GPT) Driver Header File
 *        This file contains the core data structures and interface definitions for GPT peripheral driver.
 *        Provides resource management, configuration parameters and validation macros.
 *
 * @details
 *   Contains essential elements for GPT operation:
 *     - Resource structure combining hardware registers and software state
 *     - Global instance declarations for timer and PWM information blocks
 *     - Parameter validation macro for resource checking
 *     - Channel count and bitfield definitions for timer modes
 *
 * @note
 *   Must be included after venusa_ap.h and related driver header files.
 *   All external symbols are protected by CHIP_VENUSA_DRIVER_GPT_GPT_H_ include guard.
 *
 * @author USER
 * @date Created on April 24, 2025
 */

#ifndef _GPT_GPT_H_
#define _GPT_GPT_H_

#include "venusa_ap.h"          ///< Base chip definitions
#include "Driver_GPT_Common.h" ///< Common GPT driver interfaces
#include "Driver_Common.h"    ///< Standard driver infrastructure
#include "ClockManager.h"     ///< Clock management utilities

#include "Driver_GPT_TIMER.h" ///< Timer-specific extensions
#include "Driver_GPT_PWM.h"   ///< PWM-specific extensions

/**
 * @brief Number of available GPT channels in this implementation
 * @details Fixed configuration based on hardware capabilities
 */
#define GPT_NUMBER_OF_CHANNELS        2

/**
 * @brief Bit difference threshold for timer resolution detection
 * @details Used to identify timer bit depth changes during mode transitions
 */
#define HAL_GPT_TIMER_BITS_DIFF       0x1

/**
 * @brief Bitmask for 8-bit timer mode detection
 * @details Matches against status register bits to detect 8-bit operation
 */
#define HAL_GPT_TIMER_8BITS_MASK      0x6

/**
 * @brief Position shift value for 8-bit timer status bits
 * @details Aligns with specific bits in the status register
 */
#define HAL_GPT_TIMER_8BITS_POS       1

/**
 * @brief GPT resource container structure
 * @details Aggregates all necessary components to manage a GPT instance:
 *          - Hardware register base address
 *          - Interrupt vector number
 *          - Interrupt service routine pointer
 *          - Channel state information
 *          - Specialized submodules (PWM/Timer) data structures
 *
 * @var GPT_Resources_t::reg
 *   Pointer to GPT register block (typecast from GPT_RegDef*)
 * @var GPT_Resources_t::irq_num
 *   Associated interrupt request number
 * @var GPT_Resources_t::irq_handler
 *   Function pointer to ISR handler
 * @var GPT_Resources_t::info
 *   Pointer to channel state information array
 * @var GPT_Resources_t::pwm_info
 *   Pointer to PWM-specific configuration data
 * @var GPT_Resources_t::timer_info
 *   Pointer to timer-specific configuration data
 */
typedef struct {
    GPT_RegDef *reg;
    uint32_t irq_num;
    void (*irq_handler)(void);
    GPT_Info_t *info;
    GPT_PWM_Info_t* pwm_info;
    GPT_TIMER_Info_t* timer_info;
} GPT_Resources_t;

/**
 * @brief Global GPT0 resource instance
 * @details Singleton instance for primary GPT peripheral
 */
extern GPT_Resources_t  gpt0_resources;

/**
 * @brief Default timer information block
 * @details Preallocated storage for timer channel states
 */
extern GPT_TIMER_Info_t __timer_info;

/**
 * @brief Default PWM information block
 * @details Preallocated storage for PWM channel states
 */
extern GPT_PWM_Info_t   __pwm_info;

/**
 * @brief Resource validation macro
 * @param[in] res Pointer to GPT resource structure to validate
 * @details Verifies that provided resource pointer matches the global gpt0_resources instance.
 *          Returns CSK_DRIVER_ERROR_PARAMETER if validation fails.
 * @note Designed as a run-time check before performing operations on the resource.
 */
#define CHECK_RESOURCES(res) do{ \
    if (res != &gpt0_resources) { \
        return  CSK_DRIVER_ERROR_PARAMETER; \
    } \
} while(0)

#endif /* _GPT_GPT_H_ */
