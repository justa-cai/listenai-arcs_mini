#define TAG "chat_weather"

#include "lisa_mem.h"
#include "lsc_session_voice.h"
#include "lsc_errno.h"
#include "lisa_log.h"
#include "lisa_time.h"
#include "lisa_http.h"
#include "lisa_thread.h"
static const unsigned char weather_inc_file[] = {
#include <weather.pcm.inc>
};

static void voice_event_cb(session_voice_event_e evt, void *data, uint32_t size, void *usr)
{
	LISA_NLOGI("[%s] evt:%d", __FUNCTION__, evt);

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

void chat_weather_proc(void)
{
	int ret;
	ret = session_voice_init();

	ret = session_voice_add_evt_callback(
		voice_event_cb, SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_IAT | SESSION_VOICE_REPLY_URL,
		NULL);

	session_voice_config_t config = {
		.vad_enable = true,
		.speex_size = 0,
		.aue = "raw",
	};
	ret = session_voice_set_config(&config);

	ret = session_voice_start();

	ret = mock_record_audio_stream_send((uint8_t *)weather_inc_file, sizeof(weather_inc_file));
}
