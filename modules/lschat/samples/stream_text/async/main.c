#define TAG "main"

#include "lisa_log.h"
#include "lisa_thread.h"

#include "lsc.h"
#include "lsc_stream_text_thread.h"

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

void sample_lsc_stream_text_thread(void)
{
	int err;
	const char *url = "https://staging-api.iflyos.cn/external/skill_app_action/text_streaming_mock?delay=100";

	err = lsc_stream_text_request_thread_async(url, sse_evt_cb_handle, NULL, false);
	if (err) {
		LISA_NLOGE("lsc_stream_text_request_thread_async failed, err:%d", err);
		return;
	}
	LISA_NLOGI("lsc_stream_text_request thread sample done");
}

int main(void)
{
	int err;

	err = lsc_stream_text_request_thread_init();
	if (err) {
		LISA_NLOGE("lsc_stream_text_request_thread_init failed, err:%d", err);
		return -1;
	}

	sample_lsc_stream_text_thread();

	while (1) {
		lisa_thread_mdelay(100);
	}
}
