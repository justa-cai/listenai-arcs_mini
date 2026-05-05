#include "shell.h"
#include "chip.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "boot_config.h"
#include "boot_flash.h"

#include "FreeRTOS.h"
#include "task.h"

#include "Driver_CRYPTO.h"
#include "PowerManager.h"
#include "disk/disk_access.h"

#include "boot_version.h"
#include "sha256.h"
#include "sysheap.h"
#include "ftsdc021.h"

#if CONFIG_MD5
#include "md5.h"
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"
#include "boot_mem_chunk.h"

#define ROOT_CMD (1 << 0)

SHELL_EXPORT_USER(ROOT_CMD, root, listenai, root);
extern int boot_watchdog_feed(void);
static uint8_t boot_upgrade_mode = 0;
static uint32_t boot_handshake_ok = 0;
static uint8_t boot_handshake_timeout_backup = 0;
static uint8_t boot_need_handshake = 0;

struct shell_async_param {
    char **argv;
    int argc;
    int (*func)(int argc, char *argv[]);
    volatile int *error;
    volatile int *done;
};

bool boot_handshake_is_need(void)
{
    return boot_need_handshake;
}

bool boot_handshake_is_ok(void)
{
    return boot_handshake_ok;
}

void boot_handshake(bool ok)
{
    boot_handshake_ok = ok;
}

static void shell_async_task(void *p)
{
    struct shell_async_param *param = p;

    volatile int *error = param->error;
    volatile int *done = param->done;

    int r = param->func(param->argc, param->argv);
    if (error) {
        *error = r;
    }

    for (int i = 0; i < param->argc; i++) {
        free(param->argv[i]);
    }
    free(param->argv);
    free(param);

    if (done) {
        *done = 1;
    }

    vTaskDelete(NULL);
}

