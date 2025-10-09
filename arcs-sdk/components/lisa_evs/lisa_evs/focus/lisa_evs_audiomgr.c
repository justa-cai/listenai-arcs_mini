#include <stdio.h>

#include "lisa_evs_audiomgr.h"
#include "lisa_mem.h"
#include "lisa_log.h"


#define TAG "lisa_audiomgr"

void _construct_focus_inst(focus_state_t **focus_inst, channel_type_e type, int priority, focus_state_e init_state)
{
	(*focus_inst) = (focus_state_t *)lisa_mem_calloc(1, sizeof(focus_state_t));
	(*focus_inst)->m_channel = type;
	(*focus_inst)->m_priority = priority;
	(*focus_inst)->m_state = init_state;
}

lisa_evs_audiomgr_t *lisa_evs_audiomgr_create()
{
	lisa_evs_audiomgr_t *handle = (lisa_evs_audiomgr_t *)lisa_mem_calloc(1, sizeof(lisa_evs_audiomgr_t));

	_construct_focus_inst(&handle->m_aip, AIP, 30, NONE);
	_construct_focus_inst(&handle->m_tts, TTS, 10, NONE);
	_construct_focus_inst(&handle->m_alert, ALERT, 40, NONE);
	_construct_focus_inst(&handle->m_content, CONTENT, 50, NONE);
	_construct_focus_inst(&handle->m_local, LOCAL, 10, NONE);
	_construct_focus_inst(&handle->m_extra, EXTRA, 50, NONE);

	return handle;
}

void lisa_evs_audiomgr_destory(lisa_evs_audiomgr_t *handle)
{
	if (handle != NULL) {
		lisa_mem_free(handle->m_aip);
		lisa_mem_free(handle->m_alert);
		lisa_mem_free(handle->m_content);
		lisa_mem_free(handle->m_tts);
		lisa_mem_free(handle->m_local);
		lisa_mem_free(handle->m_extra);
		lisa_mem_free(handle);
	}
}

static int _capture_on(channel_type_e channel_name, focus_state_t *focus_state)
{
	switch (channel_name) {
		case AIP:
			if (focus_state->m_channel == TTS || focus_state->m_channel == LOCAL ||
					focus_state->m_channel == ALERT) {
				return 1;
			}
			break;
		case TTS:
			// if (focus_state->m_channel == EXTRA) {
			// 	return 1;
			// }
			break;
		case ALERT:
			if (focus_state->m_channel == TTS || focus_state->m_channel == LOCAL ||
					focus_state->m_channel == AIP) {
				return 1;
			}
			break;
		case CONTENT:
			if (focus_state->m_channel == LOCAL || focus_state->m_channel == ALERT ||
					focus_state->m_channel == AIP || focus_state->m_channel == TTS || focus_state->m_channel == EXTRA) {
				return 1;
			}
			break;
		case LOCAL:
			break;
		case EXTRA:
			if (focus_state->m_channel == LOCAL || focus_state->m_channel == ALERT ||
					focus_state->m_channel == AIP || focus_state->m_channel == TTS || focus_state->m_channel == CONTENT) {
				return 1;
			}
			break;
		default:
			break;
	}
	return 0;
}

static focus_state_t *_get_focus_state(lisa_evs_audiomgr_t *handle, channel_type_e name)
{
	switch (name) {
		case AIP:
			return handle->m_aip;
		case TTS:
			return handle->m_tts;
		case ALERT:
			return handle->m_alert;
		case CONTENT:
			return handle->m_content;
		case LOCAL:
			return handle->m_local;
		case EXTRA:
			return handle->m_extra;
	}
	return NULL;  // modify by ljzhang15 2020-06-15
}

static void _set_channel_focus(lisa_evs_audiomgr_t *handle, channel_type_e name, focus_state_e state, channel_type_e by_which)
{
	focus_state_t *focus_state = _get_focus_state(handle, name);
	if (focus_state->m_state != state) {
		if (handle->m_foreground_channel != NULL && handle->m_foreground_channel->m_channel == name &&
				state != FOREGROUND) {
			handle->m_foreground_channel = NULL;
		} else if (handle->m_background_channel != NULL &&
				   handle->m_background_channel->m_channel == name && state != BACKGROUND) {
			handle->m_background_channel = NULL;
		}

		if (state == FOREGROUND) {
			handle->m_foreground_channel = _get_focus_state(handle, name);
		} else if (state == BACKGROUND) {
			handle->m_background_channel = _get_focus_state(handle, name);
		}
		focus_state->m_state = state;
		// LISA_LOGD(TAG, "channel. %d ==> %d", name, state); //modify by ljzhang15 2020-06-15

		for (int i = 0; i < CHANNEL_NUM; i++) {
			if (handle->m_callbacks[i] != NULL && handle->m_callbacks[i]->m_channel_type == name) {
				handle->m_callbacks[i]->on_focus_state(state, by_which);
			}
		}
	}
}

