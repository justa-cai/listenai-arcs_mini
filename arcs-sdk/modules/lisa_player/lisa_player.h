/**
 * @brief 播放器接口
 * @author mokee
 * @date 2022-05-10
 */

#ifndef __LISTENAI_PLAYER_INTERFACE_H__
#define __LISTENAI_PLAYER_INTERFACE_H__

#include <stdint.h>

#define PLAYER_HANDLE void *

typedef enum PlayerState {
	PLAYER_ST_ERROR = 0,  // 错误
	PLAYER_ST_NONE,
	PLAYER_ST_READY_TO_PLAY, //准备去播
	PLAYER_ST_PREPARED,  // 缓冲完成
	PLAYER_ST_PLAYING,  // 播放中
	PLAYER_ST_PAUSED,  // 暂停
	PLAYER_ST_STOPED,  // 停止
	PLAYER_ST_PLAYBACK_COMPLETE,  // 播放完成
} PlayerState;

typedef enum PlayerEvt {
	PLAYER_EVT_ERROR = 0,  // 播放错误
	PLAYER_EVT_PREPARED,  // 缓冲完成
	PLAYER_EVT_PLAYING,  // 播放中
	PLAYER_EVT_PAUSED,  // 已暂停
	PLAYER_EVT_STOPED,  // 已停止
	PLAYER_EVT_NEARLY_COMPLETE,  // 即将播放完成
	PLAYER_EVT_PLAYBACK_COMPLETE,  // 播放完成
	PLAYER_EVT_SEEKING,  // Seek中
	PLAYER_EVT_SEEK_COMPLETE,  // Seek完成

	PLAYER_EVT_INIT = 100, //  初始化状态
} PlayerEvt;

typedef enum PlayerErr {
	PLAYER_OK = 0,  // Normal
	PLAYER_ERR = -1,  // Common error
	PLAYER_OP_FAIL = -2,  // Operation fail
	PLAYER_NO_MEMORY = -3,  // Not enough memory
	PLAYER_INVALID_HANDLE = -4,  // Invalid handle
	PLAYER_INVALID_STATE = -5,  // Invalid state
	PLAYER_INVALID_CTX = -6,  // Invalid context
	PLAYER_INVALID_PARAM = -7,  // Invalid parameter
	PLAYER_INVALID_FMT = -8,  // Invalid resource
	// ......
	PLAYER_DEMUX_ERR = -100,  // Demux error
	PLAYER_DEMUX_ERR_SEEK = -101,  // Demux seek fail
	PLAYER_DEMUX_ERR_FORMAT = -102,  // format error
	PLAYER_DEMUX_ERR_READ = -103,  // read error
	PLAYER_DEMUX_SEEK_UNSUPPORT = -104,  // unsupport seek
	PLAYER_DEMUX_SEEK_ERRORTIME = -105,  // seek time erroe
	// ......
	PLAYER_DECODE_ERR = -200,  // Decode fail
	PLAYER_DECODE_INIT_ERR = -201,  // Decode init error
	PLAYER_DECODE_FORMAT_ERR = -202,  // Error format
	PLAYER_DECODE_INIT_RESAMPLE_ERR = -203,  // Init resample err
	PLAYER_DECODE_UNSUPPORT_CHL_ERR = -204,  // Unsupport channel
	// ......
	PLAYER_PLAY_ERR = -300,  // Play fail
	PLAYER_PLAY_TRACK_INIT_ERR = -301,  // Play track init fail
	PLAYER_PLAY_WRITE_ERR = -302,  // Play track write error
	// ......
	PLAYER_PROBE_FORMAT_ERROR = -401,  // probe format error
	PLAYER_PROBE_FORMAT_UNKNOW = -402,  // probe format unkonw
	// ......
	PLAYER_AUTH_FAIL = -501,  // auth check fail
} PlayerErr;

/** 播放事件回调 */
typedef int (*player_callback)(PlayerEvt evt, int arg1, int arg2, int id);

/**
 * @brief 设置LOG等级
 * @param level log等级
 * 		0: 不打印LOG
 * 		1: 打印ERROR LOG
 * 		2: 打印WARNING LOG
 * 		3: 打印INFO LOG
 * 		4: 打印DEBUG LOG
 * 		5: 打印VERBOSE LOG
 * @note 高等级包含低等级LOG打印
 */
void lisa_player_set_loglev(int level);

/**
 * @brief 	获取播放器版本号
 * @return 	Lisa Player Version
 */
const char * lisa_player_get_version();

/**
 * @brief 创建播放器
 * @param name 播放器名称
 * @param id 播放器id
 * @return PLAYER_HANDLE
 */
PLAYER_HANDLE lisa_player_create(const char *name, int id);

/**
 * @brief 创建播放器
 * @param name 播放器名称
 * @param id 播放器id
 * @param card_name 声卡节点
 * @return PLAYER_HANDLE
 */
PLAYER_HANDLE lisa_player_create_by_card(const char *name, int id, const char *const card_name);

/**
 * @brief 销毁播放器
 * @param h 句柄
 */
void lisa_player_destory(PLAYER_HANDLE h);

/**
 * @brief 设置播放器回调
 * @param h 句柄
 */
PlayerErr lisa_player_set_callback(PLAYER_HANDLE h, player_callback cb);

