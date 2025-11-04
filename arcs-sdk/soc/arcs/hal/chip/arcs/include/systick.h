/*
 * systick.h
 *
 *  Created on: 2020骞�12鏈�18鏃�
 *      Author: USER
 */

#ifndef INCLUDE_BSP_SYSTICK_H_
#define INCLUDE_BSP_SYSTICK_H_

/***************************************************************************//**
* \file cy_systick.h
* \version 1.10
*
* Provides the API declarations of the SysTick driver.
*
********************************************************************************
* \copyright
* Copyright 2016-2019 Cypress Semiconductor Corporation
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTICK_MAX_INT             0xFFFFFFFFUL

typedef struct __systick_info{
    volatile uint32_t interval;
    volatile uint32_t systick_value;
}_systick_info;

void SysTick_Open(uint64_t interval);

void SysTick_Close(void);

uint32_t SysTick_Time(void);

uint32_t SysTick_Value(void);

void SysTick_Delay_Ms(uint32_t nms);

void SysTick_Delay_Us(uint32_t nus);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_BSP_SYSTICK_H_ */
