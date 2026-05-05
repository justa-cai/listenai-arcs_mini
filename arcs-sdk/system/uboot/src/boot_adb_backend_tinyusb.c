#include "boot_adb_backend.h"

#include "boot_config.h"

#include "FreeRTOS.h"
#include "syslog.h"
#include "task.h"
#include "tusb.h"

extern int boot_watchdog_feed(void);
void boot_tusb_appclass_force_link(void);

#define BOOT_ADB_USB_TASK_STACK_WORDS 1024

static StackType_t boot_adb_usb_task_stack[BOOT_ADB_USB_TASK_STACK_WORDS];
static StaticTask_t boot_adb_usb_task_tcb;

static bool boot_adb_backend_tinyusb_usb_stack_init(void)
{
    printk("boot adb: usb-stack-init backend=tinyusb inited=%d\n", tud_inited());
    boot_tusb_appclass_force_link();
    return tusb_init();
}

static void boot_adb_backend_tinyusb_usb_task(void *arg)
{
    (void)arg;

    for (;;) {
        boot_watchdog_feed();
        tud_task();
    }
}

static bool boot_adb_backend_tinyusb_usb_task_start(void)
{
#if defined(CONFIG_FREERTOS_STATIC_ALLOCATION_ENABLE)
    TaskHandle_t task = xTaskCreateStatic(
        boot_adb_backend_tinyusb_usb_task,
        "boot_adb_usb",
        BOOT_ADB_USB_TASK_STACK_WORDS,
        NULL,
        CONFIG_ADB_TASK_PRIORITY,
        boot_adb_usb_task_stack,
        &boot_adb_usb_task_tcb);

    return task != NULL;
#else
    BaseType_t created = xTaskCreate(
        boot_adb_backend_tinyusb_usb_task,
        "boot_adb_usb",
        BOOT_ADB_USB_TASK_STACK_WORDS,
        NULL,
        CONFIG_ADB_TASK_PRIORITY,
        NULL);

    return created == pdPASS;
#endif
}

static void boot_adb_backend_tinyusb_soft_disconnect(void)
{
    tud_disconnect();
}

static void boot_adb_backend_tinyusb_soft_connect(void)
{
    tud_connect();
}

bool boot_adb_backend_bind_runtime_ops(boot_adb_runtime_ops_t *ops)
{
    if (ops == NULL) {
        return false;
    }

    ops->usb_stack_init = boot_adb_backend_tinyusb_usb_stack_init;
    ops->usb_task_start = boot_adb_backend_tinyusb_usb_task_start;
    ops->soft_disconnect = boot_adb_backend_tinyusb_soft_disconnect;
    ops->soft_connect = boot_adb_backend_tinyusb_soft_connect;
    return true;
}
