/**
 * @file    sd_music_http.h
 * @brief   SD 卡云端上报层 — TF 卡事件和文件列表上报
 *
 * 本层负责：
 *   - TF 卡插入/移除事件上报（POST /v1/media/tf-card/events）
 *   - 音频文件列表分批导入（POST /v1/media/tf-card/files/import）
 *   - JWT 鉴权 header 构建
 *   - API URL 构建（含 staging/integration 环境后缀）
 *
 * 不依赖文件扫描、卡硬件操作。仅从 voice_music_list 读取曲目数据用于上报。
 *
 * 流式上报：
 *   编排层先将当前批次曲目通过 voice_music_list_set() 写入 OFFLINE 列表，
 *   再调用本函数上报。每批最多 SD_MUSIC_HTTP_FILES_BATCH_MAX 个文件；
 *   HTTP 层会在构造 JSON 前清洗 UTF-8 字段。
 *   文件列表使用 HTTPC_request_r() 在单次 HTTP 请求内流式发送 body，
 *   不受 lisa_http 小 body 封装限制；较小请求体会直接使用普通 POST。
 *   最后一批标记 is_last_batch=true 触发服务端处理差异并补全元数据。
 *   同一扫描任务使用相同的 scan_id（由编排层生成）。
 */

#ifndef SD_MUSIC_HTTP_H
#define SD_MUSIC_HTTP_H

#include <stdbool.h>
#include <stdint.h>

#include "voice_music_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 文件导入单批最大数量 */
#define SD_MUSIC_HTTP_FILES_BATCH_MAX 100

/**
 * @brief 上报 TF 卡插入/移除事件
 *
 * POST /v1/media/tf-card/events
 *
 * @param card_id    SD 卡 CID 十六进制字符串
 * @param event_type "inserted" 或 "removed"
 * @param capacity   卡容量（字节），仅 inserted 事件使用
 * @param http_status_code HTTP 响应码输出，0 表示未获取；可为 NULL
 * @param http_error_code  HTTP 客户端错误码输出，0 表示无；可为 NULL
 * @return 0 成功，负值失败
 */
int sd_music_http_report_card_event(const char *card_id, const char *event_type,
                                    uint64_t capacity, int *http_status_code,
                                    int *http_error_code);

/**
 * @brief 上报一批音频文件列表（从 voice_music_list 读取）
 *
 * POST /v1/media/tf-card/files/import
 *
 * 调用前需先将本批曲目通过 voice_music_list_set(MUSIC_LIST_OFFLINE, ...)
 * 写入 OFFLINE 列表。本函数从列表中读取曲目数据，并在单次 HTTP
 * 请求内按 HTTP 客户端缓冲大小流式发送。
 *
 * @param card_id       SD 卡 CID 十六进制字符串
 * @param scan_id       本次扫描任务 ID（同一扫描任务保持不变）
 * @param count         当前 OFFLINE 列表中待上报的曲目数
 * @param is_last_batch 是否为最后一批
 * @param http_status_code HTTP 响应码输出，0 表示未获取；可为 NULL
 * @param http_error_code  HTTP 客户端错误码输出，0 表示无；可为 NULL
 * @return 0 成功，负值失败
 */
int sd_music_http_report_files(const char *card_id, const char *scan_id,
                                int count, bool is_last_batch,
                                int *http_status_code,
                                int *http_error_code);

#ifdef __cplusplus
}
#endif

#endif /* SD_MUSIC_HTTP_H */
