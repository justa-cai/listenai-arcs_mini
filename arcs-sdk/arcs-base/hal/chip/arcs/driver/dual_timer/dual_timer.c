/**
  ******************************************************************************
  * @file    dual_timer.c
  * @author  ListenAI Application Team
  * @brief   DUALTIMERS HAL module driver.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 ListenAI.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ListenAI under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
#include "dual_timer_reg.h"
#include "Driver_DUAL_TIMER.h"
#include "arcs_ap.h"


/* Private typedef -----------------------------------------------------------*/
#define TIMER_FLAG_INITIALIZED      (1UL << 0)
#define TIMER_FLAG_POWERED          (1UL << 1)
#define TIMER_FLAG_CH_ACTIVED(CH)   (1UL << (2 + CH))

#define CSK_TIMER_MAX_CHANNEL_NUM   (2)
#define CSK_TIMER_CLR_INT           (0x951202)


typedef struct _TIMER_INFO {
    CSK_TIMER_SignalEvent_t cb_event;
    void* workspace;
    uint32_t reload;
    uint32_t cnt;
} DUALTIMERS_INFO;

typedef struct _TIMER_RESOURCES {
	DUAL_TIMER_RegDef* reg;
    uint32_t irq_num;
    void (*irq_handler)(void);
    DUALTIMERS_INFO info[CSK_TIMER_MAX_CHANNEL_NUM];
    uint32_t flags;
} DUALTIMERS_RESOURCES;



/* Private variables ---------------------------------------------------------*/
#define CSK_TIMER_DRV_VERSION    CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)  /* driver version */

 /* Driver Version */
static const CSK_DRIVER_VERSION DriverVersion = {
    CSK_TIMER_API_VERSION,
    CSK_TIMER_DRV_VERSION
};

static void DUALTIMERS0_IRQ_Handler(void);
static void DUALTIMERS1_IRQ_Handler(void);

static DUALTIMERS_RESOURCES dualtimers0_resources = {
		IP_TIMER0,
		IRQ_TIMER0_VECTOR,
		DUALTIMERS0_IRQ_Handler,
        {
                {0},
                {0},
        },
        0,
};

static DUALTIMERS_RESOURCES dualtimers1_resources = {
		IP_TIMER1,
        IRQ_TIMER1_VECTOR,
        DUALTIMERS1_IRQ_Handler,
        {
                {0},
                {0},
        },
        0,
};

#define CHECK_RESOURCES(res)  do{\
        if((res != &dualtimers0_resources) && (res != &dualtimers1_resources)){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)



/* Private functions ---------------------------------------------------------*/

void* DUALTIMERS0(void){
    return (void*)&dualtimers0_resources;
}

void* DUALTIMERS1(void){
    return (void*)&dualtimers1_resources;
}

CSK_DRIVER_VERSION CSK_TIMER_GetVersion(void){
    return DriverVersion;
}

int32_t DUALTIMERS_Initialize(void* res){

    CHECK_RESOURCES(res);
    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if(timer->flags & TIMER_FLAG_INITIALIZED){
        return CSK_DRIVER_OK;
    }

    {
        uint32_t i = 0;
        for(i = 0; i < CSK_TIMER_MAX_CHANNEL_NUM; i++){
            timer->info[i].cb_event = NULL;
            timer->info[i].cnt = 0;
            timer->info[i].reload = 0;
            timer->info[i].workspace = NULL;
        }
    }

    timer->flags = TIMER_FLAG_INITIALIZED;

    return CSK_DRIVER_OK;
}

int32_t DUALTIMERS_Uninitialize(void* res){
    CHECK_RESOURCES(res);
    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    {
        uint32_t i = 0;
        for(i = 0; i < CSK_TIMER_MAX_CHANNEL_NUM; i++){
            timer->info[i].cb_event = NULL;
            timer->info[i].cnt = 0;
            timer->info[i].reload = 0;
            timer->info[i].workspace = NULL;
        }
    }

    timer->flags = 0;

    return CSK_DRIVER_OK;
}

