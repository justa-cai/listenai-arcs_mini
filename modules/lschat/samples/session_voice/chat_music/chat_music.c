#define TAG "chat_music"
#include "lsc.h"
#include "lisa_mem.h"
#include "lsc_session_voice.h"
#include "lsc_errno.h"
#include "lisa_log.h"
#include "lisa_time.h"
#include "lisa_thread.h"

// 播放一首歌
static const unsigned char play_music_inc_file[] __attribute__((unused)) = {
#include <play_music.pcm.inc>
};

// 恢复播放
static const unsigned char resume_music_inc_file[] __attribute__((unused)) = {
#include <resume_music.pcm.inc>
};

// 暂停播放
static const unsigned char pause_music_inc_file[] __attribute__((unused)) = {
#include <pause_music.pcm.inc>
};

// 下一首
static const unsigned char next_music_inc_file[] __attribute__((unused)) = {
#include <next_music.pcm.inc>
};

// 上一首
static const unsigned char last_music_inc_file[] __attribute__((unused)) = {
#include <last_music.pcm.inc>
};

static void voice_event_cb(session_voice_event_e evt, void *data, uint32_t size, void *usr)
{
	LISA_NLOGI("[%s] evt:0x%x", __FUNCTION__, evt);

	switch (evt) {
	case SESSION_VOICE_GOT_VAD:
		LISA_NLOGI("got vad");
		break;
	case SESSION_VOICE_TTS:
		LISA_NLOGI("tts url:%s", (char *)data);
		break;
	case SESSION_VOICE_IAT:
		LISA_NLOGI("iat:%s", (char *)data);
		break;
	case SESSION_VOICE_REPLY_URL:
		LISA_NLOGI("reply url:%s", (char *)data);
		break;
	case SESSION_VOICE_MUSIC_INSTR: {
		session_voice_music_instr_t *music_instr = (session_voice_music_instr_t *)data;
		LISA_NLOGI("music instruction: %d  arg: %d", music_instr->instr, music_instr->arg);
	} break;
	case SESSION_VOICE_MUSIC_LISTS: {
		uint8_t i = 0;
		session_voice_music_lists_t *item = (session_voice_music_lists_t *)data;
		for (i = 0; i < item->cnt; i++) {
			LISA_NLOGI("itme_%d : %s", i, item->items[i].id);
		}

		// 获取播放列表中第一首歌的URL
		char music_url[256];
		lsc_music_request_url(item->items[0].id, music_url);
		LISA_NLOGI("itme_%d url: %s", i, music_url);
	} break;
	default:
		break;
	}
}

/*
	模拟流式产生录音数据，
*/
static int mock_record_audio_stream_send(uint8_t *data, uint32_t size)
{
	uint32_t remain_size = size;
	uint8_t *ptr = data;
	uint32_t send_size = 0;
	LISA_NLOGI("[%s] size:%d", __FUNCTION__, size);
	int ret = 0;
	do {
		send_size = (remain_size > 5120) ? 5120 : remain_size;
		ret = session_voice_send_audio(ptr, send_size);
		lisa_thread_mdelay(100);

		ptr += send_size;
		remain_size -= send_size;
	} while (remain_size > 0);

	return ret;
}

void chat_music_proc(void)
{
	int ret;

	ret = lsc_music_active();

	ret = session_voice_init();

	ret = session_voice_add_evt_callback(voice_event_cb,
					     SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_IAT |
						     SESSION_VOICE_REPLY_URL | SESSION_VOICE_MUSIC_LISTS |
						     SESSION_VOICE_MUSIC_INSTR,
					     NULL);

	session_voice_config_t config = {
		.vad_enable = true,
		.device_id = "F97CE114C70DE8E2",
		.speex_size = 0,
		.aue = "raw",
	};
	ret = session_voice_set_config(&config);

	ret = session_voice_start();

	ret = mock_record_audio_stream_send((uint8_t *)play_music_inc_file, sizeof(play_music_inc_file));
}
