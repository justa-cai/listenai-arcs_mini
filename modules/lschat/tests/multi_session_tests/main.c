#define TAG "session_voice_tests"

#include "lisa_log.h"
#include "lisa_semaphore.h"
#include "lsc.h"
#include "lsc_session_voice.h"
#include "lsc_session_text.h"
#include "lsc_objrec.h"
#include "lsc_session_request.h"
#include "lsc_errno.h"
#include "unity.h"
#include "lisa_thread.h"
#include "lisa_mem.h"
#include "lsc_stream_text.h"

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"
#define DEVICE_ID_STRING  "F97CE114C70DE8E2"

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

lisa_semaphore_t *sem_connected;
lisa_semaphore_t *sem_disconnected;
lisa_semaphore_t *sem_auth_faild;
lisa_semaphore_t *sem_got_token;
lisa_semaphore_t *sem_got_vad;
lisa_semaphore_t *sem_got_tts;
lisa_semaphore_t *sem_got_iat;
lisa_semaphore_t *sem_got_reply_url;
lisa_semaphore_t *sem_got_finished;
lisa_semaphore_t *sem_got_music_lists;
lisa_semaphore_t *sem_resume_music;
lisa_semaphore_t *sem_pause_music;
lisa_semaphore_t *sem_next_music;
lisa_semaphore_t *sem_last_music;

// 深圳天气怎么样
static const unsigned char weather_inc_file[] __attribute__((unused)) = {
#include <weather.pcm.inc>
};

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

// 图片数据
static const uint8_t test_img[] __attribute__((unused)) = {
#include "test.jpg.inc"
};

session_objrec_t objrec_session = NULL;
uint32_t total_size, free_size_before, free_size_after;
char request_url[256];

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
	case SESSION_VOICE_MUSIC_LISTS: {
		session_voice_music_lists_t *item = (session_voice_music_lists_t *)data;
		if (item->cnt) {
			char music_url[256];
			int ret = lsc_music_request_url(item->items[0].id, music_url);
			if (ret == LSC_OK) {
				lisa_semaphore_give(sem_got_music_lists);
			}
		}
		break;
	}
	case SESSION_VOICE_MUSIC_INSTR: {
		session_voice_music_instr_t *music_instr = (session_voice_music_instr_t *)data;
		LISA_NLOGI("music instr:%d", music_instr->instr);
		switch (music_instr->instr) {
		case SESSION_VOICE_MUSIC_INSTR_REPLAY:
			lisa_semaphore_give(sem_resume_music);
			break;
		case SESSION_VOICE_MUSIC_INSTR_CLOSE:
			lisa_semaphore_give(sem_pause_music);
			break;
		case SESSION_VOICE_MUSIC_INSTR_PAST:
			lisa_semaphore_give(sem_last_music);
			break;
		case SESSION_VOICE_MUSIC_INSTR_NEXT:
			lisa_semaphore_give(sem_next_music);
			break;
		default:
			break;
		}
		break;
	}
	case SESSION_VOICE_AIUI_CTRL: {
		LISA_NLOGI("aiui ctrl:%s", (char *)data);
		// 上一首，下一首，暂停播放，恢复播放可能会落阈到AIUI技能中而非音乐技能中
		lisa_semaphore_give(sem_resume_music);
		lisa_semaphore_give(sem_pause_music);
		lisa_semaphore_give(sem_last_music);
		lisa_semaphore_give(sem_next_music);
		break;
	}
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

static void test_session_text_init(void)
{
	int ret;
	ret = session_text_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_session_objrec_init(void)
{
	objrec_session = session_objrec_new();
	TEST_ASSERT_NOT_EQUAL(NULL, objrec_session);
}

static void test_session_request_init(void)
{
	int ret;
	ret = session_request_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_session_voice_init(void)
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
		.aue = "raw",
	};
	ret = session_voice_set_config(&config);
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_session_voice_normal(void)
{
	int ret;

	session_voice_config_t config = {
		.vad_enable = true,
		.device_id = DEVICE_ID_STRING,
	};
	ret = session_voice_set_config(&config);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	lisa_thread_mdelay(500);

	ret = session_voice_start();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = mock_record_audio_stream_send((uint8_t *)play_music_inc_file, sizeof(play_music_inc_file));
	TEST_ASSERT_NOT_EQUAL(LSC_ERR, ret);

	ret = lisa_semaphore_take(sem_got_vad, 8000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_tts, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_iat, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_reply_url, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_music_lists, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_finished, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);
}

static void objrec_stream_text_evt_cb_handle(int evt, const char *data, void *user)
{
	LISA_NLOGI("sse evt:%d, data:%s", evt, data);
}

static void test_session_objrec_normal(void)
{
	int ret = 0;

	struct session_objrec_result *result = lisa_mem_alloc(sizeof(struct session_objrec_result));
	TEST_ASSERT_NOT_EQUAL(NULL, result);

	ret = session_objrec_run(objrec_session, test_img, sizeof(test_img), result, 10 * 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	/* got result, request the text */
	struct lsc_stream_text_request_ctx *ctx =
		lsc_stream_text_request_new(result->text_url, objrec_stream_text_evt_cb_handle, NULL);
	TEST_ASSERT_NOT_EQUAL(NULL, ctx);

	ret = lsc_stream_text_request_start(ctx, 10);
	TEST_ASSERT_EQUAL(LISA_OK, ret);
}

static void test_session_text_normal(void)
{
	int ret = 0;
	char temp[] = "清除记忆";
	ret = session_text_send(temp);
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_session_request_normal(void)
{
	int ret = 0;
	char text[] = "深圳天气";
	memset((void *)request_url, 0, sizeof(request_url));
	ret = session_request_xtts(text, request_url, 30000);
	TEST_ASSERT_EQUAL(LSC_OK, ret);
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
	lisa_semaphore_reset(sem_got_music_lists);
	lisa_semaphore_reset(sem_resume_music);
	lisa_semaphore_reset(sem_pause_music);
	lisa_semaphore_reset(sem_next_music);
	lisa_semaphore_reset(sem_last_music);
	lisa_semaphore_reset(sem_got_finished);
}

void tearDown(void)
{
	lisa_mem_info_t info = {0};
	lisa_mem_info(&info);
	LISA_NLOGI("total size:%d, free size:%d, max alloced size:%d", info.total_size, info.free_size,
		   info.max_alloced_size);
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
	sem_got_music_lists = lisa_semaphore_create(1);
	sem_resume_music = lisa_semaphore_create(1);
	sem_pause_music = lisa_semaphore_create(1);
	sem_next_music = lisa_semaphore_create(1);
	sem_last_music = lisa_semaphore_create(1);
	sem_got_finished = lisa_semaphore_create(1);

	UNITY_BEGIN();

	RUN_TEST(test_lsc_init);
	RUN_TEST(test_session_voice_init);
	RUN_TEST(test_session_text_init);
	RUN_TEST(test_session_objrec_init);
	RUN_TEST(test_session_request_init);
	RUN_TEST(test_session_voice_normal);
	RUN_TEST(test_session_objrec_normal);
	RUN_TEST(test_session_text_normal);
	RUN_TEST(test_session_request_normal);

	UNITY_END();

	return 0;
}