static int shell_async(int argc, char *argv[], int (*func)(int argc, char *argv[]), bool wait)
{
    struct shell_async_param *param = malloc(sizeof(struct shell_async_param));
    if (param == NULL) {
        return -1;
    }

    memset(param, 0, sizeof(struct shell_async_param));

    char **argv_cpy = calloc(argc, sizeof(char *));
    if (argv_cpy == NULL) {
        free(param);
        return -1;
    }
    for (int i = 0; i < argc; i++) {
        argv_cpy[i] = malloc(strlen(argv[i]) + 1);
        strcpy(argv_cpy[i], argv[i]);
    }

    volatile int error = -1;
    volatile int done = 0;

    int priority = 2;
    param->argv = argv_cpy;
    param->argc = argc;
    param->func = func;
    param->error = &error;
    param->done = &done;

    TaskHandle_t task;
    xTaskCreate(shell_async_task, "shell_async", 1024 * 1, param, priority, &task);

    if (task == NULL) {
        for (int i = 0; i < argc; i++) {
            free(argv_cpy[i]);
        }
        free(argv_cpy);
        free(param);
        return -1;
    }

    if (!wait) {
        return 0;
    }

    while (!done) {
        extern void adb_shell_flush(void);
        adb_shell_flush();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return error;
}

static bool raw_disk_access(const char *path)
{
    return strncmp(path, "/RAW", strlen("/RAW")) == 0;
}

static int raw_disk_get_info_by_path(char *path, char **name, uint64_t *addr, uint64_t *size)
{
    /* /RAW/name/addr/size */
    char *p = strstr(path, "/RAW/");
    char *token;
    char *saveptr;

    if (p != path) {
        return -1;
    }
    p += strlen("/RAW/");
    *size = 0;
    *addr = 0;

    token = strtok_r(p, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }
    *name = token;

    token = strtok_r(NULL, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }
    *addr = strtoull(token, NULL, 16);

    token = strtok_r(NULL, "/", &saveptr);
    if (token == NULL) {
        return -1;
    }
    *size = strtoull(token, NULL, 16);

    return 0;
}

#ifdef CONFIG_MD5
static int md5_calc_file(Shell *sh, const char *path, uint8_t md5[16], bool show_progress)
{
    return -1;
}

static int md5_calc_disk(Shell *sh, const char *disk_name, uint64_t addr, uint64_t size, uint8_t md5[16], bool show_progress)
{
    int r;
    uint32_t sec_size;
    uint8_t *buff;
    uint32_t buff_size;
    uint64_t read_size;
    uint8_t last_progress = 0;

    r = disk_access_init(disk_name);
    if (r) {
        shellPrint(sh, "disk access init error, disk name:%s\n", disk_name);
        return -1;
    }

    r = disk_access_ioctl(disk_name, DISK_IOCTL_GET_SECTOR_SIZE, &sec_size);
    if (r) {
        shellPrint(sh, "disk access get sector size error, disk name:%s\n", disk_name);
        return -1;
    }

    if (addr % sec_size != 0) {
        shellPrint(sh, "addr is not align with sec_size, addr:0x%x, sec_size:%d\n", addr, sec_size);
        return -1;
    }

    buff_size = MEM_CHUNK_SIZE / (sec_size) * (sec_size);
    assert(buff_size);

    buff = boot_mem_large_chunk_get((uint32_t)portMAX_DELAY);
    if (buff == NULL) {
        shellPrint(sh, "md5_calc_disk buff malloc failed\n");
        return -1;
    }

    MD5Context ctx;
    md5Init(&ctx);

    read_size = 0;
    uint64_t target_size = addr + size;

    while (addr < target_size) {
        uint64_t read_len = target_size - addr;
        if (read_len > buff_size) {
            read_len = buff_size;
        }

        uint32_t cnt = read_len / sec_size;
        uint32_t actual_len;
        if (cnt == 0) {
            cnt = 1;
            actual_len = read_len;
        } else {
            actual_len = cnt * sec_size;
        }

        r = disk_access_read(disk_name, buff, addr / sec_size, cnt);
        if (r) {
            shellPrint(sh, "disk access read error, disk name:%s\n", disk_name);
            md5Finalize(&ctx);
            boot_mem_large_chunk_put(buff);
            return -1;
        }
        addr += cnt * sec_size;

        md5Update(&ctx, buff, actual_len);
        extern int boot_watchdog_feed(void);
        boot_watchdog_feed();

        if (show_progress) {
            if (target_size >= addr) {
                uint8_t progress = 100 - ((target_size - addr) * 100 / size);
                if (progress != last_progress) {
                    last_progress = progress;
                    shellPrint(sh, "[%3d%%]\r", progress);
                }
            }
        }
    }

    md5Finalize(&ctx);
    memcpy(md5, ctx.digest, 16);
    if (show_progress && last_progress != 100) {
        shellPrint(sh, "[%3d%%]\r", 100);
    }
    boot_mem_large_chunk_put(buff);

    return 0;
}

static int md5sum(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc < 2) {
        shellPrint(sh, "Usage: md5sum <path> [-p]\r\n");
        return -1;
    }
    bool show_progress = false;
    if (argc > 2) {
        if (strcmp(argv[2], "-p") == 0) {
            show_progress = true;
        }
    }

    char *path = argv[1];
    char *path_cpy = malloc(strlen(path) + 1);
    if (path_cpy == NULL) {
        shellPrint(sh, "malloc failed\r\n");
        return -1;
    }
    strcpy(path_cpy, path);
    char *name;
    uint64_t addr;
    uint64_t size;
    uint8_t md5[16];
    int r;

    if (raw_disk_access(path)) {
        r = raw_disk_get_info_by_path(path, &name, &addr, &size);
        if (r != 0) {
            shellPrint(sh, "Usage: md5sum </RAW/diskname/addr/size>\r\n");
            return -1;
        }
        r = md5_calc_disk(sh, name, addr, size, md5, show_progress);
        if (r != 0) {
            shellPrint(sh, "path %s md5 calc failed\r\n", path);
            return -1;
        }
    } else {
        r = md5_calc_file(sh, path, md5, show_progress);
        if (r != 0) {
            shellPrint(sh, "path %s md5 calc failed\r\n", path);
            return -1;
        }
    }

    char *md5_str = malloc(32 + 1);
    if (md5_str == NULL) {
        shellPrint(sh, "malloc failed\r\n");
        return -1;
    }

    for (int i = 0; i < 16; i++) {
        sprintf(md5_str + i * 2, "%02x", md5[i]);
    }
    md5_str[32] = '\0';

    shellPrint(sh, "%s %s\r\n", md5_str, path_cpy);
    free(md5_str);
    free(path_cpy);

    return 0;
}

static int md5sum_shell(int argc, char *argv[])
{
    return shell_async(argc, argv, md5sum, true);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, md5sum,
                 md5sum_shell, md5 calc);
#endif

void reboot_software(void)
{
    vTaskDelay(pdMS_TO_TICKS(200));
    __HAL_PMU_WholeChip_RST_ENABLE();
}

void reboot_hardware(void)
{
    vTaskDelay(pdMS_TO_TICKS(200));

    /* like por */
    IP_AON_CTRL->REG_AON_SW_RESET.all = 0xCAFE000A;
}

