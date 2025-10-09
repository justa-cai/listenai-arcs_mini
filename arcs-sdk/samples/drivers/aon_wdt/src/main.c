#define LOG_TAG "Aon_Wdt Sample"
#include "Driver_AON_WDT.h"
#include "PowerManager.h"
#include "lisa_log.h"

static void *AON_WDT_Handler = NULL;
// AON_WDT_Trigger 为触发标志，判断是否触发中断
static volatile uint16_t AON_WDT_Trigger = 0;
// AON_WDT_Reload 表示看门狗计数初始值
static volatile uint32_t AON_WDT_Reload = 20000;
// AON_WDT中断回调函数，触发时喂狗
void AON_WDT_Feed_EventCallback(void *workspace)
{
    LOGI("Aon_Wdt trigger,feed dog");

    AON_WDT_Refresh(AON_WDT_Handler);

    AON_WDT_Trigger = 1;
}
// Aon_Wdt初始化函数
void AON_WDT_Init(void)
{
    AON_WDT_Handler = AON_WDT();// Aon_Wdt句柄初始化

    AON_WDT_Initialize(AON_WDT_Handler, AON_WDT_Feed_EventCallback, NULL);

    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);

    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, AON_WDT_Reload);
}
// Aon_Wdt去初始化函数
void AON_WDT_UnInit(void)
{
    AON_WDT_Disable(AON_WDT_Handler);

    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF);

    AON_WDT_Uninitialize(AON_WDT_Handler);
}
// AON_WDT中断模式，在计数值减到0时产生中断
void AON_WDT_INT_Feed_Sample(void)
{
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_INTERRUPT_EN | HAL_AON_WDT_CTRL_INT_MODE | HAL_AON_WDT_RST_PMU_DOMAIN,
                    1);
    AON_WDT_Enable(AON_WDT_Handler);

    while (!AON_WDT_Trigger);

    AON_WDT_Trigger = 0;
}

int main(int argc, char **argv)
{
    LOGI("Hello, world! Aon_Wdt");

    AON_WDT_Init();

    AON_WDT_INT_Feed_Sample();

    AON_WDT_UnInit();

    LOGI("Aon_Wdt end");
}
