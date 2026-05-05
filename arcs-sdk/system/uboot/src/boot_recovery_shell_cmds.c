#include "shell.h"
#include "chip.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "PowerManager.h"
#include "boot_config.h"
#include "boot_flash.h"
#include "boot_md5.h"
#include "adb/adb_sync_ext_disk.h"

#define ROOT_CMD (1 << 0)

SHELL_EXPORT_USER(ROOT_CMD, root, listenai, root);

#ifdef BOOT_RECOVERY_SHELL_CMDS_UNIT_TEST
#define BOOT_RECOVERY_SHELL_TESTABLE
#else
#define BOOT_RECOVERY_SHELL_TESTABLE static
#endif

static uint8_t boot_upgrade_mode;
static uint32_t boot_handshake_ok;
static uint8_t boot_need_handshake;
static uint8_t boot_handshake_timeout_backup;

extern int boot_watchdog_feed(void);

bool boot_handshake_is_need(void)
{
    return boot_need_handshake != 0;
}

bool boot_handshake_is_ok(void)
{
    return boot_handshake_ok != 0;
}

void boot_handshake(bool ok)
{
    boot_handshake_ok = ok ? 1u : 0u;
}

static void boot_upgrade_mode_set(bool enter)
{
    boot_upgrade_mode = enter ? 1u : 0u;
}

uint8_t boot_upgrade_mode_get(void)
{
    return boot_upgrade_mode;
}

static bool boot_recovery_shell_arg_match(const char *arg, const char *full)
{
    if (arg == NULL || full == NULL || arg[0] == '\0') {
        return false;
    }

    return strncmp(full, arg, strlen(arg)) == 0;
}

BOOT_RECOVERY_SHELL_TESTABLE int boot_recovery_shell_upgrade_command(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc != 2) {
        shellPrint(sh, "Usage: upgrade [enter/exit/status]\r\n");
        return -1;
    }

    if (boot_recovery_shell_arg_match(argv[1], "enter")) {
        shellPrint(sh, "upgrade enter successfully\r\n");
        boot_upgrade_mode_set(true);
        return 0;
    }

    if (boot_recovery_shell_arg_match(argv[1], "exit")) {
        shellPrint(sh, "upgrade exit successfully\r\n");
        boot_upgrade_mode_set(false);
        return 0;
    }

    if (boot_recovery_shell_arg_match(argv[1], "status")) {
        shellPrint(sh, "upgrade status: %s\r\n",
                   boot_upgrade_mode_get() ? "enter" : "exit");
        return 0;
    }

    shellPrint(sh, "Usage: upgrade [enter/exit/status]\r\n");
    return -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(ROOT_CMD) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 upgrade, boot_recovery_shell_upgrade_command, upgrade boot);

static bool boot_recovery_shell_raw_disk_access(const char *path)
{
    return path != NULL && strncmp(path, "/RAW/", strlen("/RAW/")) == 0;
}

BOOT_RECOVERY_SHELL_TESTABLE int boot_recovery_shell_raw_disk_get_info_by_path(char *path, char **name,
                                                                                uint64_t *addr, uint64_t *size)
{
    char *disk_end;
    char *addr_token;
    char *size_token;
    char *endptr = NULL;

    if (path == NULL || name == NULL || addr == NULL || size == NULL) {
        return -1;
    }

    if (strncmp(path, "/RAW/", strlen("/RAW/")) != 0) {
        return -1;
    }

    *addr = 0;
    *size = 0;
    *name = path + strlen("/RAW/");
    if ((*name)[0] == '\0') {
        return -1;
    }

    disk_end = strchr(*name, '/');
    if (disk_end == NULL || disk_end == *name) {
        return -1;
    }
    *disk_end = '\0';

    addr_token = disk_end + 1;
    size_token = strchr(addr_token, '/');
    if (size_token == NULL || size_token == addr_token) {
        return -1;
    }
    *size_token = '\0';
    size_token++;
    if (size_token[0] == '\0' || strchr(size_token, '/') != NULL) {
        return -1;
    }

    *addr = strtoull(addr_token, &endptr, 16);
    if (endptr == addr_token || *endptr != '\0') {
        return -1;
    }

    *size = strtoull(size_token, &endptr, 16);
    if (endptr == size_token || *endptr != '\0' || *size == 0u) {
        return -1;
    }

    return 0;
}

