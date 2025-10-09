#define TAG "play_mode"

#include <stdlib.h>
#include <string.h>
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_time.h"
#include "play_mode.h"


play_mode_t *listen_play_mode_create()
{
	play_mode_t *handle = (play_mode_t *)lisa_mem_calloc(1, sizeof(play_mode_t));
	if (handle == NULL) {
		LISA_LOGE(TAG, "play_mode handle malloc err");
		goto HANDLE_ERROR;
	}
    handle->m_play_mode = MODE_ORDER;
	return handle;
HANDLE_ERROR:
	return NULL;
}

int listen_playlist_init(play_mode_t *handle, audio_out_t *item, int item_size)
{
	if (handle == NULL) {
		LISA_LOGE(TAG, "handle is NULL!");
		return -1;
	}
	if (item == NULL || item_size <= 0) {
		LISA_LOGE(TAG, "item or item_size is NULL!");
		return -1;
	}
	if ((handle->m_music_list) != NULL && handle->m_music_size > 0) {
		lisa_mem_free(handle->m_music_list);
	}
    handle->m_curr_index = -1;
    handle->m_play_mode = MODE_ORDER;
	handle->m_music_size = item_size;
	handle->m_music_list = (audio_out_t *)lisa_mem_calloc(item_size, sizeof(audio_out_t));
	audio_out_t *audio;
	for (int i = 0; i < item_size; i++) {
		audio = &((handle->m_music_list)[i]);
		memcpy(audio, item + i, sizeof(audio_out_t));
		LISA_LOGD(TAG, "play url = %s", audio->m_url);
	}
	return 0;
}

int listen_switch_playmode(play_mode_t *handle, PLAY_MODE_E mode)
{
	if (handle == NULL) {
		LISA_LOGE(TAG, "handle is NULL!");
		return -1;
	}
    handle->m_play_mode = mode;
	return 0;
}

audio_out_t *listen_next_audio(play_mode_t *handle, int *code)
{
	if (handle == NULL) {
		LISA_LOGE(TAG, "handle is NULL!");
		// 没有歌曲
        *code = -1;
		return NULL;
	}
	if (handle->m_music_list == NULL || handle->m_music_size == 0) {
		LISA_LOGE(TAG, "handle list is empty!");
		// 没有歌曲
        *code = -1;
		return NULL;
	}
	switch (handle->m_play_mode) {
		case MODE_ORDER: {
			if (handle->m_curr_index >= (handle->m_music_size - 1)) {
				// 已经是最后一首了
                *code = 1;
				handle->m_curr_index = handle->m_music_size - 1;
				return &((handle->m_music_list)[handle->m_curr_index]);
			}
			handle->m_curr_index++;
		} break;
		case MODE_CYCLE: {
			handle->m_curr_index++;
			if (handle->m_curr_index >= (handle->m_music_size - 1)) {
				handle->m_curr_index = 0;
			}
		} break;
		case MODE_RANDOM: {
			uint32_t random;
			uint32_t index;
			uint8_t retry_count = 0;
			while (retry_count < 3) {
				retry_count++;
				random = lisa_rand32();
				LISA_LOGD(TAG, "random is:%d", random);
				index = random % (handle->m_music_size);
                LISA_LOGD(TAG, "index is:%d", index);
				if (index != handle->m_curr_index) {
					handle->m_curr_index = index;
					break;
				}
			}
			if (retry_count == 3) {
                // 三次都随机同一首歌直接播放下一首
				handle->m_curr_index++;
				if (handle->m_curr_index >= (handle->m_music_size - 1)) {
					handle->m_curr_index = 0;
				}
			}
		} break;
		default:
			break;
	}
	*code = 0;
	return &((handle->m_music_list)[handle->m_curr_index]);
}

audio_out_t *listen_pre_audio(play_mode_t *handle, int *code)
{
	if (handle == NULL) {
		LISA_LOGE(TAG, "handle is NULL!");
		// 没有歌曲
        *code = -1;
		return NULL;
	}
	if (handle->m_music_list == NULL || handle->m_music_size == 0) {
		LISA_LOGE(TAG, "handle list is empty!");
		// 没有歌曲
        *code = -1;
		return NULL;
	}
	switch (handle->m_play_mode) {
		case MODE_ORDER: {
			if (handle->m_curr_index <= 0) {
				// 已经是第一首了
                *code = 1;
				handle->m_curr_index = 0;
				return &((handle->m_music_list)[handle->m_curr_index]);
			}
			handle->m_curr_index--;
		} break;
		case MODE_CYCLE: {
			handle->m_curr_index--;
			if (handle->m_curr_index <= 0) {
				handle->m_curr_index = handle->m_music_size - 1;
			}
		} break;
		case MODE_RANDOM: {
			uint32_t random = lisa_rand32();
			uint32_t index;
			uint8_t retry_count = 0;
            LISA_LOGD(TAG, "random is:%d", random);
			while (retry_count < 3) {
				retry_count++;
				index = random % (handle->m_music_size);
                LISA_LOGD(TAG, "index is:%d", index);
				if (index != handle->m_curr_index) {
					handle->m_curr_index = index;
				}
			}
			if (retry_count == 3) {
                // 三次都随机同一首歌直接播放下一首
				handle->m_curr_index--;
				if (handle->m_curr_index <= 0) {
					handle->m_curr_index = handle->m_music_size - 1;
				}
			}
		} break;
		default:
			break;
	}
	*code = 0;
	return &((handle->m_music_list)[handle->m_curr_index]);
}

audio_out_t *listen_get_curr_audio(play_mode_t *handle)
{
	if (handle == NULL) {
		LISA_LOGE(TAG, "handle is NULL!");
		return NULL;
	}
	if (handle->m_music_list == NULL || handle->m_music_size == 0) {
		LISA_LOGE(TAG, "handle list is empty!");
		return NULL;
	}
	if (handle->m_curr_index < 0 || handle->m_curr_index > (handle->m_music_size - 1)) {
		return NULL;
	}
	return &((handle->m_music_list)[handle->m_curr_index]);
}

int listen_playlist_deinit(play_mode_t *handle)
{
    if (handle == NULL) {
		LISA_LOGE(TAG, "handle is NULL!");
		return -1;
	}
	if (handle->m_music_list != NULL && handle->m_music_size > 0) {
		lisa_mem_free(handle->m_music_list);
	}
    handle->m_music_list = NULL;
	handle->m_music_size = 0;
    return 0;
}

int listen_play_mode_destory(play_mode_t *handle)
{
    if (handle == NULL) {
		LISA_LOGE(TAG, "handle is NULL!");
		return -1;
	}
	listen_playlist_deinit(handle);
	lisa_mem_free(handle);
	return 0;
}