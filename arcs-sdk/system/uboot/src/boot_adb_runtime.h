#ifndef __BOOT_ADB_RUNTIME_H__
#define __BOOT_ADB_RUNTIME_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BOOT_ADB_RUNTIME_RESULT_STARTED = 0,
    BOOT_ADB_RUNTIME_RESULT_STORAGE_PREPARE_FAILED = -1,
    BOOT_ADB_RUNTIME_RESULT_ADB_INIT_FAILED = -2,
    BOOT_ADB_RUNTIME_RESULT_SYNC_INIT_FAILED = -3,
    BOOT_ADB_RUNTIME_RESULT_USB_STACK_INIT_FAILED = -4,
    BOOT_ADB_RUNTIME_RESULT_USB_TASK_START_FAILED = -5,
    BOOT_ADB_RUNTIME_RESULT_INVALID_OPS = -6,
} boot_adb_runtime_result_t;

typedef enum {
    BOOT_ADB_RUNTIME_STAGE_ENABLE_USB_CLOCK = 0,
    BOOT_ADB_RUNTIME_STAGE_STORAGE_PREPARE,
    BOOT_ADB_RUNTIME_STAGE_ADB_INIT,
    BOOT_ADB_RUNTIME_STAGE_SHELL_INIT,
    BOOT_ADB_RUNTIME_STAGE_SYNC_INIT,
    BOOT_ADB_RUNTIME_STAGE_USB_DISCONNECT,
    BOOT_ADB_RUNTIME_STAGE_USB_STACK_INIT,
    BOOT_ADB_RUNTIME_STAGE_USB_TASK_START,
    BOOT_ADB_RUNTIME_STAGE_USB_CONNECT,
    BOOT_ADB_RUNTIME_STAGE_READY,
} boot_adb_runtime_stage_t;

typedef struct {
    bool sync_enabled;
    void (*trace_stage)(boot_adb_runtime_stage_t stage);
    void (*enable_usb_clock)(void);
    /* Optional: prepare BOOT_ADB_STORAGE_DEFAULT_ROOT before sync starts. */
    bool (*storage_prepare)(void);
    bool (*adb_init)(void);
    void (*shell_init)(void);
    bool (*sync_init)(void);
    bool (*usb_stack_init)(void);
    bool (*usb_task_start)(void);
    void (*soft_disconnect)(void);
    void (*soft_connect)(void);
} boot_adb_runtime_ops_t;

boot_adb_runtime_result_t boot_adb_runtime_bringup(const boot_adb_runtime_ops_t *ops);
const char *boot_adb_runtime_result_string(boot_adb_runtime_result_t result);
const char *boot_adb_runtime_stage_string(boot_adb_runtime_stage_t stage);
const char *boot_adb_runtime_reason_string(uint32_t boot_info_raw);
void boot_adb_runtime_log_skip(uint32_t boot_info_raw);

#ifndef BOOT_ADB_RUNTIME_UNIT_TEST
boot_adb_runtime_result_t boot_adb_runtime_start(uint32_t boot_info_raw);
#endif

#endif /* __BOOT_ADB_RUNTIME_H__ */
