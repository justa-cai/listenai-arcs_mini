#include "Driver_TRNG.h"
#include "ClockManager.h"
#include <stdio.h>

/* Private macro -------------------------------------------------------------*/
#define TRNGCOUNT 5 // 只打印5次

/* Private variables ---------------------------------------------------------*/
volatile uint32_t testCnt = 0;

static void TRNG_DataGenerate_Event(void *param)
{
    uint32_t trngdata;
    testCnt++;
    trngdata = HAL_TRNG_GetData(TRNG());
    printf("trng data is 0x%x\n", trngdata);
    HAL_TRNG_Enable(TRNG());
    if (testCnt >= TRNGCOUNT) {
        HAL_TRNG_Disable(TRNG());
        printf("trng interrupt test end!!!!");
    }
}
// 中断模式
void TRNG_Test_Interrupt(void)
{
    /*********TRNG test interrupt modes********/
    printf("TRNG test interrupt modes, test begin");
    testCnt = 0;

    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);

    // initialize
    HAL_TRNG_Initialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_FULL);
    HAL_TRNG_Control(TRNG(), CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);

    HAL_TRNG_RegisterCallback(TRNG(), TRNG_DataGenerate_Event);
    HAL_TRNG_InterruptEnable(TRNG());

    printf("trng data is below:\n");

    HAL_TRNG_Enable(TRNG());
}

int main(int argc, char **argv)
{
    printf("Hello, world! TRNG\n");
    __HAL_CRM_TRNG_CLK_ENABLE();
    TRNG_Test_Interrupt();
}
