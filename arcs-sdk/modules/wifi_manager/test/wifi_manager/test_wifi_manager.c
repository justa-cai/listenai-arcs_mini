#include "unity.h"
#include "wifi_manager/wifi_manager.h"
#include "wifi_manager_os_ops.h"
#include "wifi_manager_mem_ops.h"

#include "mock_wifi_manager_storage.h"
#include "mock_esp_heap_caps.h"
#include "mock_lisa_kv.h"
#include "mock_wifi_ops.h"
#include "mock_platform_dev.h"
#include "fake_lisa_mutex.h"
#include "fake_lisa_thread.h"
#include "mock_task.h"
#include "mock_os_ops.h"
#include "mock_mem_ops.h"

#include "lisa_time.h"

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include <signal.h>
#include <unistd.h>

DEFINE_FFF_GLOBALS;

#ifndef LISA_WAIT_FOREVER
#define LISA_WAIT_FOREVER (0xFFFFFFFFU)
#endif

// Define queue message event types
#define WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START 0
#define WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_STOP  1

void* wifi_mgr_queue;

wifi_mgr_ops_t wifi_mgr_ops;

typedef struct{
    uint32_t event;
    void* payload;
} wifi_mgr_queue_message_t;

static int custom_wifi_mgr_init(void)
{
    int ret = 0;
    
    wifi_mgr_ops.mem_ops = mock_mem_ops_get();
    wifi_mgr_ops.os_ops = mock_os_ops_get();
    wifi_mgr_ops.wifi_ops = mock_wifi_ops_get();

    ret = wifi_mgr_init(&wifi_mgr_ops);
    wifi_mgr_sta_enable();
    
    return ret;
}

void setUp(void)
{
    mock_wifi_storage_init();
    mock_os_ops_init();
    mock_mem_ops_init();
    mock_esp_heap_caps_init();
    mock_lisa_kv_init();
    mock_wifi_ops_init();
    mock_platform_dev_init();
    fake_lisa_mutex_init();
    fake_lisa_thread_init();
    mock_task_init();

    custom_wifi_mgr_init();

    wifi_mgr_queue = mock_queue_create_fake.return_val;
}

// 测试后清理
void tearDown(void)
{
    wifi_mgr_deinit();

    mock_wifi_storage_reset();
    mock_lisa_kv_reset();
    mock_wifi_ops_reset();
    mock_platform_dev_reset();
    fake_lisa_mutex_reset();
    fake_lisa_thread_reset();
    mock_task_reset();
    mock_esp_heap_caps_reset();
    mock_os_ops_reset();
    mock_mem_ops_reset();
}

static int custom_scan_ap_failed(wifi_mgr_scan_info_t *scan_info, uint32_t size, uint32_t timeout)
{
    (void)scan_info;
    (void)size;
    (void)timeout;


    return -EIO;
}

void test_wifi_manager_init(void)
{
    int ret = 0;
    wifi_mgr_deinit();
    mock_wifi_ops_reset();
    
    mock_mem_ops_reset();
    mock_os_ops_reset();

    ret = custom_wifi_mgr_init();
    TEST_ASSERT_EQUAL(0, ret);

    TEST_ASSERT_EQUAL(1, mock_wifi_init_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_mutex_create_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_queue_create_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_thread_create_fake.call_count);
    
}

void test_wifi_manager_init_called_storage_init(void)
{
    int ret = 0;

    wifi_mgr_deinit();
    mock_wifi_storage_reset();

    ret = custom_wifi_mgr_init();
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, wifi_storage_init_fake.call_count);
}

void test_wifi_manager_deinit(void)
{
    int ret = 0;
    
    mock_os_ops_reset();

    ret = wifi_mgr_deinit();
    TEST_ASSERT_EQUAL(0, ret);

    TEST_ASSERT_EQUAL(1, mock_mutex_delete_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_queue_delete_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_thread_delete_fake.call_count);
    TEST_ASSERT_TRUE(mock_free_fake.call_count > 0);
}

