/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Sample-local pinmux / display / touch 初始化。
 * 参考 samples/media/lvgl/lvgl8/widgets/src/main.c 的模式：
 *   - 应用层覆写 arcs_evb 默认 pinmux (spi1 / gpioa / gpiob / i2c0 / pwm)，
 *     以便 LCD_CS(PB5) 作为 GPIO 由应用驱动，而不是 SPI 硬件 CS；
 *   - 不经过 board_* 默认配置接口，直接构造 lisa_display_config_t /
 *     lisa_touch_bus_config_t。
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_touch.h"
#include "lisa_thread.h"
#include "lisa_mem.h"
#include "lisa_gpio.h"
#include "lisa_sdmmc.h"

#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "lvgl.h"

#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "IOMuxManager.h"
#include "ClockManager.h"
#include "board.h"
#include "log_print.h"
#include "tusb.h"
#include "arcs_ap.h"
#include "disk/disk_access.h"
#include <disk/disk.h>

#include "lsfs.h"
#include "lvfs.h"

#include "app_context.h"
#include "ui_main_menu.h"
#include "ui_playback.h"
#include "wifi_utils.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "avi_player"
#include <lisa_log.h>

#define TOUCH_DEVICE        "touch_cst328"
#define I2C_DEVICE          "i2c0"
/* SDMMC_MOUNT_POINT 与 AUDIO_DEVICE_NAME 由 app_context.h 定义 */

#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE)
#define DISK_BLOCK_SIZE 512

/* ================================================================
 * Board Pin Mux override (SPI LCD + CST328 touch on arcs_evb)
 * 与 samples/media/lvgl/lvgl8/widgets 的覆写保持一致，差异点：
 *   - 同样是 SPI1 4-wire + PB5 软 CS + PA0 PWM 背光；
 *   - avi_player 不改 SDIO / UART / audio，这些沿用 board 默认 pinmux。
 * ================================================================ */
#ifdef CONFIG_BOARD_ARCS_EVB

#define LCD_CS_PIN          5
#define LCD_SPI_CLK_PIN     3
#define LCD_SPI_DATA_PIN    1

/* Touch I2C 引脚定义 */
#define LISA_TOUCH_I2C_SDA_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SDA_PIN   22
#define LISA_TOUCH_I2C_SDA_FUNC  8

#define LISA_TOUCH_I2C_SCL_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_SCL_PIN   23
#define LISA_TOUCH_I2C_SCL_FUNC  8

/* Touch GPIO 引脚定义 */
#define LISA_TOUCH_I2C_RST_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_RST_PIN   25
#define LISA_TOUCH_I2C_RST_FUNC  0

#define LISA_TOUCH_I2C_INT_PORT  CSK_IOMUX_PAD_A
#define LISA_TOUCH_I2C_INT_PIN   24
#define LISA_TOUCH_I2C_INT_FUNC  0

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_RST_PIN, CSK_IOMUX_FUNC_ALTER1);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_RST_PORT, LISA_TOUCH_I2C_RST_PIN, LISA_TOUCH_I2C_RST_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_INT_PORT, LISA_TOUCH_I2C_INT_PIN, LISA_TOUCH_I2C_INT_FUNC);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, PA_EN_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_TE_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CD_PIN, CSK_IOMUX_FUNC_DEFAULT);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}

void lisa_i2c0_pinmux()
{
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SDA_PORT, LISA_TOUCH_I2C_SDA_PIN, LISA_TOUCH_I2C_SDA_FUNC);
    IOMuxManager_PinConfigure(LISA_TOUCH_I2C_SCL_PORT, LISA_TOUCH_I2C_SCL_PIN, LISA_TOUCH_I2C_SCL_FUNC);
}

void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, LCD_SPI_DATA_PIN, CSK_IOMUX_FUNC_ALTER6);
}

void lisa_pwm_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, CSK_IOMUX_FUNC_ALTER12);
}
#endif /* CONFIG_BOARD_ARCS_EVB */

lisa_device_t *display_device = NULL;

/* ================================================================
 * USB MSC Callbacks (required by TinyUSB linker)
 * ================================================================ */

static bool ejected = false;

