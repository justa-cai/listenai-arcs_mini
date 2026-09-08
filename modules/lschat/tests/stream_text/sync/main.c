#include "lisa_semaphore.h"
#define TAG "main"

#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_thread.h"

#include "lsc_stream_text.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

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

void setUp(void)
{
}

void tearDown(void)
{
}

static void sse_evt_cb_handle(int evt, const char *data, void *user)
{
	LISA_NLOGI("sse_evt_cb_handle, evt:%d, data:%s", evt, data);
}

static void test_lsc_stream_text_request_new(void)
{
	struct lsc_stream_text_request_ctx *ctx = lsc_stream_text_request_new("", sse_evt_cb_handle, NULL);
	TEST_ASSERT_NOT_NULL(ctx);
	lsc_stream_text_request_delete(ctx);
}

static void test_lsc_stream_text_request_new_invalid_param(void)
{
	struct lsc_stream_text_request_ctx *ctx;
	ctx = lsc_stream_text_request_new(NULL, sse_evt_cb_handle, NULL);
	TEST_ASSERT_NULL(ctx);

	ctx = lsc_stream_text_request_new("", NULL, NULL);
	TEST_ASSERT_NULL(ctx);
}

static void test_lsc_stream_text_request_new_delete_mem_leak(void)
{
	size_t total_size1;
	size_t free_size1;
	size_t total_size2;
	size_t free_size2;

	lisa_mem_get_info(&total_size1, &free_size1);

	struct lsc_stream_text_request_ctx *ctx = lsc_stream_text_request_new("", sse_evt_cb_handle, NULL);
	TEST_ASSERT_NOT_NULL(ctx);
	lsc_stream_text_request_delete(ctx);

	lisa_mem_get_info(&total_size2, &free_size2);
	TEST_ASSERT_EQUAL_INT(total_size1, total_size2);
	TEST_ASSERT_EQUAL_INT(free_size1, free_size2);
}

#define SSE_CAPTURE_MAX_EVENTS 4
#define SSE_CAPTURE_DATA_SIZE 256

struct sse_capture {
	int data_count;
	int done_count;
	char data[SSE_CAPTURE_MAX_EVENTS][SSE_CAPTURE_DATA_SIZE];
};

static void sse_capture_cb(int evt, const char *data, void *user)
{
	struct sse_capture *capture = user;

	if (evt == SSE_EVT_DATA && capture->data_count < SSE_CAPTURE_MAX_EVENTS) {
		strncpy(capture->data[capture->data_count], data,
			sizeof(capture->data[capture->data_count]) - 1U);
		capture->data_count++;
	} else if (evt == SSE_EVT_DONE) {
		capture->done_count++;
	}
}

static void test_lsc_stream_text_request_feed_fragmented_event(void)
{
	const char *payload = "{\"index\":0,\"text\":\"hello\"}";
	const char *frame =
		"event: data\r\n"
		"data: {\"index\":0,\"text\":\"hello\"}\r\n\r\n";
	size_t frame_len = strlen(frame);

	for (size_t split = 1; split < frame_len; split++) {
		struct sse_capture capture = {0};
		struct lsc_stream_text_request_ctx *ctx =
			lsc_stream_text_request_new("", sse_capture_cb, &capture);

		TEST_ASSERT_NOT_NULL(ctx);
		TEST_ASSERT_EQUAL_INT(0, lsc_stream_text_request_feed(ctx, frame, split));
		TEST_ASSERT_EQUAL_INT(0, capture.data_count);
		TEST_ASSERT_EQUAL_INT(0, lsc_stream_text_request_feed(
			ctx, frame + split, frame_len - split));
		TEST_ASSERT_EQUAL_INT(1, capture.data_count);
		TEST_ASSERT_EQUAL_STRING(payload, capture.data[0]);
		lsc_stream_text_request_delete(ctx);
	}
}

