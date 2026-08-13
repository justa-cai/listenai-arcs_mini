#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "shell.h"
#include "lisa_shell.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tusb.h"
#include "ClockManager.h"
#include "arcs_ap.h"
#include "disk/disk_access.h"
#include "disk/disk.h"
#include "IOMuxManager.h"
#include "board.h"
#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_sdmmc.h"
#include "lisa_touch.h"
#include "lsfs.h"
#include "lvfs.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lua_lisa_all.h"
#include "lua_lvgl_all.h"

/* ---------- Constants ---------- */

#define LUA_LINE_MAXLEN    256
#define LUA_SCRIPT_MAXLEN  (32 * 1024)
#define USBD_STACK_SIZE    (3 * configMINIMAL_STACK_SIZE)
#define DISK_BLOCK_SIZE    512
#define LVGL_STACK_SIZE    (4 * 1024)

#define SDMMC_DEVICE       "SD:"
#define SDMMC_MOUNT_POINT  "/" SDMMC_DEVICE
#define LUA_SCRIPT_DIR     SDMMC_MOUNT_POINT "/"
#define DISPLAY_DEVICE     "display"
#define TOUCH_DEVICE       "touch_cst328"
#define I2C_DEVICE         "i2c0"

#ifdef CONFIG_BOARD_ARCS_EVB
#define LCD_CS_PIN              5
#define LCD_SPI_CLK_PIN         3
#define LCD_SPI_DATA_PIN        1
#define LISA_TOUCH_I2C_SDA_PIN  22
#define LISA_TOUCH_I2C_SCL_PIN  23
#define LISA_TOUCH_I2C_RST_PIN  25
#define LISA_TOUCH_I2C_INT_PIN  24

void lisa_gpioa_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LISA_TOUCH_I2C_RST_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LISA_TOUCH_I2C_INT_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_i2c0_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LISA_TOUCH_I2C_SDA_PIN, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LISA_TOUCH_I2C_SCL_PIN, 8);
}

void lisa_spi1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

void lisa_pwm_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, CSK_IOMUX_FUNC_ALTER12);
}
#endif

/* ---------- LVGL display/touch ---------- */

static lisa_device_t *lvgl_display_device;

static void lvgl_task(void *param)
{
    (void)param;

    while (1) {
        lua_lvgl_lock();
        uint32_t wait_time = lv_timer_handler();
        lua_lvgl_unlock();

        if (wait_time == 0) {
            wait_time = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(wait_time));
    }
}

static int lvgl_display_init(void)
{
    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");
    lisa_device_t *spi_dev = lisa_device_get("spi1");
    lisa_device_t *pwm_dev = lisa_device_get("pwm0");

    if (!lisa_device_ready(gpioa_dev) || !lisa_device_ready(gpiob_dev) ||
        !lisa_device_ready(spi_dev) || !lisa_device_ready(pwm_dev)) {
        printf("LVGL: display bus device not ready\n");
        return -1;
    }

    lisa_display_config_t display_config = {
        .panel_name = "st7789p3",
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {.spi_4wire = {
            .spi_dev = spi_dev,
            .cs_gpio = gpiob_dev,
            .cs_pin = LCD_CS_PIN,
            .dc_gpio = gpiob_dev,
            .dc_pin = LCD_CD_PIN,
            .spi_freq = 50 * 1000 * 1000,
        }},
        .backlight = {
            .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
            .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
            .config.pwm = {
                .channel = 0,
                .dev = pwm_dev,
                .freq = 2000,
            },
        },
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
    };

    lvgl_display_device = lisa_device_get(DISPLAY_DEVICE);
    if (!lvgl_display_device) {
        printf("LVGL: failed to get %s device\n", DISPLAY_DEVICE);
        return -1;
    }

    int ret = lisa_display_attach_bus(lvgl_display_device, &display_config);
    if (ret != 0) {
        printf("LVGL: display bus attach failed (%d)\n", ret);
        return ret;
    }

    lv_port_disp_init(lvgl_display_device);
    if (lv_disp_get_default() == NULL) {
        printf("LVGL: display driver register failed\n");
        return -1;
    }

    return 0;
}

