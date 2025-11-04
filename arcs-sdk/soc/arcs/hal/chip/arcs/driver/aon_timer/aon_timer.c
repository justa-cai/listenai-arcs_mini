/*
 * aon_timer.c
 *
 *  Created on: Mar 29, 2022
 *      Author: USER
 */

#include "Driver_AON_TIMER.h"
#include "PowerManager.h"
#include "arcs_ap.h"

#define AON_TIMER_LOAD_VALUE_MASK       (0xFFFFFF)

#define AON_TIMER_FLAG_INITIALIZED      (1UL << 0)
#define AON_TIMER_FLAG_POWERED          (1UL << 1)

#define AON_TIMER_CLEAR_IRQ()	\
do{	\
	IP_AON_TIMER->REG_OS_TIMER_IRQ_CLR.all = 0x1;	\
	while(IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS);	\
}while(0)

typedef struct _AON_TIMER_INFO {
    HAL_AON_TIMER_SignalEvent_t cb_event;
    void* workspace;
    uint32_t reload_cnt;
    uint32_t mode;
} AON_TIMER_INFO;

typedef struct _AON_TIMER_RESOURCES {
	AON_TIMER_RegDef* reg;
    uint32_t irq_num;
    void (*irq_handler)(void);
    AON_TIMER_INFO* info;
    uint32_t flags;
} AON_TIMER_RESOURCES;

static void AON_TIMER_Handler(void);

/// AON TIMER
static AON_TIMER_INFO aon_timer_info = {0};
static AON_TIMER_RESOURCES aon_timer_resources = {
        IP_AON_TIMER,
		IRQ_AON_TIMER_VECTOR,
		AON_TIMER_Handler,
        &aon_timer_info,
        0,
};

#define AON_TIMER_CHECK_RESOURCES(res)  do{\
        if(res != &aon_timer_resources){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)

void* AON_TIMER(){
    return (void*)&aon_timer_resources;
}

/// AON TIMER FUNCTION CONFIGURE
int32_t AON_TIMER_Initialize(void* res, HAL_AON_TIMER_SignalEvent_t cb_event, void* workspace){
    AON_TIMER_CHECK_RESOURCES(res);
    AON_TIMER_RESOURCES *aon_timer = (AON_TIMER_RESOURCES*) res;

    if (aon_timer->flags & AON_TIMER_FLAG_INITIALIZED){
        return CSK_DRIVER_OK;
    }

    aon_timer->info->cb_event = cb_event;
    aon_timer->info->workspace = workspace;

    aon_timer->info->mode = HAL_AON_TIMER_MODE_Normal;
    aon_timer->info->reload_cnt = 0;

    aon_timer->flags = AON_TIMER_FLAG_INITIALIZED;

    return CSK_DRIVER_OK;
}

int32_t AON_TIMER_Uninitialize(void* res){
    AON_TIMER_CHECK_RESOURCES(res);
    AON_TIMER_RESOURCES *aon_timer = (AON_TIMER_RESOURCES*) res;

    aon_timer->info->cb_event = NULL;
    aon_timer->info->mode = HAL_AON_TIMER_MODE_Normal;
    aon_timer->info->reload_cnt = 0;

    aon_timer->flags = 0;

    return CSK_DRIVER_OK;
}

