#define TAG "session_voice"

#include "lsc_errno.h"
#include "lsc_conn.h"
#include "lsc_common.h"
#include "lisa_thread.h"
#include "lisa_mutex.h"
#include "lisa_semaphore.h"
#include "lisa_queue.h"
#include "lisa_log.h"
#include "lsc_sessions_core.h"
#include "lsc_session_voice.h"
#include "cJSON.h"
#include "lsc_config.h"

#define SESSION_VOICE_PROC_THREAD_PRIORITY   (LISA_OS_PRIORITY_NORMAL)
#define SESSION_VOICE_PROC_THREAD_STACK_SIZE (4 * 1024)
#define SESSION_VOICE_MSG_COUNT              (10)
#define SEESION_VOICE_SYNC_TIMEOUT_MS        (3000)
#define SESSION_VOICE_FULL_DUPLEX_TIMEOUT_S  (40)

/* clang-format off */
#define SESSION_VOICE_LOCK(mutex)                           		\
	do {                                                     		\
		lisa_mutex_lock(mutex, LISA_WAIT_FOREVER);  				\
	} while (0)

#define SESSION_VOICE_UNLOCK(mutex)                                 \
	do {                                                       		\
		lisa_mutex_unlock(mutex);  									\
	} while (0)
/* clang-format on */
typedef struct player_msg_s {
	sessions_event_e evt;
	void *data;
	uint32_t size;
} session_voice_msg_t;

typedef struct {
	session_voice_config_t config;
	session_t *ss;
	lisa_evt_publisher_t *pub;
	lisa_thread_t *proc_thread;
	lisa_queue_t *msg_que;
	lisa_semaphore_t *sem_started;
	volatile bool need_audio;
	lisa_mutex_t *mutex;
	char current_model_id[64]; /* 当前使用的模型ID */
} session_voice_t;

static session_voice_t *g_session_voice_obj = NULL;

static bool voice_session_if_full_duplex(void)
{
	session_voice_config_t config = g_session_voice_obj->config;

	return config.inter_mode == SESSION_VOICE_INTER_MODE_ONESHOT ? false : true;
}

static void voice_session_event_cb(sessions_event_e evt, void *data, uint32_t size, void *usr)
{
	session_voice_t *obj = g_session_voice_obj;
	session_voice_msg_t msg = {0};

	// SESSION_VOICE_RAW_DATA is not registered, dispatch is not required
	if (evt == SESSION_RESULT_RAW_DATA &&
	    lisa_evt_publisher_has_subscriber(obj->pub, SESSION_VOICE_RAW_DATA) == 0) {
		return;
	}

	LISA_NLOGI("[%s] evt:%s, size:%d", __FUNCTION__, STRINGS_SESSIONS_CORE_EVT(evt), size);

	// free mem in msg thread
	if (data && size > 0) {
		msg.data = lisa_mem_alloc(size);
		CHECK_COND_RETURN(msg.data, "no mem");

		memcpy(msg.data, data, size);
	}
	msg.size = size;
	msg.evt = evt;

	int ret = lisa_queue_push(obj->msg_que, &(msg), sizeof(session_voice_msg_t), 0);
	CHECK_COND_RETURN(ret == LISA_OK, "queue push faild(ret = %d)", ret);
}

