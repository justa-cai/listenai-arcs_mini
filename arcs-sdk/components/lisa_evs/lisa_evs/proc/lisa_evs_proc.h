#ifndef __LISA_EVS_SDK_PROC_H__
#define __LISA_EVS_SDK_PROC_H__

#include "lisa_evs.h"

typedef enum lisa_evs_proc_ret {
	LISA_EVS_PROC_SUCC = 0,
	LISA_EVS_PROC_JSON_ERR = -1,
	LISA_EVS_PROC_NO_RESP = -2,
	LISA_EVS_PROC_NO_RESULT = -3,
} lisa_evs_proc_ret_e;

/**
 * @brief   处理WebSocket消息内容
 * @param   ws_msg    消息内容
 * @param   ws_cb     回调函数
 * @return  int
 */
lisa_evs_proc_ret_e lisa_evs_process(
		const uint8_t *const ws_msg, int msg_size, const struct lisa_evs_cb *const evs_cb);

#endif