/**
 * @file gpt_timer.h
 * @brief GPT Timer Driver Interface Header File
 *        This file declares the interface for General Purpose Timer (GPT) timer functionality,
 *        including data types, function prototypes and integration points with the base GPT driver.
 *
 * @details
 *   Contains essential declarations for:
 *     - Timer configuration structures and constants
 *     - Channel control macros and bitfields
 *     - Function prototypes for timer operations
 *     - Integration with common driver framework
 *
 * @note
 *   Must be included after <gpt.h> in source files.
 *   All declarations are protected by _GPT_TIMER_H_ include guard.
 *
 * @author USER
 * @date Created on April 25, 2025
 */

#ifndef _GPT_TIMER_H_
#define _GPT_TIMER_H_

#include "Driver_GPT_TIMER.h" ///< PWM-specific driver interface definitions
#include "gpt.h"             ///< Base GPT peripheral definitions

#endif /* _GPT_TIMER_H_ */
