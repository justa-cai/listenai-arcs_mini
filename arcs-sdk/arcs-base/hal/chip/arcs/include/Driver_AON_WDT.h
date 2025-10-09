/*
 * Driver_AON_WDT.h
 *
 *  Created on: Mar 30, 2022
 *      Author: USER
 */

#ifndef INCLUDE_DRIVER_DRIVER_AON_WDT_H_
#define INCLUDE_DRIVER_DRIVER_AON_WDT_H_

#include "Driver_Common.h"

#define HAL_AON_WDT_TIME_Pos                    0
#define HAL_AON_WDT_TIME_Msk                   (0x1UL << HAL_AON_WDT_TIME_Pos)
#define HAL_AON_WDT_TIME_CFG                   (0x1UL << HAL_AON_WDT_TIME_Pos)

#define HAL_AON_WDT_INTERRUPT_Pos               1
#define HAL_AON_WDT_INTERRUPT_Msk              (0x1UL << HAL_AON_WDT_INTERRUPT_Pos)
#define HAL_AON_WDT_INTERRUPT_EN               (0x1UL << HAL_AON_WDT_INTERRUPT_Pos)

#define HAL_AON_WDT_MODE_CTRL_Pos               2
#define HAL_AON_WDT_MODE_CTRL_Msk              (0x3UL << HAL_AON_WDT_MODE_CTRL_Pos)
#define HAL_AON_WDT_CTRL_RESET_MODE            (0x1UL << HAL_AON_WDT_MODE_CTRL_Pos)
#define HAL_AON_WDT_CTRL_INT_MODE              (0x2UL << HAL_AON_WDT_MODE_CTRL_Pos)

#define HAL_AON_WDT_RST_DOMAIN_Pos              4
#define HAL_AON_WDT_RST_DOMAIN_Msk             (0x3UL << HAL_AON_WDT_RST_DOMAIN_Pos)
#define HAL_AON_WDT_RST_CORE_DOMAIN            (0x1UL << HAL_AON_WDT_RST_DOMAIN_Pos)
#define HAL_AON_WDT_RST_PMU_DOMAIN             (0x2UL << HAL_AON_WDT_RST_DOMAIN_Pos)

typedef void (*HAL_AON_WDT_SignalEvent_t)(void* workspace);

int32_t AON_WDT_Initialize(void* res, HAL_AON_WDT_SignalEvent_t callback, void* workspace);

int32_t AON_WDT_Uninitialize(void* res);

int32_t AON_WDT_PowerControl(void* res, CSK_POWER_STATE state);

int32_t AON_WDT_Control(void* res, uint32_t control, uint32_t arg);

int32_t AON_WDT_Enable(void* res);

int32_t AON_WDT_Refresh(void* res);

int32_t AON_WDT_Disable(void* res);

void* AON_WDT(void);

#endif /* INCLUDE_DRIVER_DRIVER_AON_WDT_H_ */
