#include <stdio.h>
#include <stdint.h>

#include "sdk_version.h"

#if CONFIG_SYS_INIT
#include "sys_init.h"
#endif

#if CONFIG_MODULE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "sysheap.h"

#if !defined(CONFIG_MODULE_HEAP)
#error "CONFIG_MODULE_HEAP must be defined"
#endif

#endif

__attribute__((weak)) void soc_pre_init(void)
{

}

__attribute__((weak)) void soc_init(void)
{
}

__attribute__((weak)) int soc_cpu_id_get(void)
{
    return 0;
}

#if CONFIG_MODULE_FREERTOS
void main_task(void *pvParameters)
{
    /* POST_KERNEL initialization - RTOS is now running */
#if CONFIG_SYS_INIT
    sys_init_run_level(SYS_INIT_LEVEL_POST_KERNEL);
#endif

    /* PRE_APPLICATION initialization - before main() */
#if CONFIG_SYS_INIT
    sys_init_run_level(SYS_INIT_LEVEL_PRE_APPLICATION);
#endif

    int main(int argc, char *argv[]);
    main(0, NULL);
    vTaskDelete(NULL);
}
#endif

#if CONFIG_BANNER
__attribute__((weak)) void boot_banner(void)
{
    extern int soc_cpu_id_get(void);
    printf("\n********SDK %s @ %s********\n", SDK_VERSION_STRING, BUILD_VERSION);
    printf("Running on cpu-id: %d\n", soc_cpu_id_get());
}
#endif

#if CONFIG_LOG
__attribute__((weak)) int lisa_log_init(void)
{
    printf("warning, lisa_log_init is not implemented\n");
    return 0;
}
#endif

__attribute__((weak)) void pre_main_hook(void)
{

}

__attribute__((weak, noreturn)) void system_entry(void)
{
    /* 芯片初始化之前 */
    soc_pre_init();

    /* 芯片底层必须要的初始化 */
    soc_init();

#if CONFIG_SYS_INIT
    /* 串口设备必须在此期间初始化, 否则在日志系统初始化之前, 无法使用标准输出 */
    sys_init_run_level(SYS_INIT_LEVEL_PRE_SYSTEM_INIT);
#endif

    /* 打印横幅 */
#if CONFIG_BANNER
    boot_banner();
#endif

#if CONFIG_PSRAM_INIT
    printf("PSRAM initialize success\n");
#endif

    /* 系統heap初始化需在系统其他组件前面 */
#if CONFIG_MODULE_FREERTOS
    sysheap_init();
#endif

#if CONFIG_CONSOLE
    /* heap 可用后初始化 console（创建互斥锁等） */
    extern void console_init(void);
    console_init();
#endif

    /* 日志系统初始化, 此时可用日志系统 */
#if CONFIG_LOG
    lisa_log_init();
#if CONFIG_PRINTF_LOG_REDIRECT
    extern void printf_log_redirect_enable(void);
    printf_log_redirect_enable();
#endif
#endif

    /* 更新printf, printk的输出, 从早期打印输出接口替换更新到日志系统 */
#if CONFIG_SYS_INIT
    sys_init_run_level(SYS_INIT_LEVEL_PRE_DEVICES_INIT);
#endif

    extern void pre_main_hook(void);
    pre_main_hook();

    /* PRE_KERNEL initialization - before scheduler starts */
#if CONFIG_SYS_INIT
    sys_init_run_level(SYS_INIT_LEVEL_PRE_KERNEL);
#endif

#if !CONFIG_CPP_INIT_DEFERRED
    extern void cpp_init(void);
    cpp_init();
#endif

#if CONFIG_MODULE_FREERTOS
    xTaskCreate(main_task, "main", CONFIG_MAIN_TASK_STACK_SIZE, NULL, CONFIG_MAIN_TASK_PRIORITY, NULL);
    vTaskStartScheduler();
    while(1) {}
#else
    int main(int argc, char *argv[]);
    main(0, NULL);
    while (1) {}
#endif
}
