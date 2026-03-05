#define TAG "main"

#include "lisa_log.h"
#include "lsc_stream_text.h"

#include <stdint.h>
#include <stddef.h>

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

static void sse_evt_cb_handle(int evt, const char *data, void *user)
{
	LISA_NLOGI("sse_evt_cb_handle, evt:%d, data:%s", evt, data);
}

void sample_lsc_stream_text(void)
{
	int err;
	const char *url = "https://staging-api.iflyos.cn/external/skill_app_action/text_streaming_mock?delay=100";

	struct lsc_stream_text_request_ctx *ctx = lsc_stream_text_request_new(url, sse_evt_cb_handle, NULL);
	if (ctx == NULL) {
		LISA_NLOGE("lsc_stream_text_request_ctx new failed");
		return;
	}

	err = lsc_stream_text_request_start(ctx, 20);
	if (err) {
		LISA_NLOGE("lsc_stream_text_request_ctx start failed, err:%d", err);
		lsc_stream_text_request_delete(ctx);
		return;
	}

	lsc_stream_text_request_delete(ctx);

	LISA_NLOGI("lsc_stream_text_request sample done");
}

int main(void)
{
	/* network connection */
	net_init();
	net_up();

	sample_lsc_stream_text();
}
