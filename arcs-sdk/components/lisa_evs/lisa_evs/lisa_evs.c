#include <stdio.h>
#include <string.h>
#include "lisa_evs.h"
#include "lisa_mem.h"
#include "lisa_log.h"
#include "utils/evs_utils.h"
#include "sockets/lisa_evs_websocket.h"


#define TAG "lisa_evs"

extern lisa_evs_rec_t *lisa_evs_rec_create(lisa_evs_t *evs, lisa_evs_ws_t *ws);

lisa_evs_t *lisa_evs_create(const lisa_evs_config_t *const config, const lisa_evs_cb_t *const evs_cb,
		const lisa_evs_state_cb_t *const get_state_cb)
{
	LISA_LOGD(TAG, "lisa evs create [START]");
	if (!config) {
		LISA_LOGE(TAG, "config pointer is null.");
		return NULL;
	}
	// evs_utils_init();
	lisa_evs_t *lisa_evs = (lisa_evs_t *)lisa_mem_alloc(sizeof(lisa_evs_t));
	memset(lisa_evs, 0, sizeof(lisa_evs_t));
	lisa_evs_config_t *evs_config = NULL;
	uint8_t client_id_len = 0;
	uint8_t device_id_len = 0;
	uint8_t ota_secret_len = 0;
	lisa_evs_ws_t *_ws = NULL;
	lisa_evs_audiomgr_t *_audiomgr = NULL;
	lisa_evs_rec_t *_rec = NULL;

	if (!lisa_evs) {
		LISA_LOGE(TAG, "alloc lisa_evs pointer error.");
		goto LISA_EVS_EXIT_ERROR;
	}

	// alloc config
	evs_config = (lisa_evs_config_t *)lisa_mem_alloc(sizeof(lisa_evs_config_t));
	if (!evs_config) {
		LISA_LOGE(TAG, "alloc config pointer error.");
		goto LISA_EVS_EXIT_ERROR_CONFIG;
	}

	// client_id
	client_id_len = strlen(config->client_id);
	evs_config->client_id = (char *)lisa_mem_alloc(sizeof(char) * (client_id_len + 1));
	if (!evs_config->client_id) {
		LISA_LOGE(TAG, "alloc client_id pointer error.");
		goto LISA_EVS_EXIT_ERROR_CLIENTID;
	}
	memcpy(evs_config->client_id, config->client_id, client_id_len);
	evs_config->client_id[client_id_len] = '\0';

	// device_id
	device_id_len = strlen(config->device_id);
	evs_config->device_id = (char *)lisa_mem_alloc(sizeof(char) * (device_id_len + 1));
	if (!evs_config->device_id) {
		LISA_LOGE(TAG, "alloc device_id pointer error.");
		goto LISA_EVS_EXIT_ERROR_DEVICEID;
	}
	memcpy(evs_config->device_id, config->device_id, device_id_len);
	evs_config->device_id[device_id_len] = '\0';

	// ota_secret
	ota_secret_len = strlen(config->ota_secret);
	evs_config->ota_secret = (char *)lisa_mem_alloc(sizeof(char) * (ota_secret_len + 1));
	if (!evs_config->ota_secret) {
		LISA_LOGE(TAG, "alloc ota_secret pointer error.");
		goto LISA_EVS_EXIT_ERROR_OTASECRET;
	}
	memcpy(evs_config->ota_secret, config->ota_secret, ota_secret_len);
	evs_config->ota_secret[ota_secret_len] = '\0';
	lisa_evs->config = evs_config;

    _ws = lisa_evs_websocket_create(evs_cb);
    if (!_ws) {
		LISA_LOGE(TAG, "alloc lisa_evs_ws pointer error.");
		goto LISA_EVS_EXIT_ERROR_WS;
	}
    lisa_evs->ws = _ws;

    _audiomgr = lisa_evs_audiomgr_create();
    if(!_audiomgr) {
        LISA_LOGE(TAG, "alloc lisa_evs_auth pointer error.");
		goto LISA_EVS_EXIT_ERROR_AUDIOMGR;
    }
    lisa_evs->audiomgr = _audiomgr;

	_rec = lisa_evs_rec_create(lisa_evs, _ws);
    if(!_rec) {
        LISA_LOGE(TAG, "alloc lisa_evs_rec pointer error.");
		goto LISA_EVS_EXIT_ERROR_REC;
    }
    lisa_evs->rec = _rec;

	lisa_evs->evs_cb = evs_cb;
	lisa_evs->get_state_cb = get_state_cb;
	goto LISA_EVS_EXIT_SUCCESS;

LISA_EVS_EXIT_ERROR_REC:
	lisa_evs_audiomgr_destory(_audiomgr);
LISA_EVS_EXIT_ERROR_AUDIOMGR:
    lisa_evs_websocket_destroy(_ws);
LISA_EVS_EXIT_ERROR_WS:
    lisa_mem_free(lisa_evs->config->ota_secret);
LISA_EVS_EXIT_ERROR_OTASECRET:
	lisa_mem_free(lisa_evs->config->device_id);
LISA_EVS_EXIT_ERROR_DEVICEID:
	lisa_mem_free(lisa_evs->config->client_id);
LISA_EVS_EXIT_ERROR_CLIENTID:
	lisa_mem_free(evs_config);
LISA_EVS_EXIT_ERROR_CONFIG:
	lisa_mem_free(lisa_evs);
LISA_EVS_EXIT_ERROR:
	lisa_evs = NULL;
LISA_EVS_EXIT_SUCCESS:
	LISA_LOGD(TAG, "lisa evs create [END]");
	return lisa_evs;
}

lisa_err_t lisa_evs_destroy(lisa_evs_t *handle)
{
    LISA_LOGD(TAG, "lisa evs destory [START]");
	if (handle) {
		if (handle->config) {
			if (handle->config->client_id) {
				lisa_mem_free(handle->config->client_id);
			}
			if (handle->config->device_id) {
				lisa_mem_free(handle->config->device_id);
			}
			if (handle->config->ota_secret) {
				lisa_mem_free(handle->config->ota_secret);
			}

			lisa_mem_free(handle->config);
		}

		if(handle->access_token) {
			lisa_mem_free(handle->access_token);
		}

        lisa_evs_audiomgr_destory(handle->audiomgr);
        lisa_evs_websocket_destroy(handle->ws);

		lisa_mem_free(handle);
	}
    LISA_LOGD(TAG, "lisa evs destory [END]");
	return LISA_OK;
}

lisa_err_t lisa_evs_set_access_token(lisa_evs_t *handle, const uint8_t *const access_token, int len)
{
	if(handle && access_token) {
		if(handle->access_token) {
			lisa_mem_free(handle->access_token);
		}
		handle->access_token = (char *)lisa_mem_calloc(1, sizeof(char) * (len + 1));
		if(handle->access_token) {
			strncpy(handle->access_token, access_token, len);
			handle->access_token[len] = '\0';
			return LISA_OK;
		}
	}
	return LISA_FAIL;
}