void test_wifi_manager_init_multiple_times_should_fail(void)
{
    int ret = 0;

    ret = custom_wifi_mgr_init();
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

void test_wifi_manager_scan_sync_mode(void)
{
    int ret = 0;

    wifi_mgr_scan_info_t ap_info[10];

    ret = wifi_mgr_scan_ap(ap_info, 10, false);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, mock_wifi_scan_ap_fake.call_count);
    TEST_ASSERT_EQUAL(10, mock_wifi_scan_ap_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(LISA_WAIT_FOREVER, mock_wifi_scan_ap_fake.arg2_val);
}

void test_wifi_manager_scan_sync_mode_failed_should_return_error_code(void)
{
    int ret = 0;

    mock_wifi_scan_ap_fake.custom_fake = custom_scan_ap_failed;

    ret = wifi_mgr_scan_ap(NULL, 0, false);

    TEST_ASSERT_EQUAL(-EIO, ret);
}

void test_wifi_manager_scan_when_sta_disabled_should_return_error(void)
{
    int ret = 0;
    wifi_mgr_scan_info_t ap_info[10];

    wifi_mgr_sta_disable();

    ret = wifi_mgr_scan_ap(ap_info, 10, false);

    TEST_ASSERT_EQUAL(-ENODEV, ret);
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_is_enable_fake.call_count);
    TEST_ASSERT_EQUAL(0, mock_wifi_scan_ap_fake.call_count);  // scan should not be called
}

void test_wifi_manager_scan_when_sta_enabled_should_work(void)
{
    int ret = 0;
    wifi_mgr_scan_info_t ap_info[10];

    wifi_mgr_sta_enable();
    mock_wifi_scan_ap_fake.return_val = 5;  // Mock returning 5 APs found

    ret = wifi_mgr_scan_ap(ap_info, 10, false);

    TEST_ASSERT_EQUAL(5, ret);
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_is_enable_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_wifi_scan_ap_fake.call_count);
}


static int auto_connect_start_check_queue_push_data(void *q, const void *item, size_t item_size, uint32_t timeout)
{
    (void)q;
    (void)timeout;

    wifi_mgr_queue_message_t *expect_msg = (wifi_mgr_queue_message_t *)item;

    TEST_ASSERT_EQUAL(0, expect_msg->event);   //WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START
    TEST_ASSERT_NOT_NULL(expect_msg->payload);

    return 0;
}

void test_wifi_manager_auto_connect_start(void)
{
    int ret = 0;
    wifi_mgr_autoconn_config_t config = {0};

    ret = wifi_mgr_auto_connect_start(&config);
    
    mock_queue_push_fake.custom_fake = auto_connect_start_check_queue_push_data;

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, mock_queue_push_fake.call_count);
    TEST_ASSERT_NOT_NULL(mock_queue_push_fake.arg0_val);
    TEST_ASSERT_NOT_NULL(mock_queue_push_fake.arg1_val);
    TEST_ASSERT_EQUAL(sizeof(wifi_mgr_autoconn_config_t), mock_queue_push_fake.arg2_val);
    TEST_ASSERT_EQUAL(LISA_WAIT_FOREVER, mock_queue_push_fake.arg3_val);

}

wifi_mgr_queue_message_t msg;
void test_wifi_manager_queue_push_auto_connect_start(void)
{
    int ret = 0;

    TEST_ASSERT_NOT_NULL(wifi_mgr_queue);

    msg.event = 0;
    
    msg.payload = mock_malloc(sizeof(wifi_mgr_autoconn_config_t));
    

    ret = mock_queue_push(wifi_mgr_queue, &msg, sizeof(msg), 1000);
    TEST_ASSERT_EQUAL(0, ret);

    sleep(2);

    TEST_ASSERT_EQUAL(1, mock_wifi_scan_ap_fake.call_count);

}

void test_wifi_manager_queue_push_auto_connect_start_with_null_payload(void)
{
    int ret = 0;

    msg.event = 0;
    msg.payload = NULL;

    ret = mock_queue_push(wifi_mgr_queue, &msg, sizeof(msg), 1000);
    TEST_ASSERT_EQUAL(0, ret);

    sleep(2);

    TEST_ASSERT_EQUAL(1, mock_wifi_scan_ap_fake.call_count);
}

void test_wifi_manager_auto_connect_when_sta_disabled_should_not_scan(void)
{
    int ret = 0;
    
    // Reset scan call count
    mock_wifi_scan_ap_fake.call_count = 0;
    
    wifi_mgr_sta_disable();

    msg.event = 0;  // WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START
    msg.payload = mock_malloc(sizeof(wifi_mgr_autoconn_config_t));

    ret = mock_queue_push(wifi_mgr_queue, &msg, sizeof(msg), 1000);
    TEST_ASSERT_EQUAL(0, ret);

    sleep(2);  // Wait for processing

    // When STA is disabled, autoconnect should not trigger scan
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_is_enable_fake.call_count);
    TEST_ASSERT_EQUAL(0, mock_wifi_scan_ap_fake.call_count);
}

void test_wifi_manager_auto_connect_when_sta_enabled_should_scan(void)
{
    int ret = 0;
    
    // Reset call counts
    mock_wifi_scan_ap_fake.call_count = 0;
    mock_wifi_sta_is_enable_fake.call_count = 0;
    
    // Mock STA as enabled
    mock_wifi_sta_is_enable_fake.return_val = true;
    mock_wifi_scan_ap_fake.return_val = 3;  // Mock finding 3 APs

    msg.event = 0;  // WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START
    msg.payload = mock_malloc(sizeof(wifi_mgr_autoconn_config_t));

    ret = mock_queue_push(wifi_mgr_queue, &msg, sizeof(msg), 1000);
    TEST_ASSERT_EQUAL(0, ret);

    sleep(2);  // Wait for processing

    // When STA is enabled, autoconnect should proceed to scan
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_is_enable_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_wifi_scan_ap_fake.call_count);
}

static int custom_sta_connect_failed(wifi_mgr_sta_config_t *sta_config, uint32_t timeout_ms)
{
    (void)sta_config;
    (void)timeout_ms;

    return -EIO;
}

static int custom_sta_connect_assert(wifi_mgr_sta_config_t *sta_config, uint32_t timeout_ms)
{
    (void)sta_config;
    (void)timeout_ms;

    TEST_ASSERT_EQUAL_STRING("test_ssid", sta_config->ssid);
    TEST_ASSERT_EQUAL_STRING("test_pwd", sta_config->pwd);

    return 0;
}

void test_wifi_manager_sta_connect_sync_mode(void)
{
    int ret = 0;
    wifi_mgr_sta_config_t config = {
        .ssid = "test_ssid",
        .pwd = "test_pwd"
    };

    mock_wifi_sta_connect_fake.custom_fake = custom_sta_connect_assert;

    ret = wifi_mgr_sta_connect(&config, false);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_connect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(CONFIG_WIFI_MGR_CONNECT_TIMEOUT, mock_wifi_sta_connect_fake.arg1_val);
}

void test_wifi_manager_sta_connect_async_mode(void)
{
    int ret = 0;
    wifi_mgr_sta_config_t config = {
        .ssid = "test_ssid",
        .pwd = "test_pwd"
    };

    mock_wifi_sta_connect_fake.custom_fake = custom_sta_connect_assert;

    ret = wifi_mgr_sta_connect(&config, true);

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_connect_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(LISA_NO_WAIT, mock_wifi_sta_connect_fake.arg1_val);
}

void test_wifi_manager_sta_connect_with_null_config(void)
{
    int ret = 0;

    ret = wifi_mgr_sta_connect(NULL, false);

    TEST_ASSERT_EQUAL(-EINVAL, ret);
    TEST_ASSERT_EQUAL(0, mock_wifi_sta_connect_fake.call_count);
}

void test_wifi_manager_sta_connect_failed(void)
{
    int ret = 0;
    wifi_mgr_sta_config_t config = {
        .ssid = "test_ssid",
        .pwd = "test_pwd"
    };

    mock_wifi_sta_connect_fake.custom_fake = custom_sta_connect_failed;

    ret = wifi_mgr_sta_connect(&config, false);

    TEST_ASSERT_EQUAL(-EIO, ret);
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_connect_fake.call_count);
}

