#define TAG "session_voice_tests"

#include "lisa_log.h"
#include "lisa_semaphore.h"
#include "lsc.h"
#include "lsc_session_voice.h"
#include "lsc_errno.h"
#include "unity.h"
#include "lisa_thread.h"
#include "lisa_mem.h"

#include <string.h>

#include "speex.h"
#include "ico_codec.h"

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"
#define DEVICE_ID_STRING  "F97CE114C70DE8E2"

#define ENCODE_QUALITY 8

lisa_semaphore_t *sem_connected;
lisa_semaphore_t *sem_disconnected;
lisa_semaphore_t *sem_auth_faild;
lisa_semaphore_t *sem_got_token;
lisa_semaphore_t *sem_got_vad;
lisa_semaphore_t *sem_got_tts;
lisa_semaphore_t *sem_got_iat;
lisa_semaphore_t *sem_got_reply_url;
lisa_semaphore_t *sem_got_finished;
lisa_semaphore_t *sem_result_raw_data;

static SpeexBits encoder_bits;
void *speex_encoder;

// 深圳天气怎么样
static const unsigned char weather_inc_file[] __attribute__((unused)) = {
#include <weather.pcm.inc>
};

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

size_t total_size, free_size_before, free_size_after;

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
	switch (evt) {
	case LSC_CONNECTED:
		lisa_semaphore_give(sem_connected);
		break;
	case LSC_DISCONNECTED:
		lisa_semaphore_give(sem_disconnected);
		break;
	case LSC_CLOUD_AUTH_FAILD:
		lisa_semaphore_give(sem_auth_faild);
		break;
	case LSC_GOT_TOKEN:
		lisa_semaphore_give(sem_got_token);
		break;
	default:
		break;
	}
}

static void voice_event_cb(session_voice_event_e evt, void *data, uint32_t size, void *usr)
{
	LISA_NLOGI("[%s] evt:0x%x", __FUNCTION__, evt);

	switch (evt) {
	case SESSION_VOICE_FINISH:
		lisa_semaphore_give(sem_got_finished);
		break;
	case SESSION_VOICE_GOT_VAD:
		lisa_semaphore_give(sem_got_vad);
		break;
	case SESSION_VOICE_TTS:
		LISA_NLOGI("tts url:%s", (char *)data);
		if (strlen(data) > 10) {
			lisa_semaphore_give(sem_got_tts);
		}
		break;
	case SESSION_VOICE_IAT:
		LISA_NLOGI("iat:%s", (char *)data);
		if (strlen(data)) {
			lisa_semaphore_give(sem_got_iat);
		}
		break;
	case SESSION_VOICE_REPLY_URL:
		LISA_NLOGI("reply url:%s", (char *)data);
		if (strlen(data)) {
			lisa_semaphore_give(sem_got_reply_url);
		}
		break;
	case SESSION_VOICE_RAW_DATA: {
		LISA_NLOGI("raw json:%s", (char *)data);
		lisa_semaphore_give(sem_result_raw_data);
		break;
	}
	default:
		break;
	}
}

/*
	模拟流式产生speex录音数据，
*/
static int mock_speex_audio_stream_send(uint8_t *data, uint32_t size)
{
	uint32_t remain_size = size;
	uint8_t *ptr = data;
	uint32_t send_size = 0;
	char cbits[400];
	LISA_NLOGI("[%s] size:%d", __FUNCTION__, size);
	int ret = 0;

	int framesize;
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
	in_data = NULL;

	return ret;
}

/*
	模拟流式产生ICO录音数据，
*/
static int mock_ico_audio_stream_send(uint8_t *data, uint32_t size)
{
#define AUDIO_STREAM_PCM_FRAME_SIZE (640)
#define AUDIO_STREAM_ICO_FRAME_SIZE (40)

	uint32_t remain_size = size;
	uint8_t *ptr = data;
	uint32_t send_size = 0;
	LISA_NLOGI("[%s] size:%d", __FUNCTION__, size);
	int ret = 0;
	short ico_enc_len = 0;
	uint8_t ico_data[AUDIO_STREAM_ICO_FRAME_SIZE] = {0};
	uint8_t pcm_data[AUDIO_STREAM_PCM_FRAME_SIZE] = {0};

	do {
		send_size = (remain_size > AUDIO_STREAM_PCM_FRAME_SIZE) ? AUDIO_STREAM_PCM_FRAME_SIZE : remain_size;
		memset(pcm_data, 0x0, AUDIO_STREAM_PCM_FRAME_SIZE);
		memcpy(pcm_data, ptr, send_size);
		ptr += send_size;
		remain_size -= send_size;

		ret = ico_codec_encode((short *)pcm_data, (void *)&ico_data[0], &ico_enc_len);
		if (ret != 0) {
			return -1;
		}

		ico_enc_len <<= 1;
		ret = session_voice_send_audio(ico_data, ico_enc_len);
		lisa_thread_mdelay(30);
	} while (remain_size > 0);

	return ret;
}

static void test_session_voice_speex_weather(void)
{
	int ret;

	ret = session_voice_start();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = mock_speex_audio_stream_send((uint8_t *)weather_inc_file, sizeof(weather_inc_file));
	TEST_ASSERT_NOT_EQUAL(LSC_ERR, ret);

	ret = lisa_semaphore_take(sem_got_vad, 8000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_tts, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_iat, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_reply_url, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_finished, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);
}

static void test_session_voice_ico_weather(void)
{
	int ret;

	ret = session_voice_start();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = mock_ico_audio_stream_send((uint8_t *)weather_inc_file, sizeof(weather_inc_file));
	TEST_ASSERT_NOT_EQUAL(LSC_ERR, ret);

	ret = lisa_semaphore_take(sem_got_vad, 8000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_tts, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_iat, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_reply_url, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_finished, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);
}

