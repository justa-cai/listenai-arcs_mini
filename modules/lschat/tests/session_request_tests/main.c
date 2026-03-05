#define TAG "session_request_tests"

#include "lisa_log.h"
#include "lisa_semaphore.h"
#include "lisa_thread.h"
#include "lsc.h"
#include "lsc_session_request.h"
#include "lsc_errno.h"
#include "lisa_mem.h"
#include "unity.h"

#include <string.h>

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"
#define DEVICE_ID_STRING  "F97CE114C70DE8E2"

lisa_semaphore_t *sem_connected;
lisa_semaphore_t *sem_disconnected;
lisa_semaphore_t *sem_auth_faild;
lisa_semaphore_t *sem_got_token;

char request_url[256];

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

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
	switch (evt) {
	case LSC_CONNECTED:
		lisa_semaphore_give(sem_connected);
		break;
	case LSC_DISCONNECTED:
		lisa_semaphore_give(sem_disconnected);
		break;
	case LSC_CLOUD_AUTH_FAILD:
		lisa_semaphore_give(sem_auth_faild);
		break;
	case LSC_GOT_TOKEN:
		lisa_semaphore_give(sem_got_token);
		break;
	default:
		break;
	}
}

static void test_session_request_init_with_no_lsc_init(void)
{
	int ret;
	ret = session_request_init();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);
}

static void test_session_request_api_before_init(void)
{
	int ret;
	ret = session_request_deinit();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	ret = session_request_cancel();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	char text[] = "深圳天气";

	ret = session_request_xtts(text, request_url, 30000);
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);
}

static void test_lsc_init(void)
{
	int ret;

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
	};
	ret = lsc_init(&cfg);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb,
			       NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lsc_connect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_connected, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_token, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);
}

static void test_session_request_memory_leak(void)
{
	size_t total_size;
	size_t free_size_before;
	size_t free_size_after;
	int ret;

	// 保障其他流程走完，内存释放
	lisa_thread_mdelay(1000);

	lisa_mem_get_info(&total_size, &free_size_before);
	LISA_NLOGI("before: total-%d free-%d", total_size, free_size_before);

	ret = session_request_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	char text[] = "深圳天气";
	memset((void *)request_url, 0, sizeof(request_url));
	ret = session_request_xtts(text, request_url, 30000);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	LISA_NLOGI("get url:%s", request_url);

	TEST_ASSERT(strlen(request_url) > 10);

	ret = session_request_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	// 保障流程走完，内存释放
	lisa_thread_mdelay(1000);

	lisa_mem_get_info(&total_size, &free_size_after);
	LISA_NLOGI("after: total-%d free-%d", total_size, free_size_after);

	TEST_ASSERT(free_size_before == free_size_after);
}

void setUp(void)
{
	lisa_semaphore_reset(sem_connected);
	lisa_semaphore_reset(sem_disconnected);
	lisa_semaphore_reset(sem_auth_faild);
	lisa_semaphore_reset(sem_got_token);
}

void tearDown(void)
{
}

int main(void)
{
	net_init();
	net_up();

	sem_connected = lisa_semaphore_create(1);
	sem_disconnected = lisa_semaphore_create(1);
	sem_auth_faild = lisa_semaphore_create(1);
	sem_got_token = lisa_semaphore_create(1);

	UNITY_BEGIN();

	RUN_TEST(test_session_request_init_with_no_lsc_init);
	RUN_TEST(test_session_request_api_before_init);
	RUN_TEST(test_lsc_init);
	RUN_TEST(test_session_request_memory_leak);

	UNITY_END();

	return 0;
}