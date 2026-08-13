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


#include "log_print.h"

__attribute__((weak)) void SysTick_Delay_Ms(uint32_t nms){
    uint64_t start_mtime, delta_mtime;
    //uint64_t delay_ticks = (CRM_GetMtimeFreq() * (uint64_t)nms) / 1000;
    uint64_t delay_ticks = (1000 * (uint64_t)nms);
#if  (IC_BOARD==0)  // FPGA
    delay_ticks = (1000 * (uint64_t)nms);
#endif

    start_mtime = SysTimer_GetLoadValue();

//    CLOG("MS(%d) start_mtime=%lld  delay_ticks=%lld", nms, start_mtime, delay_ticks);
//    CLOG("CRM_GetMtimeFreq=%d", CRM_GetMtimeFreq());

    do {
        delta_mtime = SysTimer_GetLoadValue() - start_mtime;
    } while (delta_mtime < delay_ticks);
}

__attribute__((weak)) void SysTick_Delay_Us(uint32_t nus){
    uint64_t start_mtime, delta_mtime;
    //uint64_t delay_ticks = (CRM_GetMtimeFreq() * (uint64_t)nus) / 1000000;
    uint64_t delay_ticks = (1 * (uint64_t)nus);
#if  (IC_BOARD==0)  // FPGA
    delay_ticks = (1 * (uint64_t)nus);
#endif

    start_mtime = SysTimer_GetLoadValue();

//    CLOG("US(%d) start_mtime=%lld  delay_ticks=%lld", nus, start_mtime, delay_ticks);

    do {
        delta_mtime = SysTimer_GetLoadValue() - start_mtime;
    } while (delta_mtime < delay_ticks);
}
