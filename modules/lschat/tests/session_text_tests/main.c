#define TAG "session_text_tests"

#include "lisa_log.h"
#include "lisa_semaphore.h"
#include "lisa_thread.h"
#include "lsc.h"
#include "lsc_session_text.h"
#include "lsc_errno.h"
#include "lisa_mem.h"
#include "unity.h"

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"
#define DEVICE_ID_STRING  "F97CE114C70DE8E2"

lisa_semaphore_t *sem_connected;
lisa_semaphore_t *sem_disconnected;
lisa_semaphore_t *sem_auth_faild;
lisa_semaphore_t *sem_got_token;

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

static void test_session_text_init_with_no_lsc_init(void)
{
	int ret;
	ret = session_text_init();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);
}

static void test_session_text_api_before_init(void)
{
	int ret;
	ret = session_text_deinit();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	session_text_config_t config = {0};
	ret = session_text_set_config(&config);
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	ret = session_text_get_config(&config);
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	ret = session_text_cancel();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	char temp[10] = "test";
	ret = session_text_send(temp);
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

static void test_session_text_memory_leak(void)
{
	size_t total_size;
	size_t free_size_before;
	size_t free_size_after;
	int ret;

	// 保障其他流程走完，内存释放
	lisa_thread_mdelay(1000);

	lisa_mem_get_info(&total_size, &free_size_before);
	LISA_NLOGI("before: total-%d free-%d", total_size, free_size_before);

	ret = session_text_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	session_text_config_t config = {
		.session_timeout_ms = 3000,
	};
	ret = session_text_set_config(&config);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	char temp[] = "清除记忆";
	ret = session_text_send(temp);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = session_text_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	lisa_mem_get_info(&total_size, &free_size_after);
	LISA_NLOGI("after: total-%d free-%d", total_size, free_size_after);

	TEST_ASSERT(free_size_before == free_size_after);
}

static void test_session_text_set_get_config(void)
{
	int ret;

	ret = session_text_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	session_text_config_t config = {
		.session_timeout_ms = 3000,
	};
	ret = session_text_set_config(&config);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	session_text_config_t config_get = {0};
	ret = session_text_get_config(&config_get);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	TEST_ASSERT(memcmp((void *)&config, (void *)&config_get, sizeof(session_text_config_t)) == 0);

	ret = session_text_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_session_text_normal_send(void)
{
	int ret;

	ret = session_text_init();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	session_text_config_t config = {
		.session_timeout_ms = 3000,
	};
	ret = session_text_set_config(&config);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	char temp[] = "清除记忆";
	ret = session_text_send(temp);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = session_text_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
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

	RUN_TEST(test_session_text_init_with_no_lsc_init);
	RUN_TEST(test_session_text_api_before_init);
	RUN_TEST(test_lsc_init);
	RUN_TEST(test_session_text_memory_leak);
	RUN_TEST(test_session_text_set_get_config);
	RUN_TEST(test_session_text_normal_send);

	UNITY_END();

	return 0;
}