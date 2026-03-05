#include "lisa_mem.h"
#define TAG "main"

#include "lsc.h"
#include "lsc_objrec.h"
#include "lsc_stream_text.h"

#include "lisa_log.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"

#include <stdint.h>
#include <stddef.h>

#define PID "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SID "c684eb9e-ff8f-4264-ad03-0369b38ab793"

#ifndef DEVICE_ID_STRING
#error "Please define DEVICE_ID_STRING first"
#endif

const uint8_t test_img[] = {
#include "test.jpg.inc"
};

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
	LISA_NLOGI("lsc event:%d", evt);
	if (evt == LSC_CONNECTED) {
		lisa_semaphore_t *sem = usr;
		if (sem) {
			lisa_semaphore_give(sem);
		}
	}
}

static void lsc_sse_evt_cb_handle(int evt, const char *data, void *user)
{
	LISA_NLOGI("sse evt:%d, data:%s", evt, data);
}

void sample_session_objrec_sync(void)
{
	int err;

	session_objrec_t objrec = session_objrec_new();
	if (objrec == NULL) {
		LISA_NLOGE("session_objrec_init failed");
		return;
	}

	struct session_objrec_result *result = lisa_mem_alloc(sizeof(struct session_objrec_result));
	if (result == NULL) {
		LISA_NLOGE("session_objrec_result alloc failed");
		session_objrec_delete(objrec);
		return;
	}

	err = session_objrec_run(objrec, test_img, sizeof(test_img), result, 10 * 1000);
	if (err) {
		LISA_NLOGE("session_objrec_run failed, err:%d", err);
		session_objrec_delete(objrec);
		lisa_mem_free(result);
		return;
	}

	/* got result, request the text */
	struct lsc_stream_text_request_ctx *ctx =
		lsc_stream_text_request_new(result->text_url, lsc_sse_evt_cb_handle, NULL);
	if (ctx) {
		err = lsc_stream_text_request_start(ctx, 10);
		if (err) {
			LISA_NLOGE("lsc_stream_text_request failed, err:%d", err);
		}
		lsc_stream_text_request_delete(ctx);
	} else {
		LISA_NLOGE("lsc_stream_text_request_ctx new failed");
	}

	session_objrec_delete(objrec);
	lisa_mem_free(result);

	LISA_NLOGI("session object sync run done");
}

static void sample_session_objrec_evt_cb_handle(session_objrec_t s, int evt, void *data, uint32_t data_len, void *user)
{
	LISA_NLOGI("sample session objrec, evt:%d, data:%s", evt, (char *)data);
}

void sample_session_objrec_async(void)
{
	int err;

	session_objrec_t objrec = session_objrec_new();
	if (objrec == NULL) {
		LISA_NLOGE("session_objrec_init failed");
		return;
	}

	err = session_objrec_run_async(objrec, test_img, sizeof(test_img), sample_session_objrec_evt_cb_handle, NULL);
	if (err) {
		LISA_NLOGE("session_objrec_run failed, err:%d", err);
		session_objrec_delete(objrec);
		return;
	}

	LISA_NLOGI("session object async run successfully");
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
	int err;
	lisa_semaphore_t *connected_sem = NULL;

	connected_sem = lisa_semaphore_create(1);
	if (connected_sem == NULL) {
		LISA_NLOGE("connected semaphore creation failed");
		return -1;
	}

	/* network connection */
	net_init();
	net_up();

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PID,
		.secret_id = SID,
	};
	err = lsc_init(&cfg);
	if (err) {
		LISA_NLOGE("lsc_init failed, err:%d", err);
		return -1;
	}

	err = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb,
			       connected_sem);
	if (err) {
		LISA_NLOGE("lsc_add_callback failed, err:%d", err);
		return -1;
	}

	err = lsc_connect();
	if (err) {
		LISA_NLOGE("lsc_connect failed, err:%d", err);
		return -1;
	}

	err = lisa_semaphore_take(connected_sem, 10 * 1000);
	if (err) {
		LISA_NLOGE("lisa_semaphore_take failed, err:%d", err);
		return -1;
	}

	sample_session_objrec_sync();
	sample_session_objrec_async();
}
