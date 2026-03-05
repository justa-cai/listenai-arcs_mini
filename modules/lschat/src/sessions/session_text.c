#define TAG "session_text"

#include "lsc_errno.h"
#include "lsc_conn.h"
#include "lsc_common.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"
#include "lisa_queue.h"
#include "lisa_log.h"
#include "lsc_sessions_core.h"
#include "lsc_session_text.h"

#define SESSION_TEXT_PROC_THREAD_PRIORITY   (LISA_OS_PRIORITY_NORMAL)
#define SESSION_TEXT_PROC_THREAD_STACK_SIZE (4 * 1024)
#define SESSION_TEXT_MSG_COUNT              (10)
#define SEESION_TEXT_SYNC_TIMEOUT_MS        (3000)

typedef struct player_msg_s {
	sessions_event_e evt;
	void *data;
	uint32_t size;
} session_text_msg_t;

typedef struct {
	session_text_config_t config;
	session_t *ss;
	lisa_evt_publisher_t *pub;
	lisa_thread_t *proc_thread;
	lisa_queue_t *msg_que;
	lisa_semaphore_t *sem_finished;
} session_text_t;

static session_text_t *g_session_text_obj = NULL;

static void text_session_event_cb(sessions_event_e evt, void *data, uint32_t size, void *usr)
{
	session_text_t *obj = g_session_text_obj;
	session_text_msg_t msg = {0};

	LISA_NLOGI("[%s] evt:%s size:%d", __FUNCTION__, STRINGS_SESSIONS_CORE_EVT(evt), size);

	// 处理线程中释放内存
	if (data && size > 0) {
		msg.data = lisa_mem_alloc(size);
		CHECK_COND_RETURN(msg.data, "no mem");

		memcpy(msg.data, data, size);
	}
	msg.size = size;
	msg.evt = evt;

	int ret = lisa_queue_push(obj->msg_que, &(msg), sizeof(session_text_msg_t), 0);
	CHECK_COND_RETURN(ret == LISA_OK, "queue push faild(ret = %d)", ret);
}

static void session_text_proc_thread(void *arg)
{
	session_text_t *obj = g_session_text_obj;
	session_text_msg_t msg;
	int ret;

	while (1) {
		ret = lisa_queue_pop(obj->msg_que, &msg, sizeof(session_text_msg_t), LISA_OS_WAIT_FOREVER);
		if (ret) {
			continue;
		}

		switch (msg.evt) {
		case SESSION_STARTED:
			break;
		case SESSION_REPLY_URL:
			lisa_evt_publisher_publish(obj->pub, SESSION_TEXT_REPLY_URL, msg.data, msg.size);
			break;
		case SESSION_TTS_URL:
			lisa_evt_publisher_publish(obj->pub, SESSION_TEXT_TTS_URL, msg.data, msg.size);
			break;
		case SESSION_FINISH:
			lisa_semaphore_give(obj->sem_finished);
			break;
		default:
			break;
		}

		if (msg.data) {
			lisa_mem_free(msg.data);
		}
	}
}

int session_text_init(void)
{
	CHECK_COND_RETURN_VAL(g_session_text_obj == NULL, LSC_INVALID_STATE, "already create");

	session_text_t *session = lisa_mem_calloc(1, sizeof(session_text_t));
	CHECK_COND_GOTO(session, _err, "no mem");

	session->ss = session_create();
	CHECK_COND_GOTO(session->ss, _err, "session create faild");

	int ret = session_add_evt_callback(session->ss, text_session_event_cb,
					   SESSION_TTS_URL | SESSION_REPLY_URL | SESSION_FINISH, NULL);
	CHECK_COND_GOTO(ret == LSC_OK, _err, "add cb faild");

	session_params_t config = {
		.data_type = "text",
		.nlu_params =
			{
				.enable = true,
			},
		.tts_params =
			{
				.enable = true,
			},
	};
	session_set_config(session->ss, &config);

	session->pub = lisa_evt_publisher_new();
	CHECK_COND_GOTO(session->pub, _err, "lisa new publisher faild");

	session->msg_que = lisa_queue_create(SESSION_TEXT_MSG_COUNT, NULL, sizeof(session_text_msg_t));
	CHECK_COND_GOTO(session->msg_que, _err, "msg_que create faild");

	session->sem_finished = lisa_semaphore_create(1);
	CHECK_COND_GOTO(session->sem_finished, _err, "sem_finished create faild");

	g_session_text_obj = session;

	lisa_thread_attr_t attr;
	attr.name = "session text proc";
	attr.priority = SESSION_TEXT_PROC_THREAD_PRIORITY;
	attr.stack_size = SESSION_TEXT_PROC_THREAD_STACK_SIZE;
	session->proc_thread = lisa_thread_create(&attr, session_text_proc_thread, NULL);
	CHECK_COND_GOTO(session->proc_thread, _err, "session_voice_proc_thread create faild");

	return LSC_OK;
_err:
	if (session) {
		if (session->ss) {
			session_destroy(session->ss);
		}

		if (session->pub) {
			lisa_evt_publisher_destroy(session->pub);
		}

		if (session->proc_thread) {
			lisa_thread_delete(session->proc_thread);
		}

		if (session->msg_que) {
			lisa_queue_delete(session->msg_que);
		}

		if (session->sem_finished) {
			lisa_semaphore_delete(session->sem_finished);
		}

		lisa_mem_free(session);
		g_session_text_obj = NULL;
	}

	return LSC_ERR;
}