static int lvgl_touch_init(void)
{
    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    lisa_device_t *touch_dev = lisa_device_get(TOUCH_DEVICE);

    if (!lisa_device_ready(gpioa_dev) || !lisa_device_ready(i2c_dev) ||
        !lisa_device_ready(touch_dev)) {
        printf("LVGL: touch bus device not ready\n");
        return -1;
    }

    lisa_touch_bus_config_t bus_config = {
        .bus_type = LISA_TOUCH_BUS_I2C,
        .config = {.i2c = {
            .i2c_dev = i2c_dev,
            .int_gpio = gpioa_dev,
            .int_pin = LISA_TOUCH_I2C_INT_PIN,
            .rst_gpio = gpioa_dev,
            .rst_pin = LISA_TOUCH_I2C_RST_PIN,
        }},
    };

    int ret = lisa_touch_attach_bus(touch_dev, &bus_config);
    if (ret != 0) {
        printf("LVGL: touch bus attach failed (%d)\n", ret);
        return ret;
    }

    lv_port_indev_init(touch_dev);
    return 0;
}

static int lvgl_display_touch_init(void)
{
    lua_lvgl_lock_init();
    lv_init();

    int ret = lvgl_display_init();
    if (ret != 0) {
        return ret;
    }

    ret = lvgl_touch_init();
    if (ret != 0) {
        return ret;
    }

    lua_lvgl_lock();
    lv_timer_handler();
    lua_lvgl_unlock();
    lisa_display_blanking_off(lvgl_display_device);

    BaseType_t task_ret = xTaskCreate(lvgl_task, "lvgl_ui", LVGL_STACK_SIZE, NULL,
                                      configMAX_PRIORITIES - 2, NULL);
    if (task_ret != pdPASS) {
        printf("LVGL: failed to create ui task\n");
        return -1;
    }

    printf("LVGL display/touch ready\n");
    return 0;
}

/* ---------- Filesystem ---------- */

static struct lsfs_mount_t sdmmc_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = SDMMC_MOUNT_POINT,
    .fs_data = NULL,
};

static bool fs_mounted = false;

static int fs_init_and_mount(void)
{
    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);
    lvfs_init();
    lsfs_init();

    int ret = lsfs_mount(&sdmmc_mnt);
    if (ret != 0) {
        printf("Mount failed, formatting...\n");
        ret = lsfs_mkfs(LSFS_FATFS, SDMMC_DEVICE, NULL, 0);
        if (ret == 0) {
            ret = lsfs_mount(&sdmmc_mnt);
        }
    }

    if (ret == 0) {
        fs_mounted = true;
        printf("Mounted %s\n", SDMMC_MOUNT_POINT);
    } else {
        printf("Failed to mount filesystem: %d\n", ret);
    }
    return ret;
}

/* ---------- USB MSC ---------- */

static bool ejected = false;

void tud_mount_cb(void)   { ejected = false; }
void tud_umount_cb(void)  { }
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }
void tud_resume_cb(void)  { }

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                         uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;
    const char vid[] = "LISTENAI";
    const char pid[] = "Lua Scripts";
    const char rev[] = "1.0";
    memcpy(vendor_id,   vid, strlen(vid));
    memcpy(product_id,  pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void)lun;
    if (ejected) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    uint32_t sc = 0, ss = 0;

    if (disk_access_ioctl(pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &sc) != 0 ||
        disk_access_ioctl(pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &ss) != 0) {
        sc = 0x1000;
        ss = DISK_BLOCK_SIZE;
    }
    *block_count = sc;
    *block_size  = ss;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                            bool start, bool load_eject)
{
    (void)lun; (void)power_condition;
    if (load_eject) {
        if (!start) {
            if (disk_access_status(CONFIG_DISK_SDMMC_VOLUME_NAME) == DISK_STATUS_OK)
                ejected = true;
            else
                return false;
        } else {
            ejected = false;
        }
    }
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           void *buffer, uint32_t bufsize)
{
    (void)lun;
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    if (disk_access_status(pdrv) != DISK_STATUS_OK) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return -1;
    }
    if (disk_access_read(pdrv, buffer, lba, bufsize / DISK_BLOCK_SIZE) != 0)
        return -1;
    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                            uint8_t *buffer, uint32_t bufsize)
{
    (void)lun;
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    if (disk_access_status(pdrv) != DISK_STATUS_OK) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return -1;
    }
    if (disk_access_write(pdrv, buffer, lba, bufsize / DISK_BLOCK_SIZE) != 0)
        return -1;
    return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
    (void)lun;
    return disk_access_status(CONFIG_DISK_SDMMC_VOLUME_NAME) == DISK_STATUS_OK;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                         void *buffer, uint16_t bufsize)
{
    (void)buffer; (void)bufsize;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

static void usb_device_task(void *param)
{
    (void)param;
    tud_init(BOARD_TUD_RHPORT);
    while (1) {
        tud_task();
    }
}

static void usb_msc_init(void)
{
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    tud_disconnect();
    tusb_init();
    tud_connect();

    xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL,
                configMAX_PRIORITIES - 1, NULL);
}

