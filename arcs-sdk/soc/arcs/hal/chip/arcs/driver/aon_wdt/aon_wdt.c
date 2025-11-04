/*
 * aon_timer.c
 *
 *  Created on: Mar 29, 2022
 *      Author: USER
 */

#include "Driver_AON_WDT.h"
#include "arcs_ap.h"

#define AON_WDT_LOAD_VALUE_MASK                     (0xFFFFFF)

#define AON_WDT_FLAG_INITIALIZED                    (1UL << 0)
#define AON_WDT_FLAG_POWERED                        (1UL << 1)

#define AON_WDT_DOMAIN_CTRL_RESET_PMU                0x5856E201
#define AON_WDT_DOMAIN_CTRL_RESET_DBB                0x5856E200

#define AON_WDT_STOP_PROTECT_LOCK                    0xDEADFACE
#define AON_WDT_STOP_PROTECT_RELEASE                 0xBABEBEEF

#define AON_WDT_START_PROTECT_LOCK                   0xBADBEE01
#define AON_WDT_START_PROTECT_RELEASE                0xBADBEE00

typedef struct _AON_WDT_INFO {
    HAL_AON_WDT_SignalEvent_t cb_event;
    void* workspace;
} AON_WDT_INFO;

typedef struct _AON_WDT_RESOURCES {
	AON_WDT_RegDef* reg;
    uint32_t irq_num;
    void (*irq_handler)(void);
    AON_WDT_INFO* info;
    uint32_t flags;
} AON_WDT_RESOURCES;

static void AON_WDT_Handler(void);

/// AON WDT
static AON_WDT_INFO aon_wdt_info = {0};
static AON_WDT_RESOURCES aon_wdt_resources = {
		IP_AON_WDT,
		IRQ_AON_WDT_VECTOR,
		AON_WDT_Handler,
        &aon_wdt_info,
        0
};

#define AON_WDT_CHECK_RESOURCES(res)  do{\
        if(res != &aon_wdt_resources){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)

void* AON_WDT(void){
    return (void*)&aon_wdt_resources;
}

/// AON WDT FUNCTION CONFIGURE

int32_t AON_WDT_Initialize(void* res, HAL_AON_WDT_SignalEvent_t callback, void* workspace){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_RESOURCES *aon_wdt = (AON_WDT_RESOURCES*) res;

    if (aon_wdt->flags & AON_WDT_FLAG_INITIALIZED){
        return CSK_DRIVER_OK;
    }

    aon_wdt->info->cb_event = callback;
    aon_wdt->info->workspace = workspace;

    aon_wdt->flags = AON_WDT_FLAG_INITIALIZED;

    return CSK_DRIVER_OK;
}

int32_t AON_WDT_Uninitialize(void* res){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_RESOURCES *aon_wdt = (AON_WDT_RESOURCES*) res;

    aon_wdt->info->cb_event = NULL;
    aon_wdt->info->workspace = NULL;

    aon_wdt->flags = 0;

    return CSK_DRIVER_OK;
}