static int reboot(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    shellPrint(sh, "reboot system...\r\n");

    vTaskDelay(pdMS_TO_TICKS(100));
    bool flash_lock = true;

    if (argc < 2) {
        reboot_software();
        return 0;
    }

    if (argc > 2) {
        if (strcmp("--no-flash-lock", argv[2]) == 0) {
            flash_lock = false;
        }
    }

    if (flash_lock) {
        boot_flash_lock_resume();
    }

    if (strncmp(argv[1], "soft", strlen(argv[1])) == 0) {
        reboot_software();
    } else if (strncmp(argv[1], "hard", strlen(argv[1])) == 0) {
        reboot_hardware();
    } else {
        shellPrint(sh, "Usage: reboot [soft/hard]\r\n");
        return -1;
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, reboot,
                 reboot, reboot system);

static int recovery(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc != 2) {
        shellPrint(sh, "Usage: recovery exit\r\n");
        return -1;
    }

    if (strncmp(argv[1], "exit", strlen(argv[1])) == 0) {
        /* clear recovery request */
        int r = boot_recovery_hardware_exit();
        if (r != 0) {
            shellPrint(sh, "recovery exit failed, r = %d\r\n", r);
            return -1;
        }

        struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
        info->req = 0;
        info->recover_reason = RECOVER_REASON_NONE;
        info->reboot_cnt = 0;
        shellPrint(sh, "recovery exit successfully\r\n");
        return 0;
    } else if (strncmp(argv[1], "soft", strlen(argv[1])) == 0) {
        struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;

        info->recover_reason = RECOVER_REASON_SOFT_REQ;
        shellPrint(sh, "recovery software enter successfully\r\n");
        boot_recovery_software_enter();
        return 0;
    } else if (strncmp(argv[1], "hard", strlen(argv[1])) == 0) {
        int r = boot_recovery_hardware_enter();
        if (r != 0) {
            shellPrint(sh, "recovery hardware enter failed, r = %d\r\n", r);
            return -1;
        }
        shellPrint(sh, "recovery hardware enter successfully\r\n");
        return 0;
    }

    shellPrint(sh, "Usage: recovery [soft/hard/exit]\r\n");

    return -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, recovery,
                 recovery, recovery system);

static void boot_upgrade_mode_set(bool enter)
{
    boot_upgrade_mode = enter;
}

uint8_t boot_upgrade_mode_get(void)
{
    return boot_upgrade_mode;
}

void disk_device_write_start(const char *name, uint64_t addr, uint64_t size)
{    
    printf("disk_device_write_start, name:%s, addr:0x%08x, 0x%08x, size:0x%08x, 0x%08x\n", name, addr >> 32,
        addr & 0xFFFFFFFF, size >> 32, size & 0xFFFFFFFF);

    if ((strncmp(name, "NAND", 4) != 0) && (strncmp(name, "FLASH", 5) != 0)) {
        return;
    }

    /* 清除flash锁 */
    boot_flash_lock_clear();

    if (addr >= BOOT_PROTECT_AREA_SIZE) {
        /* 非boot地址的写入，锁boot区域, 防止boot区域被意外写入 */
        boot_flash_lock_boot();
    }

    return;
}

void disk_device_write_done(const char *name, uint64_t addr, uint64_t size)
{
    printf("disk_device_write_done, name:%s, addr:0x%08x, 0x%08x, size:0x%08x, 0x%08x\n", name, addr >> 32,
           addr & 0xFFFFFFFF, size >> 32, size & 0xFFFFFFFF);

    if ((strncmp(name, "NAND", 4) != 0) && (strncmp(name, "FLASH", 5) != 0)) {
        return;
    }

    boot_flash_lock_resume();

    return;
}

bool disk_device_can_access(const char *name, uint64_t addr)
{
    static bool is_init = false;
    printf("disk_device_can_access, name:%s, addr:0x%08x, addr:0x%08x\n", name, addr >> 32, addr & 0xFFFFFFFF);
    int r;

    if (boot_handshake_is_need()) {
        if (!boot_handshake_is_ok()) {
            return false;
        }
    }

    if ((strncmp(name, "SDRAW", 5) == 0) && !is_init)
    {
        extern int sdmmc_hard_init(void);
        sdmmc_hard_init();
        is_init = true;
    }

    if ((strncmp(name, "NAND", 4) != 0)
        && (strncmp(name, "FLASH", 5) != 0)) {
        return true;
    }

    if ((addr < BOOT_PROTECT_AREA_SIZE) && !boot_upgrade_mode_get()) {
        return false;
    }

    return true;
}