int32_t DUALTIMERS_PowerControl(void* res, CSK_POWER_STATE state){
    CHECK_RESOURCES(res);
    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    switch (state)
    {
    case CSK_POWER_OFF:
        if ((timer->flags & TIMER_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

        // Disable UART IRQ
        disable_IRQ(timer->irq_num);

        timer->flags &= ~TIMER_FLAG_POWERED;

        // Uninstall IRQ Handler
        register_ISR(timer->irq_num, NULL, NULL);

        break;

    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:
        if ((timer->flags & TIMER_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }
        if ((timer->flags & TIMER_FLAG_POWERED) != 0U) {
            return CSK_DRIVER_OK;
        }

        // TODO
        // Power and clock manager
        // Reset current peripheral

        // Clear interrupt pending status
        timer->reg->REG_INTCLR0.all = CSK_TIMER_CLR_INT;
        timer->reg->REG_INTCLR1.all = CSK_TIMER_CLR_INT;

        timer->flags = TIMER_FLAG_INITIALIZED | TIMER_FLAG_POWERED;

        // Configure interrupt handle
        register_ISR(timer->irq_num, timer->irq_handler, NULL);
        enable_IRQ(timer->irq_num);

        break;
    }
    return CSK_DRIVER_OK;
}

int32_t DUALTIMERS_Control(void* res, uint32_t control, uint32_t ch){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    switch (control & CSK_TIMER_PRESCALE_Msk){
    case CSK_TIMER_PRESCALE_Divide_1:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.TIMERPRE = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.TIMERPRE = 0x0;
        }
        break;
    case CSK_TIMER_PRESCALE_Divide_16:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.TIMERPRE = 0x1;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.TIMERPRE = 0x1;
        }
        break;
    case CSK_TIMER_PRESCALE_Divide_256:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.TIMERPRE = 0x2;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.TIMERPRE = 0x2;
        }
        break;
    }

    switch (control & CSK_TIMER_SIZE_Msk){
    case CSK_TIMER_SIZE_16Bit:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.SIZE = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.SIZE = 0x0;
        }
        break;
    case CSK_TIMER_SIZE_32Bit:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.SIZE = 0x1;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.SIZE = 0x1;
        }
        break;
    }

    switch (control & CSK_TIMER_MODE_Msk){
    case CSK_TIMER_MODE_FreeRunning:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.MODE = 0x0;
            timer->reg->REG_CONTROL0.bit.ONESHOT = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.MODE = 0x0;
            timer->reg->REG_CONTROL1.bit.ONESHOT = 0x0;
        }
        break;
    case CSK_TIMER_MODE_Periodic:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.MODE = 0x1;
            timer->reg->REG_CONTROL0.bit.ONESHOT = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.MODE = 0x1;
            timer->reg->REG_CONTROL1.bit.ONESHOT = 0x0;
        }
        break;
    case CSK_TIMER_MODE_OneShot:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.ONESHOT = 0x1;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.ONESHOT = 0x1;
        }
        break;
    }

    switch (control & CSK_TIMER_INTERRUPT_Msk){
    case CSK_TIMER_INTERRUPT_Enabled:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.INT_ENABLE = 0x1;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.INT_ENABLE = 0x1;
        }
        break;
    case CSK_TIMER_INTERRUPT_Disabled:
        if(ch == CSK_TIMER_CHANNEL_0){
            timer->reg->REG_CONTROL0.bit.INT_ENABLE = 0x0;
        }else if(ch == CSK_TIMER_CHANNEL_1){
            timer->reg->REG_CONTROL1.bit.INT_ENABLE = 0x0;
        }
        break;
    }

    return CSK_DRIVER_OK;
}

int32_t DUALTIMERS_SetTimerCallback(void* res, uint32_t channel, CSK_TIMER_SignalEvent_t cb_event, void* workspace){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    timer->info[channel].cb_event = cb_event;
    timer->info[channel].workspace = workspace;

    return CSK_DRIVER_OK;
}

