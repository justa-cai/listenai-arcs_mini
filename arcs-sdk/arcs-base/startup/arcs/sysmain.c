#include <stdio.h>
#include <stdint.h>
#include "arcs_ap.h"
#include "log_print.h"
#include "syslog.h"
#include "arcs_sdk_version.h"

#if CONFIG_ARCS_AP_CORE
#include "rf_cali.h"
#endif

#if CONFIG_MODULE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"

#if !defined(CONFIG_MODULE_HEAP)

#error "CONFIG_MODULE_HEAP must be defined"
#endif

#include "sysheap.h"

#endif

#if CONFIG_MODULE_FREERTOS
void main_task(void *pvParameters)
{
    int main(int argc, char *argv[]);
    main(0, NULL);
    vTaskDelete(NULL);
}
#endif

#if CONFIG_SYSLOG_BANNER
__attribute__((weak)) void boot_banner(void)
{
    printf("\n********Arcs SDK@%s-@v%d.%d.%d********\n", VERSION_COMMIT, VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
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
    syslog_init(CONFIG_SYSLOG_UART_PORT, CONFIG_SYSLOG_UART_BAUDRATE);
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
    sysheap_init();

#if CONFIG_LOG
    lisa_log_init();
#endif

    extern void pre_main_hook(void);
    pre_main_hook();

    cpp_init();

    xTaskCreate(main_task, "main", CONFIG_MAIN_TASK_STACK_SIZE, NULL, CONFIG_MAIN_TASK_PRIORITY, NULL);
    vTaskStartScheduler();
    while(1) {

    }
#else
    int main(int argc, char *argv[]);
    main(0, NULL);
    while (1) {
    }
#endif
}
