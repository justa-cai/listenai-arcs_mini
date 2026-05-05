#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "boot_adb_runtime.h"
#include "boot_adb_storage.h"
#include "boot_config.h"

#define TEST_ASSERT_EQUAL_INT(expected, actual) TEST_ASSERT_EQUAL_UINT32((uint32_t)(expected), (uint32_t)(actual))

static char g_call_order[64];
static size_t g_call_order_len;
static bool g_storage_prepare_result;
static bool g_adb_init_result;
static bool g_sync_init_result;
static bool g_tusb_init_result;
static bool g_usb_task_start_result;
static char g_stage_order[64];
static size_t g_stage_order_len;

static char g_storage_call_order[64];
static size_t g_storage_call_order_len;
static int g_sdmmc_init_result;
static int g_disk_init_result;
static int g_lsfs_init_result;
static int g_mount_sd_result;
static int g_mkdir_result;
static bool g_root_created;
static boot_adb_storage_path_state_t g_root_state_before_create;
static boot_adb_storage_path_state_t g_root_state_after_create;

static uint32_t make_boot_info_raw(uint8_t recover_reason)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.recover_reason = recover_reason;
    return bits.raw;
}

static void record_call(char c)
{
    g_call_order[g_call_order_len++] = c;
    g_call_order[g_call_order_len] = '\0';
}

static void record_storage_call(char c)
{
    g_storage_call_order[g_storage_call_order_len++] = c;
    g_storage_call_order[g_storage_call_order_len] = '\0';
}

static void record_stage_call(char c)
{
    g_stage_order[g_stage_order_len++] = c;
    g_stage_order[g_stage_order_len] = '\0';
}

static void stub_trace_stage(boot_adb_runtime_stage_t stage)
{
    switch (stage) {
    case BOOT_ADB_RUNTIME_STAGE_ENABLE_USB_CLOCK:
        record_stage_call('c');
        break;
    case BOOT_ADB_RUNTIME_STAGE_STORAGE_PREPARE:
        record_stage_call('p');
        break;
    case BOOT_ADB_RUNTIME_STAGE_ADB_INIT:
        record_stage_call('a');
        break;
    case BOOT_ADB_RUNTIME_STAGE_SHELL_INIT:
        record_stage_call('s');
        break;
    case BOOT_ADB_RUNTIME_STAGE_SYNC_INIT:
        record_stage_call('y');
        break;
    case BOOT_ADB_RUNTIME_STAGE_USB_DISCONNECT:
        record_stage_call('d');
        break;
    case BOOT_ADB_RUNTIME_STAGE_USB_STACK_INIT:
        record_stage_call('t');
        break;
    case BOOT_ADB_RUNTIME_STAGE_USB_TASK_START:
        record_stage_call('u');
        break;
    case BOOT_ADB_RUNTIME_STAGE_USB_CONNECT:
        record_stage_call('n');
        break;
    case BOOT_ADB_RUNTIME_STAGE_READY:
        record_stage_call('r');
        break;
    default:
        record_stage_call('?');
        break;
    }
}

static void stub_enable_usb_clock(void)
{
    record_call('c');
}

static bool stub_storage_prepare(void)
{
    record_call('p');
    return g_storage_prepare_result;
}

static bool stub_adb_init(void)
{
    record_call('a');
    return g_adb_init_result;
}

static void stub_shell_init(void)
{
    record_call('s');
}

static bool stub_sync_init(void)
{
    record_call('y');
    return g_sync_init_result;
}

static bool stub_tusb_init(void)
{
    record_call('t');
    return g_tusb_init_result;
}

static bool stub_usb_task_start(void)
{
    record_call('u');
    return g_usb_task_start_result;
}

static void stub_soft_disconnect(void)
{
    record_call('d');
}

static void stub_soft_connect(void)
{
    record_call('n');
}

static int stub_sdmmc_init(void)
{
    record_storage_call('h');
    return g_sdmmc_init_result;
}

static int stub_disk_init(const void *dev)
{
    if (dev != NULL) {
        return -99;
    }
    record_storage_call('d');
    return g_disk_init_result;
}

static int stub_lsfs_init(void)
{
    record_storage_call('i');
    return g_lsfs_init_result;
}

static int stub_mount_sd(void)
{
    record_storage_call('m');
    return g_mount_sd_result;
}

static boot_adb_storage_path_state_t stub_root_state(const char *path)
{
    if (strcmp(path, BOOT_ADB_STORAGE_DEFAULT_ROOT) != 0) {
        return BOOT_ADB_STORAGE_PATH_ERROR;
    }
    record_storage_call('p');
    return g_root_created ? g_root_state_after_create : g_root_state_before_create;
}