static int upgrade(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc != 2) {
        shellPrint(sh, "Usage: upgrade <enter/exit>\r\n");
        return -1;
    }

    if (strncmp(argv[1], "enter", strlen(argv[1])) == 0) {
        shellPrint(sh, "upgrade enter successfully\r\n");
        boot_upgrade_mode_set(true);
        return 0;
    } else if (strncmp(argv[1], "exit", strlen(argv[1])) == 0) {
        shellPrint(sh, "upgrade exit successfully\r\n");
        boot_upgrade_mode_set(false);
        return 0;
    } else if (strncmp(argv[1], "status", strlen(argv[1])) == 0) {
        shellPrint(sh, "upgrade status: %s\r\n", boot_upgrade_mode ? "enter" : "exit");
        return 0;
    }

    shellPrint(sh, "Usage: upgrade [enter/exit/status]\r\n");

    return -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(ROOT_CMD) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 upgrade, upgrade, upgrade boot);

void exception_app_handle(void)
{
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;

    printf("exception_app_handle\n");

    info->recover_reason = RECOVER_REASON_APP_INVALID;
    boot_recovery_software_enter();
    reboot_software();
}

static int info_version(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    shellPrint(sh, "version: %d.%d.%d\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);
    shellPrint(sh, "commit: %s\r\n", VERSION_COMMIT);

    return 0;
}

static int info_reason(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
    shellPrint(sh, "reason: %d, handshake_timeout: %d\r\n", info->recover_reason, boot_handshake_timeout_backup);

    return 0;
}

static int info_sn(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    extern const char *device_id_str_get(void);
    shellPrint(sh, "serial num: %s\r\n", device_id_str_get());

    return 0;
}

static int info_flash(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();
    int r;

    uint32_t size;
    r = boot_flash_size_get(&size);
    if (r) {
        shellPrint(sh, "flash size get failed\r\n");
        return -1;
    }
    const char *unit = size >= (1 * 1024 * 1024) ? "MB" : "KB";
    shellPrint(sh, "size: %d%s\r\n", size / (1 * 1024 * 1024), unit);

    struct flash_status_register *fl_status;
    fl_status = boot_flash_status_register_get(0);
    if (fl_status) {
        shellPrint(sh, "startup status register: 0, 0x%04x, 0x%04x, 0x%04x\r\n", fl_status->register_1,
                   fl_status->register_2, fl_status->register_3);
    }

#if CONFIG_DUAL_FLASH
    fl_status = boot_flash_status_register_get(1);
    if (fl_status) {
        shellPrint(sh, "startup status register: 1, 0x%04x, 0x%04x, 0x%04x\r\n", fl_status->register_1,
                   fl_status->register_2, fl_status->register_3);
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

    shellPrint(sh, "current status register: 0, 0x%04x, 0x%04x, 0x%04x\r\n", fl_sta.register_1, fl_sta.register_2,
               fl_sta.register_3);

#if CONFIG_DUAL_FLASH
    r = boot_flash_reg_get(1, 1, &fl_sta.register_1);
    r |= boot_flash_reg_get(1, 2, &fl_sta.register_2);
    r |= boot_flash_reg_get(1, 3, &fl_sta.register_3);
    if (r) {
        shellPrint(sh, "flash1 status register get failed\r\n");
        return -1;
    }

    shellPrint(sh, "current status register: 1, 0x%04x, 0x%04x, 0x%04x\r\n", fl_sta.register_1, fl_sta.register_2,
               fl_sta.register_3);
#endif

    return 0;
}

static int info_sd(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    extern SDHostInfo SDHost[];

    const char *card_type[6] = {
        "unknown", "sd", "mmc", "sdio", "sdio-combo", "emmc",
    };

    extern u8* SDC_ShowTransferSpeed(Bus_Speed speed);
    extern void SDC_ShowCapacity(u64 cap, s8* buf);
    uint64_t capacity;
    uint8_t buf[64];
    uint8_t ip_idx = 0;
    capacity = (u64)SDHost[ip_idx].Card->numOfBlocks;
    capacity <<= 9;
    SDC_ShowCapacity(capacity, buf);
    shellPrint(sh, "width: %d\r\n", SDHost[ip_idx].Card->bus_width);
    shellPrint(sh, "speed: %s\r\n", SDC_ShowTransferSpeed(SDHost[ip_idx].Card->speed));
    shellPrint(sh, "capacity: %s\r\n", buf);

    if (SDHost[ip_idx].Card->CardType >= 6) {
        shellPrint(sh, "type: unknown\r\n");
    } else {
        shellPrint(sh, "type: %s\r\n", card_type[SDHost[ip_idx].Card->CardType]);
    }

    return 0;
}

static int info(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc < 2) {
        shellPrint(sh, "info version/flash/sn\r\n");
        return -1;
    }

    if (strncmp(argv[1], "version", strlen(argv[1])) == 0) {
        return info_version(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "flash", strlen(argv[1])) == 0) {
        return info_flash(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "sn", strlen(argv[1])) == 0) {
        return info_sn(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "sd", strlen(argv[1])) == 0) {
        return info_sd(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "reason", strlen(argv[1])) == 0) {
        return info_reason(argc - 1, argv + 1);
    }

    shellPrint(sh, "info version/flash/sn/sd/reason\r\n");

    return -1;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, info, info,
                 info boot);

static int flash_regw(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc != 5) {
        shellPrint(sh, "Usage: regw <idx> <addr> <data> <mask>\r\n");
        return -1;
    }
    uint8_t idx = strtoul(argv[1], NULL, 16);
    uint32_t addr = strtoul(argv[2], NULL, 16);
    uint32_t data_in = strtoul(argv[3], NULL, 16);
    uint32_t data_mask = strtoul(argv[4], NULL, 16);
    uint32_t data_out = 0;

    int res = boot_flash_reg_set(idx, addr, data_in, data_mask, &data_out);
    if (res) {
        shellPrint(sh, "flash reg write failed, idx:%d, addr:0x%x, in:0x%x, mask:0x%x\r\n", idx, addr, data_in,
                   data_mask);
        return -1;
    }

    shellPrint(sh, "flash reg write successfully, idx:%d, addr:0x%x, in:0x%x, mask:0x%x, out:0x%x\r\n", idx, addr,
               data_in, data_mask, data_out);

    return 0;
}

