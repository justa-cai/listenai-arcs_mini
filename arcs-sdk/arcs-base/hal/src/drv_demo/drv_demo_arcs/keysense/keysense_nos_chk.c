#include "stdio.h"
#include <string.h>

#include "chip.h"

#include "Driver_KEYSENSE.h"
#include "IOMuxManager.h"

#include "systick.h"
#include "log_print.h"


/********************** Private micro define begin ***************************/

/********************** Private micro define end ***************************/


/************************* Private typedef begin ***************************/
typedef void (*function)(void);

/*************************** Private typedef end ***************************/


/******************* Private functions prototype begin ********************/
void WaitgPassFlag(uint32_t count);
void KEYSENSE0_AllMode_Interrupt(void);
void KEYSENSE1_AllMode_Interrupt(void);

/******************* Private functions prototype end ********************/

/******************* Private variables begin ********************/
static uint32_t gPassFlag = 0;

static function test_function_array[] = {
        KEYSENSE0_AllMode_Interrupt,
       // KEYSENSE1_AllMode_Interrupt,
};


/******************* Private variables end ********************/

int main(void)
{
    logInit(0, 115200);
    CLOG("KEYSENSE test all modes, test begin");

    uint32_t times;
    for(times = 0; times < sizeof(test_function_array)/sizeof(test_function_array[0]); times++){
        test_function_array[times]();
    }

	while(1);
}


void WaitgPassFlag(uint32_t count)
{
	for(uint32_t i=0; i<count; i++){
		while(gPassFlag == 0);
		gPassFlag = 0;
	}
}


static void KEYSENSE0_WAKEUP_Event(void* param){
	CLOG("KEYSENSE0 wakeup event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP);
}


static void KEYSENSE0_ADCTRIGGER_Event(void* param){
	CLOG("KEYSENSE0 adctrigger event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
}


static void KEYSENSE0_RELEASE_Event(void* param){
	CLOG("KEYSENSE0 release event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_RELEASE);
}


static void KEYSENSE0_PRESS_Event(void* param){
	CLOG("KEYSENSE0 press event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_PRESS);
}


static void KEYSENSE1_WAKEUP_Event(void* param){
    CLOG("KEYSENSE1 wakeup event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP);
}


static void KEYSENSE1_ADCTRIGGER_Event(void* param){
    CLOG("KEYSENSE1 adctrigger event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER);
}


static void KEYSENSE1_RELEASE_Event(void* param){
    CLOG("KEYSENSE1 release event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_RELEASE);
}


static void KEYSENSE1_PRESS_Event(void* param){
    CLOG("KEYSENSE1 press event generate");
    HAL_KEYSENSE_InterruptDisable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_PRESS);
}


void Default_Handler(void)
{
	 CLOG("default event generate");
}


//KEYSENSE_RegDef *keyReg;

void KEYSENSE0_AllMode_Interrupt(void)
{
	/*********KEYSENSE test all modes********/
    //initialize
    HAL_KEYSENSE_Initialize(KEYSENSE0());

    //PB3 Keysense0
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 2, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 5;

    HAL_KEYSENSE_Control(KEYSENSE0(), CSK_KEYSENSE_THD);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_WAKEUP, KEYSENSE0_WAKEUP_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_ADCTRIG, KEYSENSE0_ADCTRIGGER_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_RELEASE, KEYSENSE0_RELEASE_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE0(), CSK_KEYSENSE_PRESS, KEYSENSE0_PRESS_Event);
    HAL_KEYSENSE_InterruptEnable(KEYSENSE0(), CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP | \
    										 CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER | \
											 CSK_KEYSENSE_INTERRUPT_MODE_RELEASE |\
											 CSK_KEYSENSE_INTERRUPT_MODE_PRESS);

    HAL_KEYSENSE_Enable(KEYSENSE0());
}


void KEYSENSE1_AllMode_Interrupt(void)
{
  /*********KEYSENSE test all modes********/
    //initialize
    HAL_KEYSENSE_Initialize(KEYSENSE1());

    //PB4 Keysense1
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.bit.PAD_AON_GPIOB_03_ANA_SEL = 5;

    HAL_KEYSENSE_Control(KEYSENSE1(), CSK_KEYSENSE_THD);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_WAKEUP, KEYSENSE1_WAKEUP_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_ADCTRIG, KEYSENSE1_ADCTRIGGER_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_RELEASE, KEYSENSE1_RELEASE_Event);
    HAL_KEYSENSE_RegisterCallback(KEYSENSE1(), CSK_KEYSENSE_PRESS, KEYSENSE1_PRESS_Event);
    HAL_KEYSENSE_InterruptEnable(KEYSENSE1(), CSK_KEYSENSE_INTERRUPT_MODE_WAKEUP | \
                                             CSK_KEYSENSE_INTERRUPT_MODE_ADCTRIGGER | \
                                             CSK_KEYSENSE_INTERRUPT_MODE_RELEASE |\
                                             CSK_KEYSENSE_INTERRUPT_MODE_PRESS);

    HAL_KEYSENSE_Enable(KEYSENSE1());
}

