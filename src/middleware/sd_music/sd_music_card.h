/**
 * @file    sd_music_card.h
 * @brief   SD 卡硬件操作层 — 引脚复用、驱动复位、卡状态检测
 *
 * 本层负责所有与 SD 卡硬件直接交互的操作，包括：
 *   - SDIO 引脚复用配置
 *   - SDMMC 驱动 LISA/HAL 双重重置
 *   - 卡在位检测（通过文件 I/O 或物理扇区读，不依赖目录缓存）
 *   - CID 和容量读取
 *
 * 不依赖 HTTP、文件扫描等上层逻辑。
 */

#ifndef SD_MUSIC_CARD_H
#define SD_MUSIC_CARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief SD 卡 CID 128-bit 的十六进制字符串长度（含 null） */
#define SD_MUSIC_CARD_ID_LEN 33

/**
 * @brief 配置 SDIO 引脚复用
 *
 * 覆盖板级 SDK 中 __attribute__((weak)) 的空实现，
 * 由 lisa_sdmmc_probe() 内部自动调用。
 */
void sd_music_card_pinmux_init(void);

/**
 * @brief 重置 SDMMC 驱动初始化标志（LISA 层 + HAL 层）
 *
 * 卡拔出后两层的 initialized 标志均保持为 true，导致重插卡时
 * probe/detect 走快速路径跳过硬件初始化。本函数同时清零两层标志，
 * 确保下次 probe 走完整流程（CMD0/ACMD41/CMD2/CMD3 等）。
 */
void sd_music_card_sdmmc_reset(void);

/**
 * @brief 通过读取指定文件检测 SD 卡是否可访问
 *
 * 尝试 open + read 第一个字节，强制触发数据扇区物理 I/O，
 * 不受 FatFS 元数据缓存影响。
 *
 * @param test_file_path 测试用文件路径（如 /SD:/audio/xxx.mp3），
 *                       NULL 或空字符串直接返回 false
 * @return true 卡可访问，false 不可访问
 */
bool sd_music_card_test_read(const char *test_file_path);

/**
 * @brief 检测 SD 卡是否仍可真实访问
 *
 * 优先读取指定文件；当卡内没有 MP3 或文件探测失败时，直接读取
 * SD 物理 0 扇区，避免 opendir(/SD:/) 命中 FatFS 目录缓存导致
 * 拔卡后仍被误判为可访问。
 *
 * @param test_file_path 可选测试文件路径
 * @return true 卡可访问，false 不可访问
 */
bool sd_music_card_test_access(const char *test_file_path);

/**
 * @brief 通过 opendir 检测 SD 卡根目录是否可访问（降级检测）
 *
 * @param dir_path 目录路径，NULL 则使用 /SD:/
 * @return true 可访问，false 不可访问
 */
bool sd_music_card_test_dir(const char *dir_path);

/**
 * @brief 获取 SD 卡 CID 的十六进制字符串
 *
 * 无卡、驱动卡对象无效或 CID 全 0 时返回空字符串。
 *
 * @param buf     输出缓冲区
 * @param buf_len 缓冲区大小（建议 SD_MUSIC_CARD_ID_LEN）
 */
void sd_music_card_get_cid_str(char *buf, int buf_len);

/**
 * @brief 获取 SD 卡容量（字节）
 *
 * @return 容量字节数，无卡时返回 0
 */
uint64_t sd_music_card_get_capacity(void);

/**
 * @brief 获取 SD 卡文件系统容量和剩余空间（字节）
 *
 * @param total_bytes 输出总空间，可为 NULL
 * @param free_bytes  输出剩余空间，可为 NULL
 * @return 0 成功，负值失败
 */
int sd_music_card_get_storage_usage(uint64_t *total_bytes, uint64_t *free_bytes);

/**
 * @brief 打印 SD 卡 CID 和容量信息到日志
 */
void sd_music_card_log_info(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_MUSIC_CARD_H */