static int flash_regr(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc != 3) {
        shellPrint(sh, "Usage: regr <idx> <addr>\r\n");
        return -1;
    }
    uint8_t idx = strtoul(argv[1], NULL, 16);
    uint32_t addr = strtoul(argv[2], NULL, 16);
    uint32_t data_out = 0;

    int res = boot_flash_reg_get(idx, addr, &data_out);
    if (res) {
        shellPrint(sh, "flash reg read failed, idx:%d, addr:0x%x\r\n", idx, addr);
        return -1;
    }

    shellPrint(sh, "idx:%d, addr:0x%x=out:0x%x\r\n", idx, addr, data_out);

    return 0;
}

static int flash_auto_unlock(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc != 2) {
        shellPrint(sh, "Usage: flash auto-unlock [on/off]\r\n");
        return -1;
    }

    if (strncmp(argv[1], "on", strlen(argv[1])) == 0) {
        boot_flash_auto_unlock_mode_set(true);
    } else if (strncmp(argv[1], "off", strlen(argv[1])) == 0) {
        boot_flash_auto_unlock_mode_set(false);
    } else {
        shellPrint(sh, "Usage: flash auto-unlock [on/off]\r\n");
        return -1;
    }

    return 0;
}

static SemaphoreHandle_t flash_erase_lock = NULL;
static uint32_t flash_erase_error = 0;
static uint32_t flash_erase_elapsed = 0;

static bool flash_erase_busy()
{
    if (flash_erase_lock == NULL) {
        return false;
    }
    if (xSemaphoreTake(flash_erase_lock, 0) == pdTRUE) {
        xSemaphoreGive(flash_erase_lock);
        return false;
    }

    return true;
}

static int flash_erase_low(uint32_t addr, uint32_t size)
{
    printf("flash erase start addr:0x%x, size:0x%x\r\n", addr, size);

    if (addr < 0x30000) {
        printf("flash erase not allowed, addr:0x%x, size:0x%x\r\n", addr, size);
        return -1;
    }

    const int erase_size = 64 * 1024;
    int n = size / erase_size;
    int r = 0;
    for (int i = 0; i < n; i++) {
        r = boot_flash_erase((uint8_t *)(addr + 0x30000000 + i * erase_size), erase_size);
        if (r) {
            break;
        }
    }
    if (r == 0) {
        int remain = size % erase_size;
        if (remain) {
            r = boot_flash_erase((uint8_t *)(addr + 0x30000000 + n * erase_size), remain);
        }
    }

    return r;
}