int32_t DUALTIMERS_SetTimerPeriodByCount(void* res, uint32_t channel, uint32_t count){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    timer->info[channel].reload = count;
    // config load and bgload register
    if(channel == CSK_TIMER_CHANNEL_0){
        timer->reg->REG_LOAD0.all = count;    // this set current load register = count
        timer->reg->REG_BGLOAD0.all = count;  // this set load register = count when load register turn to 0
    }else if(channel == CSK_TIMER_CHANNEL_1){
        timer->reg->REG_LOAD1.all = count;    // this set current load register = count
        timer->reg->REG_BGLOAD1.all = count;  // this set load register = count when load register turn to 0
    }

    return CSK_DRIVER_OK;
}

int32_t DUALTIMERS_StartTimer(void* res, uint32_t channel){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    if ((timer->flags & TIMER_FLAG_CH_ACTIVED(channel))) {
        return CSK_DRIVER_OK;
    }

    timer->flags = timer->flags | TIMER_FLAG_CH_ACTIVED(channel);

    // config enable register
    if(channel == CSK_TIMER_CHANNEL_0){
        timer->reg->REG_CONTROL0.bit.ENABLE = 0x1;
    }else if(channel == CSK_TIMER_CHANNEL_1){
        timer->reg->REG_CONTROL1.bit.ENABLE = 0x1;
    }

    return CSK_DRIVER_OK;
}

int32_t DUALTIMERS_ReadTimerCount(void* res, uint32_t channel, uint32_t *count){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    if ((timer->flags & TIMER_FLAG_CH_ACTIVED(channel)) == 0) {
        *count = 0;
        return CSK_DRIVER_OK;
    }

    // get value from reg
    if(channel == CSK_TIMER_CHANNEL_0){
        *count = timer->reg->REG_VALUE0.all;
    }else if(channel == CSK_TIMER_CHANNEL_1){
        *count = timer->reg->REG_VALUE1.all;
    }

    return CSK_DRIVER_OK;
}

int32_t DUALTIMERS_StopTimer(void* res, uint32_t channel){
    CHECK_RESOURCES(res);

    DUALTIMERS_RESOURCES* timer = (DUALTIMERS_RESOURCES*)res;

    if ((timer->flags & TIMER_FLAG_POWERED) == 0U) {
        return CSK_DRIVER_ERROR;
    }

    if ((timer->flags & TIMER_FLAG_CH_ACTIVED(channel)) == 0) {
        return CSK_DRIVER_OK;
    }

    timer->flags = timer->flags & (~TIMER_FLAG_CH_ACTIVED(channel));

    // config disable register
    if(channel == CSK_TIMER_CHANNEL_0){
        timer->reg->REG_CONTROL0.bit.ENABLE = 0x0;
    }else if(channel == CSK_TIMER_CHANNEL_1){
        timer->reg->REG_CONTROL1.bit.ENABLE = 0x0;
    }

    return CSK_DRIVER_OK;
}

static void
DUALTIMERS_IRQ_Handler(DUALTIMERS_RESOURCES *timer){
    uint32_t ret0, ret1;
    uint32_t event = 0;
    ret0 = timer->reg->REG_MIS0.all;
    ret1 = timer->reg->REG_MIS1.all;

    if(ret0){
        // clear interrupt status
        timer->reg->REG_INTCLR0.all = CSK_TIMER_CLR_INT;
        event = CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH0;
        if(timer->info[0].cb_event){
            timer->info[0].cb_event(event, timer->info[0].workspace);
        }
    }

    event = 0;

    if(ret1){
        timer->reg->REG_INTCLR1.all = CSK_TIMER_CLR_INT;
        event = CSK_TIMER_EVENT_ONESTEP_COMPLETE_CH1;
        if(timer->info[1].cb_event){
            timer->info[1].cb_event(event, timer->info[1].workspace);
        }
    }

}

static void DUALTIMERS0_IRQ_Handler(void){
	DUALTIMERS_IRQ_Handler(&dualtimers0_resources);
}

static void DUALTIMERS1_IRQ_Handler(void){
	DUALTIMERS_IRQ_Handler(&dualtimers1_resources);
}
