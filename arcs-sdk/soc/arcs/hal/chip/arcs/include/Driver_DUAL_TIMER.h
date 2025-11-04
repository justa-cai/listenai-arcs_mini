/*
 * Driver_DUAL_TIMER.h
 *
 *
 *      Author: USER
 */

 #ifndef __DRIVER_DUALTIMERS_H__
 #define __DRIVER_DUALTIMERS_H__
 
 #include "Driver_Common.h"
 
 #define CSK_TIMER_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */
 
 /****** TIMER Channel Parameter *****/
 #define CSK_TIMER_CHANNEL_0                 (0)
 #define CSK_TIMER_CHANNEL_1                 (1)
 
 /****** TIMER Control Codes *****/
 
 #define CSK_TIMER_PRESCALE_Pos              (0)
 #define CSK_TIMER_PRESCALE_Msk              (3UL << CSK_TIMER_PRESCALE_Pos)
 #define CSK_TIMER_PRESCALE_Divide_1         (0UL << CSK_TIMER_PRESCALE_Pos)
 #define CSK_TIMER_PRESCALE_Divide_16        (1UL << CSK_TIMER_PRESCALE_Pos)
 #define CSK_TIMER_PRESCALE_Divide_256       (2UL << CSK_TIMER_PRESCALE_Pos)
 
 #define CSK_TIMER_SIZE_Pos                  (2)
 #define CSK_TIMER_SIZE_Msk                  (1UL << CSK_TIMER_SIZE_Pos)
 #define CSK_TIMER_SIZE_16Bit                (0UL << CSK_TIMER_SIZE_Pos)
 #define CSK_TIMER_SIZE_32Bit                (1UL << CSK_TIMER_SIZE_Pos)
 
 #define CSK_TIMER_MODE_Pos                  (3)
 #define CSK_TIMER_MODE_Msk                  (3UL << CSK_TIMER_MODE_Pos)
 #define CSK_TIMER_MODE_FreeRunning          (0UL << CSK_TIMER_MODE_Pos)
 #define CSK_TIMER_MODE_Periodic             (1UL << CSK_TIMER_MODE_Pos)
 #define CSK_TIMER_MODE_OneShot              (2UL << CSK_TIMER_MODE_Pos)
 
 #define CSK_TIMER_INTERRUPT_Pos             (5)
 #define CSK_TIMER_INTERRUPT_Msk             (1UL << CSK_TIMER_INTERRUPT_Pos)
 #define CSK_TIMER_INTERRUPT_Enabled         (0UL << CSK_TIMER_INTERRUPT_Pos)
 #define CSK_TIMER_INTERRUPT_Disabled        (1UL << CSK_TIMER_INTERRUPT_Pos)
 
 
 typedef void (*CSK_TIMER_SignalEvent_t) (uint32_t event, void* workspace);
 
 #define CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH0    (1UL << 0)
 #define CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH1    (1UL << 1)
 
 CSK_DRIVER_VERSION CSK_DUALTIMERS_GetVersion(void);
 
 int32_t DUALTIMERS_Initialize(void* res);
 
 int32_t DUALTIMERS_Uninitialize(void* res);
 
 int32_t DUALTIMERS_PowerControl(void* res, CSK_POWER_STATE state);
 
 int32_t DUALTIMERS_Control(void* res, uint32_t control, uint32_t ch);
 
 int32_t DUALTIMERS_SetTimerCallback(void* res, uint32_t channel, CSK_TIMER_SignalEvent_t cb_event, void* workspace);
 
 int32_t DUALTIMERS_SetTimerPeriodByCount(void* res, uint32_t channel, uint32_t count);
 
 int32_t DUALTIMERS_StartTimer(void* res, uint32_t channel);
 
 int32_t DUALTIMERS_ReadTimerCount(void* res, uint32_t channel, uint32_t *count);
 
 int32_t DUALTIMERS_StopTimer(void* res, uint32_t channel);
 
 void* DUALTIMERS0(void);
 
 void* DUALTIMERS1(void);

 #endif /*  */
 
