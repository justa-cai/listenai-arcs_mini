#ifndef __LISA_EVS_SDK_AUTH_H__
#define __LISA_EVS_SDK_AUTH_H__

#include "lisa_evs_error.h"
#include "lisa_evs_config.h"

/// get devicecode响应报文
typedef struct lisa_evs_get_devicecode_response {
	/// device code
	uint8_t device_code[80];
	/// user_code
	uint8_t user_code[32];
} lisa_evs_get_devicecode_response_t;

/// get token响应报文
typedef struct lisa_evs_get_token_response {
	/// token type
	uint8_t m_token_type[8];
	/// access token
	uint8_t m_access_token[80];
	/// refresh token
	uint8_t m_refresh_token[80];
	/// 有效期
	long long m_expires_in;
	/// 生成时间
	long long m_created_time;
} lisa_evs_get_token_response_t;

/// evs auth回调
typedef struct lisa_evs_auth_cb {
	/**
	 * @brief 					获取devicecode、usercode成功回调
	 * @param code_response		lisa_evs_get_devicecode_response_t结构体
	 */
	void (*get_devicecode_success)(const lisa_evs_get_devicecode_response_t *const code_response);
	/**
	 * @brief 					获取token成功回调
	 * @param code_response		lisa_evs_get_token_response_t结构体
	 */
	void (*get_token_success)(const lisa_evs_get_token_response_t *const token_response);
	/**
	 * @brief 					获取devicecode、usercode失败回调
	 * @param msg				消息文本
	 */
	void (*get_devicecode_failed)(const uint8_t *const msg);
	/**
	 * @brief 					获取token失败回调
	 * @param msg				消息文本
	 */
	void (*get_token_failed)(const uint8_t *const msg);
} lisa_evs_auth_cb_t;

/**
 * @brief evs_auth结构体
 */
typedef struct lisa_evs_auth {
	/// evs auth回调
	const lisa_evs_auth_cb_t *evs_auth_cb;
	/// evs auth配置参数
	const lisa_evs_config_t * evs_config;
} lisa_evs_auth_t;

/**
 * @brief 					auth初始化函数
 * @param  auth_cb          auth回调
 * @param  config           auth配置参数
 * @return lisa_evs_auth_t* auth句柄
 */
lisa_evs_auth_t* lisa_evs_auth_create(const lisa_evs_auth_cb_t *const auth_cb, const lisa_evs_config_t *const config);

/**
 * @brief 					auth销毁
 * @param  handle           auth句柄
 * @return lisa_err_e 
 */
lisa_err_e lisa_evs_auth_destory(const lisa_evs_auth_t *const handle);

/**
 * @brief 					获取device_code，一般首次开机联上网调用一次即可
 * @param  handle           auth句柄
 * @return lisa_err_e
 */
lisa_err_e lisa_evs_auth_request_device_code(const lisa_evs_auth_t *const handle);

/**
 * @brief 					获取refresh token，refresh token应用层可以持久化
 * @param  handle           auth句柄
 * @param  user_code		user_code，通过lisa_evs_auth_request_device_code接口获取
 * @param  device_code      device_code，通过lisa_evs_auth_request_device_code接口获取
 * @return lisa_err_e
 */
lisa_err_e lisa_evs_auth_request_refreshtoken(
		const lisa_evs_auth_t *const handle, const uint8_t *const user_code, const uint8_t *const device_code);

/**
 * @brief 					获取access token，access token过期需要调用该接口
 * @param  handle           auth句柄
 * @param  refresh_token    refresh_token，refresh token应用层可以持久化
 * @return lisa_err_e
 */
lisa_err_e lisa_evs_auth_request_accesstoken(
		const lisa_evs_auth_t *const handle, const uint8_t *const refresh_token);

#endif
