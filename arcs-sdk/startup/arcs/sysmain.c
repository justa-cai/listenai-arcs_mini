#include <stdio.h>
#include <stdint.h>
#include "arcs_ap.h"
#include "log_print.h"
#include "syslog.h"
#include "sdk_version.h"

#if CONFIG_SYS_INIT
#include "sys_init.h"
#endif

#if CONFIG_ARCS_AP_CORE
#include "rf_cali.h"
#endif

#if CONFIG_MODULE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "memap.h"

#if !defined(CONFIG_MODULE_HEAP)

#error "CONFIG_MODULE_HEAP must be defined"
#endif

#include "sysheap.h"

#endif

#if CONFIG_MODULE_FREERTOS
void main_task(void *pvParameters)
{

    /* PRE_APPLICATION initialization - before main() */
#if CONFIG_SYS_INIT
    sys_init_run_level(SYS_INIT_LEVEL_PRE_APPLICATION);  /* SYS_INIT_LEVEL_PRE_APPLICATION */
#endif

    int main(int argc, char *argv[]);
    main(0, NULL);
    vTaskDelete(NULL);
}
#endif

#if CONFIG_SYSLOG_BANNER
__attribute__((weak)) void boot_banner(void)
{
    printf("\n********Arcs SDK %s @ %s********\n", SDK_VERSION_STRING, BUILD_VERSION);
    printf("Running on hart-id: %ld\n", (unsigned long)__get_hart_id());
}
#endif

#if CONFIG_LOG
__attribute__((weak)) int lisa_log_init(void)
{
    printf("warning, lisa_log_init is not implemented\n");
    return 0;
}
#endif

__always_inline static void cpp_init(void)
{
#if CONFIG_LINK_CPP_RUNTIME_SECTIONS
    /* 调用C++全局构造函数 */
    extern void atexit(void (*function)(void));
    extern void __libc_init_array(void);
    extern void __libc_fini_array(void);
    
    /* 纯 C函数调用初始化C++全局构造函数 */
    atexit(__libc_fini_array);
    __libc_init_array();
#endif //CONFIG_LINK_CPP_RUNTIME_SECTIONS

#if CONFIG_CPP_EXCEPTIONS
    struct object {
        long placeholder[ 10 ];
    };
    void __register_frame_info(const void *begin, struct object * ob);
    extern char __eh_frame[];

    static struct object ob;
    __register_frame_info(__eh_frame, &ob);
#endif // CONFIG_CPP_EXCEPTIONS
}

__attribute__((weak)) void abort(void)
{
    assert(0);
}

__attribute__((weak, noreturn)) void entry(void)
{
    syslog_init_early();
    log_print_hook_set(syslog_raw_output_v);

#if CONFIG_WATCHDOG_ENABLE
    boot_watchdog_init();
#elif CONFIG_BOOT_WITH_WATCHDOG
    boot_watchdog_init();
    boot_watchdog_enable(0);
#endif

#if CONFIG_SYSLOG_BANNER
    boot_banner();
#endif

#if CONFIG_MODULE_FREERTOS
#if CONFIG_PSRAM_INIT
    /* prepare PSRAM */
    uint32_t rdly = 18, wdly = 22;
    if (PSRAM_Initialize(&rdly, &wdly, 1) == 0) {
        CLOGD("PSRAM initialize success");
    }
    HAL_InvalidateDCache_by_Addr((void *)MEM_PSRAM_BASE, MEM_PSRAM_SIZE);
#endif

    scatload_psram();

    sysheap_init();

#if CONFIG_LOG
    lisa_log_init();
#endif

    /* PRE_DEVICES_INIT initialization - before device init */
#if CONFIG_SYS_INIT
    extern int sys_init_run_level(uint8_t level);
    sys_init_run_level(SYS_INIT_LEVEL_PRE_DEVICES_INIT);  /* SYS_INIT_LEVEL_PRE_DEVICES_INIT */
#endif

#if CONFIG_LISA_DEVICE
#if !CONFIG_LISA_DEVICE_MANUAL_INIT
    lisa_device_init();
#endif
#endif

    extern void pre_main_hook(void);
    pre_main_hook();

    /* PRE_KERNEL initialization - before scheduler starts */
#if CONFIG_SYS_INIT
    sys_init_run_level(SYS_INIT_LEVEL_PRE_KERNEL);  /* SYS_INIT_LEVEL_PRE_KERNEL */
#endif

    cpp_init();

    /* POST_KERNEL initialization - RTOS is now running */
#if CONFIG_SYS_INIT
    sys_init_run_level(SYS_INIT_LEVEL_POST_KERNEL);  /* SYS_INIT_LEVEL_POST_KERNEL */
#endif

    xTaskCreate(main_task, "main", CONFIG_MAIN_TASK_STACK_SIZE, NULL, CONFIG_MAIN_TASK_PRIORITY, NULL);
    vTaskStartScheduler();
    /* POST_KERNEL initialization - RTOS is now running */
#if CONFIG_SYS_INIT
    sys_init_run_level(SYS_INIT_LEVEL_POST_KERNEL);  /* SYS_INIT_LEVEL_POST_KERNEL */
#endif
    while(1) {

    }
#else
    int main(int argc, char *argv[]);
    main(0, NULL);
    while (1) {
    }
#endif
}