int32_t AON_TIMER_PowerControl(void* res, CSK_POWER_STATE state){
    AON_TIMER_CHECK_RESOURCES(res);
    AON_TIMER_RESOURCES *aon_timer = (AON_TIMER_RESOURCES*) res;

    switch (state){
    case CSK_POWER_OFF:
        if ((aon_timer->flags & AON_TIMER_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

		// Disable UART IRQ
		disable_IRQ(aon_timer->irq_num);

		aon_timer->flags &= ~AON_TIMER_FLAG_POWERED;

		// Uninstall IRQ Handler
		register_ISR(aon_timer->irq_num, NULL, NULL);

        break;

    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:
        if(!(aon_timer->flags & AON_TIMER_FLAG_INITIALIZED)){
            return CSK_DRIVER_ERROR;
        }

        if(aon_timer->flags & AON_TIMER_FLAG_POWERED){
            return CSK_DRIVER_OK;
        }

        __HAL_PMU_AON_TIMER_ENABLE();

		register_ISR(aon_timer->irq_num, aon_timer->irq_handler, NULL);
		enable_IRQ(aon_timer->irq_num);

        aon_timer->flags = AON_TIMER_FLAG_POWERED;

        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

int32_t AON_TIMER_Control(void* res, uint32_t control){
    AON_TIMER_CHECK_RESOURCES(res);
    AON_TIMER_RESOURCES *aon_timer = (AON_TIMER_RESOURCES*) res;

    if(!(aon_timer->flags & AON_TIMER_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    aon_timer->info->mode = control & HAL_AON_TIMER_MODE_Msk;
    switch (control & HAL_AON_TIMER_MODE_Msk){
    case HAL_AON_TIMER_MODE_Wrapping:
        aon_timer->reg->REG_OSTIMER_CTRL.bit.WRAP_MODE = 0x1;
        aon_timer->reg->REG_OSTIMER_CTRL.bit.REPEAT_MODE = 0x0;
        break;
    case HAL_AON_TIMER_MODE_Repeat:
        aon_timer->reg->REG_OSTIMER_CTRL.bit.WRAP_MODE = 0x0;
        aon_timer->reg->REG_OSTIMER_CTRL.bit.REPEAT_MODE = 0x1;
        break;
    case HAL_AON_TIMER_MODE_Normal:
        aon_timer->reg->REG_OSTIMER_CTRL.bit.WRAP_MODE = 0x0;
        aon_timer->reg->REG_OSTIMER_CTRL.bit.REPEAT_MODE = 0x0;
        break;
    }

    switch (control & HAL_AON_TIMER_INTERRUPT_Msk){
    case HAL_AON_TIMER_INTERRUPT_Enabled:
        aon_timer->reg->REG_OS_TIMER_IRQ_MASK.all = 0x1;
        break;
    case HAL_AON_TIMER_INTERRUPT_Disabled:
        aon_timer->reg->REG_OS_TIMER_IRQ_MASK.all = 0x0;
        break;
    }

    return CSK_DRIVER_OK;
}

int32_t AON_TIMER_SetTimerPeriodByCount(void* res, uint32_t count){
    AON_TIMER_CHECK_RESOURCES(res);
    AON_TIMER_RESOURCES *aon_timer = (AON_TIMER_RESOURCES*) res;

    if(!(aon_timer->flags & AON_TIMER_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    aon_timer->reg->REG_OSTIMER_CTRL.bit.LOADVAL = count & AON_TIMER_LOAD_VALUE_MASK;
    aon_timer->reg->REG_OSTIMER_CTRL.bit.LOADER = 0x1;

    return CSK_DRIVER_OK;
}

int32_t AON_TIMER_StartTimer(void* res){
    AON_TIMER_CHECK_RESOURCES(res);
    AON_TIMER_RESOURCES *aon_timer = (AON_TIMER_RESOURCES*) res;

    if(!(aon_timer->flags & AON_TIMER_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    aon_timer->reg->REG_OSTIMER_CTRL.bit.ENABLE = 0x1;
    while(!aon_timer->reg->REG_OSTIMER_CTRL.bit.ENABLED);

    return CSK_DRIVER_OK;
}

int32_t AON_TIMER_ReadTimerCount(void* res, uint32_t *count){
    AON_TIMER_CHECK_RESOURCES(res);
    AON_TIMER_RESOURCES *aon_timer = (AON_TIMER_RESOURCES*) res;

    *count = aon_timer->reg->REG_OSTIMER_CURVAL.all & AON_TIMER_LOAD_VALUE_MASK;

    return CSK_DRIVER_OK;
}

int32_t AON_TIMER_StopTimer(void* res){
    AON_TIMER_CHECK_RESOURCES(res);
    AON_TIMER_RESOURCES *aon_timer = (AON_TIMER_RESOURCES*) res;

    if(!(aon_timer->flags & AON_TIMER_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    aon_timer->reg->REG_OSTIMER_CTRL.bit.ENABLE = 0x0;
    while(aon_timer->reg->REG_OSTIMER_CTRL.bit.ENABLED);

    return CSK_DRIVER_OK;
}

static void AON_TIMER_Handler(){
	aon_timer_resources.reg->REG_OS_TIMER_IRQ_CLR.all = 0x1;

    if (aon_timer_resources.info->cb_event){
    	aon_timer_resources.info->cb_event(HAL_AON_TIMER_EVENT_COMPLETE, aon_timer_resources.info->workspace);
    }

    while(aon_timer_resources.reg->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS);
}
