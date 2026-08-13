/**
 * @file gpt_pwm.h
 * @brief GPT PWM Driver Header File
 *        This file contains the interface declarations for the General Purpose Timer (GPT)
 *        Pulse Width Modulation (PWM) driver implementation. It defines data types,
 *        function prototypes and integration points with the base GPT driver.
 *
 * @details
 *   Provides type definitions and API declarations specific to PWM functionality:
 *     - Specialized PWM configuration structures
 *     - Channel control macros and bitfields
 *     - Function prototypes for PWM operations
 *     - Integration with common driver framework
 *
 * @note
 *   This header must be included after <gpt.h> and before any PWM operation code.
 *   All PWM-specific definitions are contained within the CHIP_VENUSA_DRIVER_GPT_GPT_PWM_H_ guard.
 *
 * @author USER
 * @date Created on May 12, 2025
 */

#ifndef _GPT_PWM_H_
#define _GPT_PWM_H_

#include "gpt.h"         ///< Base GPT peripheral definitions
#include "Driver_Common.h" ///< Common driver infrastructure
#include "Driver_GPT_PWM.h" ///< PWM-specific driver interface

#endif /* _GPT_PWM_H_ */
