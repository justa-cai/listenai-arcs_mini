#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lsfs.h"
#include "lisa_device.h"
#include "lisa_mutex.h"
#include "lisa_sdmmc.h"
#include "drv_sdc.h"

#include "cmd.h"
#include "shell.h"

#define TAG "shell-sd"

#define SD_ROOT_DIR  "/SD:/"

#define SD_SCAN_MAX_DEPTH 8

/* platform.c 暴露的 SD 卡挂载结构体访问器 */
extern struct lsfs_mount_t *platform_sd_mount_get(void);

/**
 * @brief 重置 SDMMC 驱动初始化标志（LISA 层 + HAL 层）
 *
 * 解决卡拔出后重插时 probe 直接返回 OK 而不执行硬件初始化的 bug。
 *
 * LISA 层：清零 priv->initialized（解除 arcs_sdmmc_probe 的 DCLP 快速路径）
 * HAL 层：清零 sdcard_init_complete（解除 lib_sdc_detect 的快速路径）
 */
static void sd_cmd_sdmmc_reset(void)
{
	Shell *shell = shellGetCurrent();
	lisa_device_t *dev = lisa_device_get("sdmmc0");
	if (!dev || !dev->priv_data) {
		shellPrint(shell, "sd: 无法获取 sdmmc0 设备\r\n");
		return;
	}
	struct sdmmc_priv_layout { lisa_mutex_t *mutex; bool initialized; };
	struct sdmmc_priv_layout *priv =
		(struct sdmmc_priv_layout *)dev->priv_data;
	priv->initialized = false;

	/* 同时重置 HAL 层 sdcard_init_complete，确保 lib_sdc_detect() 重跑完整初始化 */
	u32 done_val = 0;
	gm_sdc_api_action(0, GM_SDC_ACTION_SET_APP_INIT_DONE, &done_val, NULL);
}

/**
 * @brief 尝试恢复 SD 卡：重置驱动 → 硬件探测 → 挂载文件系统
 * @return 0 成功，-1 失败
 */
static int sd_cmd_try_recover(void)
{
	Shell *shell = shellGetCurrent();
	shellPrint(shell, "sd: 尝试恢复 SD 卡...\r\n");

	sd_cmd_sdmmc_reset();

	lisa_device_t *dev = lisa_device_get("sdmmc0");
	if (!dev) {
		shellPrint(shell, "sd: 无法获取 sdmmc0 设备\r\n");
		return -1;
	}

	if (lisa_sdmmc_probe(dev) != 0) {
		shellPrint(shell, "sd: 硬件探测失败\r\n");
		return -1;
	}

	struct lsfs_mount_t *mp = platform_sd_mount_get();
	if (!mp) {
		shellPrint(shell, "sd: 无法获取挂载结构体\r\n");
		return -1;
	}

	if (lsfs_mount(mp) != 0) {
		shellPrint(shell, "sd: 文件系统挂载失败\r\n");
		return -1;
	}

	shellPrint(shell, "sd: SD 卡恢复成功\r\n");
	return 0;
}

static int sd_scan_dir(const char *base_path, int depth, int *file_count, int *dir_count)
{
	Shell *shell = shellGetCurrent();
	struct lsfs_dir_t dir;
	static struct lsfs_dirent entry;
	static char path_slots[SD_SCAN_MAX_DEPTH + 1][256];

	if (depth > SD_SCAN_MAX_DEPTH) {
		return 0;
	}

	lsfs_dir_t_init(&dir);
	if (lsfs_opendir(&dir, base_path) != 0) {
		shellPrint(shell, "  %*s无法打开目录\r\n", depth * 2, "");
		return -1;
	}

	while (lsfs_readdir(&dir, &entry) == 0) {
		/* FAT 目录结束标记：空名称 */
		if (entry.name[0] == '\0') {
			break;
		}
		/* 跳过已删除条目 */
		if (entry.name[0] == 0xE5) {
			continue;
		}

		shellPrint(shell, "  %*s", depth * 2, "");

		if (entry.type == LSFS_DIR_ENTRY_DIR) {
			shellPrint(shell, "[DIR]  %s\r\n", entry.name);
			(*dir_count)++;

			if (depth < SD_SCAN_MAX_DEPTH) {
				char *sub_path = path_slots[depth + 1];
				memset(sub_path, 0, sizeof(path_slots[depth + 1]));
				snprintf(sub_path, sizeof(path_slots[depth + 1]), "%s%s/", base_path, entry.name);
				sd_scan_dir(sub_path, depth + 1, file_count, dir_count);
			}
		} else {
			shellPrint(shell, "%s\r\n", entry.name);
			(*file_count)++;
		}
	}

	lsfs_closedir(&dir);
	return 0;
}

static int cmd_sd_scan(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	Shell *shell = shellGetCurrent();

	/* 直接尝试打开根目录 */
	struct lsfs_dir_t dir;
	lsfs_dir_t_init(&dir);
	if (lsfs_opendir(&dir, SD_ROOT_DIR) != 0) {
		shellPrint(shell, "SD卡未就绪，尝试恢复...\r\n");
		if (sd_cmd_try_recover() != 0) {
			shellPrint(shell, "SD卡恢复失败，请确认已插入 SD 卡\r\n");
			return -1;
		}
		/* 恢复成功后重新测试 */
		lsfs_dir_t_init(&dir);
		if (lsfs_opendir(&dir, SD_ROOT_DIR) != 0) {
			shellPrint(shell, "SD卡恢复后仍无法访问\r\n");
			return -1;
		}
	}
	lsfs_closedir(&dir);

	shellPrint(shell, "SD卡已就绪，扫描文件:\r\n");

	int file_count = 0;
	int dir_count = 0;
	sd_scan_dir(SD_ROOT_DIR, 0, &file_count, &dir_count);

	shellPrint(shell, "扫描完成: %d 个文件, %d 个目录\r\n", file_count, dir_count);
	return 0;
}

static int cmd_sd_help(int argc, char **argv);

static const struct listen_cmd_t g_sd_cmds[] = {
	{"scan",  cmd_sd_scan,  "扫描 SD 卡，列出根目录内容"},
	{"help",  cmd_sd_help,  NULL},
};

static int cmd_sd_help(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	Shell *shell = shellGetCurrent();
	int cmd_len = sizeof(g_sd_cmds) / sizeof(g_sd_cmds[0]);
	for (int i = 0; i < cmd_len; i++) {
		if (g_sd_cmds[i].help != NULL && strcmp(g_sd_cmds[i].name, "help") != 0) {
			shellPrint(shell, "%-16s\t:\t%s\r\n", g_sd_cmds[i].name, g_sd_cmds[i].help);
		}
	}
	return 0;
}

static int sd_cmd_handler(int argc, char **argv)
{
	if (argc <= 1) {
		cmd_sd_help(argc, argv);
		return 0;
	}

	for (int i = 0; i < sizeof(g_sd_cmds) / sizeof(g_sd_cmds[0]); i++) {
		if (strcmp(g_sd_cmds[i].name, argv[1]) == 0) {
			return g_sd_cmds[i].exec(argc - 2, argv + 2);
		}
	}

	cmd_sd_help(argc, argv);
	return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 sd,
                 sd_cmd_handler,
                 SD 卡调试命令);