/**
 * @brief 设置资源路径
 * @param h 句柄
 * @param url 资源路径
 */
PlayerErr lisa_player_seturl(PLAYER_HANDLE h, const char *url);

/**
 * @brief 写音频数据
 * 		  如果是PCM数据, seturl格式: stream://type=pcm&rate=%d&channel=%d&bits=%d
 * 						结束时, 直接传递data=NULL, size=0
 * @param h 句柄
 * @param data 数据指针，可以为NULL
 * @param size 数据大小
 * @param wait_ms 等待时长
 */
int lisa_player_put_stream_data(PLAYER_HANDLE h, uint8_t *data, uint32_t size, uint32_t wait_ms);

/**
 * @brief 开始播放
 * @param h 句柄
 */
PlayerErr lisa_player_play(PLAYER_HANDLE h);

/**
 * @brief 暂停操作
 * @param h 句柄
 */
PlayerErr lisa_player_pause(PLAYER_HANDLE h);

/**
 * @brief 恢复操作
 * @param h 句柄
 */
PlayerErr lisa_player_resume(PLAYER_HANDLE h);

/**
 * @brief 恢复同步操作
 * @param h 句柄
 */
PlayerErr lisa_player_resume_sync(PLAYER_HANDLE h);

/**
 * @brief 停止操作
 * @param h 句柄
 */
PlayerErr lisa_player_stop(PLAYER_HANDLE h);

/**
 * @brief 同步停止操作
 * @param h 句柄
 */
PlayerErr lisa_player_stop_sync(PLAYER_HANDLE h);

/**
 * @brief 播放器重置
 * @param h 句柄
 */
PlayerErr lisa_player_reset(PLAYER_HANDLE h);

/**
 * @brief Seek
 * @param h 句柄
 * @param seek_ms Seek时长(ms为单位)
 */
PlayerErr lisa_player_seek(PLAYER_HANDLE h, int seek_ms);

/**
 * @brief Seek同步操作
 * @param h 句柄
 * @param seek_ms Seek时长(ms为单位)
 */
PlayerErr lisa_player_seek_sync(PLAYER_HANDLE h, int seek_ms);

/**
 * @brief 获取播放器的状态
 * @param h 句柄
 * @return 播放器状态
 */
PlayerState lisa_player_get_state(PLAYER_HANDLE h);

/**
 * @brief 获取播放资源总长度
 * @param h 句柄
 * @return >= 0 播放资源总长度
 * @return < 0 操作错误
 */
int32_t lisa_player_get_duration(PLAYER_HANDLE h);

/**
 * @brief 获取播放位置
 * @param h 句柄
 * @return >= 0 播放位置
 * @return < 0 操作错误
 */
int32_t lisa_player_get_pos(PLAYER_HANDLE h);

/**
 * @brief 设置播放器音量
 * @param h 句柄
 * @param vol 指定音量
 * @return >= 0 设置后的音量
 * @return < 0 设置失败
 */
PlayerErr lisa_player_set_vol(PLAYER_HANDLE h, int vol);

/**
 * @brief 关闭连接前置处理
 * @param h 句柄
 * @return > 0 关闭成功
 * @return < 0 关闭失败
 */
PlayerErr lisa_player_pre_close(PLAYER_HANDLE h);

/**
 * @brief 抛掉开头固定时间的数据，在lisa_player_seturl之前调用
 * @param h 句柄
 * @param throw_time_ms 多长时间的数据，单位毫秒
 */
PlayerErr lisa_player_throw_begin_data(PLAYER_HANDLE h, int throw_time_ms);

/**
 * @brief  抛掉开头能量低的音频
 * @param  h                句柄
 * @param  max_ms           最大检测时间
 * @return PlayerErr 
 */
PlayerErr lisa_player_throw_low_energy(PLAYER_HANDLE h, int max_ms);

/**
 * @brief 	创建 Micro 播放器
 * @param	track_buf_size	播放缓存大小
 * @return 	PLAYER_HANDLE
 */
PLAYER_HANDLE lisa_player_micro_create(uint32_t track_buf_size);

/**
 * @brief 	播放 Micro 播放器
 * @param 	h 		句柄
 * @param	url		播放链接
 * @return  PlayerErr
 */
PlayerErr lisa_player_micro_play(PLAYER_HANDLE h, const char *const url);

/**
 * @brief 	停止 Micro 播放器
 * @param  	h		句柄
 * @return 	PlayerErr
 */
PlayerErr lisa_player_micro_stop(PLAYER_HANDLE h);

/**
 * @brief 设置 Micro 播放器音量
 * @param h 句柄
 * @param vol 指定音量
 * @return >= 0 设置后的音量
 * @return < 0 设置失败
 */
PlayerErr lisa_player_micro_set_vol(PLAYER_HANDLE h, int vol);

/**
 * @brief 	销毁 Micro 播放器
 * @param 	h 		句柄
 */
void lisa_player_micro_destory(PLAYER_HANDLE h);

/**
 * @brief  抛掉开头能量低的音频
 * @param  h                句柄
 * @param  _time_ms         最大检测时间
 * @return PlayerErr 
 */
PlayerErr lisa_player_micro_throw_low_energy(PLAYER_HANDLE h, int throw_time_ms);

#endif