static void session_voice_proc_thread(void *arg)
{
	session_voice_t *obj = g_session_voice_obj;
	session_voice_msg_t msg;
	int ret;

	while (1) {
		ret = lisa_queue_pop(obj->msg_que, &msg, sizeof(session_voice_msg_t), LISA_OS_WAIT_FOREVER);
		if (ret) {
			continue;
		}

		switch (msg.evt) {
		case SESSION_STARTED:
			obj->need_audio = true;
			lisa_semaphore_give(obj->sem_started);
			break;
		case SESSION_TIMEOUT:
			obj->need_audio = false;
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_TIMEOUT, NULL, 0);
			break;
		case SESSION_FINISH:
			obj->need_audio = false;
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_FINISH, NULL, 0);
			break;
		case SESSION_GOT_VAD:
			if (!voice_session_if_full_duplex()) {
				obj->need_audio = false;
			}
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_GOT_VAD, NULL, 0);
			break;
		case SESSION_TTS_URL:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_TTS, msg.data, msg.size);
			break;
		case SESSION_IAT_TXT:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_IAT, msg.data, msg.size);
			break;
		case SESSION_IAT_START:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_IAT_START, NULL, 0);
			break;
		case SESSION_IAT_END:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_IAT_END, NULL, 0);
			break;
		case SESSION_REPLY_URL:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_REPLY_URL, msg.data, msg.size);
			break;
		case SESSION_DRAW_URL:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_DRAW, msg.data, msg.size);
			break;
		case SESSION_AIUI_CTR:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_AIUI_CTRL, msg.data, msg.size);
			break;
		case SESSION_MUSIC_LISTS: {
			session_music_lists_t *item = (session_music_lists_t *)msg.data;
			uint32_t size =
				sizeof(session_voice_music_lists_t) + item->cnt * sizeof(session_voice_music_item_t);

			session_voice_music_lists_t *lists = lisa_mem_calloc(1, size);
			if (lists) {
				lists->cnt = item->cnt;
				for (uint8_t i = 0; i < lists->cnt; i++) {
					strncpy(lists->items[i].id, item->items[i].id, sizeof(lists->items[i].id));
				}
				lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_MUSIC_LISTS, lists, size);

				lisa_mem_free(lists);
			}
			break;
		}
		case SESSION_RESULT_RAW_DATA:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_RAW_DATA, msg.data, msg.size);
			break;
		case SESSION_MUSIC_INSTR: {
			session_music_instr_t *item = (session_music_instr_t *)msg.data;
			session_voice_music_instr_t data = {0};
			switch (item->instr) {
			case SESSION_MUSIC_INSTR_VOL_SET:
				data.instr = SESSION_VOICE_MUSIC_INSTR_VOL_SET;
				data.arg = item->arg;
				break;
			case SESSION_MUSIC_INSTR_CLOSE:
				data.instr = SESSION_VOICE_MUSIC_INSTR_CLOSE;
				break;
			case SESSION_MUSIC_INSTR_REPLAY:
				data.instr = SESSION_VOICE_MUSIC_INSTR_REPLAY;
				break;
			case SESSION_MUSIC_INSTR_PAST:
				data.instr = SESSION_VOICE_MUSIC_INSTR_PAST;
				break;
			case SESSION_MUSIC_INSTR_NEXT:
				data.instr = SESSION_VOICE_MUSIC_INSTR_NEXT;
				break;
			}
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_MUSIC_INSTR, &data,
						   sizeof(session_voice_music_instr_t));
			break;
		}
		case SESSION_ERR_FRAME:
			obj->need_audio = false;
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_ERR, NULL, 0);
			break;
		case SESSION_ALARM_INTENT:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_ALARM_INTENT, msg.data, msg.size);
			break;
		case SESSION_VPR_INFO:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_VPR_INFO, msg.data, msg.size);
			break;
		case SESSION_VPR_FEATURE:
			lisa_evt_publisher_publish(obj->pub, SESSION_VOICE_VPR_FEATURE, msg.data, msg.size);
			break;			
		default:
			break;
		}

		if (msg.data) {
			lisa_mem_free(msg.data);
		}
	}
}

int session_voice_set_config(session_voice_config_t *cfg)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	SESSION_VOICE_LOCK(obj->mutex);

	memcpy((void *)&(obj->config), (void *)cfg, sizeof(session_voice_config_t));

	session_params_t config = {0};
	session_get_config(obj->ss, &config);

	if (cfg->inter_mode == SESSION_VOICE_INTER_MODE_ONESHOT) {
		config.full_duplex = false;
		if (cfg->session_timeout_ms) {
			config.session_timeout = cfg->session_timeout_ms;
		}
	} else {
		config.full_duplex = true;
		if (cfg->session_timeout_ms) {
			config.full_duplex_timeout_s = cfg->session_timeout_ms / 1000;
		}
	}

	config.asr_params.vad_enable = cfg->vad_enable;

	if (strlen(cfg->device_id) > 0) {
		strncpy(config.nlu_params.sn, obj->config.device_id, sizeof(config.nlu_params.sn));
		LISA_NLOGI("[%s] cfg->device_id:%s", __FUNCTION__, cfg->device_id);
	}

	if (strlen(cfg->aue) > 0) {
		strncpy(config.aue, cfg->aue, sizeof(config.aue));
		config.speex_size = cfg->speex_size;
	} else {
		strncpy(config.aue, "raw", sizeof(config.aue));
		config.speex_size = 0;
	}

	/* session_set_config中会将words复制到自己的全局config中, 所以这里不需要复制 */
	config.asr_params.oneshot = cfg->oneshot;
	config.asr_params.words_cnt = cfg->words_cnt;
	config.asr_params.words = cfg->words;

	session_set_config(obj->ss, &config);

	SESSION_VOICE_UNLOCK(obj->mutex);

	return LSC_OK;
}

