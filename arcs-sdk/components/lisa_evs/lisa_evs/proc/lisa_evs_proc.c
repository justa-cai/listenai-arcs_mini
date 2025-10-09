#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lisa_evs_proc.h"
#include "lisa_evs.h"
#include "cJSON.h"
#include "xz/xz_config.h"
#include "xz/xz.h"
#include "lisa_log.h"
#include "lisa_mem.h"

#define TAG "lisa_proc"

#define LISA_EVS_KEY_META ("iflyos_meta")
#define LISA_EVS_KEY_META_REQUEST_ID ("request_id")
#define LISA_EVS_KEY_RESPONSES ("iflyos_responses")
#define LISA_EVS_KEY_RESPONSE_HEADER ("header")
#define LISA_EVS_KEY_RESPONSE_HEADER_NAME ("name")
#define LISA_EVS_KEY_RESPONSE_PAYLOAD ("payload")

#define LISA_EVS_ACTION_SYSTEM_PING ("system.ping")
#define LISA_EVS_ACTION_SYSTEM_ERROR ("system.error")
#define LISA_EVS_ACTION_SYSTEM_CSU ("system.check_software_update")
#define LISA_EVS_ACTION_SYSTEM_US ("system.update_software")
#define LISA_EVS_ACTION_SYSTEM_POWEROFF ("system.power_off")
#define LISA_EVS_ACTION_SYSTEM_REVOKE_AUTH ("system.revoke_authorization")

#define LISA_EVS_ACTION_SPEAKER_SETVOL ("speaker.set_volume")

#define XZ_HEADER_LEN (6)
static uint8_t g_xz_header[XZ_HEADER_LEN] = {0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00};
#define XZ_DICT_MAX (8 * 1024)             // xz 压缩参数 固定为64K

// 拦截System、Speaker等特殊Action
// 特殊Action通过特殊回调上抛
// 返回true, 则不继续后续操作，进行下一循环
bool interceptorMessage(const cJSON *const action_json, const uint8_t *const payload,
		const lisa_evs_cb_t *const evs_cb, const uint8_t *const req_id)
{
	char *action = action_json->valuestring;
	if (action == NULL) return true;

	lisa_evs_system_cb_t *sys_cb = evs_cb->system_cb;
	bool to_continue = true;

	// System相关
	if (strcmp(action, LISA_EVS_ACTION_SYSTEM_PING) == 0) {
		if (sys_cb->on_ping)
			sys_cb->on_ping(payload, req_id);
		else
			to_continue = false;
	} else if (strcmp(action, LISA_EVS_ACTION_SYSTEM_ERROR) == 0) {
		if (sys_cb->on_error)
			sys_cb->on_error(payload, req_id);
		else
			to_continue = false;
	} else if (strcmp(action, LISA_EVS_ACTION_SYSTEM_CSU) == 0) {
		if (sys_cb->on_check_software_update)
			sys_cb->on_check_software_update(payload, req_id);
		else
			to_continue = false;
	} else if (strcmp(action, LISA_EVS_ACTION_SYSTEM_US) == 0) {
		if (sys_cb->on_update_software)
			sys_cb->on_update_software(payload, req_id);
		else
			to_continue = false;
	} else if (strcmp(action, LISA_EVS_ACTION_SYSTEM_POWEROFF) == 0) {
		if (sys_cb->on_power_off)
			sys_cb->on_power_off(payload, req_id);
		else
			to_continue = false;
	} else if (strcmp(action, LISA_EVS_ACTION_SYSTEM_REVOKE_AUTH) == 0) {
		if (sys_cb->on_revoke_auth)
			sys_cb->on_revoke_auth(payload, req_id);
		else
			to_continue = false;
		// 音量设置
	} else if (strcmp(action, LISA_EVS_ACTION_SPEAKER_SETVOL) == 0) {
		if (evs_cb->speaker_cb->on_set_vol)
			evs_cb->speaker_cb->on_set_vol(payload, req_id);
		else
			to_continue = false;
		// 其它
	} else {
		to_continue = false;
	}

	return to_continue;
}

static int _xz_decompress(char *compress_data, uint32_t compress_len, char *decompress_data, uint32_t *decompress_len)
{
		xz_crc32_init();
		xz_crc64_init();
        int ret = -1;
        struct xz_dec *xz_dec_handle = NULL;
        struct xz_buf xz_buf;
        enum xz_ret xz_ret;

        xz_dec_handle = xz_dec_init(XZ_DYNALLOC, XZ_DICT_MAX);
        if(xz_dec_handle == NULL) {
                printf("xz decompress init fail");
                return -1;
        }
        xz_buf.in = (uint8_t*)compress_data;
        xz_buf.in_pos = 0;
        xz_buf.in_size = compress_len;
        xz_buf.out = (uint8_t*)decompress_data;
        xz_buf.out_pos = 0;
        xz_buf.out_size = 64 * 1024;
		int dec_size = 0;
		while (1) {
			xz_ret = xz_dec_run(xz_dec_handle, &xz_buf);
			if (xz_ret == XZ_OK || xz_ret == XZ_STREAM_END) {
				dec_size += xz_buf.out_pos;
				xz_buf.out = (uint8_t*)decompress_data + dec_size;
			}
			if (xz_ret == XZ_OK) {
				xz_buf.out_pos = 0;
			} else if (xz_ret == XZ_STREAM_END) {
				break;
			} else {
				goto XZ_DEC_OUT;
			}
		}
		
        *decompress_len = dec_size;
        ret = 0;
XZ_DEC_OUT:
        xz_dec_end(xz_dec_handle);
        return ret;
}

