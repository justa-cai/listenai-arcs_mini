#ifndef __LISA_EVS_SDK_SPEAKER_H__
#define __LISA_EVS_SDK_SPEAKER_H__

typedef struct lisa_evs_speaker_state {
	uint8_t version[12];
	int volume;
} lisa_evs_speaker_state_t;

typedef struct lisa_evs_speaker_cb {
	/**
	 * @brief 			音量调节消息
	 * @param msg		消息文本
	 * @param req_id	请求id
	 */
	void (*on_set_vol)(const uint8_t *const msg, const uint8_t *const req_id);
} lisa_evs_speaker_cb_t;

#endif
