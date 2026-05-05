#include "boot_adb_runtime.h"
#include "boot_adb_backend.h"
#include "boot_adb_storage.h"

#include "boot_config.h"

#ifndef BOOT_ADB_RUNTIME_UNIT_TEST
#include "adb.h"
#if CONFIG_ADB_SHELL
#include "adb_shell.h"
#endif
#if CONFIG_ADB_SYNC
#include "adb_sync.h"
#endif
#include "arcs_ap.h"
#include "boot_recovery_adb_gate.h"
#include "syslog.h"
#endif

static const char *boot_adb_runtime_reason_from_value(uint8_t recover_reason)
{
    switch (recover_reason) {
    case RECOVER_REASON_NONE:
        return "none";
    case RECOVER_REASON_SOFT_REQ:
        return "soft req";
    case RECOVER_REASON_HARD_REQ:
        return "hard req";
    case RECOVER_REASON_APP_INVALID:
        return "app invalid";
    case RECOVER_REASON_AP_WDT_TIMEOUT:
        return "ap wdt timeout";
    case RECOVER_REASON_BOOT_WDT_TIMEOUT:
        return "boot wdt timeout";
    default:
        return "unknown";
    }
}

const char *boot_adb_runtime_reason_string(uint32_t boot_info_raw)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {
        .raw = boot_info_raw,
    };

    return boot_adb_runtime_reason_from_value(bits.info.recover_reason);
}

const char *boot_adb_runtime_result_string(boot_adb_runtime_result_t result)
{
    switch (result) {
    case BOOT_ADB_RUNTIME_RESULT_STARTED:
        return "started";
    case BOOT_ADB_RUNTIME_RESULT_STORAGE_PREPARE_FAILED:
        return "storage-prepare";
    case BOOT_ADB_RUNTIME_RESULT_ADB_INIT_FAILED:
        return "adb-init";
    case BOOT_ADB_RUNTIME_RESULT_SYNC_INIT_FAILED:
        return "sync-init";
    case BOOT_ADB_RUNTIME_RESULT_USB_STACK_INIT_FAILED:
        return "usb-stack-init";
    case BOOT_ADB_RUNTIME_RESULT_USB_TASK_START_FAILED:
        return "usb-task";
    case BOOT_ADB_RUNTIME_RESULT_INVALID_OPS:
        return "invalid-ops";
    default:
        return "unknown";
    }
}

const char *boot_adb_runtime_stage_string(boot_adb_runtime_stage_t stage)
{
    switch (stage) {
    case BOOT_ADB_RUNTIME_STAGE_ENABLE_USB_CLOCK:
        return "enable-usb-clock";
    case BOOT_ADB_RUNTIME_STAGE_STORAGE_PREPARE:
        return "storage-prepare";
    case BOOT_ADB_RUNTIME_STAGE_ADB_INIT:
        return "adb-init";
    case BOOT_ADB_RUNTIME_STAGE_SHELL_INIT:
        return "shell-init";
    case BOOT_ADB_RUNTIME_STAGE_SYNC_INIT:
        return "sync-init";
    case BOOT_ADB_RUNTIME_STAGE_USB_DISCONNECT:
        return "usb-disconnect";
    case BOOT_ADB_RUNTIME_STAGE_USB_STACK_INIT:
        return "usb-stack-init";
    case BOOT_ADB_RUNTIME_STAGE_USB_TASK_START:
        return "usb-task-start";
    case BOOT_ADB_RUNTIME_STAGE_USB_CONNECT:
        return "usb-connect";
    case BOOT_ADB_RUNTIME_STAGE_READY:
        return "ready";
    default:
        return "unknown";
    }
}

static void boot_adb_runtime_trace_stage(const boot_adb_runtime_ops_t *ops,
                                         boot_adb_runtime_stage_t stage)
{
    if (ops->trace_stage != NULL) {
        ops->trace_stage(stage);
    }
}

