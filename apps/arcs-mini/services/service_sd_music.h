/**
 * @file    service_sd_music.h
 * @brief   SD 卡离线音乐服务
 *
 * 职责：SD 卡插拔检测、MP3 文件扫描、云端事件/文件列表上报，
 *       并将本地可播放列表推送到 voice_music_list。
 *
 * SD 卡硬件初始化和文件系统挂载由 platform.c 在 SYS_INIT 阶段完成，
 * 本模块假设调用 service_sd_music_scan() 时 /SD:/ 已可访问。
 *
 * 本模块不负责播放控制——如何播放、何时播放、切歌策略等由上层
 *（voice_intent / MCP tool / player_event_callback）决定。
 */

#ifndef SERVICE_SD_MUSIC_H
#define SERVICE_SD_MUSIC_H

#include <stdbool.h>
#include <stddef.h>

#include "voice_music_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   初始化服务：创建互斥锁，异步处理启动 SD 状态并启动轮询线程
 * @return  0 成功拉起 sd_init 线程，-1 sd_init 线程创建失败
 *
 * @note    内部创建互斥锁保护扫描过程，扫描结果通过
 *          voice_music_list_set(MUSIC_LIST_OFFLINE) 同步到统一列表，
 *          本模块不持有曲目数据副本。
 */
int service_sd_music_init(void);

/**
 * @brief   重新扫描 SD 卡 MP3 文件
 * @param   path 扫描目录，NULL 使用默认路径 /SD:/audio/
 * @return  0 成功，负值失败
 *
 * @note    扫描结果直接同步到 voice_music_list 的 OFFLINE 子列表（替换旧列表）。
 *          调用者无需持锁，内部互斥。
 */
int service_sd_music_scan(const char *path);

/**
 * @brief   当前是否处于 TF 卡扫描/上报 UI 流程
 * @return  true 忽略按键和唤醒；false 正常处理
 */
bool service_sd_music_is_syncing(void);

#ifdef __cplusplus
}
#endif

#endif
