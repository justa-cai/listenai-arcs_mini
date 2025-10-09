#ifndef __LISA_EVS_SDK_SYSTEM_H__
#define __LISA_EVS_SDK_SYSTEM_H__

#include <stdbool.h>

typedef enum 
{
	CHECK_SUCCEED = 0,
	CHECK_FAILED,
} lisa_evs_check_result;

typedef enum {
	LISA_OTA_STATE_IDLE = 0,
	LISA_OTA_STATE_ONGOING = 1,
} lisa_evs_ota_state;

typedef struct lisa_evs_system_state
{
	uint8_t version[12];
	uint8_t firmware_version[12];
} lisa_evs_system_state_t;

typedef struct lisa_evs_platform_info
{
	uint8_t platform_name[16];
	uint8_t platform_version[16];
} lisa_evs_platform_info_t;

typedef struct lisa_evs_update_msg_s
{
	/// 检查结果
	lisa_evs_check_result result;
	/// 是否有新版本
	bool has_new_ver;
	/// 升级状态, 非必传 (有新版本时需要传递)
	lisa_evs_ota_state ota_state;
	/// 版本名, 非必传 (有新版本时需要传递)
	uint8_t* version_name;
	/// 错误描述, 非必传 (有新版本时需要传递)
	uint8_t* desc;
} lisa_evs_update_msg_t;

typedef struct lisa_evs_system_cb
{
	/**
	 * @brief 			ping消息，类似于心跳消息
	 * @param msg		消息文本
	 * @param req_id	请求id
	 */
	void (*on_ping)(const uint8_t *const msg, const uint8_t *const req_id);
	/**
	 * @brief 			error消息
	 * @param msg		消息文本
	 * @param req_id	请求id
	 */
	void (*on_error)(const uint8_t *const msg, const uint8_t *const req_id);
	/**
	 * @brief 			当用户在APP中点击【检查更新】后，你会收到云端返回的该消息
	 * @param msg		消息文本
	 * @param req_id	请求id
	 */
	void (*on_check_software_update)(const uint8_t *const msg, const uint8_t *const req_id);
	/**
	 * @brief 			当用户在APP中点击【立即更新】，或用户语音请求“立即升级”后，你会收到云端返回的该消息
	 * @param msg		消息文本
	 * @param req_id	请求id
	 */
	void (*on_update_software)(const uint8_t *const msg, const uint8_t *const req_id);
	/**
	 * @brief 			当用户通过APP解除设备绑定时，云端将会通过该response通知设备。设备收到该response后，需要清除设备本地的token信息。
	 * @param msg		消息文本
	 * @param req_id	请求id
	 */
	void (*on_revoke_auth)(const uint8_t *const msg, const uint8_t *const req_id);
	/**
	 * @brief 			ping消息，类似于心跳消息
	 * @param msg		消息文本
	 * @param req_id	请求id
	 */
	void (*on_power_off)(const uint8_t *const msg, const uint8_t *const req_id);
} lisa_evs_system_cb_t;

#endif // LISA_EVS_SDK_SYSTEM_H
