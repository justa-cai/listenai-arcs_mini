#define TAG "lsc_conn_tests"

#include <string.h>
#include "lisa_log.h"
#include "lisa_semaphore.h"
#include "lsc_conn.h"
#include "lsc_errno.h"
#include "unity.h"
#include "lisa_thread.h"
#include "lsc.h"

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"
#define DEVICE_ID_STRING  "F97CE114C70DE8E2"

lisa_semaphore_t *sem_connected;
lisa_semaphore_t *sem_connecting;
lisa_semaphore_t *sem_disconnected;
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

static void _conn_evt_cb(conn_event_e evt, void *data, uint32_t size, void *usr)
{
	LISA_NLOGD("[%s] evt:%s ", __FUNCTION__, STRINGS_CONN_EVT(evt));

	switch (evt) {
	case CONN_AUTH_SUCESS:
		strncpy(g_token, (char *)data, size);
		lisa_semaphore_give(sem_got_token);
		break;
	case CONN_CONNECTED:
		lisa_semaphore_give(sem_connected);
		break;
	case CONN_CONNECTING:
		lisa_semaphore_give(sem_connecting);
		break;
	case CONN_DISCONNECTED:
		lisa_semaphore_give(sem_disconnected);
		break;
	default:
		break;
	}
}

static void test_lsc_conn_mem_leak(void)
{
	int ret;

	lisa_mem_get_info(&total_size, &free_size_before);
	LISA_NLOGI("before: total-%d free-%d", total_size, free_size_before);

	lsc_conn_t *conn = lsc_conn_create();
	TEST_ASSERT_NOT_EQUAL(NULL, conn);

	ret = conn->add_evt_callback(_conn_evt_cb,
				     CONN_CONNECTED | CONN_CONNECTING | CONN_DISCONNECTED | CONN_AUTH_SUCESS |
					     CONN_AUTH_FAILD | CONN_DATA_CJSON,
				     NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = conn->auth(DEVICE_ID_STRING, PRODUCT_ID_STRING, SECRET_ID_STRING, NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_got_token, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = conn->connect(g_token);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_connecting, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_connected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	// 这个delay必须存在，_conn_evt_cb还在websocket上下文中
	lisa_thread_delay(1);

	ret = conn->disconnect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_disconnected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	// 这个delay必须存在，_conn_evt_cb还在websocket上下文中
	lisa_thread_delay(1);

	ret = lsc_conn_destroy(conn);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	lisa_mem_get_info(&total_size, &free_size_after);
	LISA_NLOGI("after: total-%d free-%d", total_size, free_size_after);

	TEST_ASSERT_EQUAL(free_size_before, free_size_after);
}

static void test_lsc_set_host_sta(void)
{
	int ret;
	bool host_status = false;

	lisa_mem_get_info(&total_size, &free_size_before);
	LISA_NLOGI("before: total-%d free-%d", total_size, free_size_before);

	lsc_conn_t *conn = lsc_conn_create();
	TEST_ASSERT_NOT_EQUAL(NULL, conn);

	ret = lsc_set_host_status(host_status);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = conn->add_evt_callback(
		_conn_evt_cb, CONN_CONNECTED | CONN_DISCONNECTED | CONN_AUTH_SUCESS | CONN_AUTH_FAILD | CONN_DATA_CJSON,
		NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = conn->auth(DEVICE_ID_STRING, PRODUCT_ID_STRING, SECRET_ID_STRING, NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_got_token, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = conn->connect(g_token);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_connected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	// 这个delay必须存在，_conn_evt_cb还在websocket上下文中
	lisa_thread_delay(1);

	ret = conn->disconnect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_disconnected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	// 这个delay必须存在，_conn_evt_cb还在websocket上下文中
	lisa_thread_delay(1);

	ret = lsc_conn_destroy(conn);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	lisa_mem_get_info(&total_size, &free_size_after);
	LISA_NLOGI("after: total-%d free-%d", total_size, free_size_after);

	TEST_ASSERT_EQUAL(free_size_before, free_size_after);
}

static void test_lsc_set_host_sta_before_init(void)
{
	int ret;
	bool host_status = false;

	lisa_mem_get_info(&total_size, &free_size_before);
	LISA_NLOGI("before: total-%d free-%d", total_size, free_size_before);

	ret = lsc_set_host_status(host_status);
	TEST_ASSERT_EQUAL(LSC_ERR, ret);

	lsc_conn_t *conn = lsc_conn_create();
	TEST_ASSERT_NOT_EQUAL(NULL, conn);

	ret = conn->add_evt_callback(_conn_evt_cb,
				     CONN_CONNECTED | CONN_CONNECTING | CONN_DISCONNECTED | CONN_AUTH_SUCESS |
					     CONN_AUTH_FAILD | CONN_DATA_CJSON,
				     NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = conn->auth(DEVICE_ID_STRING, PRODUCT_ID_STRING, SECRET_ID_STRING, NULL);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_got_token, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = conn->connect(g_token);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_connecting, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	ret = lisa_semaphore_take(sem_connected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	// 这个delay必须存在，_conn_evt_cb还在websocket上下文中
	lisa_thread_delay(1);

	ret = conn->disconnect();
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	ret = lisa_semaphore_take(sem_disconnected, 3000);
	TEST_ASSERT_EQUAL(LISA_OK, ret);

	// 这个delay必须存在，_conn_evt_cb还在websocket上下文中
	lisa_thread_delay(1);

	ret = lsc_conn_destroy(conn);
	TEST_ASSERT_EQUAL(LSC_OK, ret);

	lisa_mem_get_info(&total_size, &free_size_after);
	LISA_NLOGI("after: total-%d free-%d", total_size, free_size_after);

	TEST_ASSERT_EQUAL(free_size_before, free_size_after);
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
	sem_connecting = lisa_semaphore_create(1);
	sem_disconnected = lisa_semaphore_create(1);
	sem_got_token = lisa_semaphore_create(1);

	UNITY_BEGIN();

	RUN_TEST(test_lsc_conn_mem_leak);

	RUN_TEST(test_lsc_set_host_sta);

	RUN_TEST(test_lsc_set_host_sta_before_init);
	UNITY_END();

	return 0;
}