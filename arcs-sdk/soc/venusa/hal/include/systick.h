/**
 * @file systick.h
 * @brief SysTick Driver API Header File
 * @details This file contains the interface definitions for the System Timing (SysTick) driver.
 *          It provides functions for configuring and controlling the system tick timer functionality.
 * @version 1.10
 * @copyright
 * Copyright 2016-2019 Cypress Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_BSP_SYSTICK_H_
#define INCLUDE_BSP_SYSTICK_H_

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**

/**
 * @brief Maximum valid SysTick interrupt count value
 * @details Represents the maximum allowable value for SysTick interrupt counting
 */

#define SYSTICK_MAX_INT             0xFFFFFFFFUL

/**
 * @struct __systick_info
 * @brief SysTick Information Structure
 * @var interval - Time interval between SysTick interrupts (configurable)
 * @var systick_value - Current value of the SysTick counter register
 * @note Both members are declared as volatile since they directly map to hardware registers
 */
typedef struct __systick_info{
    volatile uint32_t interval;   ///< Interrupt interval configuration
    volatile uint32_t systick_value; ///< Current counter value
} _systick_info;

/**
 * @fn void SysTick_Open(uint64_t interval)
 * @brief Open and initialize the SysTick timer
 * @param[in] interval - Desired interval between interrupts in microseconds
 * @details Configures the SysTick timer with specified interval and enables it
 */
void SysTick_Open(uint64_t interval);

/**
 * @fn void SysTick_Close(void)
 * @brief Stop and disable the SysTick timer
 * @details Powers down the SysTick peripheral and stops counting
 */
void SysTick_Close(void);

/**
 * @fn uint32_t SysTick_Time(void)
 * @brief Get elapsed time since last SysTick initialization
 * @return Elapsed time in microseconds
 * @note Returns the total time recorded by the SysTick counter
 */
uint32_t SysTick_Time(void);

/**
 * @fn uint32_t SysTick_Value(void)
 * @brief Get current SysTick counter value
 * @return Current counter value
 * @details Reads the raw counter value without resetting it
 */
uint32_t SysTick_Value(void);

/**
 * @fn void SysTick_Delay_Ms(uint32_t nms)
 * @brief Block execution for specified milliseconds using SysTick
 * @param[in] nms - Number of milliseconds to delay
 * @details Uses busy waiting with SysTick counter for precise delay
 */
void SysTick_Delay_Ms(uint32_t nms);

/**
 * @fn void SysTick_Delay_Us(uint32_t nus)
 * @brief Block execution for specified microseconds using SysTick
 * @param[in] nus - Number of microseconds to delay
 * @details High-resolution delay function based on SysTick timing
 */
void SysTick_Delay_Us(uint32_t nus);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_BSP_SYSTICK_H_ */