static void test_lsc_stream_text_request_feed_coalesced_events(void)
{
	const char *input =
		"event: data\n"
		"data: first\n\n"
		"event: data\r\n"
		"data: second\r\n\r\n"
		"event: done\n"
		"data: [DONE]\n\n";
	struct sse_capture capture = {0};
	struct lsc_stream_text_request_ctx *ctx =
		lsc_stream_text_request_new("", sse_capture_cb, &capture);

	TEST_ASSERT_NOT_NULL(ctx);
	TEST_ASSERT_EQUAL_INT(0, lsc_stream_text_request_feed(ctx, input, strlen(input)));
	TEST_ASSERT_EQUAL_INT(2, capture.data_count);
	TEST_ASSERT_EQUAL_STRING("first", capture.data[0]);
	TEST_ASSERT_EQUAL_STRING("second", capture.data[1]);
	TEST_ASSERT_EQUAL_INT(1, capture.done_count);
	lsc_stream_text_request_delete(ctx);
}

static void test_lsc_stream_text_request_feed_ignores_empty_data(void)
{
	const char *input =
		"event: data\n"
		"data: first\n\n"
		"event: data\n"
		"data: \n\n"
		"event: data\r\n"
		"data:\r\n\r\n"
		"event: data\n"
		"data: second\n\n"
		"event: done\n"
		"data: [DONE]\n\n";
	struct sse_capture capture = {0};
	struct lsc_stream_text_request_ctx *ctx =
		lsc_stream_text_request_new("", sse_capture_cb, &capture);

	TEST_ASSERT_NOT_NULL(ctx);
	TEST_ASSERT_EQUAL_INT(0, lsc_stream_text_request_feed(ctx, input, strlen(input)));
	TEST_ASSERT_EQUAL_INT(2, capture.data_count);
	TEST_ASSERT_EQUAL_STRING("first", capture.data[0]);
	TEST_ASSERT_EQUAL_STRING("second", capture.data[1]);
	TEST_ASSERT_EQUAL_INT(1, capture.done_count);
	lsc_stream_text_request_delete(ctx);
}

static void test_lsc_stream_text_request_feed_utf8_split(void)
{
	const char *frame =
		"event: data\n"
		"data: {\"text\":\"\xe6\x98\x8e\xe6\x98\x8e\"}\n\n";
	const char *payload = "{\"text\":\"\xe6\x98\x8e\xe6\x98\x8e\"}";
	const char *utf8 = strstr(frame, "\xe6\x98\x8e");
	size_t split = (size_t)(utf8 - frame) + 1U;
	struct sse_capture capture = {0};
	struct lsc_stream_text_request_ctx *ctx =
		lsc_stream_text_request_new("", sse_capture_cb, &capture);

	TEST_ASSERT_NOT_NULL(ctx);
	TEST_ASSERT_EQUAL_INT(0, lsc_stream_text_request_feed(ctx, frame, split));
	TEST_ASSERT_EQUAL_INT(0, capture.data_count);
	TEST_ASSERT_EQUAL_INT(0, lsc_stream_text_request_feed(
		ctx, frame + split, strlen(frame) - split));
	TEST_ASSERT_EQUAL_INT(1, capture.data_count);
	TEST_ASSERT_EQUAL_STRING(payload, capture.data[0]);
	lsc_stream_text_request_delete(ctx);
}

static void test_lsc_stream_text_request_start_invalid_param(void)
{
	int err;
	err = lsc_stream_text_request_start(NULL, 0);
	TEST_ASSERT_NOT_EQUAL_INT(0, err);
}

static void test_lsc_stream_text_request_start(void)
{
	int err;
	struct lsc_stream_text_request_ctx *ctx;
	const char *url = "https://staging-api.iflyos.cn/external/skill_app_action/text_streaming_mock?delay=100";
	ctx = lsc_stream_text_request_new(url, sse_evt_cb_handle, NULL);
	err = lsc_stream_text_request_start(ctx, 20);
	TEST_ASSERT_EQUAL_INT(0, err);
	lsc_stream_text_request_delete(ctx);
}