boot_adb_runtime_result_t boot_adb_runtime_bringup(const boot_adb_runtime_ops_t *ops)
{
    if (ops == NULL || ops->enable_usb_clock == NULL || ops->adb_init == NULL ||
        ops->usb_stack_init == NULL || ops->usb_task_start == NULL ||
        ops->soft_disconnect == NULL || ops->soft_connect == NULL) {
        return BOOT_ADB_RUNTIME_RESULT_INVALID_OPS;
    }

    if (ops->sync_enabled && ops->sync_init == NULL) {
        return BOOT_ADB_RUNTIME_RESULT_INVALID_OPS;
    }

    boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_ENABLE_USB_CLOCK);
    ops->enable_usb_clock();

    if (ops->sync_enabled && ops->storage_prepare != NULL) {
        boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_STORAGE_PREPARE);
        if (!ops->storage_prepare()) {
            return BOOT_ADB_RUNTIME_RESULT_STORAGE_PREPARE_FAILED;
        }
    }

    boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_ADB_INIT);
    if (!ops->adb_init()) {
        return BOOT_ADB_RUNTIME_RESULT_ADB_INIT_FAILED;
    }

    if (ops->shell_init != NULL) {
        boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_SHELL_INIT);
        ops->shell_init();
    }

    if (ops->sync_enabled) {
        boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_SYNC_INIT);
        if (!ops->sync_init()) {
            return BOOT_ADB_RUNTIME_RESULT_SYNC_INIT_FAILED;
        }
    }

    boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_USB_DISCONNECT);
    ops->soft_disconnect();

    boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_USB_STACK_INIT);
    if (!ops->usb_stack_init()) {
        return BOOT_ADB_RUNTIME_RESULT_USB_STACK_INIT_FAILED;
    }

    boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_USB_TASK_START);
    if (!ops->usb_task_start()) {
        return BOOT_ADB_RUNTIME_RESULT_USB_TASK_START_FAILED;
    }

    boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_USB_CONNECT);
    ops->soft_connect();
    boot_adb_runtime_trace_stage(ops, BOOT_ADB_RUNTIME_STAGE_READY);
    return BOOT_ADB_RUNTIME_RESULT_STARTED;
}

#ifndef BOOT_ADB_RUNTIME_UNIT_TEST

static bool boot_adb_runtime_started;

static void boot_adb_runtime_enable_usb_clock(void)
{
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
}

#if CONFIG_ADB_SYNC && defined(CONFIG_BOOT_ADB_SDMMC_FS)
static bool boot_adb_runtime_storage_prepare_wrapper(void)
{
    boot_adb_storage_result_t result;

    printk("boot adb: storage-prepare start root=%s\n",
           BOOT_ADB_STORAGE_DEFAULT_ROOT);

    result = boot_adb_storage_prepare_default();

    if (result != BOOT_ADB_STORAGE_RESULT_READY) {
        printk("boot adb: storage-prepare fail stage=%s root=%s\n",
               boot_adb_storage_result_string(result),
               BOOT_ADB_STORAGE_DEFAULT_ROOT);
        return false;
    }

    printk("boot adb: mount ready path=%s\n", BOOT_ADB_STORAGE_MOUNT_POINT);
    printk("boot adb: storage-prepare ready root=%s\n",
           BOOT_ADB_STORAGE_DEFAULT_ROOT);
    return true;
}
#endif

static bool boot_adb_runtime_adb_init_wrapper(void)
{
    adb_init();
    return true;
}

#if CONFIG_ADB_SHELL
static void boot_adb_runtime_shell_init_wrapper(void)
{
    adb_shell_init();
}
#endif