static int stub_mkdir(const char *path)
{
    if (strcmp(path, BOOT_ADB_STORAGE_DEFAULT_ROOT) != 0) {
        return -99;
    }
    record_storage_call('k');
    if (g_mkdir_result == 0) {
        g_root_created = true;
    }
    return g_mkdir_result;
}

static boot_adb_storage_ops_t make_storage_ops(const char *default_root)
{
    boot_adb_storage_ops_t ops = {
        .default_root = default_root,
        .sdmmc_init = stub_sdmmc_init,
        .disk_init = stub_disk_init,
        .lsfs_init = stub_lsfs_init,
        .mount_sd = stub_mount_sd,
        .root_state = stub_root_state,
        .mkdir = stub_mkdir,
    };

    return ops;
}

static boot_adb_runtime_ops_t make_ops(bool sync_enabled)
{
    boot_adb_runtime_ops_t ops = {
        .sync_enabled = sync_enabled,
        .trace_stage = stub_trace_stage,
        .enable_usb_clock = stub_enable_usb_clock,
        .adb_init = stub_adb_init,
        .usb_stack_init = stub_tusb_init,
        .usb_task_start = stub_usb_task_start,
        .soft_disconnect = stub_soft_disconnect,
        .soft_connect = stub_soft_connect,
    };

    if (sync_enabled) {
        ops.storage_prepare = stub_storage_prepare;
        ops.sync_init = stub_sync_init;
    }

    return ops;
}

void setUp(void)
{
    memset(g_call_order, 0, sizeof(g_call_order));
    g_call_order_len = 0;
    g_storage_prepare_result = true;
    g_adb_init_result = true;
    g_sync_init_result = true;
    g_tusb_init_result = true;
    g_usb_task_start_result = true;
    memset(g_stage_order, 0, sizeof(g_stage_order));
    g_stage_order_len = 0;

    memset(g_storage_call_order, 0, sizeof(g_storage_call_order));
    g_storage_call_order_len = 0;
    g_sdmmc_init_result = 0;
    g_disk_init_result = 0;
    g_lsfs_init_result = 0;
    g_mount_sd_result = 0;
    g_mkdir_result = 0;
    g_root_created = false;
    g_root_state_before_create = BOOT_ADB_STORAGE_PATH_DIR;
    g_root_state_after_create = BOOT_ADB_STORAGE_PATH_DIR;
}

void tearDown(void)
{
}

void test_bringup_without_sync_keeps_transport_only_order(void)
{
    const boot_adb_runtime_ops_t ops = make_ops(false);

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_STARTED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "cadtun") == 0);
}

void test_bringup_without_sync_still_initializes_shell_before_usb_connect(void)
{
    boot_adb_runtime_ops_t ops = make_ops(false);

    ops.shell_init = stub_shell_init;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_STARTED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "casdtun") == 0);
}

void test_bringup_with_sync_prepares_storage_and_registers_sync_before_usb_connect(void)
{
    boot_adb_runtime_ops_t ops = make_ops(true);

    ops.shell_init = stub_shell_init;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_STARTED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "cpasydtun") == 0);
}

void test_bringup_without_sync_traces_shell_only_stages(void)
{
    boot_adb_runtime_ops_t ops = make_ops(false);

    ops.shell_init = stub_shell_init;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_STARTED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_stage_order, "casdtunr") == 0);
}

void test_bringup_failure_traces_last_stage_before_usb_task_error(void)
{
    boot_adb_runtime_ops_t ops = make_ops(true);

    ops.shell_init = stub_shell_init;
    g_usb_task_start_result = false;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_USB_TASK_START_FAILED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_stage_order, "cpasydtu") == 0);
}

void test_bringup_allows_sync_mode_without_storage_prepare_hook(void)
{
    boot_adb_runtime_ops_t ops = make_ops(true);

    ops.storage_prepare = NULL;
    ops.shell_init = stub_shell_init;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_STARTED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "casydtun") == 0);
    TEST_ASSERT_TRUE(strcmp(g_stage_order, "casydtunr") == 0);
}

void test_bringup_rejects_sync_mode_without_sync_init_hook(void)
{
    boot_adb_runtime_ops_t ops = make_ops(true);

    ops.sync_init = NULL;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_INVALID_OPS,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "") == 0);
}

void test_bringup_fails_when_storage_prepare_fails(void)
{
    const boot_adb_runtime_ops_t ops = make_ops(true);

    g_storage_prepare_result = false;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_STORAGE_PREPARE_FAILED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "cp") == 0);
}