static void test_lsc_stream_text_request_start_mem_leak_test(void)
{
	size_t total_size1;
	size_t free_size1;
	size_t total_size2;
	size_t free_size2;

	lisa_mem_get_info(&total_size1, &free_size1);

	int err;
	struct lsc_stream_text_request_ctx *ctx;
	const char *url = "https://staging-api.iflyos.cn/external/skill_app_action/text_streaming_mock?delay=100";
	ctx = lsc_stream_text_request_new(url, sse_evt_cb_handle, NULL);
	err = lsc_stream_text_request_start(ctx, 20);
	TEST_ASSERT_EQUAL_INT(0, err);
	lsc_stream_text_request_delete(ctx);
	lisa_mem_get_info(&total_size2, &free_size2);
	TEST_ASSERT_EQUAL_INT(total_size1, total_size2);
	TEST_ASSERT_EQUAL_INT(free_size1, free_size2);
}

struct stream_text_handle_msg_user {
	lisa_semaphore_t *sem_data;
	lisa_semaphore_t *sem_abort;
	lisa_semaphore_t *sem_done;
};

static void sse_evt_cb_handle_msg(int evt, const char *data, void *user)
{
	struct stream_text_handle_msg_user *msg_user = user;

	LISA_NLOGI("sse_evt_cb_handle, evt:%d, data:%s", evt, data);
	if (evt == SSE_EVT_DONE) {
		lisa_semaphore_give(msg_user->sem_done);
	} else if (evt == SSE_EVT_DATA) {
		lisa_semaphore_give(msg_user->sem_data);
	} else if (evt == SSE_EVT_ABORT) {
		lisa_semaphore_give(msg_user->sem_abort);
	}
}

static void test_lsc_stream_text_request_start_handle_msg(void)
{
	int err;
	struct lsc_stream_text_request_ctx *ctx;
	struct stream_text_handle_msg_user usr;

	usr.sem_data = lisa_semaphore_create(1);
	usr.sem_abort = lisa_semaphore_create(1);
	usr.sem_done = lisa_semaphore_create(1);

	const char *url = "https://staging-api.iflyos.cn/external/skill_app_action/text_streaming_mock?delay=100";
	ctx = lsc_stream_text_request_new(url, sse_evt_cb_handle_msg, &usr);
	err = lsc_stream_text_request_start(ctx, 20);
	TEST_ASSERT_EQUAL_INT(0, err);

	err = lisa_semaphore_take(usr.sem_data, 20 * 1000);
	TEST_ASSERT_EQUAL_INT(0, err);
	err = lisa_semaphore_take(usr.sem_done, 20 * 1000);
	TEST_ASSERT_EQUAL_INT(0, err);

	lsc_stream_text_request_delete(ctx);

	lisa_semaphore_delete(usr.sem_data);
	lisa_semaphore_delete(usr.sem_done);
	lisa_semaphore_delete(usr.sem_abort);
}

static void test_abort_thread(void *p)
{
	struct lsc_stream_text_request_ctx *ctx = p;
	lisa_thread_mdelay(600);
	int err = lsc_stream_text_request_abort(ctx);
	TEST_ASSERT_EQUAL_INT(0, err);
}

static void test_lsc_stream_text_request_abort(void)
{
	int err;
	struct lsc_stream_text_request_ctx *ctx;
	struct stream_text_handle_msg_user usr;
	const char *url = "https://staging-api.iflyos.cn/external/skill_app_action/text_streaming_mock?delay=500";

	usr.sem_data = lisa_semaphore_create(1);
	usr.sem_abort = lisa_semaphore_create(1);
	usr.sem_done = lisa_semaphore_create(1);

	ctx = lsc_stream_text_request_new(url, sse_evt_cb_handle_msg, &usr);
	lisa_thread_attr_t attr = {
		.name = "test_abort",
		.priority = LISA_OS_PRIORITY_NORMAL,
		.stack_size = 1024,
	};
	lisa_thread_t *th = lisa_thread_create(&attr, test_abort_thread, ctx);

	err = lsc_stream_text_request_start(ctx, 20);
	TEST_ASSERT_NOT_EQUAL_INT(0, err);
	err = lisa_semaphore_take(usr.sem_abort, 20 * 1000);
	TEST_ASSERT_EQUAL_INT(0, err);
	err = lisa_semaphore_take(usr.sem_done, 20 * 1000);
	TEST_ASSERT_NOT_EQUAL_INT(0, err);

	lsc_stream_text_request_delete(ctx);

	lisa_semaphore_delete(usr.sem_data);
	lisa_semaphore_delete(usr.sem_done);
	lisa_semaphore_delete(usr.sem_abort);

	lisa_thread_delete(th);
}

