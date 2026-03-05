/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "lsc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup session_voice 闲聊会话
 *
 * @{
 */

/**
 * @brief 定义音乐列表项结构体
 *
 * @param id 音乐id
 *
 */
typedef struct {
	char id[64];
} session_voice_music_item_t;

/**
 * @brief 定义音乐列表结构体
 *
 * @param cnt 列表数量
 * @param items 列表项
 *
 */
typedef struct {
	uint32_t cnt;
	session_voice_music_item_t items[];
} session_voice_music_lists_t;

/**
 * @brief 定义音乐控制指令类型
 *
 */
typedef enum {
	/** 下一首 */
	SESSION_VOICE_MUSIC_INSTR_NEXT,
	/** 上一首 */
	SESSION_VOICE_MUSIC_INSTR_PAST,
	/** 重播 */
	SESSION_VOICE_MUSIC_INSTR_REPLAY,
	/** 停止播放 */
	SESSION_VOICE_MUSIC_INSTR_CLOSE,
	/** 音量设置 */
	SESSION_VOICE_MUSIC_INSTR_VOL_SET,
} session_voice_music_instr_e;

/**
 * @brief 定义音乐控制指令结构体
 *
 * @param instr 指令类型
 * @param arg 指令参数
 *
 */
typedef struct {
	session_voice_music_instr_e instr;
	int arg;
} session_voice_music_instr_t;

/**
 * @brief 定义会话事件类型
 *
 *
 */
typedef enum {
	/** 识别到VAD */
	SESSION_VOICE_GOT_VAD = BIT(0),
	/** 语义TTS结果 */
	SESSION_VOICE_TTS = BIT(2),
	/** 语音识别结果 */
	SESSION_VOICE_IAT = BIT(3),
	/** 当前会话结束 */
	SESSION_VOICE_FINISH = BIT(4),
	/** 会话超时 */
	SESSION_VOICE_TIMEOUT = BIT(5),
	/** 音乐列表 */
	SESSION_VOICE_MUSIC_LISTS = BIT(8),
	/** 音乐控制指令 */
	SESSION_VOICE_MUSIC_INSTR = BIT(9),
	/** 语音文本结果 */
	SESSION_VOICE_REPLY_URL = BIT(10),
	/** 生图结果 */
	SESSION_VOICE_DRAW = BIT(11),
	/** AIUI技能控制 */
	SESSION_VOICE_AIUI_CTRL = BIT(12),
	/** 会话错误 */
	SESSION_VOICE_ERR = BIT(13),
	/** 闹钟意图 */
	SESSION_VOICE_ALARM_INTENT = BIT(14),
	/** 识别到VAD */
	SESSION_VOICE_IAT_START = BIT(15),
	/** 识别到VAD */
	SESSION_VOICE_IAT_END = BIT(16),
	/** 声纹信息(男女老少)*/
	SESSION_VOICE_VPR_INFO = BIT(19),
	/** 声纹ID*/
	SESSION_VOICE_VPR_FEATURE = BIT(20),
	/** 原始结果数据 */
	SESSION_VOICE_RAW_DATA = BIT(30),
} session_voice_event_e;

/**
 * @typedef session_voice_event_cb_t
 * 定义回调函数类型，不同事件类型对应的data数据类型不同。
 * 事件类型和对应的data数据类型如下：
 *
 * 事件类型                  | data类型
 * --------------------------|-----------------
 * SESSION_VOICE_GOT_VAD     | NULL
 * SESSION_VOICE_TTS        | char*
 * SESSION_VOICE_IAT        | char*
 * SESSION_VOICE_FINISH     | NULL
 * SESSION_VOICE_MUSIC_LISTS | session_voice_music_lists_t
 * SESSION_VOICE_MUSIC_INSTR | session_voice_music_instr_t
 * SESSION_VOICE_REPLY_URL  | char*
 * SESSION_VOICE_DRAW       | char*
 * SESSION_VOICE_AIUI_CTRL   | char*
 * SESSION_VOICE_ERR        | NULL
 *
 * @param evt 事件类型
 * @param data 事件数据
 * @param size 事件数据长度
 * @param usr 用户参数
 */
typedef void (*session_voice_event_cb_t)(session_voice_event_e evt, void *data, uint32_t size, void *usr);

/**
 * @brief 定义交互模式
 *
 */
typedef enum {
	/** 单工模式 */
	SESSION_VOICE_INTER_MODE_ONESHOT = 0,
	/** 全双工模式 */
	SESSION_VOICE_INTER_MODE_CONTINUE = 1,
	SESSION_VOICE_INTER_MODE_MAX = 2,
} session_voice_interactive_mode_e;

/**
 * @brief 定义初始化参数类型
 *
 */
typedef struct {
	/** 交互模式 */
	session_voice_interactive_mode_e inter_mode;
	/** 会话超时时间 */
	int session_timeout_ms;
	/** 是否开启云端vad */
	bool vad_enable;
	/** vad检测超时时间 */
	int vad_timeout_ms;
	/** 设备id */
	char device_id[64];
	/** 音频编码格式: raw|ico|speex|speex-wb */
	char aue[16];
	/** 使用speex编码必须定义此字段的值 */
	int speex_size;
	bool oneshot;
	char **words;
	uint32_t words_cnt;
} session_voice_config_t;

/**
 * @brief 会话初始化
 *
 * @retval 0： 成功
 */
int session_voice_init(void);

/**
 * @brief 会话逆初始化
 *
 * @retval 0： 成功
 */
int session_voice_deinit(void);

/**
 * @brief 参数设置
 *
 * @param cfg 配置参数
 *
 * @retval 0： 设置成功
 */
int session_voice_set_config(session_voice_config_t *cfg);

/**
 * @brief 获取设置参数
 *
 * @retval cfg： 配置参数
 */
int session_voice_get_config(session_voice_config_t *cfg);

/**
 * @brief 添加事件回调函数
 * 支持多次添加回调函数
 *
 * @param cb 回调函数
 * @param evt 监听的事件，参考@see session_voice_event_e
 * @param usr 用户参数
 *
 * @retval 0： 成功
 */
int session_voice_add_evt_callback(session_voice_event_cb_t cb, session_voice_event_e evt, void *usr);

/**
 * @brief 移除已注册的事件回调函数
 *
 * @param cb 已注册的回调函数
 *
 * @retval 0： 成功
 */
int session_voice_remove_evt_callback(session_voice_event_cb_t cb);

/**
 * @brief 启动会话
 *
 * @retval 0： 成功
 */
int session_voice_start(void);

/**
 * @brief 中止会话
 *
 * @retval 0： 成功
 */
int session_voice_cancel(void);

/**
 * @brief 发送音频
 *
 * 通过该接口，将语音数据发送到云端处理。
 *
 * @note 支持流式发送，音频格式必须为16k 16bit 单通道 PCM格式
 *
 * @param data 音频数据
 * @param size 音频大小
 *
 * @retval 0： 成功
 */
int session_voice_send_audio(uint8_t *data, uint32_t size);

/**
 * @brief 主动中止音频发送
 *
 * 当未开启云端vad功能时，客户端可以主动结束音频发送，云端会结束识别并进行对应的处理
 *
 * @retval 0： 成功
 */
int session_voice_end_audio(void);

/**
 * @brief 设置当前会话使用的模型ID
 *
 * 该函数允许应用层设置当前会话使用的模型ID，该ID将在会话开始时发送给服务器
 * 如果未设置，则使用默认模型
 *
 * @param model_id 模型ID字符串，如果为NULL则使用默认模型
 *
 * @retval 0： 成功
 */
int session_voice_set_model_id(const char *model_id);

/**
 * @brief 获取当前会话使用的模型ID
 *
 * @param model_id 输出参数，用于存储模型ID
 * @param max_len model_id缓冲区的最大长度
 *
 * @retval 0： 成功
 */
int session_voice_get_model_id(char *model_id, size_t max_len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif