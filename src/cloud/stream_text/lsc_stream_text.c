
#define TAG "stream_text"

#include "lsc_stream_text.h"

#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_http.h"
#include "lisa_thread.h"
#include "lisa_queue.h"
#include "lisa_semaphore.h"
#include "lisa_mutex.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>

static inline uint8_t iscrlf(char c)
{
	return (c == '\r') || (c == '\n');
}

static inline int strip_crlf_end(char *str, uint32_t len)
{
	while (len > 0 && iscrlf(str[len - 1])) {
		len--;
	}
	str[len] = '\0';

	return len;
}

static int sse_evt_parsev2(const char *text, sse_evt_cb_t cb, void *user)
{
	if (cb == NULL) {
		return 0;
	}
	int length = strlen(text);
	char *temp_buf = lisa_mem_alloc(length + 1);
	if (temp_buf == NULL) {
		return 0;
	}

	memcpy(temp_buf, text, length);
	temp_buf[length] = 0;

	char *p;
	char *next;
	char *end;
	bool match_end = false;

	p = (char *)temp_buf;
	p[length] = '\0';
	end = p + length;

	p = strstr(p, "event: data");
	if (p == NULL) {
		p = strstr(temp_buf, "event: done");
		if (p) {
			cb(SSE_EVT_DONE, NULL, user);
		}
		lisa_mem_free(temp_buf);
		return 0;
	}

	while (p && !match_end) {
		/* skip length of 'event: data' */
		p += 11;
		p = strstr(p, "data: ");
		if (p == NULL) {
			/* error */
			break;
		}

		/* skip length of 'data: ' */
		p += 6;

		next = strstr(p, "event: data");
		if (next == NULL) {
			next = strstr(p, "event: done");
			if (next) {
				match_end = true;
			}
		}

		int l = 0;
		if (next == NULL) {
			l = (int)end - (int)p;
		} else {
			l = (int)next - (int)p;
		}

		l = strip_crlf_end(p, l);
		cb(SSE_EVT_DATA, p, user);
		p = next;
		(void)l;
	}

	if (match_end) {
		cb(SSE_EVT_DONE, NULL, user);
	}

	lisa_mem_free(temp_buf);

	return 0;
}

static void lisa_http_on_data(lisa_http_data_t *data)
{
}

static int http_on_chunk_handle(lisa_http_data_t *http_data)
{
	LOGD("response content: %s", (char *)http_data->buf);
	struct lsc_stream_text_request_ctx *ctx = http_data->user;

	if (ctx == NULL || ctx->cb == NULL) {
		LOGI("invalid stream request context");
		return 0;
	}

	sse_evt_parsev2(http_data->buf, ctx->cb, ctx->user);

	if (ctx->exit_sem && (lisa_semaphore_take(ctx->exit_sem, 0) == 0)) {
		LOGI("lsc stream request take exit sem, abort current request");
		ctx->cb(SSE_EVT_ABORT, NULL, ctx->user);
		return -1;
	}

	return 0;
}

static int lsc_stream_text_request_abort_by_sem(lisa_semaphore_t *sem)
{
	if (sem == NULL) {
		return -1;
	}

	return lisa_semaphore_give(sem);
}

int lsc_stream_text_request_abort(struct lsc_stream_text_request_ctx *ctx)
{
	if (ctx == NULL || ctx->exit_sem == NULL) {
		return -1;
	}

	return lsc_stream_text_request_abort_by_sem(ctx->exit_sem);
}

struct lsc_stream_text_request_ctx *lsc_stream_text_request_new(const char *url, sse_evt_cb_t cb, void *user)
{
	if (url == NULL || cb == NULL) {
		return NULL;
	}

	struct lsc_stream_text_request_ctx *ctx = lisa_mem_alloc(sizeof(struct lsc_stream_text_request_ctx));
	if (ctx == NULL) {
		return NULL;
	}

	ctx->url = lisa_mem_alloc(strlen(url) + 1);
	if (ctx->url == NULL) {
		lisa_mem_free(ctx);
		return NULL;
	}

	strcpy(ctx->url, url);

	ctx->cb = cb;
	ctx->user = user;

	ctx->exit_sem = lisa_semaphore_create(1);
	if (ctx->exit_sem == NULL) {
		lisa_mem_free(ctx->url);
		lisa_mem_free(ctx);
		return NULL;
	}

	return ctx;
}

int lsc_stream_text_request_start(struct lsc_stream_text_request_ctx *ctx, uint32_t timeout)
{
	lisa_http_request_t req;

	if (ctx == NULL) {
		return -1;
	}

	memset(&req, 0, sizeof(lisa_http_request_t));
	req.method = LISA_HTTP_GET;
	req.url = ctx->url;
	req.timeout = timeout;
	req.on_data = lisa_http_on_data;
	req.body = NULL;
	req.body_len = 0;
	req.headers = NULL;
	req.user = ctx;

	lisa_http_t *http = lisa_http_init(&req);
	if (http == NULL) {
		return -1;
	}

	int err = lisa_http_perform_chunked_with_cb(http, http_on_chunk_handle);
	lisa_http_cleanup(http);

	return err;
}

void lsc_stream_text_request_delete(struct lsc_stream_text_request_ctx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	if (ctx->exit_sem) {
		lisa_semaphore_delete(ctx->exit_sem);
	}

	if (ctx->url) {
		lisa_mem_free(ctx->url);
	}

	lisa_mem_free(ctx);
}
