/**
 * @file    sd_music_card.c
 * @brief   SD 卡硬件操作层实现
 */

#include "sd_music_card.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TAG "sd_card"
#include "lisa_log.h"
#include "lisa_device.h"
#include "lisa_mutex.h"
#include "lisa_sdmmc.h"
#include "drv_sdc.h"
#include "lsfs.h"

/* ---- 常量 ---- */

#define SD_MUSIC_DISK_DEVICE   "SD:"
#define SD_MUSIC_MOUNT_POINT   "/" SD_MUSIC_DISK_DEVICE
#define SD_MUSIC_ROOT_DIR          SD_MUSIC_MOUNT_POINT "/"
#define SD_MUSIC_PROBE_SECTOR_SIZE 512

/* ---- 引脚复用 ---- */

#if defined(CONFIG_BOARD_ARCS_MINI)
#include "IOMuxManager.h"
void lisa_sdio_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 6, CSK_IOMUX_FUNC_ALTER15); /* CLK */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 7, CSK_IOMUX_FUNC_ALTER15); /* CMD */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER15); /* DAT0 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER15); /* DAT1 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 9, CSK_IOMUX_FUNC_ALTER15); /* DAT2 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 8, CSK_IOMUX_FUNC_ALTER15); /* DAT3 */
}
#endif /* CONFIG_BOARD_ARCS_MINI */

/* ---- SDMMC 驱动重置 ---- */

/**
 * LISA 层 priv_data 布局（与 lisa_sdmmc 驱动实现一致）。
 * 这里声明为不完整布局仅访问前两个字段。
 */
struct sdmmc_priv_layout {
    lisa_mutex_t *mutex;
    bool initialized;
};

void sd_music_card_sdmmc_reset(void)
{
    lisa_device_t *dev = lisa_device_get("sdmmc0");
    if (!dev || !dev->priv_data) {
        LOGI("无法获取 sdmmc0 设备");
        return;
    }

    /* 清零 LISA 层 initialized 标志 */
    struct sdmmc_priv_layout *priv = (struct sdmmc_priv_layout *)dev->priv_data;
    priv->initialized = false;

    /* 清零 HAL 层 sdcard_init_complete 标志 */
    u32 done_val = 0;
    gm_sdc_api_action(0, GM_SDC_ACTION_SET_APP_INIT_DONE, &done_val, NULL);
}

/* ---- 卡在位检测 ---- */

bool sd_music_card_test_read(const char *test_file_path)
{
    if (!test_file_path || test_file_path[0] == '\0') {
        return false;
    }

    struct lsfs_file_t file;
    lsfs_file_t_init(&file);
    if (lsfs_open(&file, test_file_path, LSFS_O_READ) == 0) {
        uint8_t dummy;
        int nread = lsfs_read(&file, &dummy, 1);
        lsfs_close(&file);
        return (nread > 0);
    }
    /* open 失败由上层通过物理扇区读兜底确认。 */
    return false;
}

static bool sd_music_card_test_sector(void)
{
    lisa_device_t *dev = lisa_device_get("sdmmc0");
    if (!dev) {
        return false;
    }

    uint8_t sector[SD_MUSIC_PROBE_SECTOR_SIZE] __attribute__((aligned(64)));
    return lisa_sdmmc_read(dev, sector, 0, 1) == 0;
}

bool sd_music_card_test_access(const char *test_file_path)
{
    if (test_file_path && test_file_path[0] != '\0' &&
        sd_music_card_test_read(test_file_path)) {
        return true;
    }

    return sd_music_card_test_sector();
}

bool sd_music_card_test_dir(const char *dir_path)
{
    const char *path = dir_path ? dir_path : SD_MUSIC_ROOT_DIR;

    struct lsfs_dir_t dir;
    lsfs_dir_t_init(&dir);
    if (lsfs_opendir(&dir, path) == 0) {
        lsfs_closedir(&dir);
        return true;
    }
    return false;
}

/* ---- CID / 容量 ---- */

void sd_music_card_get_cid_str(char *buf, int buf_len)
{
    if (!SDHost[0].Card) {
        if (buf && buf_len > 0) buf[0] = '\0';
        return;
    }
    u64 cid_hi = SDHost[0].Card->CID_HI;
    u64 cid_lo = SDHost[0].Card->CID_LO;
    if (cid_hi == 0 && cid_lo == 0) {
        if (buf && buf_len > 0) buf[0] = '\0';
        LOGW("SD 卡 CID 全 0，按无卡处理");
        return;
    }
    snprintf(buf, buf_len, "%08x%08x%08x%08x",
             (u32)(cid_hi >> 32), (u32)cid_hi,
             (u32)(cid_lo >> 32), (u32)cid_lo);
}

uint64_t sd_music_card_get_capacity(void)
{
    if (!SDHost[0].Card) return 0;
    return (uint64_t)SDHost[0].Card->numOfBlocks * SDHost[0].Card->read_block_len;
}

int sd_music_card_get_storage_usage(uint64_t *total_bytes, uint64_t *free_bytes)
{
    struct lsfs_statvfs stat = {0};
    int ret = lsfs_statvfs(SD_MUSIC_ROOT_DIR, &stat);
    if (ret != 0) {
        LOGW("获取 SD 卡文件系统空间失败: %d", ret);
        return ret;
    }

    uint64_t block_size = stat.f_frsize ? stat.f_frsize : stat.f_bsize;
    uint64_t total = (uint64_t)stat.f_blocks * block_size;
    uint64_t free = (uint64_t)stat.f_bfree * block_size;

    if (total == 0) {
        total = sd_music_card_get_capacity();
    }

    if (total_bytes) {
        *total_bytes = total;
    }
    if (free_bytes) {
        *free_bytes = free;
    }
    return 0;
}

void sd_music_card_log_info(void)
{
    if (!SDHost[0].Card) return;
    u64 cid_hi = SDHost[0].Card->CID_HI;
    u64 cid_lo = SDHost[0].Card->CID_LO;
    if (cid_hi == 0 && cid_lo == 0) {
        LOGW("SD卡信息无效：CID 全 0");
        return;
    }
    u32 blk_num = SDHost[0].Card->numOfBlocks;
    u32 blk_len = SDHost[0].Card->read_block_len;
    u64 cap_bytes = (u64)blk_num * blk_len;
    LOGI("SD卡信息 — CID=%08x%08x%08x%08x, 容量=%uMB",
         (u32)(cid_hi >> 32), (u32)cid_hi,
         (u32)(cid_lo >> 32), (u32)cid_lo,
         (u32)(cap_bytes >> 20));
}