static int flash_erase(int argc, char *argv[])
{
    int r;
    if (flash_erase_lock == NULL) {
        flash_erase_lock = xSemaphoreCreateMutex();
    }
    
    Shell *sh = shellGetCurrent();
    if (argc != 3) {
        shellPrint(sh, "Usage: flash erase <addr> <size>/<addr> all/ group <addr:size;addr:size;>\r\n");
        return -1;
    }

    shellPrint(sh, "flash erase, argv[1]: %s, argv[2]: %s\r\n", argv[1], argv[2]);

    uint32_t size;
    uint32_t addr;

    if (xSemaphoreTake(flash_erase_lock, 0) != pdTRUE) {
        printf("flash erase in progress\r\n");
        shellPrint(sh, "flash erase in progress\r\n");
        return -1;
    }

    flash_erase_error = 0;
    flash_erase_elapsed = 0;
    uint32_t start = xTaskGetTickCount();
    if (strncmp(argv[2], "all", 3) == 0) {
        boot_flash_size_get(&size);
        addr = strtoul(argv[1], NULL, 16);
        size -= addr;
        r = flash_erase_low(addr, size);
        if (r) {
            printf("flash erase failed, addr:0x%x, size:0x%x\r\n", addr, size);
            shellPrint(sh, "flash erase failed, addr:0x%x, size:0x%x\r\n", addr, size);
            goto exit;
        }
    } else if (strncmp(argv[1], "group", 5) == 0) {
        shellPrint(sh, "flash erase multiple request start, argv[2]: %s\r\n", argv[2]);
        char *p = argv[2];
        char *token;
        char *saveptr;

        token = strtok_r(p, ",", &saveptr);
        while (token) {
            char *saveptr2;
            char *addr_p = strtok_r(token, ":", &saveptr2);
            char *size_p = strtok_r(NULL, ":", &saveptr2);
            if (addr_p && size_p) {
                addr = strtoul(addr_p, NULL, 16);
                size = strtoul(size_p, NULL, 16);
                printf("flash erase start, addr:0x%x, size:0x%x\r\n", addr, size);
                shellPrint(sh, "flash erase start, addr:0x%x, size:0x%x\r\n", addr, size);
                r = flash_erase_low(addr, size);
                if (r) {
                    printf("flash erase failed, addr:0x%x, size:0x%x\r\n", addr, size);
                    shellPrint(sh, "flash erase failed, addr:0x%x, size:0x%x\r\n", addr, size);
                    goto exit;
                }
            }
            token = strtok_r(NULL, ",", &saveptr);
        }
    } else {
        addr = strtoul(argv[1], NULL, 16);
        size = strtoul(argv[2], NULL, 16);
        r = flash_erase_low(addr, size);
        if (r) {
            printf("flash erase failed, addr:0x%x, size:0x%x\r\n", addr, size);
            shellPrint(sh, "flash erase failed, addr:0x%x, size:0x%x\r\n", addr, size);
            goto exit;
        }
    }
exit:
    flash_erase_error = r;
    flash_erase_elapsed = xTaskGetTickCount() - start;
    printf("flash erase done, elapsed: %d ms, error: %d\r\n", flash_erase_elapsed, flash_erase_error);
    shellPrint(sh, "flash erase done, elapsed: %d ms, error: %d\r\n", flash_erase_elapsed, flash_erase_error);

    xSemaphoreGive(flash_erase_lock);


    return 0;
}

static int flash_async_erase(int argc, char *argv[])
{
    return shell_async(argc, argv, flash_erase, false);
}

static int flash_erase_status(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    shellPrint(sh, "busy : %s\r\n", flash_erase_busy() ? "true" : "false");
    shellPrint(sh, "error: %d\r\n", flash_erase_error);
    shellPrint(sh, "cost : %d\r\n", flash_erase_elapsed);

    return 0;
}

static int flash(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc < 2) {
        shellPrint(sh, "Usage: flash [regw/regr/auto-unlock/erase/erase-async/erase-status]\r\n");
        return -1;
    }

    if (strncmp(argv[1], "regw", strlen(argv[1])) == 0) {
        return flash_regw(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "regr", strlen(argv[1])) == 0) {
        return flash_regr(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "auto-unlock", strlen(argv[1])) == 0) {
        return flash_auto_unlock(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "erase", strlen(argv[1])) == 0) {
        return flash_erase(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "erase-async", strlen(argv[1])) == 0) {
        return flash_async_erase(argc - 1, argv + 1);
    } else if (strncmp(argv[1], "erase-status", strlen(argv[1])) == 0) {
        return flash_erase_status(argc - 1, argv + 1);
    }

    shellPrint(sh, "Usage: flash [regw/regr/auto-unlock/erase/erase-async/erase-status]\r\n");

    return -1;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(ROOT_CMD) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, flash,
                 flash, flash);

static int sha256_calc_file(Shell *sh, const char *path, uint8_t sha256_out[32], bool show_progress)
{
    return -1;
}

static void sha256_update_callback(void *ctx, const uint8_t *data, uint32_t len, void *user_data)
{
    QueueHandle_t inq = (QueueHandle_t)user_data;
    xQueueSend(inq, &data, portMAX_DELAY);
}

#define SHA256_BUF_CNT  2