void tud_mount_cb(void)         { ejected = false; }
void tud_umount_cb(void)        { }
void tud_suspend_cb(bool rw)    { (void)rw; }
void tud_resume_cb(void)        { }

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;
    memcpy(vendor_id, "TinyUSB", 7);
    memcpy(product_id, "Mass Storage", 12);
    memcpy(product_rev, "1.0", 3);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void)lun;
    if (ejected) { tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00); return false; }
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    uint32_t sc = 0, ss = 0;
    if (disk_access_ioctl(pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &sc) != 0 ||
        disk_access_ioctl(pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &ss) != 0) {
        sc = 0x1000; ss = DISK_BLOCK_SIZE;
    }
    *block_count = sc; *block_size = ss;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t pc, bool start, bool load_eject)
{
    (void)lun; (void)pc;
    if (load_eject) {
        if (!start) {
            if (disk_access_status(CONFIG_DISK_SDMMC_VOLUME_NAME) == DISK_STATUS_OK) ejected = true;
            else return false;
        } else { ejected = false; }
    }
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    (void)lun;
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    if (disk_access_status(pdrv) != DISK_STATUS_OK) { tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00); return -1; }
    if (disk_access_read(pdrv, buffer, lba, bufsize / DISK_BLOCK_SIZE) != 0) { tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x03, 0x00); return -1; }
    return bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    (void)lun;
    const char *pdrv = CONFIG_DISK_SDMMC_VOLUME_NAME;
    if (disk_access_status(pdrv) != DISK_STATUS_OK) { tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00); return -1; }
    if (disk_access_write(pdrv, buffer, lba, bufsize / DISK_BLOCK_SIZE) != 0) { tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x03, 0x00); return -1; }
    return bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) { (void)lun; return disk_access_status(CONFIG_DISK_SDMMC_VOLUME_NAME) == DISK_STATUS_OK; }

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    (void)buffer; (void)bufsize;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

void usb_device_task(void *param)
{
    (void)param;
    tud_init(BOARD_TUD_RHPORT);
    while (1) { tud_task(); }
}

void user_usbd_msc_init(void)
{
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
    tud_disconnect();
    tusb_init();
    tud_connect();
}

/* ================================================================
 * SDMMC / Filesystem / Speaker PA
 * ================================================================ */

struct lsfs_mount_t sdmmc_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = SDMMC_MOUNT_POINT,
    .fs_data = NULL,
};

static int board_storage_init(void)
{
    lisa_device_t *sdmmc = lisa_device_get("sdmmc0");
    if (!sdmmc) { LISA_LOGE(LOG_TAG, "No sdmmc device"); return -1; }
    if (lisa_sdmmc_probe(sdmmc) != LISA_DEVICE_OK) { LISA_LOGE(LOG_TAG, "Disk probe fail"); return -1; }
    if (lisa_sdmmc_status(sdmmc) != LISA_SDMMC_STATUS_OK) { LISA_LOGE(LOG_TAG, "Disk not ready"); return -1; }

    uint32_t sc, ss;
    lisa_sdmmc_get_sector_count(sdmmc, &sc);
    lisa_sdmmc_get_sector_size(sdmmc, &ss);
    LISA_LOGI(LOG_TAG, "SD: %u sectors, %llu MB", sc, (uint64_t)sc * ss / (1024*1024));

    disk_init(NULL);
    lvfs_init();
    lsfs_init();

    int ret = lsfs_mount(&sdmmc_mnt);
    if (ret != 0) {
        LISA_LOGW(LOG_TAG, "Mount failed, formatting...");
        lsfs_mkfs(LSFS_FATFS, "SD:", NULL, 0);
        ret = lsfs_mount(&sdmmc_mnt);
    }
    if (ret != 0) { LISA_LOGE(LOG_TAG, "FS mount failed: %d", ret); return ret; }
    LISA_LOGI(LOG_TAG, "FS mounted");

    /* 确保 README 约定的目录结构存在：Video/、URL/、WiFi/
     * 首次上电或格式化后用户不用再手动建目录，直接丢 avi / ini 即可。
     */
    static const char * const required_dirs[] = {
        SDMMC_MOUNT_POINT "/Video",
        SDMMC_MOUNT_POINT "/URL",
        SDMMC_MOUNT_POINT "/WiFi",
    };
    for (size_t i = 0; i < sizeof(required_dirs) / sizeof(required_dirs[0]); ++i) {
        struct lsfs_dirent ent;
        if (lsfs_stat(required_dirs[i], &ent) == 0) {
            continue;   /* 已存在，跳过 */
        }
        int mret = lsfs_mkdir(required_dirs[i]);
        if (mret == 0) {
            LISA_LOGI(LOG_TAG, "Created %s", required_dirs[i]);
        } else {
            LISA_LOGW(LOG_TAG, "mkdir %s failed: %d", required_dirs[i], mret);
        }
    }

    /* Speaker PA enable (PA_EN_PIN on gpioa) */
    lisa_device_t *gpio_dev = lisa_device_get("gpioa");
    if (gpio_dev && lisa_device_ready(gpio_dev)) {
        lisa_gpio_configure(gpio_dev, PA_EN_PIN, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_LOW);
        lisa_gpio_write_pin(gpio_dev, PA_EN_PIN, LISA_GPIO_HIGH);
    }

    return 0;
}