lisa_evs_proc_ret_e lisa_evs_process(const uint8_t *const ws_msg, int msg_size, const lisa_evs_cb_t *const evs_cb)
{
	if (msg_size < XZ_HEADER_LEN) return LISA_EVS_PROC_JSON_ERR;

	bool is_xz = true;
	for (int i = 0; i < XZ_HEADER_LEN; i++) {
		if (ws_msg[i] != g_xz_header[i]) {
			is_xz = false;
			break;
		}
	}
	cJSON *info = NULL;
	char* xz_decode_buffer = NULL;
	if (is_xz) {
		LISA_LOGD(TAG, "is xz message");

		xz_decode_buffer = lisa_mem_alloc(msg_size * 4);
		memset(xz_decode_buffer, 0, msg_size * 4);
		uint32_t decode_size = 0;
		int ret = _xz_decompress((char*)ws_msg, msg_size, xz_decode_buffer, &decode_size);
		if (ret != 0) {
			lisa_mem_free(xz_decode_buffer);
			return LISA_EVS_PROC_JSON_ERR;
		}
		xz_decode_buffer[decode_size + 1] = '\0';
		info = cJSON_Parse(xz_decode_buffer);
	} else {
		LISA_LOGD(TAG, "is not xz message");
		info = cJSON_Parse(ws_msg);
	}
	if (info == NULL) {
		if (xz_decode_buffer) {
			lisa_mem_free(xz_decode_buffer);
		}
		return LISA_EVS_PROC_JSON_ERR;
	}

	lisa_evs_proc_ret_e ret = LISA_EVS_PROC_NO_RESULT;
	char *req_id = NULL;

	// 解析meta
	if (cJSON_HasObjectItem(info, LISA_EVS_KEY_META)) {
		cJSON *meta_json = cJSON_GetObjectItem(info, LISA_EVS_KEY_META);
		// 提取request_id
		if (cJSON_HasObjectItem(meta_json, LISA_EVS_KEY_META_REQUEST_ID)) {
			cJSON *req_id_json = cJSON_GetObjectItem(meta_json, LISA_EVS_KEY_META_REQUEST_ID);
			if (req_id_json) req_id = req_id_json->valuestring;
		}
		// Meta Json信息字符串化
		char *meta_str = cJSON_PrintUnformatted(meta_json);
		if (evs_cb->websocket_cb->message)
			evs_cb->websocket_cb->message(LISA_EVS_KEY_META, meta_str, strlen(meta_str), req_id);
		cJSON_free(meta_str);
		ret = LISA_EVS_PROC_NO_RESP;
	}

	// 解析各个Response
	uint8_t resp_num = 0;
	if (cJSON_HasObjectItem(info, LISA_EVS_KEY_RESPONSES)) {
		cJSON *resp_json = cJSON_GetObjectItem(info, LISA_EVS_KEY_RESPONSES);
		int len = cJSON_GetArraySize(resp_json);
		if (len > 0) {
			for (int i = 0; i < len; i++) {
				// 获取Response Item
				cJSON *item_json = cJSON_GetArrayItem(resp_json, i);
				// 解析Header
				cJSON *item_header_json =
						cJSON_GetObjectItem(item_json, LISA_EVS_KEY_RESPONSE_HEADER);
				if (item_header_json == NULL) continue;
				cJSON *head_name_json =
						cJSON_GetObjectItem(item_header_json, LISA_EVS_KEY_RESPONSE_HEADER_NAME);
				if (head_name_json == NULL) continue;
				// 解析Payload
				cJSON *item_payload_json =
						cJSON_GetObjectItem(item_json, LISA_EVS_KEY_RESPONSE_PAYLOAD);
				if (item_payload_json == NULL) continue;
				char *payload = cJSON_PrintUnformatted(item_payload_json);

				resp_num++;
				if (interceptorMessage(head_name_json, payload, evs_cb, req_id)) {
					cJSON_free(payload);
					continue;
				}

				// 回调
				if (evs_cb->websocket_cb->message)
					evs_cb->websocket_cb->message(
							head_name_json->valuestring, payload, strlen(payload), req_id);
				cJSON_free(payload);
			}
		}
	}
	if (resp_num > 0) {
		ret = LISA_EVS_PROC_SUCC;
	}

	cJSON_Delete(info);
	if (xz_decode_buffer) {
		lisa_mem_free(xz_decode_buffer);
	}
	return ret;
}
