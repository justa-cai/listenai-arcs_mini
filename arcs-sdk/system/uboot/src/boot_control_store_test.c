#include "boot_control_store.h"
#include "boot_control_store_test.h"

#include <string.h>

static boot_ota_request_record_t g_record;
static int g_has_record;
static boot_mode_t g_mode;
static int g_sequence;
static int g_save_sequence;
static int g_mode_sequence;
static uboot_ota_failure_info_t g_failure;

static void boot_control_store_test_set_failure_none(uboot_ota_failure_info_t *info)
{
    memset(info, 0, sizeof(*info));
    info->reason = UBOOT_OTA_FAILURE_NONE;
    info->detail = UBOOT_OTA_FAILURE_DETAIL_NONE;
}

void boot_control_store_test_reset(void)
{
    memset(&g_record, 0, sizeof(g_record));
    g_has_record = 0;
    g_mode = BOOT_MODE_NORMAL;
    g_sequence = 0;
    g_save_sequence = 0;
    g_mode_sequence = 0;
    boot_control_store_test_set_failure_none(&g_failure);
}

boot_mode_t boot_control_store_test_get_mode(void)
{
    return g_mode;
}

int boot_control_store_test_get_save_sequence(void)
{
    return g_save_sequence;
}

int boot_control_store_test_get_mode_sequence(void)
{
    return g_mode_sequence;
}

int boot_control_store_test_get_failure(uboot_ota_failure_info_t *info)
{
    return boot_control_store_get_failure(info);
}

int boot_control_store_save(const boot_ota_request_record_t *record)
{
    if (record == NULL) {
        return -1;
    }

    g_record = *record;
    g_has_record = 1;
    g_save_sequence = ++g_sequence;
    return 0;
}

int boot_control_store_load(boot_ota_request_record_t *record)
{
    if (record == NULL || !g_has_record) {
        return -1;
    }

    *record = g_record;
    return 0;
}

int boot_control_store_clear(void)
{
    memset(&g_record, 0, sizeof(g_record));
    g_has_record = 0;
    return 0;
}

int boot_control_store_get_mode(boot_mode_t *mode)
{
    if (mode == NULL) {
        return -1;
    }

    *mode = g_mode;
    return 0;
}

int boot_control_store_set_mode(boot_mode_t mode)
{
    g_mode = mode;
    g_mode_sequence = ++g_sequence;
    return 0;
}

int boot_control_store_get_failure(uboot_ota_failure_info_t *info)
{
    if (info == NULL) {
        return -1;
    }

    *info = g_failure;
    return 0;
}

int boot_control_store_set_failure(const uboot_ota_failure_info_t *info)
{
    if (info == NULL) {
        return -1;
    }

    g_failure = *info;
    return 0;
}

int boot_control_store_clear_failure(void)
{
    boot_control_store_test_set_failure_none(&g_failure);
    return 0;
}