void test_wifi_manager_sta_enable(void)
{
    int ret = 0;

    mock_wifi_sta_enable_fake.return_val = 0;
    mock_wifi_sta_enable_fake.call_count = 0;

    ret = wifi_mgr_sta_enable();

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_enable_fake.call_count);
}

void test_wifi_manager_sta_disable(void)
{
    int ret = 0;

    mock_wifi_sta_disable_fake.return_val = 0;
    mock_wifi_sta_disable_fake.call_count = 0;

    ret = wifi_mgr_sta_disable();

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_disable_fake.call_count);
}

void test_wifi_manager_sta_is_enable(void)
{
    bool enabled = false;

    mock_wifi_sta_is_enable_fake.return_val = true;

    enabled = wifi_mgr_sta_is_enable();

    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_is_enable_fake.call_count);
}

// Test callback handler for disconnect events
static int disconnect_callback_call_count = 0;
static wifi_mgr_connection_info_t last_connection_info;

static void test_disconnect_callback(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    (void)arg;
    disconnect_callback_call_count++;
    if (connection_info) {
        last_connection_info = *connection_info;
    }
}

void test_wifi_manager_disconnect_callback_when_connected_should_trigger(void)
{
    // Reset callback counter
    disconnect_callback_call_count = 0;
    
    // Add connection callback
    wifi_mgr_sta_add_connection_cb(test_disconnect_callback, NULL);
    
    // Simulate WiFi connected event through mock callback system
    wifi_mgr_wifi_event_t connected_event = WIFI_MGR_WIFI_EVT_STA_CONNECTED;
    wifi_mgr_sta_config_t connected_ap = {
        .ssid = "test_ssid",
        .bssid = "aa:bb:cc:dd:ee:ff",
        .pwd = "test_pwd"
    };
    
    // Get the registered callback from mock and call it to simulate connected event
    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);
    
    // Trigger connected event
    registered_cb->handler(connected_event, &connected_ap, sizeof(connected_ap), registered_cb->arg);
    
    // Reset callback counter after connect event
    disconnect_callback_call_count = 0;
    
    // Now simulate disconnect event
    wifi_mgr_wifi_event_t disconnect_event = WIFI_MGR_WIFI_EVT_STA_DISCONNECTED;
    registered_cb->handler(disconnect_event, NULL, 0, registered_cb->arg);
    
    // Callback should be triggered since device was connected
    TEST_ASSERT_EQUAL(1, disconnect_callback_call_count);
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_DISCONNECTED, last_connection_info.status);
    
    // Clean up
    wifi_mgr_sta_remove_connection_cb(test_disconnect_callback);
}

void test_wifi_manager_disconnect_callback_when_not_connected_should_not_trigger(void)
{
    // Reset callback counter
    disconnect_callback_call_count = 0;
    
    // Add connection callback
    wifi_mgr_sta_add_connection_cb(test_disconnect_callback, NULL);
    
    // Get the registered callback from mock
    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);
    
    // Simulate disconnect event without being connected first
    wifi_mgr_wifi_event_t disconnect_event = WIFI_MGR_WIFI_EVT_STA_DISCONNECTED;
    registered_cb->handler(disconnect_event, NULL, 0, registered_cb->arg);
    
    // Callback should NOT be triggered since device was never connected
    TEST_ASSERT_EQUAL(0, disconnect_callback_call_count);
    
    // Clean up
    wifi_mgr_sta_remove_connection_cb(test_disconnect_callback);
}

void test_wifi_manager_connection_failed_should_not_trigger_callback(void)
{
    // Reset callback counter
    disconnect_callback_call_count = 0;
    
    // Add connection callback
    wifi_mgr_sta_add_connection_cb(test_disconnect_callback, NULL);
    
    // Get the registered callback from mock
    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);
    
    // Simulate connection failed event
    wifi_mgr_wifi_event_t failed_event = WIFI_MGR_WIFI_EVT_STA_CONNECTION_FAILED;
    int reason = -1;
    registered_cb->handler(failed_event, &reason, sizeof(reason), registered_cb->arg);
    
    // Callback should NOT be triggered for connection failures
    TEST_ASSERT_EQUAL(0, disconnect_callback_call_count);
    
    // Clean up
    wifi_mgr_sta_remove_connection_cb(test_disconnect_callback);
}

