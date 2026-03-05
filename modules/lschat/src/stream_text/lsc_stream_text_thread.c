#include "lsc_stream_text.h"

#include "lisa_thread.h"
#include "lisa_queue.h"
#include "lisa_semaphore.h"
#include "lisa_mutex.h"
#include "lisa_log.h"

#include <stdint.h>
#include <string.h>

static lisa_thread_t *thread = NULL;
static lisa_queue_t *queue = NULL;
static lisa_semaphore_t *exit_sem = NULL;
static lisa_mutex_t *request_busy_lock = NULL;
static lisa_mutex_t *lock = NULL;
static struct lsc_stream_text_request_ctx *curr_ctx = NULL;
static uint8_t init = 0;

struct request_thread_arg {
	lisa_queue_t *queue;
	lisa_mutex_t *busy_lock;
};

static struct request_thread_arg thread_arg;

static inline struct lsc_stream_text_request_ctx *get_curr_ctx(void)
{
	return curr_ctx;
}

static inline void set_curr_ctx(struct lsc_stream_text_request_ctx *ctx)
{
	curr_ctx = ctx;
}

static void lsc_stream_text_request_thread(void *arg)
{
	struct request_thread_arg *td_arg = arg;
	struct lsc_stream_text_request_ctx *ctx;

	if (td_arg->queue == NULL || td_arg->busy_lock == NULL) {
		LISA_NLOGI("request thread arg invalid");
		return;
	}

	while (1) {
		int err;
		err = lisa_queue_pop(td_arg->queue, &ctx, sizeof(struct lsc_stream_text_request_ctx *),
				     LISA_WAIT_FOREVER);
		if (err) {
			LISA_NLOGE("lisa queue pop error: %d", err);
			continue;
		}

		set_curr_ctx(ctx);

		lisa_mutex_lock(td_arg->busy_lock, LISA_WAIT_FOREVER);

		LISA_NLOGI("request start, url:%s", ctx->url);
		err = lsc_stream_text_request_start(ctx, 120);
		lsc_stream_text_request_delete(ctx);
		set_curr_ctx(NULL);
		if (err) {
			LISA_NLOGE("request error: %d", err);
		} else {
			LISA_NLOGI("request done");
		}

		/* request thread idle now */
		lisa_mutex_unlock(td_arg->busy_lock);
	}
}

int lsc_stream_text_request_thread_init(void)
{
	if (init) {
		return 0;
	}

	/* prepare request thread */
	queue = lisa_queue_create(4, "stream text", sizeof(struct lsc_stream_text_request_ctx *));
	if (queue == NULL) {
		LISA_NLOGE("lsc stream queue creation failed");
		goto init_err_exit;
	}

	request_busy_lock = lisa_mutex_create();
	if (request_busy_lock == NULL) {
		LISA_NLOGE("lsc stream busy lock creation failed");
		goto init_err_exit;
	}

	exit_sem = lisa_semaphore_create(1);
	if (exit_sem == NULL) {
		LISA_NLOGE("lsc stream exit_sem creation failed");
		goto init_err_exit;
	}

	lock = lisa_mutex_create();
	if (lock == NULL) {
		LISA_NLOGE("lsc stream lock creation failed");
		goto init_err_exit;
	}

	lisa_thread_attr_t attr;
	attr.name = "stream-text";
	attr.stack_size = 20480;
	attr.priority = LISA_OS_PRIORITY_NORMAL;

	thread_arg.busy_lock = request_busy_lock;
	thread_arg.queue = queue;

	thread = lisa_thread_create(&attr, lsc_stream_text_request_thread, &thread_arg);
	if (thread == NULL) {
		LISA_NLOGE("lsc stream thread creation failed");
		goto init_err_exit;
	}

	init = 1;

	return 0;

init_err_exit:
	if (thread) {
		lisa_thread_delete(thread);
	}

	if (queue) {
		lisa_queue_delete(queue);
	}

	if (request_busy_lock) {
		lisa_mutex_delete(request_busy_lock);
	}

	if (lock) {
		lisa_mutex_delete(lock);
	}

	if (exit_sem == NULL) {
		lisa_semaphore_delete(exit_sem);
	}

	thread = NULL;
	queue = NULL;
	request_busy_lock = NULL;
	exit_sem = NULL;
	lock = NULL;

	init = 0;

	return -1;
}

void lsc_stream_text_request_thread_deinit()
{
}

void lsc_stream_text_request_thread_abort_all()
{
	/* lock */
	lisa_mutex_lock(lock, LISA_WAIT_FOREVER);

	/* clear queue and send abort msg */
	struct lsc_stream_text_request_ctx abort_ctx;
	while (lisa_queue_pop(queue, &abort_ctx, sizeof(struct lsc_stream_text_request_ctx), 0) == 0) {
		LISA_NLOGI("stream text drop request url:%s", abort_ctx.url);
		if (abort_ctx.cb) {
			abort_ctx.cb(SSE_EVT_ABORT, NULL, abort_ctx.cb);
		}
	}

	if (lisa_mutex_lock(request_busy_lock, 0) != 0) {
		struct lsc_stream_text_request_ctx *curr = get_curr_ctx();
		if (curr) {
			/* if a request is running, abort it */
			if (lsc_stream_text_request_abort(curr)) {
				LISA_NLOGE("send abort sem failed");
			}
		}

		/* wait request thread idle */
		lisa_mutex_lock(request_busy_lock, LISA_WAIT_FOREVER);
		lisa_mutex_unlock(request_busy_lock);
	} else {
		lisa_mutex_unlock(request_busy_lock);
	}

	/* unlock */
	lisa_mutex_unlock(lock);
}

int lsc_stream_text_request_thread_async(const char *url, sse_evt_cb_t cb, void *user, bool abort_all)
{
	int err;

	if (!init) {
		return -1;
	}

	if (url == NULL || strlen(url) == 0 || cb == NULL) {
		return -1;
	}

	/* prepare request context */
	struct lsc_stream_text_request_ctx *ctx;

	ctx = lsc_stream_text_request_new(url, cb, user);
	if (ctx == NULL) {
		return -1;
	}

	if (abort_all) {
		lsc_stream_text_request_thread_abort_all();
	}

	/* send the request to thread now */
	LISA_NLOGI("request ready, url: %s", ctx->url);
	err = lisa_queue_push(queue, &ctx, sizeof(struct lsc_stream_text_request_ctx *), 10 * 1000);
	if (err) {
		LISA_NLOGE("lsc stream queue push failed, err:%d", err);
	}

	return err;
}