void lisa_evs_audiomgr_acquire_channel(lisa_evs_audiomgr_t *handle, channel_type_e channel_name)
{
	LISA_LOGD(TAG, "%d acquire_channel", channel_name);
	focus_state_t *channel = _get_focus_state(handle, channel_name);
	if (handle->m_foreground_channel != NULL) {
		// 如果存在前景音频通道
		if (_capture_on(channel_name, handle->m_foreground_channel)) {
			// 如果当前前景音频通道在请求的音频通道的抢焦点列表中，
			// ---设置当前前景音频通道为无焦点状态
			// ---再重新请求音频通道焦点
			_set_channel_focus(handle, handle->m_foreground_channel->m_channel, NONE, channel_name);
			lisa_evs_audiomgr_acquire_channel(handle, channel_name);
		} else if (channel->m_priority == handle->m_foreground_channel->m_priority) {
			// 如果当前前景音频通道的优先级与请求的音频通道的优先级相同
			// ---设置当前前景音频通道为无焦点状态
			// ---设置请求的音频通道为前景状态
			_set_channel_focus(handle, handle->m_foreground_channel->m_channel, NONE, channel_name);
			_set_channel_focus(handle, channel_name, FOREGROUND, channel_name);
		} else if (channel->m_priority < handle->m_foreground_channel->m_priority) {
			// 如果当前前景音频通道的优先级低于请求的音频通道的优先级
			// ---如果当前存在背景音频通道，则先设置当前背景音频通道为无焦点状态
			// ---设置当前前景音频通道为背景状态
			// ---设置请求的音频通道为前景状态
			if (handle->m_background_channel != NULL) {
				_set_channel_focus(handle, handle->m_background_channel->m_channel, NONE, channel_name);
			}
			_set_channel_focus(handle, handle->m_foreground_channel->m_channel, BACKGROUND, channel_name);
			_set_channel_focus(handle, channel_name, FOREGROUND, channel_name);
		} else if (channel->m_priority > handle->m_foreground_channel->m_priority) {
			// 如果当前前景音频通道的优先级高于请求的音频通道的优先级
			// ---如果当前不存在背景音频通道，则设置请求的音频通道为背景状态
			// ---如果当前存在背景音频通道且当前背景音频通道的优先级低于请求的音频通道的优先级
			//    ---设置当前的背景音频通道为无焦点
			//    ---设置请求的音频通道为背景状态
			if (handle->m_background_channel == NULL) {
				_set_channel_focus(handle, channel_name, BACKGROUND, channel_name);
			} else if (channel->m_priority <= handle->m_foreground_channel->m_priority) {
				_set_channel_focus(handle, handle->m_background_channel->m_channel, NONE, channel_name);
				_set_channel_focus(handle, channel_name, BACKGROUND, channel_name);
			}
		}
	} else if (handle->m_background_channel != NULL) {
		// 如果当前无前景音频通道，但有背景音频通道
		if (channel->m_priority < handle->m_background_channel->m_priority) {
			// 如果当前背景音频通道的优先级低于请求的音频通道的优先级
			// ---设置请求的音频通道为前景状态
			_set_channel_focus(handle, channel_name, FOREGROUND, channel_name);
		} else if (channel->m_priority == handle->m_background_channel->m_priority) {
			// 如果当前背景音频通道的优先与请求的音频通道的优先级相同
			// ---设置当前的背景音频通道为无焦点
			// ---设置请求的音频通道为前景状态
			if (handle->m_background_channel->m_channel == channel_name) {
				_set_channel_focus(handle, handle->m_background_channel->m_channel, FOREGROUND, channel_name);
			} else {
				_set_channel_focus(handle, handle->m_background_channel->m_channel, NONE, channel_name);
				_set_channel_focus(handle, channel_name, FOREGROUND, channel_name);
			}
		} else {
			// 如果当前背景音频通道的优先级高于请求的音频通道的优先级
			// ---设置当前的背景音频通道为前景状态
			// ---设置请求的音频通道为背景状态
			_set_channel_focus(handle, handle->m_background_channel->m_channel, FOREGROUND, channel_name);
			_set_channel_focus(handle, channel_name, BACKGROUND, channel_name);
		}
	} else {
		// 如果当前无前景音频通道也无背景音频通道
		// ---设置请求的音频通道为前景状态
		_set_channel_focus(handle, channel_name, FOREGROUND, channel_name);
	}
}

void lisa_evs_audiomgr_release_channel(lisa_evs_audiomgr_t *handle, channel_type_e channel_name)
{
	LISA_LOGD(TAG, "%d release_channel", channel_name);

	// 设置释放的音频通道为无焦点状态
	_set_channel_focus(handle, channel_name, NONE, channel_name);
	if (handle->m_foreground_channel == NULL && handle->m_background_channel != NULL) {
		// 设置当前背景音频通道为前景状态
		_set_channel_focus(handle, handle->m_background_channel->m_channel, FOREGROUND, channel_name);
	}
}

void lisa_evs_audiomgr_add_channel_callback(lisa_evs_audiomgr_t *handle, channel_callback_cb *callback)
{
	for (int i = 0; i < CHANNEL_NUM; i++) {
		if (handle->m_callbacks[i] == NULL) {
			handle->m_callbacks[i] = callback;
			return;
		}
	}
}