void test_bringup_fails_when_adb_init_fails(void)
{
    const boot_adb_runtime_ops_t ops = make_ops(false);

    g_adb_init_result = false;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_ADB_INIT_FAILED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "ca") == 0);
}

void test_bringup_fails_when_sync_init_fails(void)
{
    const boot_adb_runtime_ops_t ops = make_ops(true);

    g_sync_init_result = false;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_SYNC_INIT_FAILED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "cpay") == 0);
}

void test_bringup_fails_when_usb_stack_init_fails(void)
{
    const boot_adb_runtime_ops_t ops = make_ops(true);

    g_tusb_init_result = false;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_USB_STACK_INIT_FAILED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "cpaydt") == 0);
}

void test_bringup_fails_when_usb_task_start_fails(void)
{
    const boot_adb_runtime_ops_t ops = make_ops(true);

    g_usb_task_start_result = false;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_RUNTIME_RESULT_USB_TASK_START_FAILED,
                          boot_adb_runtime_bringup(&ops));
    TEST_ASSERT_TRUE(strcmp(g_call_order, "cpaydtu") == 0);
}

void test_reason_string_reports_explicit_and_unknown_reasons(void)
{
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_reason_string(
                                make_boot_info_raw(RECOVER_REASON_SOFT_REQ)),
                            "soft req") == 0);
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_reason_string(
                                make_boot_info_raw(UINT8_MAX)),
                            "unknown") == 0);
}

void test_result_string_reports_new_sync_failure_stages(void)
{
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_result_string(
                                BOOT_ADB_RUNTIME_RESULT_STORAGE_PREPARE_FAILED),
                            "storage-prepare") == 0);
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_result_string(
                                BOOT_ADB_RUNTIME_RESULT_SYNC_INIT_FAILED),
                            "sync-init") == 0);
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_result_string(
                                BOOT_ADB_RUNTIME_RESULT_USB_STACK_INIT_FAILED),
                            "usb-stack-init") == 0);
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_result_string(
                                BOOT_ADB_RUNTIME_RESULT_USB_TASK_START_FAILED),
                            "usb-task") == 0);
}

void test_stage_string_reports_machine_readable_runtime_stages(void)
{
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_stage_string(
                                BOOT_ADB_RUNTIME_STAGE_SHELL_INIT),
                            "shell-init") == 0);
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_stage_string(
                                BOOT_ADB_RUNTIME_STAGE_USB_STACK_INIT),
                            "usb-stack-init") == 0);
    TEST_ASSERT_TRUE(strcmp(boot_adb_runtime_stage_string(
                                BOOT_ADB_RUNTIME_STAGE_USB_TASK_START),
                            "usb-task-start") == 0);
}

void test_storage_prepare_mounts_sd_and_creates_default_root_when_missing(void)
{
    const boot_adb_storage_ops_t ops = make_storage_ops(BOOT_ADB_STORAGE_DEFAULT_ROOT);

    g_root_state_before_create = BOOT_ADB_STORAGE_PATH_MISSING;
    g_root_state_after_create = BOOT_ADB_STORAGE_PATH_DIR;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_STORAGE_RESULT_READY,
                          boot_adb_storage_prepare(&ops));
    TEST_ASSERT_TRUE(strcmp(g_storage_call_order, "hdimpkp") == 0);
}

void test_storage_prepare_skips_mkdir_when_default_root_exists(void)
{
    const boot_adb_storage_ops_t ops = make_storage_ops(BOOT_ADB_STORAGE_DEFAULT_ROOT);

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_STORAGE_RESULT_READY,
                          boot_adb_storage_prepare(&ops));
    TEST_ASSERT_TRUE(strcmp(g_storage_call_order, "hdimp") == 0);
}

void test_storage_prepare_rejects_non_sd_default_root(void)
{
    const boot_adb_storage_ops_t ops = make_storage_ops("/RAM:/adb/");

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_STORAGE_RESULT_INVALID_ROOT,
                          boot_adb_storage_prepare(&ops));
    TEST_ASSERT_TRUE(strcmp(g_storage_call_order, "") == 0);
}

void test_storage_prepare_fails_when_sdmmc_init_fails(void)
{
    const boot_adb_storage_ops_t ops = make_storage_ops(BOOT_ADB_STORAGE_DEFAULT_ROOT);

    g_sdmmc_init_result = -1;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_STORAGE_RESULT_SDMMC_INIT_FAILED,
                          boot_adb_storage_prepare(&ops));
    TEST_ASSERT_TRUE(strcmp(g_storage_call_order, "h") == 0);
}

