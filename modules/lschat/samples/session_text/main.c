#define TAG "main"

#include "lisa_log.h"
#include "lisa_semaphore.h"
#include "lisa_thread.h"
#include "lsc.h"
#include "lsc_session_text.h"
#include "lsc_errno.h"
#include "lisa_mem.h"
#include <string.h>

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"
#ifndef DEVICE_ID_STRING
#error "Please define DEVICE_ID_STRING first"
#endif

char request_url[256];
lisa_semaphore_t *sem_connected;

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
	switch (evt) {
	case LSC_CONNECTED:
		lisa_semaphore_give(sem_connected);
		break;
	default:
		break;
	}
}

static void text_event_cb(session_text_event_e evt, void *data, uint32_t size, void *usr)
{
	switch (evt) {
	case SESSION_TEXT_TTS_URL:
		LISA_NLOGI("tts_url: %s", (char *)data);
		break;
	case SESSION_TEXT_REPLY_URL:
		LISA_NLOGI("reply url: %s", (char *)data);
		break;
	default:
		break;
	}
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

int main(void)
{
	net_init();
	net_up();

	int ret;

	sem_connected = lisa_semaphore_create(1);

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
	};
	ret = lsc_init(&cfg);
	if (ret) {
		LISA_NLOGW("lsc_init faild");
		return 0;
	}

	ret = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb,
			       NULL);
	if (ret) {
		LISA_NLOGW("lsc_add_callback faild");
		return 0;
	}

	ret = lsc_connect();
	ret |= lisa_semaphore_take(sem_connected, 1000);
	if (ret) {
		LISA_NLOGW("lsc_connect faild");
		return 0;
	}

	ret = session_text_init();
	if (ret) {
		LISA_NLOGW("session_text_init faild");
		return 0;
	}

	ret = session_text_add_evt_callback(text_event_cb, SESSION_TEXT_TTS_URL | SESSION_TEXT_REPLY_URL, NULL);

	ret = session_text_send("深圳天气如何");
	if (ret) {
		LISA_NLOGW("session_text_send faild");
		return 0;
	}

	return 0;
}