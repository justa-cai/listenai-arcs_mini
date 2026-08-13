#include "ClockManager.h"
#include "systick.h"

#if CONFIG_SYS_INIT && CONFIG_ARCS_AP_CORE
#include "sys_init.h"
#endif

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

#define SYS_TIME_BASE_CP_MAGIC 0x53544350U

struct sys_time_base_cp_data {
    volatile uint32_t base_ms;
    volatile uint32_t magic;
};

#if defined(CONFIG_MEM_IPC_ISOLATED) && CONFIG_MEM_IPC_ISOLATED
#define SYS_TIME_BASE_CP_SECTION __attribute__((section(".ipc.sys.time.base"), used))
#else
#define SYS_TIME_BASE_CP_SECTION
#endif

static SYS_TIME_BASE_CP_SECTION struct sys_time_base_cp_data sys_time_base_cp = {
    .base_ms = 0,
    .magic = 0,
};

void SysTimeBaseSetCP(uint32_t ms)
{
    sys_time_base_cp.magic = 0;
    __asm__ volatile("fence rw, rw" ::: "memory");
    sys_time_base_cp.base_ms = ms;
    __asm__ volatile("fence rw, rw" ::: "memory");
    sys_time_base_cp.magic = SYS_TIME_BASE_CP_MAGIC;
}

#if CONFIG_SYS_INIT && CONFIG_ARCS_AP_CORE
static int sys_time_base_cp_auto_init(void)
{
    SysTimeBaseSetCP(__get_rv_time() / 1000);
    return 0;
}

#if CONFIG_MODULE_FREERTOS
SYS_INIT(sys_time_base_cp_auto_init, SYS_INIT_LEVEL_PRE_APPLICATION, SYS_INIT_SUB_PRIORITY_LAST);
#else
SYS_INIT(sys_time_base_cp_auto_init, SYS_INIT_LEVEL_PRE_KERNEL, SYS_INIT_SUB_PRIORITY_LAST);
#endif
#endif

#if !CONFIG_ARCS_AP_CORE
static uint32_t sys_time_base_cp_get(void)
{
    if (sys_time_base_cp.magic != SYS_TIME_BASE_CP_MAGIC) {
        return 0;
    }

    __asm__ volatile("fence rw, rw" ::: "memory");
    return sys_time_base_cp.base_ms;
}
#endif

uint32_t SysTimeMsGet(void)
{
#if CONFIG_ARCS_AP_CORE
    return __get_rv_time() / 1000;
#else
    return sys_time_base_cp_get() + __get_rv_time() / 1000;
#endif
}