void test_wifi_manager_multiple_disconnect_events_should_trigger_once(void)
{
    // Reset callback counter
    disconnect_callback_call_count = 0;
    
    // Add connection callback
    wifi_mgr_sta_add_connection_cb(test_disconnect_callback, NULL);
    
    // Get the registered callback from mock
    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);
    
    // Simulate WiFi connected event first
    wifi_mgr_wifi_event_t connected_event = WIFI_MGR_WIFI_EVT_STA_CONNECTED;
    wifi_mgr_sta_config_t connected_ap = {
        .ssid = "test_ssid",
        .bssid = "aa:bb:cc:dd:ee:ff",
        .pwd = "test_pwd"
    };
    
    registered_cb->handler(connected_event, &connected_ap, sizeof(connected_ap), registered_cb->arg);
    
    // Reset callback counter after connect event
    disconnect_callback_call_count = 0;
    
    // Simulate first disconnect event
    wifi_mgr_wifi_event_t disconnect_event = WIFI_MGR_WIFI_EVT_STA_DISCONNECTED;
    registered_cb->handler(disconnect_event, NULL, 0, registered_cb->arg);
    
    // First disconnect should trigger callback
    TEST_ASSERT_EQUAL(1, disconnect_callback_call_count);
    
    // Simulate second disconnect event (device is already disconnected)
    registered_cb->handler(disconnect_event, NULL, 0, registered_cb->arg);
    
    // Second disconnect should NOT trigger callback
    TEST_ASSERT_EQUAL(1, disconnect_callback_call_count);
    
    // Clean up
    wifi_mgr_sta_remove_connection_cb(test_disconnect_callback);
}

int main(void)
{
    
    UNITY_BEGIN();

    // init/deinit test cases
    RUN_TEST(test_wifi_manager_init);
    RUN_TEST(test_wifi_manager_init_called_storage_init);
    RUN_TEST(test_wifi_manager_init_multiple_times_should_fail);
    RUN_TEST(test_wifi_manager_deinit);

    // auto connect test cases
    RUN_TEST(test_wifi_manager_auto_connect_start);
    RUN_TEST(test_wifi_manager_queue_push_auto_connect_start);
    RUN_TEST(test_wifi_manager_queue_push_auto_connect_start_with_null_payload);
    RUN_TEST(test_wifi_manager_auto_connect_when_sta_disabled_should_not_scan);
    RUN_TEST(test_wifi_manager_auto_connect_when_sta_enabled_should_scan);
    
    // scan test cases
    RUN_TEST(test_wifi_manager_scan_sync_mode);
    RUN_TEST(test_wifi_manager_scan_sync_mode_failed_should_return_error_code);
    RUN_TEST(test_wifi_manager_scan_when_sta_disabled_should_return_error);
    RUN_TEST(test_wifi_manager_scan_when_sta_enabled_should_work);

    // sta connect test cases
    RUN_TEST(test_wifi_manager_sta_connect_sync_mode);
    RUN_TEST(test_wifi_manager_sta_connect_async_mode); 
    RUN_TEST(test_wifi_manager_sta_connect_with_null_config);
    RUN_TEST(test_wifi_manager_sta_connect_failed);

    // sta enable/disable/is_enable test cases
    RUN_TEST(test_wifi_manager_sta_enable);
    RUN_TEST(test_wifi_manager_sta_disable);
    RUN_TEST(test_wifi_manager_sta_is_enable);
    
    // disconnect callback test cases
    RUN_TEST(test_wifi_manager_disconnect_callback_when_connected_should_trigger);
    RUN_TEST(test_wifi_manager_disconnect_callback_when_not_connected_should_not_trigger);
    RUN_TEST(test_wifi_manager_connection_failed_should_not_trigger_callback);
    RUN_TEST(test_wifi_manager_multiple_disconnect_events_should_trigger_once);

    return UNITY_END();
}