/* ---------- Lua helpers ---------- */

static int lua_shell_readline(Shell *shell, char *buf, int size)
{
    int pos = 0;
    char ch;

    while (pos < size - 1) {
        if (shell->read(&ch, 1) != 1)
            continue;

        if (ch == '\r' || ch == '\n') {
            shell->write("\r\n", 2);
            break;
        }
        if (ch == 0x04 && pos == 0)
            return -1;
        if (ch == '\b' || ch == 0x7F) {
            if (pos > 0) { pos--; shell->write("\b \b", 3); }
            continue;
        }
        if (ch < 0x20)
            continue;

        buf[pos++] = ch;
        shell->write(&ch, 1);
    }
    buf[pos] = '\0';
    return pos;
}

/**
 * @brief Execute a Lua chunk and print results/errors to shell.
 */
static void lua_exec_and_print(Shell *shell, lua_State *L, int status)
{
    if (status != LUA_OK) {
        shellPrint(shell, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    int nresults = lua_gettop(L);
    if (nresults > 0) {
        luaL_checkstack(L, LUA_MINSTACK, "too many results");
        lua_getglobal(L, "tostring");
        for (int i = 1; i <= nresults; i++) {
            lua_pushvalue(L, -1);
            lua_pushvalue(L, i);
            lua_pcall(L, 1, 1, 0);
            const char *s = lua_tostring(L, -1);
            if (i > 1) shellPrint(shell, "\t");
            shellPrint(shell, "%s", s ? s : "");
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        shellPrint(shell, "\n");
    }
    lua_settop(L, 0);
}

/* ---------- Shell commands ---------- */

/**
 * @brief "lua" — enter interactive Lua REPL.
 */
static int cmd_lua(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();
    char line[LUA_LINE_MAXLEN];

    lua_State *L = luaL_newstate();
    if (!L) {
        shellPrint(shell, "Error: failed to create Lua state\n");
        return -1;
    }
    luaL_openlibs(L);
    luaopen_lisa_all(L);
    luaopen_lvgl_all(L);

    shellPrint(shell, "%s\n", LUA_COPYRIGHT);
    shellPrint(shell, "Type 'exit' or Ctrl-D to quit.\n");

    while (1) {
        shellPrint(shell, "> ");
        int len = lua_shell_readline(shell, line, sizeof(line));
        if (len < 0) { shellPrint(shell, "\n"); break; }
        if (len == 0) continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;

        /* Try as expression first, then as statement */
        char try_buf[LUA_LINE_MAXLEN + 8];
        snprintf(try_buf, sizeof(try_buf), "return %s", line);
        int status = luaL_loadstring(L, try_buf);
        if (status != LUA_OK) {
            lua_pop(L, 1);
            status = luaL_loadstring(L, line);
        }
        if (status != LUA_OK) {
            shellPrint(shell, "%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        }
        int base = lua_gettop(L);
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
        int nresults = lua_gettop(L) - base + 1;
        if (status != LUA_OK) {
            shellPrint(shell, "%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        } else if (nresults > 0) {
            lua_exec_and_print(shell, L, LUA_OK);
        }
    }

    lua_close(L);
    return 0;
}

/**
 * @brief "luarun <file>" — execute a .lua script from the SD card.
 */
static int cmd_luarun(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();

    if (argc < 2) {
        shellPrint(shell, "Usage: luarun <filename.lua>\n");
        return -1;
    }

    if (!fs_mounted) {
        shellPrint(shell, "Error: filesystem not mounted\n");
        return -1;
    }

    /* Build full path: argv[1] is the filename */
    char path[128];
    if (argv[1][0] == '/') {
        snprintf(path, sizeof(path), "%s", argv[1]);
    } else {
        snprintf(path, sizeof(path), "%s%s", LUA_SCRIPT_DIR, argv[1]);
    }

    /* Read file via lsfs */
    struct lsfs_file_t file;
    lsfs_file_t_init(&file);
    int ret = lsfs_open(&file, path, LSFS_O_READ);
    if (ret != 0) {
        shellPrint(shell, "Error: cannot open '%s' (%d)\n", path, ret);
        return -1;
    }

    ssize_t fsize = lsfs_lsize(&file);
    if (fsize <= 0 || fsize > LUA_SCRIPT_MAXLEN) {
        shellPrint(shell, "Error: invalid file size (%d)\n", (int)fsize);
        lsfs_close(&file);
        return -1;
    }

    char *script = pvPortMalloc(fsize + 1);
    if (!script) {
        shellPrint(shell, "Error: out of memory\n");
        lsfs_close(&file);
        return -1;
    }

    ssize_t nread = lsfs_read(&file, script, fsize);
    lsfs_close(&file);

    if (nread != fsize) {
        shellPrint(shell, "Error: read %d/%d bytes\n", (int)nread, (int)fsize);
        vPortFree(script);
        return -1;
    }
    script[fsize] = '\0';

    /* Execute */
    shellPrint(shell, "Running %s (%d bytes)...\n", path, (int)fsize);

    lua_State *L = luaL_newstate();
    if (!L) {
        shellPrint(shell, "Error: failed to create Lua state\n");
        vPortFree(script);
        return -1;
    }
    luaL_openlibs(L);
    luaopen_lisa_all(L);
    luaopen_lvgl_all(L);

    int status = luaL_loadbuffer(L, script, fsize, argv[1]);
    vPortFree(script);

    if (status == LUA_OK) {
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
    }
    lua_exec_and_print(shell, L, status);

    lua_close(L);
    return 0;
}

/**
 * @brief "luals [dir]" — list .lua files on the SD card.
 */
static int cmd_luals(int argc, char **argv)
{
    Shell *shell = shellGetCurrent();

    if (!fs_mounted) {
        shellPrint(shell, "Error: filesystem not mounted\n");
        return -1;
    }

    const char *dir = LUA_SCRIPT_DIR;
    char path[128];
    if (argc >= 2) {
        if (argv[1][0] == '/') {
            snprintf(path, sizeof(path), "%s", argv[1]);
        } else {
            snprintf(path, sizeof(path), "%s%s", LUA_SCRIPT_DIR, argv[1]);
        }
        dir = path;
    }

    struct lsfs_dir_t ldir;
    struct lsfs_dirent entry;
    lsfs_dir_t_init(&ldir);

    int ret = lsfs_opendir(&ldir, dir);
    if (ret != 0) {
        shellPrint(shell, "Error: cannot open directory '%s' (%d)\n", dir, ret);
        return -1;
    }

    shellPrint(shell, "Files in %s:\n", dir);
    int count = 0;
    while (1) {
        ret = lsfs_readdir(&ldir, &entry);
        if (ret != 0 || entry.name[0] == 0) break;

        if (entry.type == LSFS_DIR_ENTRY_DIR) {
            shellPrint(shell, "  <DIR>  %s\n", entry.name);
        } else {
            shellPrint(shell, "  %8lu  %s\n", entry.size, entry.name);
        }
        count++;
    }
    lsfs_closedir(&ldir);

    if (count == 0) {
        shellPrint(shell, "  (empty)\n");
    }
    return 0;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
    lua,
    cmd_lua,
    enter interactive Lua interpreter
);

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
    luarun,
    cmd_luarun,
    run a Lua script file from SD card
);

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
    luals,
    cmd_luals,
    list files on SD card
);

/* ---------- Main ---------- */

int main(int argc, char **argv)
{
    printf("Lua Scripting Shell + USB MSC\n");

    /* Mount filesystem */
    fs_init_and_mount();

    if (lvgl_display_touch_init() != 0) {
        printf("LVGL display/touch init failed; Lua LVGL scripts will not render\n");
    }

    /* Start USB MSC so host can copy .lua files */
    usb_msc_init();
    printf("USB MSC ready — connect USB to copy .lua scripts\n");

    /* Start shell */
    int ret = lisa_shell_init();
    if (ret != 0) {
        printf("Failed to initialize shell (error: %d)\n", ret);
        return ret;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    return 0;
}
