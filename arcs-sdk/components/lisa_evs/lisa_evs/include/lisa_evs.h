#ifndef __LISA_EVS_SDK_H__
#define __LISA_EVS_SDK_H__

#include <stdbool.h>
#include "lisa_evs_config.h"
#include "lisa_evs_audio.h"
#include "lisa_evs_auth.h"
#include "lisa_evs_audiomgr.h"
#include "lisa_evs_error.h"
#include "lisa_evs_rec.h"
#include "lisa_evs_speaker.h"
#include "lisa_evs_system.h"
#include "lisa_evs_ws.h"
#include "cJSON.h"

typedef struct lisa_dictionary_lookup_cb
{

	/**
	 * @brief	词典查询请求失败
	 */
	void (*on_dictionary_lookup_failed)();
	
	/**
	 * @brief 	词典查询请求成功
	 * @param	结果报文
	 */
	void (*on_dictionary_lookup_succ)(const uint8_t *const result);
} lisa_dictionary_lookup_cb_t;

typedef struct lisa_evs_cb {
	/// websocket 事件回调
	lisa_evs_websocket_cb_t *websocket_cb;
	/// 系统相关response, (此回调传空, 则相关信息通过websocket_cb的message接口回调回去)
	lisa_evs_system_cb_t *system_cb;
	/// 扬声器控制response, (此回调传空, 则相关信息通过websocket_cb的message接口回调回去)
	lisa_evs_speaker_cb_t *speaker_cb;
} lisa_evs_cb_t;

typedef struct lisa_evs_state {
	lisa_evs_system_state_t system_state;
	lisa_evs_recognizer_state_t recognizer_state;
	lisa_evs_playback_state_t playback_state;
	lisa_evs_speaker_state_t speaker_state;
	lisa_evs_platform_info_t platform_info;
} lisa_evs_state_t;

typedef struct lisa_evs_state_cb {
	/// sdk获取状态回调，开发者必须实现的！！！
	lisa_evs_state_t (*get_state)();
} lisa_evs_state_cb_t;

typedef struct lisa_evs {
	lisa_evs_config_t *config;
	/// evs websocket句柄
	struct lisa_evs_ws_s *ws;
	/// evs audiomgr句柄
	lisa_evs_audiomgr_t *audiomgr;
	// evs recognizer句柄
	lisa_evs_rec_t *rec;
	/// evs 协议必须实现的回调，包含websocket、auth、云端下发的response
	const lisa_evs_cb_t *evs_cb;
	/// evs 获取各模块状态回调，发起请求时会通过此回调获取各个模块状态
	const lisa_evs_state_cb_t *get_state_cb;
	// evs access token
	uint8_t *access_token;
} lisa_evs_t;

/**
 * @brief 					sdk初始化函数
 * @param  config       	初始化配置参数，必填
 * @param  evs_cb			EVS相关事件回调, 非局部变量
 * @param  get_state_cb		获取各模块状态的回调, 非局部变量
 * @return lisa_evs_t* 		sdk句柄
 */
lisa_evs_t *lisa_evs_create(const lisa_evs_config_t *const config, const lisa_evs_cb_t *const evs_cb,
		const lisa_evs_state_cb_t *const get_state_cb);

/**
 * @brief 					sdk销毁函数
 * @param  handle           sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_destroy(lisa_evs_t *handle);

/**
 * @brief 设置Access Token
 * @param handle		sdk句柄
 * @param access_token 	由Auth模块获取
 * @param len			长度
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_set_access_token(lisa_evs_t *handle, const uint8_t *const access_token, int len);

/**
 * @brief 					播放状态同步
 * @param  handle           sdk句柄
 * @param  progress         播放状态
 * @return lisa_err_t 		返回码
 */
lisa_err_t lisa_evs_progress_sync(
		const lisa_evs_t *const handle, const lisa_evs_play_info_t *const progress);

/**
 * @brief 					系统状态同步
 * @param  handle           sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_system_sync(const lisa_evs_t *const handle);

/**
 * @brief 连接EVS
 * @param handle 			sdk句柄
 * @param token				access token
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_connect(const lisa_evs_t *const handle, const uint8_t *const token);

/**
 * @brief 与EVS断开连接
 * @param handle 			sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_disconnect(const lisa_evs_t *const handle);

/**
 * @brief 					同步ota检测结果
 * 触发时机:
 * 		1. 用户触发检查升级后，会收到云端返回的check_software_update
 * 		2. 调用接入的OTA服务的检查更新接口 (iFLYOS 有提供通用的OTA服务: https://doc.iflyos.cn/device/ota.html)
 * 		3. 将检查结果通过 lisa_evs_upload_check_result 发送给云端
 * 		4. 云端将检查更新结果反馈给用户
 * @param  handle           sdk句柄
 * @param  msg              检测结果
 * @return lisa_err_t
 */
lisa_err_t lisa_evs_upload_check_result(
		const lisa_evs_t *const handle, const lisa_evs_update_msg_t *const msg);

/**
 * @brief 公共请求接口
 * @param handle 			sdk句柄
 * @param request_json 		请求JSON
 * @param req_id 			请求ID
 * @return int
 */
int lisa_evs_client_request(
		const lisa_evs_t *const handle, cJSON *request_json, const uint8_t *const req_id);

/**
 * @brief  有道词典查词接口
 * @param  word             需要查询的词
 * @return lisa_err_t 
 */
lisa_err_t lisa_dictionary_lookup(const lisa_evs_t *const handle, const uint8_t *const word, const lisa_dictionary_lookup_cb_t *const cb);

typedef void (*lisa_http_resp_cb)(const uint8_t *const data, uint32_t len);

/**
 * @brief  获取口语评测难度列表
 * @param  handle           sdk句柄
 * @param  cb               结果回调
 * @return lisa_err_t 
 */
lisa_err_t lisa_get_evaluate_level(const lisa_evs_t *const handle, const lisa_http_resp_cb cb);

/**
 * @brief  获取随机题目
 * @param  handle           sdk句柄
 * @param  lev              [in]题目难度等级
 * @param  type             [in]题型
 * @param  cb               结果回调
 * @return lisa_err_t 		
 */
lisa_err_t lisa_get_evaluate_quest(const lisa_evs_t *const handle, const uint8_t *const lev, const uint8_t *const type, const lisa_http_resp_cb cb);

/**
 * @brief  获取在线tts语速
 * @param  handle           sdk句柄
 * @param  cb               结果回调
 * @return lisa_err_t 
 */
lisa_err_t lisa_get_online_tts_speed(const lisa_evs_t *const handle, const lisa_http_resp_cb cb);

/**
 * @brief 
 * @param  handle           sdk句柄
 * @param  lang             [in]语种(english,mandarin)
 * @param  speed            [in]语速(slow,middle,fast)
 * @param  cb               结果回调
 * @return lisa_err_t 
 */
lisa_err_t lisa_set_online_tts_speed(const lisa_evs_t *const handle, const uint8_t *const lang, const uint8_t *const speed, const lisa_http_resp_cb cb);

#endif  // LISA_EVS_SDK_C_H
