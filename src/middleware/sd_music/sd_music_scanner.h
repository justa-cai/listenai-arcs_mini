/**
 * @file    sd_music_scanner.h
 * @brief   SD 卡 MP3 文件扫描层 — 目录遍历、ID3v1 标签解析
 *
 * 本层负责：
 *   - 递归扫描目录中的 MP3 文件
 *   - ID3v1 标签读取与编码转换（GBK→UTF-8）
 *
 * 不依赖 HTTP、SD 卡硬件操作。仅依赖 lsfs 文件系统 API 和 voice_music_list 类型定义。
 *
 * 扫描策略：
 *   扫描（sd_music_scanner_scan）采用单次遍历 + 批回调模式，
 *   每收集 BATCH_MAX 个文件回调一次；调用方可用 max_total 控制扫描上限。
 *   避免在内存中缓存全部文件列表，适合资源受限的嵌入式设备。
 */

#ifndef SD_MUSIC_SCANNER_H
#define SD_MUSIC_SCANNER_H

#include <stdbool.h>
#include <stdint.h>

#include "voice_music_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 扫描路径字符串最大长度 */
#define SD_MUSIC_SCANNER_MAX_PATH AUIDO_OUT_URL_LEN

/** @brief 扫描单批最大文件数 */
#define SD_MUSIC_SCANNER_BATCH_MAX 100

/** @brief 本地播放列表默认最大累积文件数 */
#define SD_MUSIC_SCANNER_MAX_TOTAL 100

/**
 * @brief 扫描回调类型
 *
 * 扫描器每收集到一批文件时调用，或扫描结束时调用（剩余文件 + is_last=true）。
 *
 * @param tracks     本批文件数组（静态缓冲区，回调返回后内容可能被覆盖）
 * @param count      本批文件数量，结束通知批次可为 0
 * @param is_last    是否为最后一批
 * @param user_data  透传数据
 * @return 0 继续扫描，非 0 终止扫描
 */
typedef int (*sd_music_scanner_batch_cb)(music_item_t *tracks, int count,
                                          bool is_last, void *user_data);

typedef struct {
    uint32_t reported_count;      /**< 已回调给调用方的 MP3 文件数 */
    uint32_t skipped_count;       /**< 因文件名、目录名、层级或路径限制未上报的 MP3 文件数 */
    uint32_t unknown_skip_count;  /**< 无法展开统计的跳过目录数 */
} sd_music_scanner_stats_t;

/**
 * @brief 扫描 MP3 文件，逐批回调
 *
 * 单次遍历目录树，每收集 @ref SD_MUSIC_SCANNER_BATCH_MAX 个文件调用一次回调，
 * 或扫描结束时回调剩余文件（is_last=true）。
 * 最多扫描 max_total 个文件，达到上限后回调最后一批并停止。
 * max_total <= 0 时不限制扫描总数。
 *
 * @param path       扫描目录路径（NULL 或空字符串使用默认路径 /SD:/audio/，
 *                   默认路径不存在时降级到 /SD:/）
 * @param max_total  最大扫描文件数；本地播放列表建议 SD_MUSIC_SCANNER_MAX_TOTAL，
 *                   云端全量上报可传 0
 * @param callback   每批回调（扫描器线程内同步调用）
 * @param user_data  回调透传数据
 * @param stats      输出扫描统计，可为 NULL
 * @return 0 成功，负值失败
 */
int sd_music_scanner_scan(const char *path, int max_total,
                           sd_music_scanner_batch_cb callback,
                           void *user_data,
                           sd_music_scanner_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* SD_MUSIC_SCANNER_H */