BOOT_RECOVERY_SHELL_TESTABLE int boot_recovery_shell_info_command(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    struct adb_sync_raw_flash_stats raw_flash_stats;

    if (argc == 2 && boot_recovery_shell_arg_match(argv[1], "reason")) {
        shellPrint(sh, "reason: %d, handshake_timeout: %d\r\n",
                   info->recover_reason, boot_handshake_timeout_backup);
        return 0;
    }

    if (argc == 2 && boot_recovery_shell_arg_match(argv[1], "rawflash")) {
        adb_sync_raw_flash_stats_get(&raw_flash_stats);
        shellPrint(sh, "rawflash bytes=%llu total_ms=%u erase_ms=%u write_ms=%u\r\n",
                   (unsigned long long)raw_flash_stats.bytes,
                   raw_flash_stats.total_ms,
                   raw_flash_stats.erase_ms,
                   raw_flash_stats.write_ms);
        return 0;
    }

    if (argc == 2 && boot_recovery_shell_arg_match(argv[1], "sn")) {
        extern const char *device_id_str_get(void);
        shellPrint(sh, "serial num: %s\r\n", device_id_str_get());
        return 0;
    }

    if (argc == 2 && boot_recovery_shell_arg_match(argv[1], "flash")) {
        uint32_t size;
        int r = boot_flash_size_get(&size);
        if (r) {
            shellPrint(sh, "flash size get failed\r\n");
            return -1;
        }
        const char *unit = size >= (1 * 1024 * 1024) ? "MB" : "KB";
        uint32_t size_val = size >= (1 * 1024 * 1024) ? size / (1 * 1024 * 1024)
                                                      : size / 1024;
        shellPrint(sh, "size: %u%s\r\n", size_val, unit);

        struct flash_status_register *fl_status = boot_flash_status_register_get(0);
        if (fl_status) {
            shellPrint(sh, "startup status register: 0, 0x%04x, 0x%04x, 0x%04x\r\n",
                       fl_status->register_1, fl_status->register_2, fl_status->register_3);
        }
#if CONFIG_DUAL_FLASH
        fl_status = boot_flash_status_register_get(1);
        if (fl_status) {
            shellPrint(sh, "startup status register: 1, 0x%04x, 0x%04x, 0x%04x\r\n",
                       fl_status->register_1, fl_status->register_2, fl_status->register_3);
        }
#endif

        struct flash_status_register fl_sta;
        r = boot_flash_reg_get(0, 1, &fl_sta.register_1);
        r |= boot_flash_reg_get(0, 2, &fl_sta.register_2);
        r |= boot_flash_reg_get(0, 3, &fl_sta.register_3);
        if (r) {
            shellPrint(sh, "flash0 status register get failed\r\n");
            return -1;
        }
        shellPrint(sh, "current status register: 0, 0x%04x, 0x%04x, 0x%04x\r\n",
                   fl_sta.register_1, fl_sta.register_2, fl_sta.register_3);
#if CONFIG_DUAL_FLASH
        r = boot_flash_reg_get(1, 1, &fl_sta.register_1);
        r |= boot_flash_reg_get(1, 2, &fl_sta.register_2);
        r |= boot_flash_reg_get(1, 3, &fl_sta.register_3);
        if (r) {
            shellPrint(sh, "flash1 status register get failed\r\n");
            return -1;
        }
        shellPrint(sh, "current status register: 1, 0x%04x, 0x%04x, 0x%04x\r\n",
                   fl_sta.register_1, fl_sta.register_2, fl_sta.register_3);
#endif
        return 0;
    }

    shellPrint(sh, "Usage: info [reason|rawflash|sn|flash]\r\n");
    return -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 info, boot_recovery_shell_info_command, info boot);

static bool boot_recovery_shell_is_flash_disk(const char *name)
{
    return strcmp(name, "NAND") == 0 || strcmp(name, "FLASH") == 0;
}

