#include "lisa_mem.h"
#include <string.h>
#define TAG "main"
#include "lsc.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "lsc_session_voice.h"

#include "speex.h"

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"

#ifndef DEVICE_ID_STRING
#error "Please define DEVICE_ID_STRING first"
#endif

#define ENCODE_QUALITY 8

static SpeexBits encoder_bits;
void *speex_encoder;

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
	char cbits[400];
	LISA_NLOGI("[%s] size:%d", __FUNCTION__, size);
	int ret = 0;

	// Init speex encoder
	speex_bits_init(&encoder_bits);
	speex_encoder = speex_encoder_init(&speex_wb_mode);

	int framesize;
	int quality = ENCODE_QUALITY;
	speex_encoder_ctl(speex_encoder, SPEEX_SET_QUALITY, &quality);
	speex_encoder_ctl(speex_encoder, SPEEX_GET_FRAME_SIZE, &framesize);

	short *in_data = (short *)lisa_mem_calloc(sizeof(short), framesize);
	if (in_data == NULL) {
		LISA_NLOGI("lisa_mem_calloc failed");
		speex_bits_destroy(&encoder_bits);
		if (speex_encoder) {
			speex_encoder_destroy(speex_encoder);
		}
		return -1;
	}

	do {
		send_size = (remain_size > (framesize * sizeof(short))) ? framesize * sizeof(short) : remain_size;
		memset((char *)in_data, 0x0, framesize * sizeof(short));
		memcpy(in_data, ptr, send_size);

		speex_bits_reset(&encoder_bits);
		speex_encode_int(speex_encoder, in_data, &encoder_bits);
		memset(cbits, 0x0, sizeof(cbits));
		int nb_bytes = speex_bits_write(&encoder_bits, cbits, sizeof(cbits));

		ret = session_voice_send_audio(cbits, nb_bytes);
		lisa_thread_mdelay(20);

		ptr += send_size;
		remain_size -= send_size;
	} while (remain_size > 0);

	lisa_mem_free(in_data);
	speex_bits_destroy(&encoder_bits);
	speex_encoder_destroy(speex_encoder);

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
		.speex_size = 70,
		.aue = "speex-wb",
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