#include "evs_auth_mgr.h"
#include "lisa_evs_auth.h"
#include "lisa_semaphore.h"
#include "lisa_log.h"
#include "lisa_err.h"
#include "lisa_typedef.h"
#include "lisa_thread.h"
#include <string.h>

#define TAG "ScanpenApp_auth"

#define EVS_KEY_REFRESH_TOKEN ("refresh_token")

// 鉴权相关
static lisa_evs_auth_t *g_auth_handle = NULL;
static lisa_evs_auth_cb_t g_auth_cb;
static char g_user_code[80] = {0};
static char g_device_code[80] = {0};
static char g_refresh_token[80] = {0};
static char g_access_token[80] = {0};

lisa_semaphore_t *g_auth_semaphore = NULL;

/// 获取devicecode、usercode回调
void get_devicecode_success_cb(const lisa_evs_get_devicecode_response_t *const code_response)
{
	strcpy(g_user_code, code_response->user_code);
	strcpy(g_device_code, code_response->device_code);
	LISA_LOGD(TAG, "get device code success, device code: %s, usr code: %s",
			code_response->device_code, code_response->user_code);
	lisa_semaphore_give(g_auth_semaphore);
}

/// 获取token回调
void get_token_success_cb(const lisa_evs_get_token_response_t *const token_response)
{
	strcpy(g_refresh_token, token_response->m_refresh_token);
    strcpy(g_access_token, token_response->m_access_token);
	LISA_LOGD(TAG, "get token success, access token: %s, refresh token: %s",
			token_response->m_access_token, token_response->m_refresh_token);
    //保存refresh_token
	//ef_set_env(EVS_KEY_REFRESH_TOKEN, g_refresh_token);
    lisa_semaphore_give(g_auth_semaphore);
}

/// 获取devicecode, usercode失败
void get_devicecode_fail_cb(const uint8_t *const msg)
{
	LISA_LOGE(TAG, "get device code fail, msg: %s", msg);
	lisa_semaphore_give(g_auth_semaphore);
}

/// 获取token失败
void get_token_failed_cb(const uint8_t *const msg)
{
	LISA_LOGE(TAG, "get token fail, msg: %s", msg);
	lisa_semaphore_give(g_auth_semaphore);
}

int start_evs_auth(const lisa_evs_config_t *const config)
{
    int ret = 0;
	
	if (g_auth_handle == NULL) {
    	g_auth_cb.get_devicecode_success = get_devicecode_success_cb;
    	g_auth_cb.get_token_success = get_token_success_cb;
    	g_auth_cb.get_devicecode_failed = get_devicecode_fail_cb;
    	g_auth_cb.get_token_failed = get_token_failed_cb;
    	g_auth_handle = lisa_evs_auth_create(&g_auth_cb, config);
		if (g_auth_handle == NULL) {
			LISA_LOGE(TAG, "Failed to create evs auth handle");
			return -1;
		}
    	g_auth_semaphore = lisa_semaphore_create(2);
		if (g_auth_semaphore == NULL) {
			LISA_LOGE(TAG, "Failed to create evs auth sema");
			lisa_evs_auth_destory(g_auth_handle);
			g_auth_handle = NULL;
			return -1;
		}
    } else {
		memset(g_user_code, 0, sizeof(g_user_code));
		memset(g_device_code, 0, sizeof(g_device_code));
		memset(g_refresh_token, 0, sizeof(g_refresh_token));
		memset(g_access_token, 0, sizeof(g_access_token));
    }

    //先从flash里面读取refresh_token
    //char *refreshToken = ef_get_env(EVS_KEY_REFRESH_TOKEN);
	char *refreshToken = NULL;
	//LISA_LOGD(TAG, "ef_get_env refreshToken : %s\n", refreshToken);
	if (refreshToken == NULL || refreshToken[0] == '\0') {
    	//refresh_token不存在则需要先调用接口code，再调用接口获取access_token
	    ret = lisa_evs_auth_request_device_code(g_auth_handle);
		if (ret != 0)
			return -1;
	    lisa_semaphore_take(g_auth_semaphore, LISA_OS_WAIT_FOREVER);
	    lisa_thread_delay(1);
	    ret = lisa_evs_auth_request_refreshtoken(g_auth_handle, g_user_code, g_device_code);
		if (ret != 0)
			return -1;
	    lisa_semaphore_take(g_auth_semaphore, LISA_OS_WAIT_FOREVER);
	} else {
    	//refresh_token存在直接调用接口获取access_token
		strcpy(g_refresh_token, refreshToken);
        //刷新access token接口
	    ret = lisa_evs_auth_request_accesstoken(g_auth_handle, g_refresh_token);
		if (ret != 0)
			return -1;
	    lisa_semaphore_take(g_auth_semaphore, LISA_OS_WAIT_FOREVER);
    }
	return LISA_OK;
}

const char *get_evs_access_token()
{
    return g_access_token;
}
