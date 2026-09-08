
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

#define SSE_BUFFER_INITIAL_SIZE 512U
#define SSE_BUFFER_MAX_SIZE (16U * 1024U)

static int sse_buffer_reserve(struct lsc_stream_text_request_ctx *ctx, size_t append_len)
{
	size_t required;
	size_t capacity;
	char *buffer;

	if (append_len > SSE_BUFFER_MAX_SIZE - ctx->sse_buffer_len) {
		LISA_NLOGE("SSE event exceeds %u bytes", (unsigned)SSE_BUFFER_MAX_SIZE);
		return -1;
	}

	required = ctx->sse_buffer_len + append_len + 1U;
	if (required <= ctx->sse_buffer_capacity) {
		return 0;
	}

	capacity = ctx->sse_buffer_capacity ? ctx->sse_buffer_capacity : SSE_BUFFER_INITIAL_SIZE;
	while (capacity < required) {
		capacity *= 2U;
		if (capacity > SSE_BUFFER_MAX_SIZE + 1U) {
			capacity = SSE_BUFFER_MAX_SIZE + 1U;
			break;
		}
	}

	if (ctx->sse_buffer) {
		buffer = lisa_mem_realloc(ctx->sse_buffer, capacity);
	} else {
		buffer = lisa_mem_alloc(capacity);
	}
	if (!buffer) {
		LISA_NLOGE("allocate SSE buffer failed, capacity=%u", (unsigned)capacity);
		return -1;
	}

	ctx->sse_buffer = buffer;
	ctx->sse_buffer_capacity = capacity;
	return 0;
}

static bool sse_find_event_end(const char *buffer, size_t len,
			       size_t *event_len, size_t *delimiter_len)
{
	for (size_t i = 0; i + 1U < len; i++) {
		if (i + 3U < len && buffer[i] == '\r' && buffer[i + 1U] == '\n' &&
		    buffer[i + 2U] == '\r' && buffer[i + 3U] == '\n') {
			*event_len = i;
			*delimiter_len = 4U;
			return true;
		}
		if (buffer[i] == '\n' && buffer[i + 1U] == '\n') {
			*event_len = i;
			*delimiter_len = 2U;
			return true;
		}
	}

	return false;
}

static void sse_parse_event(char *event, size_t len, size_t delimiter_len,
			    struct lsc_stream_text_request_ctx *ctx)
{
	char *line = event;
	char *end = event + len;
	char *data_out = event;
	size_t data_len = 0;
	bool is_data = false;
	bool is_done = false;
	bool has_event = false;
	bool has_data = false;
	size_t event_line_count = 0;
	size_t data_line_count = 0;

	event[len] = '\0';
	while (line < end) {
		char *line_end = memchr(line, '\n', (size_t)(end - line));
		char *next = line_end ? line_end + 1 : end;
		char *value;
		size_t line_len = line_end ? (size_t)(line_end - line) : (size_t)(end - line);

		if (line_len > 0U && line[line_len - 1U] == '\r') {
			line_len--;
		}
		line[line_len] = '\0';

		if (strncmp(line, "event:", 6U) == 0) {
			event_line_count++;
			has_event = true;
			value = line + 6U;
			while (*value == ' ' || *value == '\t') {
				value++;
			}
			is_data = strcmp(value, "data") == 0 || strcmp(value, "message") == 0;
			is_done = strcmp(value, "done") == 0;
		} else if (strncmp(line, "data:", 5U) == 0) {
			data_line_count++;
			value = line + 5U;
			if (*value == ' ') {
				value++;
			}
			if (has_data) {
				data_out[data_len++] = '\n';
			}
			size_t value_len = strlen(value);
			memmove(data_out + data_len, value, value_len);
			data_len += value_len;
			has_data = true;
		}

		line = next;
	}