int session_text_deinit(void)
{
	session_text_t *obj = g_session_text_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	if (obj->ss) {
		session_destroy(obj->ss);
	}

	if (obj->pub) {
		lisa_evt_publisher_destroy(obj->pub);
	}

	if (obj->proc_thread) {
		lisa_thread_delete(obj->proc_thread);
	}

	if (obj->msg_que) {
		lisa_queue_delete(obj->msg_que);
	}

	if (obj->sem_finished) {
		lisa_semaphore_delete(obj->sem_finished);
	}

	lisa_mem_free(obj);
	g_session_text_obj = NULL;

	return LSC_OK;
}

int session_text_set_config(session_text_config_t *cfg)
{
	session_text_t *obj = g_session_text_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	memcpy((void *)&(obj->config), (void *)cfg, sizeof(session_text_config_t));

	return LSC_OK;
}

int session_text_get_config(session_text_config_t *cfg)
{
	session_text_t *obj = g_session_text_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	memcpy((void *)cfg, (void *)&(obj->config), sizeof(session_text_config_t));

	return LSC_OK;
}

int session_text_add_evt_callback(session_text_event_cb_t cb, session_text_event_e evt, void *usr)
{
	session_text_t *obj = g_session_text_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	int ret = lisa_evt_publisher_evt_add(obj->pub, evt, (lisa_evt_publisher_cb_t)cb, usr);
	CHECK_COND_RETURN_VAL(ret == 0, LSC_ERR, "evt add faild");

	return LSC_OK;
}

int session_text_remove_evt_callback(session_text_event_cb_t cb)
{
	session_text_t *obj = g_session_text_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	lisa_evt_publisher_cb_remove(obj->pub, (lisa_evt_publisher_cb_t)cb);

	return LSC_OK;
}

int session_text_cancel(void)
{
	session_text_t *obj = g_session_text_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	return LSC_OK;
}

int session_text_send(char *txt)
{
	session_text_t *obj = g_session_text_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	session_params_t config = {
		.data_type = "text",
		.nlu_params =
			{
				.enable = true,
			},
		.tts_params =
			{
				.enable = true,
			},
	};
	session_set_config(obj->ss, &config);

	lisa_semaphore_reset(obj->sem_finished);

	int ret = session_start(obj->ss, NULL);
	CHECK_COND_RETURN_VAL(ret == LSC_OK, LSC_ERR, "session start faild");

	ret = session_send_bin(obj->ss, (uint8_t *)txt, strlen(txt) + 1);
	CHECK_COND_RETURN_VAL(ret == LSC_OK, LSC_ERR, "session send bin faild");

	lisa_err_t err = lisa_semaphore_take(obj->sem_finished, SEESION_TEXT_SYNC_TIMEOUT_MS);
	if (err != LISA_OK) {
		LISA_NLOGE("[%s] wait sem timeout", __FUNCTION__);
		return LSC_ERR;
	}

	return LSC_OK;
}

int session_text_tts_synth(const char *txt)
{
	session_text_t *obj = g_session_text_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	session_params_t config = {
		.data_type = "text",
		.nlu_params =
			{
				.enable = false,
			},
		.tts_params =
			{
				.enable = true,
			},
	};
	session_set_config(obj->ss, &config);

	lisa_semaphore_reset(obj->sem_finished);

	int ret = session_start(obj->ss, NULL);
	CHECK_COND_RETURN_VAL(ret == LSC_OK, LSC_ERR, "session start faild");

	ret = session_send_bin(obj->ss, (uint8_t *)txt, strlen(txt) + 1);
	CHECK_COND_RETURN_VAL(ret == LSC_OK, LSC_ERR, "session send bin faild");

	lisa_err_t err = lisa_semaphore_take(obj->sem_finished, SEESION_TEXT_SYNC_TIMEOUT_MS);
	if (err != LISA_OK) {
		LISA_NLOGE("[%s] wait sem timeout", __FUNCTION__);
		return LSC_ERR;
	}

	return LSC_OK;
}