static void test_lsc_deinit(void)
{
	int ret;

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_lsc_init(void)
{
	int ret;

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
	};
	ret = lsc_init(&cfg);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb,
			       NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lsc_connect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_connected, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_token, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);
}

static void test_session_voice_speex_deinit(void)
{
	int ret;
	ret = session_voice_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	speex_bits_destroy(&encoder_bits);
	speex_encoder_destroy(speex_encoder);
	speex_encoder = NULL;
}

static void test_session_voice_ico_deinit(void)
{
	int ret;
	ret = session_voice_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ico_codec_reset();
}

static void test_session_speexnb_init(void)
{
	int ret;
	ret = session_voice_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = session_voice_add_evt_callback(voice_event_cb,
					     SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_IAT |
						     SESSION_VOICE_AIUI_CTRL | SESSION_VOICE_MUSIC_INSTR |
						     SESSION_VOICE_MUSIC_LISTS | SESSION_VOICE_REPLY_URL |
						     SESSION_VOICE_DRAW | SESSION_VOICE_FINISH,
					     NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	session_voice_config_t config = {
		.vad_enable = true,
		.device_id = DEVICE_ID_STRING,
		.speex_size = 38,
		.aue = "speex",
	};
	ret = session_voice_set_config(&config);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	// Init speex encoder
	speex_bits_init(&encoder_bits);
	speex_encoder = speex_encoder_init(&speex_nb_mode);
	TEST_ASSERT_NOT_EQUAL(speex_encoder, NULL);

	int quality = ENCODE_QUALITY;
	ret = speex_encoder_ctl(speex_encoder, SPEEX_SET_QUALITY, &quality);
	TEST_ASSERT_EQUAL(0, ret);
}

static void test_session_speexwb_init(void)
{
	int ret;
	ret = session_voice_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = session_voice_add_evt_callback(voice_event_cb,
					     SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_IAT |
						     SESSION_VOICE_AIUI_CTRL | SESSION_VOICE_MUSIC_INSTR |
						     SESSION_VOICE_MUSIC_LISTS | SESSION_VOICE_REPLY_URL |
						     SESSION_VOICE_DRAW | SESSION_VOICE_FINISH,
					     NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	session_voice_config_t config = {
		.vad_enable = true,
		.device_id = DEVICE_ID_STRING,
		.speex_size = 70,
		.aue = "speex-wb",
	};
	ret = session_voice_set_config(&config);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	// Init speex encoder
	speex_bits_init(&encoder_bits);
	speex_encoder = speex_encoder_init(&speex_wb_mode);
	TEST_ASSERT_NOT_EQUAL(speex_encoder, NULL);

	int quality = ENCODE_QUALITY;
	ret = speex_encoder_ctl(speex_encoder, SPEEX_SET_QUALITY, &quality);
	TEST_ASSERT_EQUAL(0, ret);
}

static void test_session_ico_init(void)
{
	int ret;
	ret = session_voice_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = session_voice_add_evt_callback(voice_event_cb,
					     SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_IAT |
						     SESSION_VOICE_AIUI_CTRL | SESSION_VOICE_MUSIC_INSTR |
						     SESSION_VOICE_MUSIC_LISTS | SESSION_VOICE_REPLY_URL |
						     SESSION_VOICE_DRAW | SESSION_VOICE_FINISH,
					     NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	session_voice_config_t config = {
		.vad_enable = true,
		.device_id = DEVICE_ID_STRING,
		.speex_size = 0,
		.aue = "ico",
	};
	ret = session_voice_set_config(&config);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = ico_encode_init();
	TEST_ASSERT_EQUAL(0, ret);
}

void setUp(void)
{
	lisa_semaphore_reset(sem_connected);
	lisa_semaphore_reset(sem_disconnected);
	lisa_semaphore_reset(sem_auth_faild);
	lisa_semaphore_reset(sem_got_token);
	lisa_semaphore_reset(sem_got_reply_url);
	lisa_semaphore_reset(sem_got_iat);
	lisa_semaphore_reset(sem_got_tts);
	lisa_semaphore_reset(sem_got_vad);
	lisa_semaphore_reset(sem_got_finished);
	lisa_semaphore_reset(sem_result_raw_data);
}

void tearDown(void)
{
	lisa_thread_delay(2);
}

int main(void)
{
	net_init();
	net_up();

	sem_connected = lisa_semaphore_create(1);
	sem_disconnected = lisa_semaphore_create(1);
	sem_auth_faild = lisa_semaphore_create(1);
	sem_got_token = lisa_semaphore_create(1);
	sem_got_reply_url = lisa_semaphore_create(1);
	sem_got_iat = lisa_semaphore_create(1);
	sem_got_tts = lisa_semaphore_create(1);
	sem_got_vad = lisa_semaphore_create(1);
	sem_got_finished = lisa_semaphore_create(1);
	sem_result_raw_data = lisa_semaphore_create(1);

	UNITY_BEGIN();

	RUN_TEST(test_lsc_init);

	RUN_TEST(test_session_speexnb_init);
	RUN_TEST(test_session_voice_speex_weather);
	RUN_TEST(test_session_voice_speex_deinit);

	RUN_TEST(test_session_speexwb_init);
	RUN_TEST(test_session_voice_speex_weather);
	RUN_TEST(test_session_voice_speex_deinit);

	RUN_TEST(test_session_ico_init);
	RUN_TEST(test_session_voice_ico_weather);
	RUN_TEST(test_session_voice_ico_deinit);

	RUN_TEST(test_lsc_deinit);

	UNITY_END();

	return 0;
}