int session_voice_get_config(session_voice_config_t *cfg)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	SESSION_VOICE_LOCK(obj->mutex);

	memcpy((void *)cfg, (void *)&(obj->config), sizeof(session_voice_config_t));

	SESSION_VOICE_UNLOCK(obj->mutex);
	return LSC_OK;
}

int session_voice_add_evt_callback(session_voice_event_cb_t cb, session_voice_event_e evt, void *usr)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	SESSION_VOICE_LOCK(obj->mutex);

	int ret = lisa_evt_publisher_evt_add(obj->pub, evt, (lisa_evt_publisher_cb_t)cb, usr);
	if (ret) {
		SESSION_VOICE_UNLOCK(obj->mutex);
		return LSC_ERR;
	}

	SESSION_VOICE_UNLOCK(obj->mutex);
	return LSC_OK;
}

int session_voice_remove_evt_callback(session_voice_event_cb_t cb)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	SESSION_VOICE_LOCK(obj->mutex);

	lisa_evt_publisher_cb_remove(obj->pub, (lisa_evt_publisher_cb_t)cb);

	SESSION_VOICE_UNLOCK(obj->mutex);

	return LSC_OK;
}

int session_voice_start(void)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	SESSION_VOICE_LOCK(obj->mutex);

	lisa_semaphore_reset(obj->sem_started);

	int ret = session_start(obj->ss, NULL);
	if (ret) {
		SESSION_VOICE_UNLOCK(obj->mutex);
		return LSC_ERR;
	}

	lisa_err_t err = lisa_semaphore_take(obj->sem_started, SEESION_VOICE_SYNC_TIMEOUT_MS);
	if (err != LISA_OK) {
		LISA_NLOGE("[%s] wait sem timeout", __FUNCTION__);
		SESSION_VOICE_UNLOCK(obj->mutex);
		return LSC_ERR;
	}

	SESSION_VOICE_UNLOCK(obj->mutex);

	return LSC_OK;
}

int session_voice_cancel(void)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	SESSION_VOICE_LOCK(obj->mutex);

	int ret = session_cancel(obj->ss);
	if (ret) {
		SESSION_VOICE_UNLOCK(obj->mutex);
		return LSC_ERR;
	}

	SESSION_VOICE_UNLOCK(obj->mutex);

	return LSC_OK;
}

#define AUDIO_SEND_DATA_MAX_SIZE (2560)

int session_voice_send_audio(uint8_t *data, uint32_t size)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	/*
		1 接收到started帧后，允许上传音频
		2 单工模式下，接收到vaded帧后，上传音频无效
		3 全双工模式下，接收到finished帧后，上传音频无效
	*/
	if (obj->need_audio == false) {
		return LSC_INVALID_STATE;
	}

	uint32_t send_size = 0;
	uint32_t reminds_size = size;
	uint8_t *ptr = data;
	int ret = 0;

	do {
		send_size = (reminds_size > AUDIO_SEND_DATA_MAX_SIZE) ? AUDIO_SEND_DATA_MAX_SIZE : reminds_size;
		ret = session_send_bin(obj->ss, ptr, send_size);
		if (ret != LSC_OK) {
			return LSC_ERR;
		}
		reminds_size -= send_size;
		ptr += send_size;
	} while (reminds_size > 0);

	return LSC_OK;
}

int session_voice_end_audio(void)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	SESSION_VOICE_LOCK(obj->mutex);

	int ret = session_end(obj->ss);
	if (ret) {
		SESSION_VOICE_UNLOCK(obj->mutex);
		return LSC_ERR;
	}

	SESSION_VOICE_UNLOCK(obj->mutex);

	return LSC_OK;
}