void test_storage_prepare_fails_when_mount_fails(void)
{
    const boot_adb_storage_ops_t ops = make_storage_ops(BOOT_ADB_STORAGE_DEFAULT_ROOT);

    g_mount_sd_result = -1;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_STORAGE_RESULT_MOUNT_FAILED,
                          boot_adb_storage_prepare(&ops));
    TEST_ASSERT_TRUE(strcmp(g_storage_call_order, "hdim") == 0);
}

void test_storage_prepare_fails_when_root_create_fails(void)
{
    const boot_adb_storage_ops_t ops = make_storage_ops(BOOT_ADB_STORAGE_DEFAULT_ROOT);

    g_root_state_before_create = BOOT_ADB_STORAGE_PATH_MISSING;
    g_mkdir_result = -1;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_STORAGE_RESULT_ROOT_CREATE_FAILED,
                          boot_adb_storage_prepare(&ops));
    TEST_ASSERT_TRUE(strcmp(g_storage_call_order, "hdimpk") == 0);
}

void test_storage_prepare_fails_when_root_is_not_a_directory(void)
{
    const boot_adb_storage_ops_t ops = make_storage_ops(BOOT_ADB_STORAGE_DEFAULT_ROOT);

    g_root_state_before_create = BOOT_ADB_STORAGE_PATH_OTHER;

    TEST_ASSERT_EQUAL_INT(BOOT_ADB_STORAGE_RESULT_ROOT_VERIFY_FAILED,
                          boot_adb_storage_prepare(&ops));
    TEST_ASSERT_TRUE(strcmp(g_storage_call_order, "hdimp") == 0);
}

void test_storage_result_string_reports_mount_and_root_stages(void)
{
    TEST_ASSERT_TRUE(strcmp(boot_adb_storage_result_string(
                                BOOT_ADB_STORAGE_RESULT_MOUNT_FAILED),
                            "mount") == 0);
    TEST_ASSERT_TRUE(strcmp(boot_adb_storage_result_string(
                                BOOT_ADB_STORAGE_RESULT_ROOT_VERIFY_FAILED),
                            "root-verify") == 0);
}

int main(void)
{
    UnityBegin("system/uboot/test/boot_adb_runtime/test_boot_adb_runtime.c");

    RUN_TEST(test_bringup_without_sync_keeps_transport_only_order, __LINE__);
    RUN_TEST(test_bringup_without_sync_still_initializes_shell_before_usb_connect, __LINE__);
    RUN_TEST(test_bringup_with_sync_prepares_storage_and_registers_sync_before_usb_connect, __LINE__);
    RUN_TEST(test_bringup_without_sync_traces_shell_only_stages, __LINE__);
    RUN_TEST(test_bringup_failure_traces_last_stage_before_usb_task_error, __LINE__);
    RUN_TEST(test_bringup_allows_sync_mode_without_storage_prepare_hook, __LINE__);
    RUN_TEST(test_bringup_rejects_sync_mode_without_sync_init_hook, __LINE__);
    RUN_TEST(test_bringup_fails_when_storage_prepare_fails, __LINE__);
    RUN_TEST(test_bringup_fails_when_adb_init_fails, __LINE__);
    RUN_TEST(test_bringup_fails_when_sync_init_fails, __LINE__);
    RUN_TEST(test_bringup_fails_when_usb_stack_init_fails, __LINE__);
    RUN_TEST(test_bringup_fails_when_usb_task_start_fails, __LINE__);
    RUN_TEST(test_reason_string_reports_explicit_and_unknown_reasons, __LINE__);
    RUN_TEST(test_result_string_reports_new_sync_failure_stages, __LINE__);
    RUN_TEST(test_stage_string_reports_machine_readable_runtime_stages, __LINE__);
    RUN_TEST(test_storage_prepare_mounts_sd_and_creates_default_root_when_missing, __LINE__);
    RUN_TEST(test_storage_prepare_skips_mkdir_when_default_root_exists, __LINE__);
    RUN_TEST(test_storage_prepare_rejects_non_sd_default_root, __LINE__);
    RUN_TEST(test_storage_prepare_fails_when_sdmmc_init_fails, __LINE__);
    RUN_TEST(test_storage_prepare_fails_when_mount_fails, __LINE__);
    RUN_TEST(test_storage_prepare_fails_when_root_create_fails, __LINE__);
    RUN_TEST(test_storage_prepare_fails_when_root_is_not_a_directory, __LINE__);
    RUN_TEST(test_storage_result_string_reports_mount_and_root_stages, __LINE__);

    return UnityEnd();
}