int32_t AON_WDT_PowerControl(void* res, CSK_POWER_STATE state){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_RESOURCES *aon_wdt = (AON_WDT_RESOURCES*) res;

    switch (state){
    case CSK_POWER_OFF:
        if ((aon_wdt->flags & AON_WDT_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

		// Disable UART IRQ
		disable_IRQ(aon_wdt->irq_num);

		aon_wdt->flags &= ~AON_WDT_FLAG_POWERED;

		// Uninstall IRQ Handler
		register_ISR(aon_wdt->irq_num, NULL, NULL);

        break;

    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:
        if(!(aon_wdt->flags & AON_WDT_FLAG_INITIALIZED)){
            return CSK_DRIVER_ERROR;
        }

        if(aon_wdt->flags & AON_WDT_FLAG_POWERED){
            return CSK_DRIVER_OK;
        }

        // Clear reset status
        aon_wdt->reg->REG_AON_WDT_IRQ_CLR.all = 0x1;
        while(aon_wdt->reg->REG_AON_WDT_IRQ_CAUSE.bit.WDT_RESET_OCURRED) {
        	// Do nothing, intentionally empty
        }

		register_ISR(aon_wdt->irq_num, aon_wdt->irq_handler, NULL);
		enable_IRQ(aon_wdt->irq_num);

        aon_wdt->flags = AON_WDT_FLAG_POWERED;

        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

int32_t AON_WDT_Control(void* res, uint32_t control, uint32_t arg){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_RESOURCES *aon_wdt = (AON_WDT_RESOURCES*) res;

    if(!(aon_wdt->flags & AON_WDT_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    // Set time load value
    if (control & HAL_AON_WDT_TIME_Msk){
    	aon_wdt->reg->REG_AONWDTTIMER_LOADVAL.all = arg & AON_WDT_LOAD_VALUE_MASK;
    }

    // Enable interrupt
    if (control & HAL_AON_WDT_INTERRUPT_Msk){
        if (arg){
            aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDT_INT_MASK = 0x1;
        } else {
            aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDT_INT_MASK = 0x0;
        }
    }

    // Set WDT trigger mode
    switch (control & HAL_AON_WDT_MODE_CTRL_Msk){
    	case HAL_AON_WDT_CTRL_RESET_MODE:
    	{
			aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDT_MODE = 0x1;
    	}
    		break;
    	case HAL_AON_WDT_CTRL_INT_MODE:
    	{
			aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDT_MODE = 0x0;
    	}
    		break;
    }

    // Set WDT reset domain
    switch (control & HAL_AON_WDT_RST_DOMAIN_Msk){
		case HAL_AON_WDT_RST_CORE_DOMAIN:
		{
			aon_wdt->reg->REG_RESET_PMU_EN.all = AON_WDT_DOMAIN_CTRL_RESET_DBB;
		}
			break;
		case HAL_AON_WDT_RST_PMU_DOMAIN:
		{
			aon_wdt->reg->REG_RESET_PMU_EN.all = AON_WDT_DOMAIN_CTRL_RESET_PMU;
		}
			break;
    }

    return CSK_DRIVER_OK;
}

int32_t AON_WDT_Enable(void* res){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_RESOURCES *aon_wdt = (AON_WDT_RESOURCES*) res;

    // Release lock and wait sync to protected bit
    do{
    	aon_wdt->reg->REG_OTHER_PROTECT.all = AON_WDT_START_PROTECT_RELEASE;
    } while(aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.START_PROTECTED);

    // Start
    aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.START = 0x1;
    while(!aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDENABLED);

    // Lock protect
    while(!aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.START_PROTECTED){
        aon_wdt->reg->REG_OTHER_PROTECT.all = AON_WDT_START_PROTECT_LOCK;
    }

    return CSK_DRIVER_OK;
}

int32_t AON_WDT_Refresh(void* res){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_RESOURCES *aon_wdt = (AON_WDT_RESOURCES*) res;

    // Release lock and wait sync to protected bit
    do{
    	aon_wdt->reg->REG_OTHER_PROTECT.all = AON_WDT_START_PROTECT_RELEASE;
    } while(aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.LOAD_PROTECTED);

    aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.RELOAD = 0x1;

    // TODO Reload bit will be pretend by protected bit
//    // Lock protect
//    while(!aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.LOAD_PROTECTED){
//    	aon_wdt->reg->REG_OTHER_PROTECT.all = AON_WDT_START_PROTECT_LOCK;
//    }

    return CSK_DRIVER_OK;
}

int32_t AON_WDT_Disable(void* res){
    AON_WDT_CHECK_RESOURCES(res);
    AON_WDT_RESOURCES *aon_wdt = (AON_WDT_RESOURCES*) res;

    // Release protect
    do {
        aon_wdt->reg->REG_STOP_PROTECT.all = AON_WDT_STOP_PROTECT_RELEASE;
    } while(aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.STOP_PROTECTED);

    // Stop
    aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.STOP = 0x1;
    while(aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.WDENABLED);

    // Lock protect
    while(!aon_wdt->reg->REG_AON_WDTTIMER_CTRL.bit.STOP_PROTECTED){
        aon_wdt->reg->REG_STOP_PROTECT.all = AON_WDT_STOP_PROTECT_LOCK;
    }

    return CSK_DRIVER_OK;
}

static void AON_WDT_Handler(void){
	// Clear interrupt source
	aon_wdt_resources.reg->REG_AON_WDT_IRQ_CLR.all = 0x1;

    if (aon_wdt_resources.info->cb_event){
    	aon_wdt_resources.info->cb_event(aon_wdt_resources.info->workspace);
    }

    while(aon_wdt_resources.reg->REG_AON_WDT_IRQ_CAUSE.bit.WDT_WAKEUP_STATUS);
}