/* ================================================================
 * UI Task - same structure as lvgl8_widgets task_ui
 * ================================================================ */

static void task_ui(void *pvParameters)
{
    (void)pvParameters;

    /* Build our main menu (replaces lv_demo_widgets) */
    ui_main_menu_create();

    LISA_LOGI(LOG_TAG, "UI ready");

    // 避免开机花屏
    lv_task_handler();

    while (1) {
        /* During playback, refresh video frame from render context */
        app_context_t *ctx = app_ctx_get();
        if (ctx->current_screen == SCREEN_PLAYBACK) {
            avi_lvgl_render_ctx_t *rctx = &ctx->render_ctx;
            if (rctx->lock && xSemaphoreTake(rctx->lock, pdMS_TO_TICKS(5)) == pdTRUE) {
                if (rctx->frame_dirty) {
                    ui_playback_refresh_frame();
                    rctx->frame_dirty = false;
                }
                xSemaphoreGive(rctx->lock);
            }
        }

        uint32_t wait_time = lv_task_handler();
        if (wait_time == 0 || wait_time > 20) {
            wait_time = 20;
        }
        lisa_thread_mdelay(wait_time);
    }
}

/* ================================================================
 * main() - Display/Touch init 与 lvgl8_widgets sample 对齐，
 *          外加 storage / wifi / audio 初始化。
 * ================================================================ */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    LISA_LOGI(LOG_TAG, "Lisa Media Player starting");

    /* --- 1. Storage init (before display, non-display stuff) --- */
    board_storage_init();

    /* --- 2. Display + Touch --- */
    lv_init();

    lisa_device_t *gpioa_dev = lisa_device_get("gpioa");
    lisa_device_t *gpiob_dev = lisa_device_get("gpiob");

    lisa_display_config_t display_config = {
        .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
        .bus_config = {.spi_4wire =
                           {
                               .spi_dev = lisa_device_get("spi1"),
                               .cs_gpio = gpiob_dev,
                               .cs_pin = LCD_CS_PIN,
                               .dc_gpio = gpiob_dev,
                               .dc_pin = LCD_CD_PIN,
                               .spi_freq = 50 * 1000 * 1000,
                           }},
        .backlight = {.type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
                      .blacklight_polarity = LISA_DISPLAY_BLACKLIGHT_POLARITY_LOW,
                      .config.pwm = {.channel = 0, .dev = lisa_device_get("pwm0"), .freq = 2000}},
        .rst_gpio = gpioa_dev,
        .rst_pin = LCD_RST_PIN,
    };

    display_device = lisa_device_get("display");
    if (!display_device) {
        LISA_LOGE(LOG_TAG, "Failed to get display device");
        return -1;
    }

    if (lisa_display_attach_bus(display_device, &display_config) != 0) {
        LISA_LOGE(LOG_TAG, "lisa_display_attach_bus failed");
        return -1;
    }
    lv_port_disp_init(display_device);

    /* --- Touch --- */
    lisa_device_t *touch_dev = lisa_device_get(TOUCH_DEVICE);
    if (!lisa_device_ready(touch_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", TOUCH_DEVICE);
        return -1;
    }

    lisa_device_t *i2c_dev = lisa_device_get(I2C_DEVICE);
    if (!lisa_device_ready(i2c_dev)) {
        LISA_LOGE(LOG_TAG, "Error: %s device not ready", I2C_DEVICE);
        return -1;
    }

    lisa_display_blanking_off(display_device);

    lisa_touch_bus_config_t bus_config = {
        .bus_type = LISA_TOUCH_BUS_I2C,
        .config = {
            .i2c = {
                .i2c_dev = i2c_dev,
                .int_gpio = gpioa_dev,
                .int_pin = LISA_TOUCH_I2C_INT_PIN,
                .rst_gpio = gpioa_dev,
                .rst_pin = LISA_TOUCH_I2C_RST_PIN,
            }
        }
    };
    int ret = lisa_touch_attach_bus(touch_dev, &bus_config);
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "Error: Touch bus attach failed (code: %d)", ret);
        return -1;
    }
    lv_port_indev_init(touch_dev);

    lisa_display_set_brightness(display_device, 90);

    /* --- 3. App context + WiFi --- */
    app_ctx_init(display_device, lisa_device_get(AUDIO_DEVICE_NAME));
    wifi_utils_init();

    /* --- 4. Start UI task (same pattern as lvgl8_widgets) --- */
    xTaskCreate(task_ui, "task_ui", 4 * 1024, NULL, configMAX_PRIORITIES - 2, NULL);

    LISA_LOGI(LOG_TAG, "UI task started");
    return 0;
}
