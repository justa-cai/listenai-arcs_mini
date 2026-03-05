#define TAG "lsc_test"

#include <string.h>
#include "lisa_log.h"
#include "lisa_semaphore.h"
#include "lsc.h"
#include "lsc_errno.h"
#include "unity.h"
#include "lisa_thread.h"

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"
#define DEVICE_ID_STRING  "F97CE114C70DE8E2"

lisa_semaphore_t *sem_connected;
lisa_semaphore_t *sem_disconnected;
lisa_semaphore_t *sem_auth_faild;
lisa_semaphore_t *sem_got_token;

size_t total_size, free_size_before, free_size_after;
char g_token[512];

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
		strcpy(g_token, (char *)data);
		lisa_semaphore_give(sem_got_token);
		break;
	default:
		break;
	}
}

static void test_lsc_init_deinit(void)
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

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_lsc_mem_leak(void)
{
	int ret;

	lisa_mem_get_info(&total_size, &free_size_before);
	LISA_NLOGI("before: total-%d free-%d", total_size, free_size_before);

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

	lisa_thread_delay(2);

	ret = lsc_disconnect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_disconnected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	lisa_thread_mdelay(500);
	lisa_mem_get_info(&total_size, &free_size_after);
	LISA_NLOGI("after: total-%d free-%d", total_size, free_size_after);

	TEST_ASSERT_EQUAL(free_size_before, free_size_after);
}

static void test_lsc_connect_disconnect(void)
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

	lisa_thread_delay(2);

	ret = lsc_disconnect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_disconnected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_lsc_reconnect_set_cfg(void)
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

	lisa_thread_delay(2);

	ret = lsc_disconnect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_disconnected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lsc_set_config(&cfg);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lsc_connect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_connected, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_token, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_lsc_set_cfg_before_init(void)
{
	int ret;

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
	};

	ret = lsc_set_config(&cfg);
	TEST_ASSERT_EQUAL(LSC_ERR, ret);

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

	lisa_thread_delay(2);

	ret = lsc_disconnect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_disconnected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lsc_connect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_lsc_more_set_cfg(void)
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

	lisa_thread_delay(2);

	ret = lsc_disconnect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_disconnected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lsc_set_config(&cfg);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lsc_set_config(&cfg);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lsc_connect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_connected, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_got_token, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_lsc_api_before_init(void)
{
	int ret;

	ret = lsc_connect();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	ret = lsc_disconnect();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	ret = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb,
			       NULL);
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	ret = lsc_remove_callback(lsc_event_cb);
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	ret = lsc_music_active();
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);

	char id[16], url[256];
	ret = lsc_music_request_url(id, url);
	TEST_ASSERT_NOT_EQUAL(LSC_OK, ret);
}

static void test_lsc_connect_with_token(void)
{
	int ret;

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
		.token = g_token,
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

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

static void test_lsc_reconnect(void)
{
	int ret;

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
		.if_auto_reconn = true,
		.reconn_interval_ms = 2000,
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

	net_down();

	lisa_thread_delay(5);

	ret = lisa_semaphore_take(sem_disconnected, 1000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	net_up();

	ret = lisa_semaphore_take(sem_connected, 5000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lsc_deinit();
	TEST_ASSERT_EQUAL(LSC_OK, ret);
}

void setUp(void)
{
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

	RUN_TEST(test_lsc_api_before_init);
	RUN_TEST(test_lsc_init_deinit);
	RUN_TEST(test_lsc_connect_disconnect);
	RUN_TEST(test_lsc_connect_with_token);
#ifdef __ZEPHYR__
	RUN_TEST(test_lsc_reconnect);
#endif
	RUN_TEST(test_lsc_mem_leak);
	RUN_TEST(test_lsc_reconnect_set_cfg);
	RUN_TEST(test_lsc_set_cfg_before_init);
	RUN_TEST(test_lsc_more_set_cfg);
	UNITY_END();

	return 0;
}