int session_voice_init(void)
{
	CHECK_COND_RETURN_VAL(g_session_voice_obj == NULL, LSC_INVALID_STATE, "already create");

	session_voice_t *session = lisa_mem_calloc(1, sizeof(session_voice_t));
	CHECK_COND_GOTO(session, _err, "no mem");

	session->ss = session_create();
	CHECK_COND_GOTO(session->ss, _err, "session create faild");

	int ret = session_add_evt_callback(session->ss, voice_session_event_cb,
					   SESSION_STARTED | SESSION_GOT_VAD | SESSION_TTS_URL | SESSION_IAT_TXT |
						   SESSION_IAT_START | SESSION_IAT_END | SESSION_AIUI_CTR |
						   SESSION_REPLY_URL | SESSION_DRAW_URL | SESSION_MUSIC_LISTS |
						   SESSION_MUSIC_INSTR | SESSION_FINISH | SESSION_ERR_FRAME |
						   SESSION_RESULT_RAW_DATA | SESSION_TIMEOUT | SESSION_ALARM_INTENT |
						   SESSION_VPR_INFO | SESSION_VPR_FEATURE,
					   NULL);
	CHECK_COND_GOTO(ret == LSC_OK, _err, "add cb faild");

	session->pub = lisa_evt_publisher_new();
	CHECK_COND_GOTO(session->pub, _err, "lisa new publisher faild");

	session_params_t config = {
		.data_type = "audio",
		.speex_size = 0,
		.asr_params =
			{
				.enable = true,
			},
		.nlu_params =
			{
				.enable = true,
			},
		.nlu_properties =
			{
				.enable = true,
			},
		.tts_params =
			{
				.enable = true,
			},
	};

	session_set_config(session->ss, &config);

	session->msg_que = lisa_queue_create(SESSION_VOICE_MSG_COUNT, NULL, sizeof(session_voice_msg_t));
	CHECK_COND_GOTO(session->msg_que, _err, "msg_que create faild");

	session->sem_started = lisa_semaphore_create(1);
	CHECK_COND_GOTO(session->sem_started, _err, "sem_started create faild");

	session->mutex = lisa_mutex_create();
	CHECK_COND_GOTO(session->mutex, _err, "mutex create faild");

	g_session_voice_obj = session;

	lisa_thread_attr_t attr;
	attr.name = "session voice proc";
	attr.priority = SESSION_VOICE_PROC_THREAD_PRIORITY;
	attr.stack_size = SESSION_VOICE_PROC_THREAD_STACK_SIZE;
	session->proc_thread = lisa_thread_create(&attr, session_voice_proc_thread, NULL);
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

		if (session->sem_started) {
			lisa_semaphore_delete(session->sem_started);
		}

		if (session->mutex) {
			lisa_mutex_delete(session->mutex);
		}

		lisa_mem_free(session);
		g_session_voice_obj = NULL;
	}
	return LSC_ERR;
}

int session_voice_deinit(void)
{
	session_voice_t *session = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(session, LSC_INVALID_PARAM, "not create");

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

	if (session->sem_started) {
		lisa_semaphore_delete(session->sem_started);
	}

	if (session->mutex) {
		lisa_mutex_delete(session->mutex);
	}

	lisa_mem_free(session);
	g_session_voice_obj = NULL;

	return LSC_OK;
}

int session_voice_set_model_id(const char *model_id)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");

	SESSION_VOICE_LOCK(obj->mutex);

	/* 如果传入的model_id为NULL，则清空当前模型ID */
	if (model_id == NULL) {
		memset(obj->current_model_id, 0, sizeof(obj->current_model_id));
	} else {
		strncpy(obj->current_model_id, model_id, sizeof(obj->current_model_id) - 1);
		obj->current_model_id[sizeof(obj->current_model_id) - 1] = '\0';
	}

	SESSION_VOICE_UNLOCK(obj->mutex);

	LISA_NLOGI("Set current model ID to: %s", model_id ? model_id : "NULL");
	return LSC_OK;
}

int session_voice_get_model_id(char *model_id, size_t max_len)
{
	session_voice_t *obj = g_session_voice_obj;
	CHECK_COND_RETURN_VAL(obj, LSC_INVALID_STATE, "not init");
	CHECK_COND_RETURN_VAL(model_id, LSC_INVALID_PARAM, "invalid param");
	CHECK_COND_RETURN_VAL(max_len > 0, LSC_INVALID_PARAM, "invalid param");

	SESSION_VOICE_LOCK(obj->mutex);

	/* 如果当前模型ID为空，则返回空字符串 */
	if (obj->current_model_id[0] == '\0') {
		model_id[0] = '\0';
	} else {
		strncpy(model_id, obj->current_model_id, max_len - 1);
		model_id[max_len - 1] = '\0';
	}

	SESSION_VOICE_UNLOCK(obj->mutex);

	return LSC_OK;
}