static int sha256_calc_disk(Shell *sh, const char *disk_name, uint64_t addr, uint64_t size, uint8_t sha256_out[32],
                            bool show_progress)
{
    int r;
    uint32_t sec_size;
    uint8_t *buff;
    uint32_t buff_size;
    uint32_t read_size;
    uint8_t last_progress = 0;
    int err = 0;

    r = disk_access_init(disk_name);
    if (r) {
        shellPrint(sh, "disk access init error, disk name:%s\n", disk_name);
        return -1;
    }

    r = disk_access_ioctl(disk_name, DISK_IOCTL_GET_SECTOR_SIZE, &sec_size);
    if (r) {
        shellPrint(sh, "disk access get sector size error, disk name:%s\n", disk_name);
        return -1;
    }

    if (addr % sec_size != 0) {
        shellPrint(sh, "addr is not align with sec_size, addr:0x%llx, sec_size:%llx\n", addr, sec_size);
        return -1;
    }

    uint64_t target_size = addr + size;

    QueueHandle_t inq = xQueueCreate(2, sizeof(void *));
    if (inq == NULL) {
        shellPrint(sh, "sha256_calc_disk queue create failed\n");
        err = -1;
        goto out;
    }
    uint8_t *buffs[SHA256_BUF_CNT] = {0};
    for (int i = 0; i < SHA256_BUF_CNT; i++) {
        buffs[i] = boot_mem_large_chunk_get((uint32_t)portMAX_DELAY);
        xQueueSend(inq, &buffs[i], portMAX_DELAY);
    }
    buff_size = MEM_CHUNK_SIZE / (sec_size) * (sec_size);
    assert(buff_size);

    void *ctx = sha256_init(sha256_update_callback, inq);
    if (ctx == NULL) {
        shellPrint(sh, "sha256_calc_disk sha256_init failed\n");
        err = -1;
        goto out;
    }

    r = sha256_start(ctx);
    if (r) {
        shellPrint(sh, "sha256_calc_disk sha256_start failed\n");
        err = -1;
        goto out;
    }

    while (addr < target_size) {
        void *buff = NULL;
        xQueueReceive(inq, &buff, portMAX_DELAY);
        uint64_t read_len = target_size - addr;
        if (read_len > buff_size) {
            read_len = buff_size;
        }
        uint32_t actual_len;
        uint32_t cnt = read_len / sec_size;
        if (cnt == 0) {
            cnt = 1;
            actual_len = read_len;
        } else {
            actual_len = cnt * sec_size;
        }

        r = disk_access_read(disk_name, buff, addr / sec_size, cnt);
        if (r) {
            shellPrint(sh, "disk access read error, disk name:%s\n", disk_name);
            err = -1;
            goto out;
        }
        addr += cnt * sec_size;
        r = sha256_update(ctx, buff, actual_len);
        if (r) {
            shellPrint(sh, "sha256_calc_disk sha256_update failed\n");
            err = -1;
            goto out;
        }
        extern int boot_watchdog_feed(void);
        boot_watchdog_feed();
        if (show_progress) {
            if (target_size >= addr) {
                uint8_t progress = 100 - ((target_size - addr) * 100 / size);
                if (progress != last_progress) {
                    last_progress = progress;
                    shellPrint(sh, "[%3d%%]\r", progress);
                }
            }
        }
    }

    r = sha256_final(ctx, sha256_out);
    if (r) {
        shellPrint(sh, "sha256_calc_disk sha256_final failed, r:%d\n", r);
        err = -1;
        goto out;
    }
    if (show_progress && last_progress != 100) {
        shellPrint(sh, "[%3d%%]\r", 100);
    }

out:
    if (ctx) {
        sha256_free(ctx);
    }
    if (inq) {
        vQueueDelete(inq);
    }

    for (int i = 0; i < SHA256_BUF_CNT; i++) {
        boot_mem_large_chunk_put(buffs[i]);
    }

    return err;
}

static int sha256(int argc, char *argv[])
{
    Shell *sh = shellGetCurrent();

    if (argc < 2) {
        shellPrint(sh, "Usage: sha256 <path> [-p]\r\n");
        return -1;
    }
    bool show_progress = false;
    if (argc > 2) {
        if (strcmp(argv[2], "-p") == 0) {
            show_progress = true;
        }
    }

    char *path = argv[1];
    char *path_cpy = malloc(strlen(path) + 1);
    if (path_cpy == NULL) {
        shellPrint(sh, "malloc failed\r\n");
        return -1;
    }
    strcpy(path_cpy, path);
    char *name;
    uint64_t addr;
    uint64_t size;
    uint8_t sha256_out[32];
    int r;

    if (raw_disk_access(path)) {
        r = raw_disk_get_info_by_path(path, &name, &addr, &size);
        if (r != 0) {
            shellPrint(sh, "Usage: sha256 </RAW/diskname/addr/size>\r\n");
            return -1;
        }
        r = sha256_calc_disk(sh, name, addr, size, sha256_out, show_progress);
        if (r != 0) {
            shellPrint(sh, "path %s sha256 calc failed\r\n", path);
            return -1;
        }
    } else {
        r = sha256_calc_file(sh, path, sha256_out, show_progress);
        if (r != 0) {
            shellPrint(sh, "path %s sha256 calc failed\r\n", path);
            return -1;
        }
    }

    char *sha256_str = malloc(64 + 1);
    if (sha256_str == NULL) {
        shellPrint(sh, "malloc failed\r\n");
        return -1;
    }

    for (int i = 0; i < 32; i++) {
        sprintf(sha256_str + i * 2, "%02x", sha256_out[i]);
    }
    sha256_str[64] = '\0';

    shellPrint(sh, "%s %s\r\n", sha256_str, path_cpy);
    free(sha256_str);
    free(path_cpy);

    return 0;
}

