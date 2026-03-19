#ifndef __PLAYER_MGR_H__
#define __PLAYER_MGR_H__

#include <stdint.h>
#include <stdbool.h>
#include "audio_out.h"
#include "listen_audiomgr.h"
#include "play_mode.h"

typedef enum {
	AIP = 0,
	TTS = 1,
	ALERT = 2,
	CONTENT = 3,
	LOCAL = 4,
} DEFAULT_PLAY_ID;

#define MAX_CAPTURE_COUNT 8

/**
 * @brief 播放动作枚举
 */
typedef enum PLAY_ACTION {
    PLAY_ACTION_PLAY = 0,          // 播放
    PLAY_ACTION_PAUSE,             // 暂停（仅 audio_player 支持）
    PLAY_ACTION_STOP,              // 停止
    PLAY_ACTION_RECOGNIZE,         // 识别开始（AIP特殊）
    PLAY_ACTION_RECOGNIZE_END,     // 识别结束（AIP特殊）
} PLAY_ACTION;

/**
 * @brief 播放器配置结构体
 */
typedef struct player_config_s {
    int id;                                  // 播放器ID
    char *name;                              // 播放器名称
    int priority;                            // 优先级
    int capture_ids[MAX_CAPTURE_COUNT];      // 抢占列表
    int capture_count;                       // 抢占列表长度
    PLAY_ACTION fg_action;                   // 前景动作
    PLAY_ACTION bg_action;                   // 背景动作
    PLAY_ACTION none_action;                 // 失焦动作
} player_config_t;

/**
 * @brief 播放状态回调
 */
typedef void (*player_status_cb_t)(int player_id, uint16_t status, void *arg);

/**
 * @brief 焦点状态回调
 */
typedef void (*player_focus_cb_t)(int player_id, focus_state_e state, int by_which, void *arg);

/**
 * @brief 初始化播放管理器
 * @param configs 播放器配置数组
 * @param config_count 配置数量
 * @return 0 成功, 其他失败
 */
int player_mgr_init(player_config_t *configs, int config_count);

/**
 * @brief 注册播放状态回调
 * @param player_id 播放器ID
 * @param cb 回调函数
 * @param arg 用户参数
 * @return 0 成功, 其他失败
 */
int player_mgr_register_status_cb(int player_id, player_status_cb_t cb, void *arg);

/**
 * @brief 注册焦点状态回调
 * @param player_id 播放器ID
 * @param cb 回调函数
 * @param arg 用户参数
 * @return 0 成功, 其他失败
 */
int player_mgr_register_focus_cb(int player_id, player_focus_cb_t cb, void *arg);

/**
 * @brief 播放音频（打断模式）
 * @param player_id 播放器ID
 * @param url 音频URL
 * @param throw_time_ms 抛出时间（毫秒）
 * @return 0 成功, 其他失败
 */
int player_mgr_play(int player_id, const char *url, int throw_time_ms);

/**
 * @brief 播放音频（支持打断/追加模式）
 * @param player_id 播放器ID
 * @param item 音频项
 * @param interrupt true=打断，false=追加
 * @return 0 成功, 其他失败
 */
int player_mgr_play_item(int player_id, audio_out_t *item, bool interrupt);

/**
 * @brief 播放音频数组
 * @param player_id 播放器ID
 * @param items 音频项数组
 * @param count 数组长度
 * @return 0 成功, 其他失败
 */
int player_mgr_play_array(int player_id, const audio_out_t *items, int count);

/**
 * @brief 停止播放
 * @param player_id 播放器ID
 * @return 0 成功, 其他失败
 */
int player_mgr_stop(int player_id);

/**
 * @brief 暂停播放（仅 audio_player 支持）
 * @param player_id 播放器ID
 * @return 0 成功, 其他失败
 */
int player_mgr_pause(int player_id);

/**
 * @brief 恢复播放（仅 audio_player 支持）
 * @param player_id 播放器ID
 * @return 0 成功, 其他失败
 */
int player_mgr_resume(int player_id);

/**
 * @brief 临时暂停播放（仅 audio_player 支持，不标记用户主动暂停）
 * @param player_id 播放器ID
 * @return 0 成功, 其他失败
 */
int player_mgr_pause_temporary(int player_id);

/**
 * @brief 播放下一首（仅 audio_player 支持）
 * @param player_id 播放器ID
 * @return 0 成功, 其他失败
 */
int player_mgr_play_next(int player_id);

/**
 * @brief 播放上一首（仅 audio_player 支持）
 * @param player_id 播放器ID
 * @return 0 成功, 其他失败
 */
int player_mgr_play_prev(int player_id);

/**
 * @brief 切换播放模式（仅 audio_player 支持）
 * @param player_id 播放器ID
 * @param mode 播放模式
 * @return 0 成功, 其他失败
 */
int player_mgr_switch_playmode(int player_id, PLAY_MODE_E mode);

/**
 * @brief 设置音量
 * @param volume 音量值
 * @return 0 成功, 其他失败
 */
int player_mgr_set_volume(int volume);

/**
 * @brief 获取音量
 * @return 音量值
 */
int player_mgr_get_volume(void);

/**
 * @brief 获取播放器配置
 * @param player_id 播放器ID
 * @return 播放器配置
 */
player_config_t *player_mgr_get_config(int player_id);

/**
 * @brief 查询播放器是否处于播放中（含准备中/已准备）
 * @param player_id 播放器ID
 * @return true 正在播放流程中，false 未在播放流程中
 */
bool player_mgr_is_playing(int player_id);


#endif