static int boot_recovery_shell_md5_command(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();
    char *path_cpy;
    char *disk_name = NULL;
    uint64_t addr = 0;
    uint64_t size = 0;
    boot_md5_context_t md5_ctx;
    uint8_t md5_out[BOOT_MD5_DIGEST_SIZE];
    char md5_str[33];

    if (argc < 2) {
        shellPrint(sh, "Usage: md5sum </RAW/NAND/addr/size>\r\n");
        return -1;
    }

    path_cpy = malloc(strlen(argv[1]) + 1u);
    if (path_cpy == NULL) {
        shellPrint(sh, "malloc failed\r\n");
        return -1;
    }
    strcpy(path_cpy, argv[1]);

    if (!boot_recovery_shell_raw_disk_access(argv[1]) ||
        boot_recovery_shell_raw_disk_get_info_by_path(path_cpy, &disk_name, &addr, &size) != 0 ||
        !boot_recovery_shell_is_flash_disk(disk_name)) {
        shellPrint(sh, "Usage: md5sum </RAW/NAND/addr/size>\r\n");
        free(path_cpy);
        return -1;
    }

    uint32_t flash_size = 0;
    if (boot_flash_size_get(&flash_size) != 0 || flash_size == 0u) {
        shellPrint(sh, "flash size get failed\r\n");
        free(path_cpy);
        return -1;
    }

    if (addr >= flash_size || size > (uint64_t)flash_size - addr) {
        shellPrint(sh,
                   "range 0x%llx+0x%llx exceeds flash size 0x%x (addr/size are hex)\r\n",
                   (unsigned long long)addr, (unsigned long long)size,
                   (unsigned int)flash_size);
        free(path_cpy);
        return -1;
    }

    boot_md5_init(&md5_ctx);

    uint64_t remaining = size;
    uint64_t offset = addr;
    while (remaining > 0) {
        uint32_t chunk = remaining > 4096 ? 4096 : (uint32_t)remaining;
        boot_md5_update(&md5_ctx, (const uint8_t *)(0x30000000UL + (uint32_t)offset), chunk);
        offset += chunk;
        remaining -= chunk;
        boot_watchdog_feed();
    }

    boot_md5_finish(&md5_ctx, md5_out);

    for (size_t i = 0; i < BOOT_MD5_DIGEST_SIZE; ++i) {
        sprintf(md5_str + (i * 2), "%02x", md5_out[i]);
    }
    md5_str[32] = '\0';
    shellPrint(sh, "%s %s\r\n", md5_str, argv[1]);

    free(path_cpy);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 md5sum, boot_recovery_shell_md5_command, md5sum);

static int boot_recovery_shell_reboot_command(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    shellPrint(sh, "reboot system...\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));

    __HAL_PMU_WholeChip_RST_ENABLE();

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 reboot, boot_recovery_shell_reboot_command, reboot system);

static void conn_timer_callback(TimerHandle_t xTimer)
{
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;

    xTimerStop(xTimer, 0);

    if (boot_handshake_is_ok()) {
        /* A successful recovery handshake clears the retry marker so the next
         * soft reboot returns to the normal boot path.
         */
        info->handshake_timeout = 0;
        printf("boot handshake ok, nothing to do\n");
        return;
    }

    printf("boot handshake timeout\n");
    printf("ready to power off!\n");
    vTaskDelay(pdMS_TO_TICKS(20));

    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all &= ~(0b1111 << 21);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all |= (0b1010 << 21);
    vTaskDelay(pdMS_TO_TICKS(20));

    printf("ready to reboot!\n");
    info->recover_reason = RECOVER_REASON_AP_WDT_TIMEOUT;
    info->req = 1;
    info->handshake_timeout = 1;

    vTaskDelay(pdMS_TO_TICKS(20));
    __HAL_PMU_WholeChip_RST_ENABLE();

    while (1) {
    }
}

void boot_handshake_detect_init(void)
{
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    TimerHandle_t timer;

    boot_handshake_timeout_backup = info->handshake_timeout;
    if (info->recover_reason != RECOVER_REASON_AP_WDT_TIMEOUT &&
        info->handshake_timeout != 1) {
        return;
    }

    printf("boot enter by ap wdt timeout or handshake timeout, wait for 3 seconds\n");
    boot_need_handshake = 1;

    timer = xTimerCreate("conn-timer", pdMS_TO_TICKS(3 * 1000), pdFALSE, NULL,
                         conn_timer_callback);
    if (timer == NULL) {
        printf("main task: create timer failed\n");
        __builtin_trap();
        return;
    }

    xTimerStart(timer, 0);
}

#ifdef BOOT_RECOVERY_SHELL_CMDS_UNIT_TEST
void boot_recovery_shell_reset_state_for_test(void)
{
    boot_upgrade_mode = 0u;
    boot_handshake_ok = 0u;
    boot_need_handshake = 0u;
    boot_handshake_timeout_backup = 0u;
}
#endif
