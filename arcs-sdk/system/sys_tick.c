#include "ClockManager.h"
#include "systick.h"


static _systick_info systick_info = {
    .interval = 0,
    .systick_value = 0,
};

__attribute__((weak)) void SysTick_Handler(void){
    systick_info.systick_value++;
}

__attribute__((weak)) void SysTick_Open(uint64_t interval){
    systick_info.systick_value = 0;
    systick_info.interval = interval;
    register_ISR(7, SysTick_Handler,NULL);
    SysTick_Config(interval);
    SysTimer_Start();
}

__attribute__((weak)) void SysTick_Close(void){
    SysTimer_Stop();
}

__attribute__((weak)) uint32_t SysTick_Time(void){
    uint32_t SysTick_VAL;
    SysTick_VAL = SysTimer_GetLoadValue();
    return (uint32_t)(systick_info.interval * systick_info.systick_value + SysTick_VAL);
}

__attribute__((weak)) uint32_t SysTick_Value(void){
    return (uint32_t)(systick_info.systick_value);
}

__attribute__((weak)) void SysTick_Delay_Ms(uint32_t nms){
    uint64_t start_mtime, delta_mtime;
    uint64_t delay_ticks = (CRM_GetMtimeFreq() * (uint64_t)nms) / 1000;

    start_mtime = SysTimer_GetLoadValue();

    do {
        delta_mtime = SysTimer_GetLoadValue() - start_mtime;
    } while (delta_mtime < delay_ticks);
}

__attribute__((weak)) void SysTick_Delay_Us(uint32_t nus){
    uint64_t start_mtime, delta_mtime;
    uint64_t delay_ticks = (CRM_GetMtimeFreq() * (uint64_t)nus) / 1000000;

    start_mtime = SysTimer_GetLoadValue();

    do {
        delta_mtime = SysTimer_GetLoadValue() - start_mtime;
    } while (delta_mtime < delay_ticks);
}

__attribute__((section(".ipc.sys.time.base"), used)) volatile uint32_t sys_time_base_cp = 0;

void SysTimeBaseSetCP(uint32_t ms)
{
    sys_time_base_cp = ms;
}

uint32_t SysTimeMsGet(void)
{
#if CONFIG_ARCS_AP_CORE
    return __get_rv_time() / 1000;
#else
    return sys_time_base_cp + __get_rv_time() / 1000;
#endif
}
