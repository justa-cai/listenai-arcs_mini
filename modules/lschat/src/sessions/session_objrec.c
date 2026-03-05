#define TAG "session-objec"

#include "lsc.h"
#include "lsc_errno.h"
#include "lsc_objrec.h"
#include "lsc_base64.h"
#include "lsc_stream_text.h"
#include "lsc_sessions_core.h"

#include "lisa_log.h"
#include "lisa_http.h"
#include "lisa_semaphore.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

struct session_objrec_async_ctx {
	struct session_objrec_result *result;
	lisa_semaphore_t *done_sem;
};

struct session_objrec {
	session_t *ss;
	void *user;
	session_objrec_evt_cb_t cb;
};

static void session_evt_cb_handle_default(session_objrec_t s, int evt, void *data, uint32_t len, void *user)
{
	struct session_objrec_async_ctx *ctx = user;
	LISA_NLOGI("session_evt_cb_handle_default, evt=%d", evt);

	if (ctx == NULL || ctx->result == NULL || ctx->done_sem == NULL) {
		LISA_NLOGE("session_evt_cb_handle_default, invalid param");
		return;
	}

	if (evt == SESSION_OBJREC_EVT_TEXT_URL) {
		int cpy_len = len >= SESSION_OBJREC_URL_MAX_SIZE ? SESSION_OBJREC_URL_MAX_SIZE - 1 : len;
		memcpy(ctx->result->text_url, data, cpy_len);
		ctx->result->text_url[cpy_len] = '\0';
		lisa_semaphore_give(ctx->done_sem);
	} else if (evt == SESSION_OBJREC_EVT_TTS_URL) {
		int cpy_len = len >= SESSION_OBJREC_URL_MAX_SIZE ? SESSION_OBJREC_URL_MAX_SIZE - 1 : len;
		memcpy(ctx->result->tts_url, data, cpy_len);
		ctx->result->tts_url[cpy_len] = '\0';
		lisa_semaphore_give(ctx->done_sem);
	}
}

static int obj_rec_run_with_base64(session_t *s, uint8_t *base64_pic, uint32_t size)
{
	int err;

	if (base64_pic == NULL || size == 0) {
		return LSC_INVALID_PARAM;
	}

	err = session_start(s, base64_pic);
	if (err) {
		LISA_NLOGE("session_start failed, err:%d", err);
		return err;
	}

	const uint8_t prompt[] = "这个图片里面有什么东西";
	err = session_send_bin(s, prompt, sizeof(prompt));
	if (err) {
		LISA_NLOGE("aiui_picture_send_text failed, err:%d", err);
		return err;
	}

	return err;
}

static void sessions_obj_event_cb_handle(sessions_event_e evt, void *data, uint32_t size, void *usr)
{
	struct session_objrec *objrec = usr;
	int objrec_evt = -1;

	LISA_NLOGI("sessions_obj_event_cb_handle, evt:%d", evt);

	if (objrec == NULL || objrec->cb == NULL) {
		LISA_NLOGE("session_obj_event, invalid arguments");
		return;
	}

	if (evt & SESSION_REPLY_URL) {
		objrec_evt = SESSION_OBJREC_EVT_TEXT_URL;
	} else if (evt & SESSION_TTS_URL) {
		objrec_evt = SESSION_OBJREC_EVT_TTS_URL;
	}

	if (objrec_evt >= 0) {
		objrec->cb(objrec, objrec_evt, data, size, objrec->user);
	}
}

void session_objrec_delete(session_objrec_t s)
{
	struct session_objrec *objrec = s;

	if (objrec) {
		if (objrec->ss) {
			session_destroy(objrec->ss);
		}
		lisa_mem_free(objrec);
	}
}

session_objrec_t session_objrec_new()
{
	int err;

	struct session_objrec *objrec = lisa_mem_alloc(sizeof(struct session_objrec));
	if (objrec == NULL) {
		return NULL;
	}

	objrec->ss = session_create();
	if (objrec->ss == NULL) {
		goto err_exit;
	}

	strcpy(objrec->ss->params.data_type, "text");

	err = session_add_evt_callback(objrec->ss, sessions_obj_event_cb_handle, SESSION_REPLY_URL | SESSION_TTS_URL,
				       objrec);
	if (err) {
		goto err_exit;
	}

	return objrec;

err_exit:

	if (objrec) {
		if (objrec->ss) {
			session_destroy(objrec->ss);
		}
		lisa_mem_free(objrec);
	}

	return NULL;
}

int session_objrec_run_async(session_objrec_t s, const void *jpg_img, uint32_t size, session_objrec_evt_cb_t cb,
			     void *user)
{
	int err;
	struct session_objrec *objrec = s;

	if (objrec == NULL || objrec->ss == NULL) {
		return LSC_INVALID_PARAM;
	}

	if (jpg_img == NULL || size == 0 || cb == NULL) {
		return LSC_INVALID_PARAM;
	}

	objrec->cb = cb;
	objrec->user = user;

	uint8_t *buf = lsc_base64_encodev2(jpg_img, size);
	if (buf == NULL) {
		return LSC_NO_MEM;
	}

	err = obj_rec_run_with_base64(objrec->ss, buf, strlen(buf));
	lisa_mem_free(buf);
	if (err) {
		return err;
	}

	return 0;
}

int session_objrec_run(session_objrec_t s, const void *jpg_img, uint32_t size, struct session_objrec_result *result,
		       int timeout)
{
	int err;
	struct session_objrec *objrec = s;

	if (objrec == NULL || result == NULL) {
		return LSC_INVALID_PARAM;
	}

	struct session_objrec_async_ctx *ctx = lisa_mem_alloc(sizeof(struct session_objrec_async_ctx));
	if (ctx == NULL) {
		return -1;
	}
	ctx->done_sem = lisa_semaphore_create(2);
	if (ctx->done_sem == NULL) {
		lisa_mem_free(ctx);
		return -1;
	}
	ctx->result = result;

	err = session_objrec_run_async(objrec, jpg_img, size, session_evt_cb_handle_default, ctx);
	if (err) {
		lisa_semaphore_delete(ctx->done_sem);
		lisa_mem_free(ctx);
		return err;
	}

	/* take sem twice */
	int i;
	for (i = 0; i < 2; i++) {
		err = lisa_semaphore_take(ctx->done_sem, timeout);
		if (err) {
			LISA_NLOGE("session objrec get result timeout, %d", i);
			lisa_semaphore_delete(ctx->done_sem);
			lisa_mem_free(ctx);
			return err;
		}
	}

	lisa_semaphore_delete(ctx->done_sem);
	lisa_mem_free(ctx);

	return 0;
}