static void test_lsc_stream_text_request_abort_mem_leak(void)
{
	int err;
	struct lsc_stream_text_request_ctx *ctx;
	struct stream_text_handle_msg_user usr;
	const char *url = "https://staging-api.iflyos.cn/external/skill_app_action/text_streaming_mock?delay=500";
	size_t total_size1;
	size_t free_size1;
	size_t total_size2;
	size_t free_size2;

	lisa_mem_get_info(&total_size1, &free_size1);

	usr.sem_data = lisa_semaphore_create(1);
	usr.sem_abort = lisa_semaphore_create(1);
	usr.sem_done = lisa_semaphore_create(1);

	ctx = lsc_stream_text_request_new(url, sse_evt_cb_handle_msg, &usr);
	lisa_thread_attr_t attr = {
		.name = "test_abort",
		.priority = LISA_OS_PRIORITY_NORMAL,
		.stack_size = 1024,
	};
	lisa_thread_t *th = lisa_thread_create(&attr, test_abort_thread, ctx);

	err = lsc_stream_text_request_start(ctx, 20);
	TEST_ASSERT_NOT_EQUAL_INT(0, err);
	err = lisa_semaphore_take(usr.sem_abort, 20 * 1000);
	TEST_ASSERT_EQUAL_INT(0, err);
	err = lisa_semaphore_take(usr.sem_done, 20 * 1000);
	TEST_ASSERT_NOT_EQUAL_INT(0, err);

	lsc_stream_text_request_delete(ctx);

	lisa_semaphore_delete(usr.sem_data);
	lisa_semaphore_delete(usr.sem_done);
	lisa_semaphore_delete(usr.sem_abort);

	lisa_thread_delete(th);
	lisa_mem_get_info(&total_size2, &free_size2);
	TEST_ASSERT_EQUAL_INT(total_size1, total_size2);
	TEST_ASSERT_EQUAL_INT(free_size1, free_size2);
}

static void test_lsc_stream_text_request_abort_invalid_param(void)
{
	int err = lsc_stream_text_request_abort(NULL);
	TEST_ASSERT_NOT_EQUAL_INT(0, err);
}

int main(void)
{
	net_init();
	net_up();

	UNITY_BEGIN();

	RUN_TEST(test_lsc_stream_text_request_new);
	RUN_TEST(test_lsc_stream_text_request_new_invalid_param);
	RUN_TEST(test_lsc_stream_text_request_new_delete_mem_leak);
	RUN_TEST(test_lsc_stream_text_request_feed_fragmented_event);
	RUN_TEST(test_lsc_stream_text_request_feed_coalesced_events);
	RUN_TEST(test_lsc_stream_text_request_feed_ignores_empty_data);
	RUN_TEST(test_lsc_stream_text_request_feed_utf8_split);
	RUN_TEST(test_lsc_stream_text_request_start_invalid_param);
	RUN_TEST(test_lsc_stream_text_request_start);
	RUN_TEST(test_lsc_stream_text_request_start_mem_leak_test);
	RUN_TEST(test_lsc_stream_text_request_start_handle_msg);
	RUN_TEST(test_lsc_stream_text_request_abort);
	RUN_TEST(test_lsc_stream_text_request_abort_invalid_param);
	RUN_TEST(test_lsc_stream_text_request_abort_mem_leak);

	UNITY_END();
}
