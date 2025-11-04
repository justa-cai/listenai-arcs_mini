/*
 * Driver_AON_TIMER.h
 *
 *  Created on: Mar 29, 2022
 *      Author: USER
 */

#ifndef INCLUDE_DRIVER_DRIVER_AON_TIMER_H_
#define INCLUDE_DRIVER_DRIVER_AON_TIMER_H_

#include "arcs_ap.h"

#include "Driver_Common.h"

#define HAL_AON_TIMER_MODE_Pos                  (0)
#define HAL_AON_TIMER_MODE_Msk                  (3UL << HAL_AON_TIMER_MODE_Pos)
#define HAL_AON_TIMER_MODE_Wrapping             (0UL << HAL_AON_TIMER_MODE_Pos)
#define HAL_AON_TIMER_MODE_Repeat               (1UL << HAL_AON_TIMER_MODE_Pos)
#define HAL_AON_TIMER_MODE_Normal               (2UL << HAL_AON_TIMER_MODE_Pos)

#define HAL_AON_TIMER_INTERRUPT_Pos             (2)
#define HAL_AON_TIMER_INTERRUPT_Msk             (1UL << HAL_AON_TIMER_INTERRUPT_Pos)
#define HAL_AON_TIMER_INTERRUPT_Enabled         (0UL << HAL_AON_TIMER_INTERRUPT_Pos)
#define HAL_AON_TIMER_INTERRUPT_Disabled        (1UL << HAL_AON_TIMER_INTERRUPT_Pos)

#define HAL_AON_TIMER_CLK_SEL_Pos               (3)
#define HAL_AON_TIMER_CLK_SEL_Msk               (1UL << HAL_AON_TIMER_CLK_SEL_Pos)
#define HAL_AON_TIMER_CLK_SEL_Rc32k             (1UL << HAL_AON_TIMER_CLK_SEL_Pos)
#define HAL_AON_TIMER_CLK_SEL_Xo32k             (0UL << HAL_AON_TIMER_CLK_SEL_Pos)

typedef void (*HAL_AON_TIMER_SignalEvent_t) (uint32_t event, void* workspace);

#define HAL_AON_TIMER_EVENT_COMPLETE            (1UL << 0)

int32_t AON_TIMER_Initialize(void* res, HAL_AON_TIMER_SignalEvent_t cb_event, void* workspace);

int32_t AON_TIMER_Uninitialize(void* res);

int32_t AON_TIMER_PowerControl(void* res, CSK_POWER_STATE state);

int32_t AON_TIMER_Control(void* res, uint32_t control);

int32_t AON_TIMER_SetTimerPeriodByCount(void* res, uint32_t count);

int32_t AON_TIMER_StartTimer(void* res);

int32_t AON_TIMER_ReadTimerCount(void* res, uint32_t *count);

int32_t AON_TIMER_StopTimer(void* res);

void* AON_TIMER(void);

#endif /* INCLUDE_DRIVER_DRIVER_AON_TIMER_H_ */
