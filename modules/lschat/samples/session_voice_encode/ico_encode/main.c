#define TAG "main"
#include <string.h>
#include "lsc.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "lsc_session_voice.h"

#include "ico_codec.h"

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"

#ifndef DEVICE_ID_STRING
#error "Please define DEVICE_ID_STRING first"
#endif

#define AUDIO_SEND_DATA_MAX_SIZE (2560)

#define AUDIO_STREAM_PCM_FRAME_SIZE (640)
#define AUDIO_STREAM_ICO_FRAME_SIZE (40)
uint8_t pcm_data[AUDIO_STREAM_PCM_FRAME_SIZE] = {0};

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
	LISA_NLOGI("evt : %s", STRINGS_LSC_EVT(evt));
}

__attribute__((weak)) int net_down(void)
{
	return 0;
}

__attribute__((weak)) int net_up(void)
{
	return 0;
}

__attribute__((weak)) int net_init(void)
{
	return 0;
}

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
	uint8_t ico_data[AUDIO_STREAM_ICO_FRAME_SIZE] = {0};
	short ico_enc_len = 0;
	LISA_NLOGI("[%s] size:%d", __FUNCTION__, size);
	int ret = 0;

	// Init ico encoder
	ret = ico_encode_init();

	do {
		send_size = (remain_size > AUDIO_STREAM_PCM_FRAME_SIZE) ? AUDIO_STREAM_PCM_FRAME_SIZE : remain_size;
		memset(pcm_data, 0x0, AUDIO_STREAM_PCM_FRAME_SIZE);
		memcpy(pcm_data, ptr, send_size);
		ptr += send_size;
		remain_size -= send_size;

		ret = ico_codec_encode((short *)pcm_data, (void *)&ico_data[0], &ico_enc_len);

		ico_enc_len <<= 1;
		ret = session_voice_send_audio(ico_data, ico_enc_len);
		lisa_thread_mdelay(30);
	} while (remain_size > 0);

	return ret;
}

void chat_weather_proc()
{
	int ret;
	ret = session_voice_init();

	ret = session_voice_add_evt_callback(
		voice_event_cb, SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_IAT | SESSION_VOICE_REPLY_URL,
		NULL);

	session_voice_config_t config = {
		.vad_enable = true,
		.speex_size = 0,
		.aue = "ico",
	};
	ret = session_voice_set_config(&config);

	ret = session_voice_start();

	ret = mock_record_audio_stream_send((uint8_t *)weather_inc_file, sizeof(weather_inc_file));
}

int main(void)
{
	net_init();
	net_up();

	int ret;

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
	};
	ret = lsc_init(&cfg);

	ret = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb,
			       NULL);

	ret = lsc_connect();

	lisa_thread_mdelay(3000);

	chat_weather_proc();

	while (1) {
		lisa_thread_mdelay(1000);
	}
}