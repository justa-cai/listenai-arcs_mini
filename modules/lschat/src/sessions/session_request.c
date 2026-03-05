#define TAG "session_request"

#include "lsc_errno.h"
#include "lsc_conn.h"
#include "lsc_common.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"
#include "lisa_log.h"
#include "lsc_sessions_core.h"
#include "lsc_session_request.h"
#include "lisa_queue.h"

typedef struct player_msg_s {
	sessions_event_e evt;
	void *data;
	uint32_t size;
} session_request_msg_t;

typedef struct {
	session_t *ss;
	lisa_queue_t *msg_que;
	lisa_evt_publisher_t *pub;
} session_request_t;

static session_request_t *g_session_request_obj = NULL;

static void request_session_event_cb(sessions_event_e evt, void *data, uint32_t size, void *usr)
{
	session_request_t *obj = g_session_request_obj;
	session_request_msg_t msg = {0};

	LISA_NLOGI("[%s] evt:%s size:%d", __FUNCTION__, STRINGS_SESSIONS_CORE_EVT(evt), size);

	switch (evt) {
	case SESSION_TTS_URL:
		if (data && size > 0) {
			msg.data = lisa_mem_alloc(size);
			CHECK_COND_RETURN(msg.data, "no mem");

			memcpy(msg.data, data, size);
		}
		msg.size = size;
		msg.evt = evt;
		break;
	case SESSION_FINISH:
		break;
	default:
		break;
	}

	int ret = lisa_queue_push(obj->msg_que, &(msg), sizeof(session_request_t), 0);
	CHECK_COND_RETURN(ret == LISA_OK, "queue push faild(ret = %d)", ret);
}

int session_request_init(void)
{
	CHECK_COND_RETURN_VAL(g_session_request_obj == NULL, LSC_INVALID_STATE, "already create");

	session_request_t *session = lisa_mem_calloc(1, sizeof(session_request_t));
	CHECK_COND_GOTO(session, _err, "no mem");

	session->ss = session_create();
	CHECK_COND_GOTO(session->ss, _err, "session create faild");

	int ret =
		session_add_evt_callback(session->ss, request_session_event_cb, SESSION_TTS_URL | SESSION_FINISH, NULL);
	CHECK_COND_GOTO(ret == LSC_OK, _err, "add cb faild");

	session->msg_que = lisa_queue_create(1, NULL, sizeof(session_request_msg_t));
	CHECK_COND_GOTO(session->msg_que, _err, "msg_que create faild");

	g_session_request_obj = session;
	return LSC_OK;
_err:
	if (session) {
		if (session->ss) {
			session_destroy(session->ss);
		}

		if (session->msg_que) {
			lisa_queue_delete(session->msg_que);
		}

		lisa_mem_free(session);
		g_session_request_obj = NULL;
	}

	return LSC_ERR;
}

int session_request_deinit(void)
{
	session_request_t *obj = g_session_request_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	if (obj->ss) {
		session_destroy(obj->ss);
	}

	if (obj->msg_que) {
		lisa_queue_delete(obj->msg_que);
	}

	lisa_mem_free(obj);
	g_session_request_obj = NULL;

	return LSC_OK;
}

int session_request_cancel(void)
{
	session_request_t *obj = g_session_request_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	return LSC_OK;
}

int session_request_xtts(char *txt, char url[256], uint32_t timeout_ms)
{
	session_request_t *obj = g_session_request_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	session_request_msg_t msg = {0};

	session_params_t config = {
		.data_type = "text",
		.tts_params =
			{
				.enable = true,
			},
	};
	session_set_config(obj->ss, &config);

	lisa_queue_clear(obj->msg_que);

	int ret = session_start(obj->ss, NULL);
	CHECK_COND_RETURN_VAL(ret == LSC_OK, LSC_ERR, "session start faild");

	ret = session_send_bin(obj->ss, (uint8_t *)txt, strlen(txt) + 1);
	CHECK_COND_RETURN_VAL(ret == LSC_OK, LSC_ERR, "session send bin faild");

	ret = lisa_queue_pop(obj->msg_que, &msg, sizeof(session_request_msg_t), timeout_ms);
	CHECK_COND_RETURN_VAL(ret == LISA_OK, LSC_ERR, "wait msg timeout");

	strcpy(url, (char *)msg.data);

	if (msg.data) {
		lisa_mem_free(msg.data);
	}

	return LSC_OK;
}