#if CONFIG_ADB_SYNC
static bool boot_adb_runtime_sync_init_wrapper(void)
{
    int ret;

#if defined(CONFIG_BOOT_ADB_SDMMC_FS)
    printk("boot adb: sync-init start root=%s\n",
           BOOT_ADB_STORAGE_DEFAULT_ROOT);
#else
    printk("boot adb: sync-init start raw-only\n");
#endif
    ret = adb_sync_init();

    if (ret != 0) {
#if defined(CONFIG_BOOT_ADB_SDMMC_FS)
        printk("boot adb: sync-init fail ret=%d root=%s\n",
               ret,
               BOOT_ADB_STORAGE_DEFAULT_ROOT);
#else
        printk("boot adb: sync-init fail ret=%d raw-only\n", ret);
#endif
        return false;
    }

#if defined(CONFIG_BOOT_ADB_SDMMC_FS)
    printk("boot adb: sync-init ready root=%s\n",
           BOOT_ADB_STORAGE_DEFAULT_ROOT);
#else
    printk("boot adb: sync-init ready raw-only\n");
#endif
    return true;
}
#endif

static void boot_adb_runtime_trace_stage_wrapper(boot_adb_runtime_stage_t stage)
{
    printk("boot adb: stage=%s\n", boot_adb_runtime_stage_string(stage));
}

static bool boot_adb_runtime_build_default_ops(boot_adb_runtime_ops_t *ops)
{
    if (ops == NULL) {
        return false;
    }

    *ops = (boot_adb_runtime_ops_t){
        .trace_stage = boot_adb_runtime_trace_stage_wrapper,
        .enable_usb_clock = boot_adb_runtime_enable_usb_clock,
        .adb_init = boot_adb_runtime_adb_init_wrapper,
#if CONFIG_ADB_SHELL
        .shell_init = boot_adb_runtime_shell_init_wrapper,
#endif
    };

#if CONFIG_ADB_SYNC
    ops->sync_enabled = true;
#if defined(CONFIG_BOOT_ADB_SDMMC_FS)
    ops->storage_prepare = boot_adb_runtime_storage_prepare_wrapper;
#endif
    ops->sync_init = boot_adb_runtime_sync_init_wrapper;
#endif

    return boot_adb_backend_bind_runtime_ops(ops);
}

void boot_adb_runtime_log_skip(uint32_t boot_info_raw)
{
    printk("boot adb: skip reason=%s raw=0x%08x\n",
           boot_adb_runtime_reason_string(boot_info_raw), boot_info_raw);
}

boot_adb_runtime_result_t boot_adb_runtime_start(uint32_t boot_info_raw)
{
    boot_adb_runtime_result_t result;
    boot_adb_runtime_ops_t ops;
#if CONFIG_ADB_SYNC
    int sync_prepare_ret;
#endif

    if (!boot_recovery_adb_should_start(boot_info_raw)) {
        boot_adb_runtime_log_skip(boot_info_raw);
        return BOOT_ADB_RUNTIME_RESULT_INVALID_OPS;
    }

    if (boot_adb_runtime_started) {
        printk("boot adb: start reason=%s raw=0x%08x state=already-started\n",
               boot_adb_runtime_reason_string(boot_info_raw), boot_info_raw);
        return BOOT_ADB_RUNTIME_RESULT_STARTED;
    }

    printk("boot adb: start reason=%s raw=0x%08x\n",
           boot_adb_runtime_reason_string(boot_info_raw), boot_info_raw);

    if (!boot_adb_runtime_build_default_ops(&ops)) {
        return BOOT_ADB_RUNTIME_RESULT_INVALID_OPS;
    }

#if CONFIG_ADB_SYNC
    sync_prepare_ret = adb_sync_prepare();
    if (sync_prepare_ret != 0) {
        printk("boot adb: sync-prepare reserve unavailable ret=%d\n",
               sync_prepare_ret);
    }
#endif

    result = boot_adb_runtime_bringup(&ops);
    if (result != BOOT_ADB_RUNTIME_RESULT_STARTED) {
        printk("boot adb: init-fail stage=%s reason=%s raw=0x%08x\n",
               boot_adb_runtime_result_string(result),
               boot_adb_runtime_reason_string(boot_info_raw),
               boot_info_raw);
        return result;
    }

    boot_adb_runtime_started = true;
    return result;
}

#else

void boot_adb_runtime_log_skip(uint32_t boot_info_raw)
{
    (void)boot_info_raw;
}

#endif
