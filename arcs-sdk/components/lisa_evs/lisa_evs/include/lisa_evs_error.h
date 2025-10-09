#ifndef __LISA_EVS_SDK_ERROR_H__
#define __LISA_EVS_SDK_ERROR_H__

/// evs sdk错误码
typedef enum {
	/// success
	LISA_CODE_SUCCESS = 0,
	/// failed
	LISA_CODE_FAILED,

	/// 未授权
	LISA_CODE_AUTH_UNAUTHORIZED = 100,
	/// token过期
	LISA_CODE_AUTH_EXPIRED,
	/// token失效
	LISA_CODE_AUTH_INVALID_TOKEN,
} lisa_err_e;

#endif  // LISA_EVS_SDK_C_ERROR_H