static int sha256_shell(int argc, char *argv[])
{
    return shell_async(argc, argv, sha256, true);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, sha256,
                 sha256_shell, sha256);

static void heaps(void)
{
    void heap_summary_info(void);
    heap_summary_info();
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, heaps, heaps, heaps);

static void tasks(void)
{
    Shell *sh = shellGetCurrent();
    uint32_t tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *const items = pvPortMalloc(tasks * sizeof(TaskStatus_t));
    if (items) {
        uint32_t total = 0;
        tasks = uxTaskGetSystemState(items, tasks, &total);
        if (total > 0) {
            shellPrint(sh, "%20s %9s %9s %9s %18s %12s %9s\r\n"
                , "[TASK]", "[STA.]", "[PRIO.]", "[STK/B]", "[TOTAL]", "[CALL.]", "[PCT.%]");
            for (TaskStatus_t *stat = items; stat < &items[tasks]; stat++) {
                shellPrint(sh, "%9s@%p %9s %9d %9d %9d@%9p %12d %8d%%\r\n", stat->pcTaskName, stat->xHandle
                    , ((const char *[]){"RUN", "RDY", "BLK", "SUS", "DEL" })[stat->eCurrentState]
                    , (int)stat->uxCurrentPriority, (int)stat->usStackHighWaterMark * sizeof(StackType_t)
                    , (stat->pxEndOfStack - stat->pxStackBase)* sizeof(StackType_t), stat->pxStackBase
                    , (int)stat->ulRunTimeCounter, (int)(100.0f * stat->ulRunTimeCounter / total));
            }
        }
        vPortFree(items);
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, tasks, tasks, tasks);


static void conn_timer_callback(TimerHandle_t xTimer)
{
    xTimerStop(xTimer, 0);
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;

    if (boot_handshake_is_ok()) {
        /* 握手成功后, 清除超时标记位
         * 表示软重启后, 不再进行握手检测, 恢复为正常模式
         */
        info->handshake_timeout = 0;
        printf("boot handshake ok, nothing to do\n");
        return;
    }

    printf("boot handshake timeout\n");
    printf("ready to power off!\n");
    /* 做个延迟,让日志输出 */
    vTaskDelay(pdMS_TO_TICKS(20));

    /* 陶云扫描笔上进行强制掉电关机 */
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all &= ~(0b1111 << 21);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_03.all |= (0b1010 << 21);
    vTaskDelay(pdMS_TO_TICKS(20));

    /* 能到这里说明有usb连接供电的情况, 强制掉电无效 */
    printf("ready to reboot!\n");

    /* 这里进行软重启到boot中 */
    /* set recovery request */
    info->recover_reason = RECOVER_REASON_AP_WDT_TIMEOUT;
    info->req = 1;

    /* 设置超时标记位, 表示重启后需要继续进行握手检测 */
    info->handshake_timeout = 1;

    vTaskDelay(pdMS_TO_TICKS(20));
    /* soft reset */
    __HAL_PMU_WholeChip_RST_ENABLE();

    /* 等待复位 */
    while (1) {
    }
}

void boot_handshake_detect_init(void)
{
    struct boot_info *info = (struct boot_info *)&IP_AON_CTRL->REG_AON_DIG_RSVD4.all;

    boot_handshake_timeout_backup = info->handshake_timeout;

    if (info->recover_reason == RECOVER_REASON_AP_WDT_TIMEOUT || info->handshake_timeout == 1) {
        printf("boot enter by ap wdt timeout or handshake timeout, wait for 3 seconds\n");
        /* 需要进行握手检测, 握手成功后, 才可通过ADB push/pull disk */
        boot_need_handshake = 1;

        TimerHandle_t timer;
        timer = xTimerCreate("conn-timer", pdMS_TO_TICKS(3 * 1000), pdFALSE, (void *)0, conn_timer_callback);
        if (timer == NULL) {
            printf("main task: create timer failed\n");
            __builtin_trap();
            return;
        }

        xTimerStart(timer, 0);
    }
}