	data_out[data_len] = '\0';
	/* SSE permits the default "message" event to omit the event field. */
	if (!has_event && has_data) {
		is_data = true;
	}
	/* Some gateways use the OpenAI-style sentinel instead of event: done. */
	if (has_data && strcmp(data_out, "[DONE]") == 0) {
		is_done = true;
		is_data = false;
	}
	LISA_NLOGD("SSE event: len=%u, delimiter=%u, event_lines=%u, data_lines=%u, "
		   "data_len=%u, type=%s",
		   (unsigned)len, (unsigned)delimiter_len, (unsigned)event_line_count,
		   (unsigned)data_line_count, (unsigned)data_len,
		   is_done ? "done" : (is_data ? "data" : "other"));
	if (is_done) {
		ctx->sse_done = true;
		ctx->cb(SSE_EVT_DONE, NULL, ctx->user);
	} else if (is_data && has_data && data_len > 0U) {
		ctx->cb(SSE_EVT_DATA, data_out, ctx->user);
	} else if (is_data && has_data) {
		LISA_NLOGD("ignore empty SSE data event");
	}
}

int lsc_stream_text_request_feed(struct lsc_stream_text_request_ctx *ctx,
				 const void *data, size_t len)
{
	if (!ctx || !ctx->cb || (!data && len > 0U)) {
		return -1;
	}
	if (ctx->sse_done || len == 0U) {
		return 0;
	}
	if (sse_buffer_reserve(ctx, len) != 0) {
		return -1;
	}

	memcpy(ctx->sse_buffer + ctx->sse_buffer_len, data, len);
	ctx->sse_buffer_len += len;
	ctx->sse_buffer[ctx->sse_buffer_len] = '\0';

	while (!ctx->sse_done) {
		size_t event_len;
		size_t delimiter_len;
		size_t consumed;

		if (!sse_find_event_end(ctx->sse_buffer, ctx->sse_buffer_len,
					&event_len, &delimiter_len)) {
			break;
		}

		sse_parse_event(ctx->sse_buffer, event_len, delimiter_len, ctx);
		consumed = event_len + delimiter_len;
		ctx->sse_buffer_len -= consumed;
		memmove(ctx->sse_buffer, ctx->sse_buffer + consumed, ctx->sse_buffer_len);
		ctx->sse_buffer[ctx->sse_buffer_len] = '\0';
	}

	return 0;
}

static void lisa_http_on_data(lisa_http_data_t *data)
{
}

static int http_on_chunk_handle(lisa_http_data_t *http_data)
{
	struct lsc_stream_text_request_ctx *ctx = http_data->user;

	if (ctx == NULL || ctx->cb == NULL) {
		LISA_NLOGI("invalid stream request context");
		return -1;
	}
	if (http_data->len < 0 || (http_data->len > 0 && http_data->buf == NULL)) {
		LISA_NLOGE("invalid response chunk: len=%d", (int)http_data->len);
		return -1;
	}
	if (http_data->len == 0) {
		return 0;
	}

	LISA_NLOGD("response chunk: len=%d", (int)http_data->len);
	if (lsc_stream_text_request_feed(ctx, http_data->buf, (size_t)http_data->len) != 0) {
		return -1;
	}

	if (ctx->exit_sem && (lisa_semaphore_take(ctx->exit_sem, 0) == 0)) {
		LISA_NLOGI("lsc stream request take exit sem, abort current request");
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

	struct lsc_stream_text_request_ctx *ctx = lisa_mem_calloc(1, sizeof(struct lsc_stream_text_request_ctx));
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
	if (err == 0 && !ctx->sse_done) {
		/* Do not leave the subtitle state pending when the peer closes without
		 * sending the optional terminal SSE event. */
		LISA_NLOGW("stream response ended without SSE done, buffered=%u",
			   (unsigned)ctx->sse_buffer_len);
		ctx->sse_done = true;
		ctx->cb(SSE_EVT_DONE, NULL, ctx->user);
	}
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
	if (ctx->sse_buffer) {
		lisa_mem_free(ctx->sse_buffer);
	}

	lisa_mem_free(ctx);
}
