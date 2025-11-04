#include "stdio.h"
#include <string.h>
#include "log_print.h"
#include "systick.h"
#include "chip.h"

#include "IOMuxManager.h"
#include "PowerManager.h"

_CP_RAM_DATA static volatile uint8_t software_lock = 0;

#define WAKEUP_ACT_JUMP_RAM         (0xAA)
#define WAKEUP_ACT_JUMP_FLASH       (0xBB)
#define GPIO_PMU_WAKEUP_FUN_SEL		1

#define WAKEUP_SOURCE_TO_STRING(src)	(	\
		src == PMU_WAKEUP_NONE ? "None wakeup" : \
		src == PMU_WAKEUP_TIMER ? "TIMER wakeup" : \
		src == PMU_WAKEUP_IWDT ? "IWDT wakeup" : \
		src == PMU_WAKEUP_KEY0 ? "KEY0 wakeup" : \
		src == PMU_WAKEUP_KEY1 ? "KEY1 wakeup" : \
		src == PMU_WAKEUP_RTC  ? "RTC  wakeup" : \
		src == PMU_WAKEUP_WIFI ? "WIFI  wakeup" : \
		src == PMU_WAKEUP_GPIOB_00 ? "GPIOB_00 wakeup" : \
		src == PMU_WAKEUP_GPIOB_01 ? "GPIOB_01 wakeup" : \
		src == PMU_WAKEUP_GPIOB_02 ? "GPIOB_02 wakeup" : \
		src == PMU_WAKEUP_GPIOB_03 ? "GPIOB_03 wakeup" : \
		src == PMU_WAKEUP_GPIOB_04 ? "GPIOB_04 wakeup" : \
		src == PMU_WAKEUP_GPIOB_05 ? "GPIOB_05 wakeup" : \
		src == PMU_WAKEUP_GPIOB_06 ? "GPIOB_06 wakeup" : \
		src == PMU_WAKEUP_GPIOB_07 ? "GPIOB_07 wakeup" : \
		src == PMU_WAKEUP_GPIOB_08 ? "GPIOB_08 wakeup" : \
		src == PMU_WAKEUP_GPIOB_09 ? "GPIOB_09 wakeup" : \
		"Unknown wakeup")

static void GetWakeupEvent(uint32_t wakesrc) {
	CLOG("%s\n", WAKEUP_SOURCE_TO_STRING(wakesrc));
}

void enable_wakeup_jump(uint32_t wakeup_addr, uint32_t wakeup_action) {
	// Set the wakeup action
	IP_AON_CTRL->REG_AON_DIG_RSVD0.all = wakeup_action;

	// Set the wakeup address
	IP_AON_CTRL->REG_AON_DIG_RSVD1.all = wakeup_addr;
}


_CP_RAM_TEXT void software_handler(void){
    *(uint32_t*)(0xe0031000 - 4) = 0;

    software_lock = 1;

    CLOGD("In software hook");
}

extern void SystemInit_Copy(void);
extern void _start(void);

int main(void)
{
	// Attention
//	SystemInit_Copy();

	logInit(0, 115200);

	// Enable global interrupt
	enable_GINT();

	register_ISR(IRQ_Software_VECTOR, software_handler, NULL);
	enable_IRQ(IRQ_Software_VECTOR);

	GetWakeupEvent(HAL_PMU_GetWakeUpCause());
	HAL_PMU_ClearWakeUpCause();
	
	// Software Interrupt, need remove system_RISCVN300.c PMP_INIT
	*(uint32_t *)(0xe0031000 - 4) = 1;
	while(!software_lock);

	// GpioB_04 high level wakeup
    HAL_PMU_GPIOPolaritySelect(PMU_WAKEUP_GPIOB_06, 0);
    HAL_PMU_EnableWakeUpSrc(PMU_WAKEUP_GPIOB_06);
	AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, GPIO_PMU_WAKEUP_FUN_SEL);

	CLOGD("Configure deeep sleep wakeup address: 0x%x", (uint32_t)&_start);
	enable_wakeup_jump((uint32_t)&_start, WAKEUP_ACT_JUMP_RAM);

	// Close 24M Xtal
	__HAL_PMU_XO24M_DISABLE();

	CLOG("enter sleep!\r\n");

	CLOG_FLUSH();

	HAL_PMU_PreConfigSleepTrigger(PMU_SLEEP_CMD_BY_AP);
	HAL_PMU_EnterDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_DEEPSLEEPENTRY_WFI); //switch mode

	while(1);
}
