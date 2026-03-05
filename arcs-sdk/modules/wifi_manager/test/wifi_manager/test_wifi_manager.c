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
#include <stdlib.h>

DEFINE_FFF_GLOBALS;

#ifndef LISA_WAIT_FOREVER
#define LISA_WAIT_FOREVER (0xFFFFFFFFU)
#endif

// Define queue message event types
#define WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START 0
#define WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_STOP  1
#define WIFI_MGR_ERROR_WPA3_PWD_OR_AUTH_FAIL        204
#define WIFI_MGR_ERROR_FOUND_SSID_BUT_KEY_MISMATCH  205

void* wifi_mgr_queue;

wifi_mgr_ops_t wifi_mgr_ops;

typedef struct{
    uint32_t event;
    void* payload;
} wifi_mgr_queue_message_t;

static int s_sta_connect_calls;
static uint8_t s_pmk_valid_history[2];
static bool s_pmk_zeroed_history[2];
static int s_queue_push_calls;
static size_t s_queue_push_sizes[2];
static bool s_mutex_locked;
static int s_lock_violation_count;
static wifi_mgr_queue_message_t s_captured_queue_msgs[4];
static int s_captured_queue_msg_count;
static wifi_mgr_sta_config_t s_last_autoconn_cfg;

static int custom_wifi_scan_ap_single(wifi_mgr_wifi_scan_info_t *ap_info, uint32_t size, uint32_t timeout);
static int custom_wifi_storage_init(wifi_storage_ctx_t *ctx, wifi_storage_ops_t *ops, void *mutex);
static int custom_wifi_storage_search_ap_pmk(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list,
                                             wifi_mgr_storage_search_mode_t search_modes, void *target);
static int custom_wifi_scan_same_ssid_diff_bssid(wifi_mgr_wifi_scan_info_t *ap_info, uint32_t size, uint32_t timeout);
static int custom_wifi_storage_search_saved_bssid(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list,
                                                  wifi_mgr_storage_search_mode_t search_modes, void *target);
static int custom_wifi_sta_connect_autoconn(wifi_mgr_sta_config_t *sta_config, uint32_t timeout_ms);
static int custom_wifi_sta_connect_no_assert(wifi_mgr_sta_config_t *sta_config, uint32_t timeout_ms);
static int custom_queue_push_record_size(void *queue, const void *item, size_t size, uint32_t timeout);
static int custom_queue_push_capture_msgs(void *queue, const void *item, size_t size, uint32_t timeout);
static void test_scan_done_callback(wifi_mgr_scan_info_t *ap_info, int ap_num, void *arg);
static int custom_queue_push_fail(void *queue, const void *item, size_t size, uint32_t timeout);
static void test_public_apis_before_init_should_fail(void);
static void test_wifi_manager_sta_get_status_before_init_should_return_disconnected(void);
static void test_wifi_manager_init_should_fail_when_wifi_ops_init_fails(void);
static void test_wifi_manager_init_should_fail_when_add_callback_fails(void);
static void test_wifi_manager_init_should_fail_when_thread_create_fails(void);
static void test_wifi_manager_deinit_should_not_call_ops_while_locked(void);
static void test_wifi_manager_scan_done_event_null_data_should_not_crash(void);
static void test_wifi_manager_scan_failed_event_null_data_should_report_error(void);
static void test_wifi_manager_event_after_deinit_should_be_ignored(void);

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
    wifi_storage_init_fake.custom_fake = custom_wifi_storage_init;

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

static int custom_wifi_storage_init(wifi_storage_ctx_t *ctx, wifi_storage_ops_t *ops, void *mutex)
{
    if (!ctx || !ops) {
        return -EINVAL;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->ops = *ops;
    ctx->mutex = mutex;
    return 0;
}

static int custom_queue_push_record_size(void *queue, const void *item, size_t size, uint32_t timeout)
{
    (void)queue;
    (void)timeout;
    if (s_queue_push_calls < 2) {
        s_queue_push_sizes[s_queue_push_calls] = size;
    }
    s_queue_push_calls++;

    const wifi_mgr_queue_message_t *msg = (const wifi_mgr_queue_message_t *)item;
    if (msg && msg->payload) {
        free(msg->payload);
    }
    return 0;
}

static int custom_queue_push_capture_msgs(void *queue, const void *item, size_t size, uint32_t timeout)
{
    (void)queue;
    (void)size;
    (void)timeout;
    const wifi_mgr_queue_message_t *msg = (const wifi_mgr_queue_message_t *)item;
    if (s_captured_queue_msg_count < (int)(sizeof(s_captured_queue_msgs)/sizeof(s_captured_queue_msgs[0]))) {
        s_captured_queue_msgs[s_captured_queue_msg_count] = *msg;
    }
    s_captured_queue_msg_count++;
    if (msg && msg->payload) {
        free(msg->payload);
    }
    return 0;
}

static int scan_done_callback_call_count;
static int last_scan_ap_count;

static void test_scan_done_callback(wifi_mgr_scan_info_t *ap_info, int ap_num, void *arg)
{
    (void)ap_info;
    (void)arg;
    scan_done_callback_call_count++;
    last_scan_ap_count = ap_num;
}

static int custom_queue_push_fail(void *queue, const void *item, size_t size, uint32_t timeout)
{
    (void)queue;
    (void)item;
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
    TEST_ASSERT_EQUAL(sizeof(wifi_mgr_queue_message_t), mock_queue_push_fake.arg2_val);
    TEST_ASSERT_EQUAL(LISA_WAIT_FOREVER, mock_queue_push_fake.arg3_val);

}

wifi_mgr_queue_message_t msg;
void test_wifi_manager_queue_push_auto_connect_start(void)
{
    int ret = 0;

    TEST_ASSERT_NOT_NULL(wifi_mgr_queue);

    msg.event = 0;
    
    msg.payload = mock_malloc(sizeof(wifi_mgr_autoconn_config_t));
    

    ret = mock_queue_push(wifi_mgr_queue, &msg, sizeof(msg), 100);
    TEST_ASSERT_EQUAL(0, ret);

    usleep(200 * 1000);

    TEST_ASSERT_EQUAL(1, mock_wifi_scan_ap_fake.call_count);

}

void test_wifi_manager_queue_push_auto_connect_start_with_null_payload(void)
{
    int ret = 0;

    msg.event = 0;
    msg.payload = NULL;

    ret = mock_queue_push(wifi_mgr_queue, &msg, sizeof(msg), 100);
    TEST_ASSERT_EQUAL(0, ret);

    usleep(200 * 1000);

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

    ret = mock_queue_push(wifi_mgr_queue, &msg, sizeof(msg), 100);
    TEST_ASSERT_EQUAL(0, ret);

    usleep(200 * 1000);  // Wait for processing

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

    ret = mock_queue_push(wifi_mgr_queue, &msg, sizeof(msg), 100);
    TEST_ASSERT_EQUAL(0, ret);

    usleep(200 * 1000);  // Wait for processing

    // When STA is enabled, autoconnect should proceed to scan
    TEST_ASSERT_EQUAL(1, mock_wifi_sta_is_enable_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_wifi_scan_ap_fake.call_count);
}

void test_wifi_manager_autoconn_retry_without_pmk_on_handshake_timeout(void)
{
    wifi_mgr_queue_message_t msg_local = {0};
    int ret = 0;

    mock_wifi_scan_ap_fake.custom_fake = custom_wifi_scan_ap_single;
    mock_wifi_sta_connect_fake.custom_fake = custom_wifi_sta_connect_autoconn;
    mock_wifi_sta_is_enable_fake.return_val = true;
    wifi_storage_search_ap_fake.custom_fake = custom_wifi_storage_search_ap_pmk;

    s_sta_connect_calls = 0;
    memset(s_pmk_valid_history, 0, sizeof(s_pmk_valid_history));
    memset(s_pmk_zeroed_history, 0, sizeof(s_pmk_zeroed_history));

    msg_local.event = WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START;
    msg_local.payload = mock_malloc(sizeof(wifi_mgr_autoconn_config_t));

    ret = mock_queue_push(wifi_mgr_queue, &msg_local, sizeof(msg_local), 100);
    TEST_ASSERT_EQUAL(0, ret);

    usleep(200 * 1000);

    TEST_ASSERT_TRUE(s_sta_connect_calls >= 2);
    TEST_ASSERT_EQUAL(1, s_pmk_valid_history[0]);
    TEST_ASSERT_EQUAL(0, s_pmk_valid_history[1]);
    TEST_ASSERT_TRUE(s_pmk_zeroed_history[1]);
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

static int custom_sta_connect_expect_validssid(wifi_mgr_sta_config_t *sta_config, uint32_t timeout_ms)
{
    (void)timeout_ms;
    TEST_ASSERT_EQUAL_STRING("ValidSSID", sta_config->ssid);
    TEST_ASSERT_EQUAL_STRING("pwd", sta_config->pwd);
    memcpy(&s_last_autoconn_cfg, sta_config, sizeof(s_last_autoconn_cfg));
    return 0;
}

static int custom_wifi_sta_connect_expect_saved_bssid(wifi_mgr_sta_config_t *sta_config, uint32_t timeout_ms)
{
    (void)timeout_ms;
    memcpy(&s_last_autoconn_cfg, sta_config, sizeof(s_last_autoconn_cfg));
    return 0;
}

static int custom_wifi_scan_same_ssid_diff_bssid(wifi_mgr_wifi_scan_info_t *ap_info, uint32_t size, uint32_t timeout)
{
    (void)size;
    (void)timeout;

    strcpy(ap_info[0].ssid, "AutoSSID");
    strcpy(ap_info[0].bssid, "aa:aa:aa:aa:aa:aa"); // 不匹配
    ap_info[0].rssi = -10;
    ap_info[0].channel = 1;
    ap_info[0].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;

    strcpy(ap_info[1].ssid, "AutoSSID");
    strcpy(ap_info[1].bssid, "11:22:33:44:55:66"); // 匹配存储
    ap_info[1].rssi = -30;
    ap_info[1].channel = 6;
    ap_info[1].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;

    return 2;
}

static int custom_wifi_storage_search_saved_bssid(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list,
                                                  wifi_mgr_storage_search_mode_t search_modes, void *target)
{
    (void)ctx;
    (void)search_modes;
    (void)target;

    wifi_mgr_sta_config_t *list = mock_mem_ops_get()->malloc(sizeof(wifi_mgr_sta_config_t));
    if (!list) {
        return -ENOMEM;
    }
    memset(list, 0, sizeof(*list));
    strcpy(list[0].ssid, "AutoSSID");
    strcpy(list[0].pwd, "AutoPwd");
    strcpy(list[0].bssid, "11:22:33:44:55:66");
    list[0].channel = 6;
    list[0].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
    *matched_list = list;
    return 1;
}

static int custom_wifi_scan_ap_single(wifi_mgr_wifi_scan_info_t *ap_info, uint32_t size, uint32_t timeout)
{
    (void)size;
    (void)timeout;

    strcpy(ap_info[0].ssid, "AutoSSID");
    strcpy(ap_info[0].bssid, "11:22:33:44:55:66");
    ap_info[0].rssi = -20;
    ap_info[0].channel = 6;
    ap_info[0].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;

    return 1;
}

static int custom_wifi_storage_search_ap_pmk(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list,
                                             wifi_mgr_storage_search_mode_t search_modes, void *target)
{
    (void)ctx;
    (void)search_modes;
    (void)target;

    wifi_mgr_sta_config_t *list = mock_mem_ops_get()->malloc(sizeof(wifi_mgr_sta_config_t));
    if (!list) {
        return -ENOMEM;
    }
    memset(list, 0, sizeof(*list));
    strcpy(list[0].ssid, "AutoSSID");
    strcpy(list[0].pwd, "AutoPwd");
    strcpy(list[0].bssid, "11:22:33:44:55:66");
    list[0].channel = 6;
    list[0].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
    for (size_t i = 0; i < sizeof(list[0].pmk); i++) {
        list[0].pmk[i] = (uint8_t)(0xA0 + i);
    }
    list[0].pmk_valid = 1;

    *matched_list = list;
    return 1;
}

static int custom_wifi_sta_connect_autoconn(wifi_mgr_sta_config_t *sta_config, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (s_sta_connect_calls < (int)(sizeof(s_pmk_valid_history) / sizeof(s_pmk_valid_history[0]))) {
        s_pmk_valid_history[s_sta_connect_calls] = sta_config->pmk_valid;
        s_pmk_zeroed_history[s_sta_connect_calls] = true;
        for (size_t i = 0; i < sizeof(sta_config->pmk); i++) {
            if (sta_config->pmk[i] != 0) {
                s_pmk_zeroed_history[s_sta_connect_calls] = false;
                break;
            }
        }
    }
    s_sta_connect_calls++;
    return -EIO;
}

static int custom_wifi_sta_connect_no_assert(wifi_mgr_sta_config_t *sta_config, uint32_t timeout_ms)
{
    (void)sta_config;
    (void)timeout_ms;
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

void test_wifi_manager_sta_connect_sync_should_pause_and_resume_autoconnect(void)
{
    wifi_mgr_autoconn_config_t cfg = {
        .interval_ms = 1000,
    };
    wifi_mgr_sta_config_t config = {
        .ssid = "user_ssid",
        .pwd = "user_pwd",
    };

    mock_queue_push_fake.custom_fake = custom_queue_push_capture_msgs;
    s_captured_queue_msg_count = 0;
    TEST_ASSERT_EQUAL(0, wifi_mgr_auto_connect_start(&cfg));
    usleep(200 * 1000);  // allow manager thread to process start
    s_captured_queue_msg_count = 0;  // reset capture after initial start
    mock_wifi_sta_connect_fake.custom_fake = custom_wifi_sta_connect_no_assert;
    TEST_ASSERT_EQUAL(0, wifi_mgr_sta_connect(&config, false));

    TEST_ASSERT_EQUAL(2, s_captured_queue_msg_count);
    TEST_ASSERT_EQUAL(WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_STOP, s_captured_queue_msgs[0].event);
    TEST_ASSERT_EQUAL(WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START, s_captured_queue_msgs[1].event);
}

void test_wifi_manager_sta_connect_async_should_resume_autoconnect_on_event(void)
{
    wifi_mgr_autoconn_config_t cfg = {
        .interval_ms = 500,
    };
    wifi_mgr_sta_config_t config = {
        .ssid = "user_async",
        .pwd = "pwd_async",
    };

    mock_queue_push_fake.custom_fake = custom_queue_push_capture_msgs;
    s_captured_queue_msg_count = 0;
    TEST_ASSERT_EQUAL(0, wifi_mgr_auto_connect_start(&cfg));
    usleep(200 * 1000);
    s_captured_queue_msg_count = 0;

    mock_wifi_sta_connect_fake.custom_fake = custom_wifi_sta_connect_no_assert;
    TEST_ASSERT_EQUAL(0, wifi_mgr_sta_connect(&config, true));
    TEST_ASSERT_EQUAL(1, s_captured_queue_msg_count);
    TEST_ASSERT_EQUAL(WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_STOP, s_captured_queue_msgs[0].event);

    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    registered_cb->handler(WIFI_MGR_WIFI_EVT_STA_CONNECTED, &config, sizeof(config), registered_cb->arg);

    usleep(200 * 1000);

    TEST_ASSERT_EQUAL(2, s_captured_queue_msg_count);
    TEST_ASSERT_EQUAL(WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START, s_captured_queue_msgs[1].event);
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
static wifi_mgr_sta_config_t s_cb_recorded_sta_info;
static int s_cb_modify_call_count;
static int s_cb_record_call_count;

static void test_disconnect_callback(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    (void)arg;
    disconnect_callback_call_count++;
    if (connection_info) {
        last_connection_info = *connection_info;
    }
}

static void test_connection_cb_modify(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    (void)arg;
    s_cb_modify_call_count++;
    if (connection_info && connection_info->sta_info) {
        strcpy(connection_info->sta_info->ssid, "mutated_ssid");
    }
}

static void test_connection_cb_record(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    (void)arg;
    s_cb_record_call_count++;
    if (connection_info && connection_info->sta_info) {
        memcpy(&s_cb_recorded_sta_info, connection_info->sta_info, sizeof(s_cb_recorded_sta_info));
    } else {
        memset(&s_cb_recorded_sta_info, 0, sizeof(s_cb_recorded_sta_info));
    }
}

static int custom_mutex_lock_check(void *mutex, uint32_t timeout)
{
    (void)mutex;
    (void)timeout;
    s_mutex_locked = true;
    return 0;
}

static int custom_mutex_unlock_check(void *mutex)
{
    (void)mutex;
    s_mutex_locked = false;
    return 0;
}

static void custom_thread_delete_check(void *thread)
{
    (void)thread;
    if (s_mutex_locked) {
        s_lock_violation_count++;
    }
}

static int custom_wifi_remove_callback_check(wifi_mgr_wifi_event_cb_t *cb)
{
    (void)cb;
    if (s_mutex_locked) {
        s_lock_violation_count++;
    }
    return 0;
}

static int custom_wifi_deinit_check(void)
{
    if (s_mutex_locked) {
        s_lock_violation_count++;
    }
    return 0;
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
    TEST_ASSERT_TRUE(mock_wifi_add_callback_fake.call_count > 0);
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
    TEST_ASSERT_TRUE(mock_wifi_add_callback_fake.call_count > 0);
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

void test_wifi_mgr_sta_disconnect_should_block_autoconnect_to_same_ap(void)
{
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);

    wifi_mgr_sta_config_t ap = {
        .ssid = "AutoSSID",
        .bssid = "11:22:33:44:55:66",
        .pwd = "AutoPwd",
    };
    registered_cb->handler(WIFI_MGR_WIFI_EVT_STA_CONNECTED, &ap, sizeof(ap), registered_cb->arg);

    mock_wifi_sta_disconnect_fake.return_val = 0;
    TEST_ASSERT_EQUAL(0, wifi_mgr_sta_disconnect(false));
    registered_cb->handler(WIFI_MGR_WIFI_EVT_STA_DISCONNECTED, &ap, sizeof(ap), registered_cb->arg);

    mock_wifi_sta_is_enable_fake.return_val = true;
    mock_wifi_scan_ap_fake.custom_fake = custom_wifi_scan_ap_single;
    wifi_storage_search_ap_fake.custom_fake = custom_wifi_storage_search_ap_pmk;
    mock_wifi_sta_connect_fake.call_count = 0;
    mock_wifi_sta_connect_fake.custom_fake = custom_sta_connect_failed;

    wifi_mgr_autoconn_config_t cfg = {
        .interval_ms = 100,
    };
    TEST_ASSERT_EQUAL(0, wifi_mgr_auto_connect_start(&cfg));

    usleep(300 * 1000);

    TEST_ASSERT_EQUAL(0, mock_wifi_sta_connect_fake.call_count);
}

static int custom_wifi_scan_with_empty_ssid(wifi_mgr_wifi_scan_info_t *ap_info, uint32_t size, uint32_t timeout)
{
    (void)size;
    (void)timeout;
    memset(ap_info, 0, sizeof(wifi_mgr_wifi_scan_info_t) * 2);
    // 第一项为空SSID
    ap_info[0].bssid[0] = 'a';
    ap_info[0].channel = 1;
    ap_info[0].rssi = -50;
    // 第二项为有效SSID
    strcpy(ap_info[1].ssid, "ValidSSID");
    strcpy(ap_info[1].bssid, "66:55:44:33:22:11");
    ap_info[1].channel = 6;
    ap_info[1].rssi = -40;
    ap_info[1].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
    return 2;
}

static int custom_wifi_storage_search_empty_first(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list,
                                             wifi_mgr_storage_search_mode_t search_modes, void *target)
{
    (void)ctx;
    (void)search_modes;
    (void)target;

    wifi_mgr_sta_config_t *list = mock_mem_ops_get()->malloc(sizeof(wifi_mgr_sta_config_t) * 2);
    if (!list) {
        return -ENOMEM;
    }
    memset(list, 0, sizeof(wifi_mgr_sta_config_t) * 2);
    // 第一条为空SSID，第二条有效
    strcpy(list[1].ssid, "ValidSSID");
    strcpy(list[1].pwd, "pwd");
    strcpy(list[1].bssid, "66:55:44:33:22:11");
    list[1].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
    *matched_list = list;
    return 2;
}

void test_autoconnect_should_skip_empty_ssid_candidates(void)
{
    mock_wifi_sta_is_enable_fake.return_val = true;
    mock_wifi_scan_ap_fake.custom_fake = custom_wifi_scan_with_empty_ssid;
    wifi_storage_search_ap_fake.custom_fake = custom_wifi_storage_search_empty_first;
    mock_wifi_sta_connect_fake.call_count = 0;
    mock_wifi_sta_connect_fake.custom_fake = custom_sta_connect_expect_validssid;
    memset(&s_last_autoconn_cfg, 0, sizeof(s_last_autoconn_cfg));
    strcpy(s_last_autoconn_cfg.ssid, "UNSET");

    wifi_mgr_queue_message_t msg_local = {0};
    msg_local.event = WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START;
    msg_local.payload = mock_malloc(sizeof(wifi_mgr_autoconn_config_t));
    TEST_ASSERT_EQUAL(0, mock_queue_push(wifi_mgr_queue, &msg_local, sizeof(msg_local), 100));

    usleep(300 * 1000);

    TEST_ASSERT_EQUAL_STRING("ValidSSID", s_last_autoconn_cfg.ssid);
}

void test_autoconnect_should_respect_saved_bssid_when_present(void)
{
    mock_wifi_sta_is_enable_fake.return_val = true;
    mock_wifi_scan_ap_fake.custom_fake = custom_wifi_scan_same_ssid_diff_bssid;
    wifi_storage_search_ap_fake.custom_fake = custom_wifi_storage_search_saved_bssid;
    mock_wifi_sta_connect_fake.call_count = 0;
    mock_wifi_sta_connect_fake.custom_fake = custom_wifi_sta_connect_expect_saved_bssid;
    memset(&s_last_autoconn_cfg, 0, sizeof(s_last_autoconn_cfg));

    wifi_mgr_queue_message_t msg_local = {0};
    msg_local.event = WIFI_MGR_QUEUE_MSG_EVENT_WIFI_AUTO_CONNECT_START;
    msg_local.payload = mock_malloc(sizeof(wifi_mgr_autoconn_config_t));
    TEST_ASSERT_EQUAL(0, mock_queue_push(wifi_mgr_queue, &msg_local, sizeof(msg_local), 100));

    usleep(300 * 1000);

    TEST_ASSERT_EQUAL(1, mock_wifi_sta_connect_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66", s_last_autoconn_cfg.bssid);
}

static int custom_wifi_storage_search_saved_no_bssid(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list,
                                                     wifi_mgr_storage_search_mode_t search_modes, void *target)
{
    (void)ctx;
    (void)search_modes;
    (void)target;

    wifi_mgr_sta_config_t *list = mock_mem_ops_get()->malloc(sizeof(wifi_mgr_sta_config_t));
    if (!list) {
        return -ENOMEM;
    }
    memset(list, 0, sizeof(*list));
    strcpy(list[0].ssid, "NoBssidSSID");
    strcpy(list[0].pwd, "pwd");
    list[0].channel = 1;
    list[0].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
    *matched_list = list;
    return 1;
}

static int custom_wifi_scan_single_diff_bssid(wifi_mgr_wifi_scan_info_t *ap_info, uint32_t size, uint32_t timeout)
{
    (void)size;
    (void)timeout;
    strcpy(ap_info[0].ssid, "NoBssidSSID");
    strcpy(ap_info[0].bssid, "bb:bb:bb:bb:bb:bb");
    ap_info[0].rssi = -35;
    ap_info[0].channel = 6;
    ap_info[0].encryption_mode = WIFI_MGR_WIFI_AUTH_WPA2_PSK;
    return 1;
}

void test_manual_disconnect_without_bssid_should_blacklist_ssid(void)
{
    mock_wifi_sta_is_enable_fake.return_val = true;
    mock_wifi_scan_ap_fake.custom_fake = custom_wifi_scan_single_diff_bssid;
    wifi_storage_search_ap_fake.custom_fake = custom_wifi_storage_search_saved_no_bssid;
    mock_wifi_sta_connect_fake.call_count = 0;
    mock_wifi_sta_connect_fake.custom_fake = custom_sta_connect_failed;
    memset(&s_last_autoconn_cfg, 0, sizeof(s_last_autoconn_cfg));

    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    wifi_mgr_sta_config_t ap = {
        .ssid = "NoBssidSSID",
        .pwd = "pwd",
        .bssid = "aa:aa:aa:aa:aa:aa",
    };
    registered_cb->handler(WIFI_MGR_WIFI_EVT_STA_CONNECTED, &ap, sizeof(ap), registered_cb->arg);

    mock_wifi_sta_disconnect_fake.return_val = 0;
    TEST_ASSERT_EQUAL(0, wifi_mgr_sta_disconnect(false));
    registered_cb->handler(WIFI_MGR_WIFI_EVT_STA_DISCONNECTED, &ap, sizeof(ap), registered_cb->arg);

    int queue_push_calls_before = mock_queue_push_fake.call_count;
    wifi_mgr_autoconn_config_t cfg = {
        .interval_ms = 100,
    };
    TEST_ASSERT_EQUAL(0, wifi_mgr_auto_connect_start(&cfg));

    usleep(300 * 1000);

    TEST_ASSERT_EQUAL(queue_push_calls_before + 1, mock_queue_push_fake.call_count);
    TEST_ASSERT_EQUAL(0, mock_wifi_sta_connect_fake.call_count);
}

void test_wifi_manager_connection_failed_should_trigger_callback(void)
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
    wifi_mgr_connect_fail_info_t fail_info = {
        .error_code = -1,
        .status_code = -1,
        .reason_code = 15,
    };
    registered_cb->handler(failed_event, &fail_info, sizeof(fail_info), registered_cb->arg);
    
    // Callback should be triggered for connection failures
    TEST_ASSERT_EQUAL(1, disconnect_callback_call_count);
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_CONNECT_FAILED, last_connection_info.status);
    TEST_ASSERT_EQUAL(15, last_connection_info.reason);
    
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

void test_wifi_manager_callbacks_should_not_share_sta_info_buffer(void)
{
    s_cb_modify_call_count = 0;
    s_cb_record_call_count = 0;
    memset(&s_cb_recorded_sta_info, 0, sizeof(s_cb_recorded_sta_info));

    wifi_mgr_sta_add_connection_cb(test_connection_cb_record, NULL);
    wifi_mgr_sta_add_connection_cb(test_connection_cb_modify, NULL);

    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);

    wifi_mgr_wifi_event_t connected_event = WIFI_MGR_WIFI_EVT_STA_CONNECTED;
    wifi_mgr_sta_config_t connected_ap = {
        .ssid = "orig_ssid",
        .bssid = "aa:bb:cc:dd:ee:ff",
        .pwd = "test_pwd"
    };
    registered_cb->handler(connected_event, &connected_ap, sizeof(connected_ap), registered_cb->arg);

    TEST_ASSERT_EQUAL(1, s_cb_modify_call_count);
    TEST_ASSERT_EQUAL(1, s_cb_record_call_count);
    TEST_ASSERT_EQUAL_STRING("orig_ssid", s_cb_recorded_sta_info.ssid);

    wifi_mgr_sta_remove_connection_cb(test_connection_cb_modify);
    wifi_mgr_sta_remove_connection_cb(test_connection_cb_record);
}

void test_wifi_manager_connected_event_null_data_should_not_crash(void)
{
    disconnect_callback_call_count = 0;
    wifi_mgr_sta_add_connection_cb(test_disconnect_callback, NULL);

    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);

    wifi_mgr_wifi_event_t connected_event = WIFI_MGR_WIFI_EVT_STA_CONNECTED;
    registered_cb->handler(connected_event, NULL, 0, registered_cb->arg);

    TEST_ASSERT_EQUAL(1, disconnect_callback_call_count);
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_CONNECTED, last_connection_info.status);
    TEST_ASSERT_NULL(last_connection_info.sta_info);

    wifi_mgr_sta_remove_connection_cb(test_disconnect_callback);
}

void test_wifi_manager_scan_failed_event_null_data_should_not_crash(void)
{
    scan_done_callback_call_count = 0;
    last_scan_ap_count = 0;
    wifi_mgr_add_scan_done_cb(test_scan_done_callback, NULL);

    TEST_ASSERT_TRUE(mock_wifi_add_callback_fake.call_count > 0);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);

    wifi_mgr_wifi_event_t failed_event = WIFI_MGR_WIFI_EVT_SCAN_FAILED;
    registered_cb->handler(failed_event, NULL, 0, registered_cb->arg);

    TEST_ASSERT_EQUAL(1, scan_done_callback_call_count);
    TEST_ASSERT_EQUAL(-1, last_scan_ap_count);

    wifi_mgr_remove_scan_done_cb(test_scan_done_callback);
}

void test_wifi_manager_auto_connect_queue_message_size(void)
{
    s_queue_push_calls = 0;
    s_queue_push_sizes[0] = 0;
    s_queue_push_sizes[1] = 0;
    mock_queue_push_fake.custom_fake = custom_queue_push_record_size;

    wifi_mgr_autoconn_config_t cfg = {
        .interval_ms = 1000,
    };

    TEST_ASSERT_EQUAL(0, wifi_mgr_auto_connect_start(&cfg));
    TEST_ASSERT_EQUAL(0, wifi_mgr_auto_connect_stop());
    TEST_ASSERT_EQUAL(2, s_queue_push_calls);
    TEST_ASSERT_EQUAL(sizeof(wifi_mgr_queue_message_t), s_queue_push_sizes[0]);
    TEST_ASSERT_EQUAL(sizeof(wifi_mgr_queue_message_t), s_queue_push_sizes[1]);
}

void test_wifi_manager_add_callback_before_init_should_fail(void)
{
    wifi_mgr_deinit();
    int ret = wifi_mgr_sta_add_connection_cb(test_disconnect_callback, NULL);
    TEST_ASSERT_EQUAL(-EIO, ret);
}

void test_public_apis_before_init_should_fail(void)
{
    wifi_mgr_sta_config_t cfg = {0};
    wifi_mgr_scan_info_t ap_list[2];

    wifi_mgr_deinit();

    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_sta_enable());
    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_sta_disable());
    TEST_ASSERT_FALSE(wifi_mgr_sta_is_enable());
    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_scan_ap(ap_list, 2, false));
    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_sta_connect(&cfg, false));
    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_sta_disconnect(false));
}

void test_wifi_manager_sta_get_status_before_init_should_return_disconnected(void)
{
    wifi_mgr_deinit();
    TEST_ASSERT_EQUAL(WIFI_MGR_STA_DISCONNECTED, wifi_mgr_sta_get_status());
}

void test_wifi_manager_init_should_fail_when_wifi_ops_init_fails(void)
{
    wifi_mgr_deinit();
    mock_os_ops_reset();
    mock_mem_ops_reset();
    mock_wifi_ops_reset();
    mock_wifi_storage_reset();

    wifi_mgr_ops.mem_ops = mock_mem_ops_get();
    wifi_mgr_ops.os_ops = mock_os_ops_get();
    wifi_mgr_ops.wifi_ops = mock_wifi_ops_get();

    mock_wifi_init_fake.return_val = -EIO;
    mock_queue_create_fake.call_count = 0;
    mock_wifi_add_callback_fake.call_count = 0;

    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_init(&wifi_mgr_ops));
    TEST_ASSERT_EQUAL(0, mock_queue_create_fake.call_count);
    TEST_ASSERT_EQUAL(0, mock_wifi_add_callback_fake.call_count);
}

void test_wifi_manager_init_should_fail_when_add_callback_fails(void)
{
    wifi_mgr_deinit();
    mock_os_ops_reset();
    mock_mem_ops_reset();
    mock_wifi_ops_reset();
    mock_wifi_storage_reset();

    wifi_mgr_ops.mem_ops = mock_mem_ops_get();
    wifi_mgr_ops.os_ops = mock_os_ops_get();
    wifi_mgr_ops.wifi_ops = mock_wifi_ops_get();

    mock_wifi_add_callback_fake.return_val = -EIO;
    mock_queue_create_fake.call_count = 0;
    mock_wifi_deinit_fake.call_count = 0;

    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_init(&wifi_mgr_ops));
    TEST_ASSERT_EQUAL(0, mock_queue_create_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_wifi_deinit_fake.call_count);
}

void test_wifi_manager_init_should_fail_when_thread_create_fails(void)
{
    wifi_mgr_deinit();
    mock_os_ops_reset();
    mock_mem_ops_reset();
    mock_wifi_ops_reset();
    mock_wifi_storage_reset();

    wifi_mgr_ops.mem_ops = mock_mem_ops_get();
    wifi_mgr_ops.os_ops = mock_os_ops_get();
    wifi_mgr_ops.wifi_ops = mock_wifi_ops_get();

    mock_thread_create_fake.custom_fake = NULL;
    mock_thread_create_fake.return_val = NULL;
    mock_wifi_remove_callback_fake.call_count = 0;
    mock_wifi_deinit_fake.call_count = 0;
    mock_queue_delete_fake.call_count = 0;
    mock_mutex_delete_fake.call_count = 0;

    TEST_ASSERT_EQUAL(-ENOMEM, wifi_mgr_init(&wifi_mgr_ops));
    TEST_ASSERT_EQUAL(1, mock_queue_delete_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_mutex_delete_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_wifi_remove_callback_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_wifi_deinit_fake.call_count);
    TEST_ASSERT_TRUE(mock_free_fake.call_count > 0);
}

void test_wifi_manager_deinit_should_not_call_ops_while_locked(void)
{
    s_mutex_locked = false;
    s_lock_violation_count = 0;

    mock_mutex_lock_fake.custom_fake = custom_mutex_lock_check;
    mock_mutex_unlock_fake.custom_fake = custom_mutex_unlock_check;
    mock_thread_delete_fake.custom_fake = custom_thread_delete_check;
    mock_wifi_remove_callback_fake.custom_fake = custom_wifi_remove_callback_check;
    mock_wifi_deinit_fake.custom_fake = custom_wifi_deinit_check;

    TEST_ASSERT_EQUAL(0, wifi_mgr_deinit());
    TEST_ASSERT_EQUAL(0, s_lock_violation_count);
}

void test_wifi_manager_scan_done_event_null_data_should_not_crash(void)
{
    scan_done_callback_call_count = 0;
    wifi_mgr_add_scan_done_cb(test_scan_done_callback, NULL);

    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);

    wifi_mgr_wifi_event_t done_event = WIFI_MGR_WIFI_EVT_SCAN_DONE;
    registered_cb->handler(done_event, NULL, 0, registered_cb->arg);

    TEST_ASSERT_EQUAL(0, scan_done_callback_call_count);

    wifi_mgr_remove_scan_done_cb(test_scan_done_callback);
}

void test_wifi_manager_scan_failed_event_null_data_should_report_error(void)
{
    scan_done_callback_call_count = 0;
    last_scan_ap_count = 0;
    wifi_mgr_add_scan_done_cb(test_scan_done_callback, NULL);

    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);

    wifi_mgr_wifi_event_t failed_event = WIFI_MGR_WIFI_EVT_SCAN_FAILED;
    registered_cb->handler(failed_event, NULL, 0, registered_cb->arg);

    TEST_ASSERT_EQUAL(1, scan_done_callback_call_count);
    TEST_ASSERT_EQUAL(-1, last_scan_ap_count);

    wifi_mgr_remove_scan_done_cb(test_scan_done_callback);
}

void test_wifi_manager_event_after_deinit_should_be_ignored(void)
{
    disconnect_callback_call_count = 0;
    wifi_mgr_sta_add_connection_cb(test_disconnect_callback, NULL);

    TEST_ASSERT_EQUAL(1, mock_wifi_add_callback_fake.call_count);
    wifi_mgr_wifi_event_cb_t *registered_cb = (wifi_mgr_wifi_event_cb_t*)mock_wifi_add_callback_fake.arg0_val;
    TEST_ASSERT_NOT_NULL(registered_cb);
    TEST_ASSERT_NOT_NULL(registered_cb->handler);

    wifi_mgr_wifi_event_handler_t handler = registered_cb->handler;
    void *handler_arg = registered_cb->arg;

    TEST_ASSERT_EQUAL(0, wifi_mgr_deinit());

    wifi_mgr_wifi_event_t connected_event = WIFI_MGR_WIFI_EVT_STA_CONNECTED;
    wifi_mgr_sta_config_t connected_ap = {
        .ssid = "test_ssid",
        .bssid = "aa:bb:cc:dd:ee:ff",
        .pwd = "test_pwd"
    };
    handler(connected_event, &connected_ap, sizeof(connected_ap), handler_arg);

    TEST_ASSERT_EQUAL(0, disconnect_callback_call_count);
}

void test_wifi_manager_auto_connect_start_queue_push_fail_should_free_payload(void)
{
    mock_free_fake.call_count = 0;
    mock_queue_push_fake.custom_fake = custom_queue_push_fail;

    wifi_mgr_autoconn_config_t cfg = {
        .interval_ms = 1000,
    };

    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_auto_connect_start(&cfg));
    TEST_ASSERT_EQUAL(1, mock_free_fake.call_count);
}

void test_wifi_manager_deinit_should_remove_callback_and_deinit(void)
{
    mock_wifi_remove_callback_fake.call_count = 0;
    mock_wifi_deinit_fake.call_count = 0;

    TEST_ASSERT_EQUAL(0, wifi_mgr_deinit());
    TEST_ASSERT_EQUAL(1, mock_wifi_remove_callback_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_wifi_deinit_fake.call_count);
}

void test_wifi_storage_save_ap_should_reject_empty_ssid(void)
{
    wifi_mgr_sta_config_t ap = {0};
    strcpy(ap.bssid, "11:22:33:44:55:66");
    strcpy(ap.pwd, "pwd");

    mock_wifi_storage_reset();
    wifi_storage_save_ap_fake.return_val = 0;
    wifi_storage_save_ap_fake.call_count = 0;
    int ret = wifi_mgr_storage_save_ap(&ap);
    TEST_ASSERT_EQUAL(-EINVAL, ret);
    TEST_ASSERT_EQUAL(0, wifi_storage_save_ap_fake.call_count);
}

void test_wifi_manager_init_storage_fail_should_cleanup_resources(void)
{
    wifi_mgr_deinit();
    mock_os_ops_reset();
    mock_mem_ops_reset();
    mock_wifi_ops_reset();
    mock_wifi_storage_reset();

    wifi_mgr_ops.mem_ops = mock_mem_ops_get();
    wifi_mgr_ops.os_ops = mock_os_ops_get();
    wifi_mgr_ops.wifi_ops = mock_wifi_ops_get();

    wifi_storage_init_fake.return_val = -EIO;
    mock_mutex_delete_fake.call_count = 0;
    mock_queue_delete_fake.call_count = 0;

    TEST_ASSERT_EQUAL(-EIO, wifi_mgr_init(&wifi_mgr_ops));
    TEST_ASSERT_EQUAL(1, mock_mutex_delete_fake.call_count);
    TEST_ASSERT_EQUAL(1, mock_queue_delete_fake.call_count);
    TEST_ASSERT_TRUE(mock_free_fake.call_count > 0);

    wifi_storage_init_fake.return_val = 0;
    TEST_ASSERT_EQUAL(0, wifi_mgr_init(&wifi_mgr_ops));
}

static int custom_wifi_storage_search_ap_success(wifi_storage_ctx_t *ctx, wifi_mgr_sta_config_t **matched_list, 
                                               wifi_mgr_storage_search_mode_t search_modes, void *target)
{
    (void)ctx;
    (void)search_modes;
    (void)target;

    int count = 3;
    *matched_list = mock_mem_ops_get()->malloc(count * sizeof(wifi_mgr_sta_config_t));
    
    for (int i = 0; i < count; i++) {
        sprintf((*matched_list)[i].ssid, "test_ap_%d", i);
    }
    
    return count;
}

void test_wifi_manager_storage_search_ap_query_count_should_work(void)
{
    int ret = 0;
    
    wifi_storage_search_ap_fake.custom_fake = custom_wifi_storage_search_ap_success;
    
    ret = wifi_mgr_storage_search_ap(NULL, 0, SEARCH_ALL, NULL);
    
    TEST_ASSERT_EQUAL(3, ret);
    // Should call free internally
    TEST_ASSERT_TRUE(mock_free_fake.call_count > 0);
}

void test_wifi_manager_storage_search_ap_with_buffer_should_copy_data(void)
{
    int ret = 0;
    wifi_mgr_sta_config_t list[3];
    
    wifi_storage_search_ap_fake.custom_fake = custom_wifi_storage_search_ap_success;
    
    ret = wifi_mgr_storage_search_ap(list, 3, SEARCH_ALL, NULL);
    
    TEST_ASSERT_EQUAL(3, ret);
    TEST_ASSERT_EQUAL_STRING("test_ap_0", list[0].ssid);
    TEST_ASSERT_EQUAL_STRING("test_ap_1", list[1].ssid);
    TEST_ASSERT_EQUAL_STRING("test_ap_2", list[2].ssid);
    // Should call free internally
    TEST_ASSERT_TRUE(mock_free_fake.call_count > 0);
}

void test_wifi_manager_storage_search_ap_with_small_buffer_should_truncate(void)
{
    int ret = 0;
    wifi_mgr_sta_config_t list[2]; // Only space for 2, but 3 returned
    
    wifi_storage_search_ap_fake.custom_fake = custom_wifi_storage_search_ap_success;
    
    ret = wifi_mgr_storage_search_ap(list, 2, SEARCH_ALL, NULL);
    
    TEST_ASSERT_EQUAL(3, ret); // Returns actual found count
    TEST_ASSERT_EQUAL_STRING("test_ap_0", list[0].ssid);
    TEST_ASSERT_EQUAL_STRING("test_ap_1", list[1].ssid);
    // Should call free internally
    TEST_ASSERT_TRUE(mock_free_fake.call_count > 0);
}

void test_wifi_manager_storage_search_ap_internal_fail(void)
{
    int ret = 0;
    
    wifi_storage_search_ap_fake.return_val = -ENOMEM;
    
    ret = wifi_mgr_storage_search_ap(NULL, 0, SEARCH_ALL, NULL);
    
    TEST_ASSERT_EQUAL(-ENOMEM, ret);
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
    RUN_TEST(test_wifi_manager_auto_connect_queue_message_size);
    RUN_TEST(test_wifi_manager_auto_connect_when_sta_disabled_should_not_scan);
    RUN_TEST(test_wifi_manager_auto_connect_when_sta_enabled_should_scan);
    RUN_TEST(test_wifi_manager_autoconn_retry_without_pmk_on_handshake_timeout);
    
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
    RUN_TEST(test_wifi_manager_sta_connect_sync_should_pause_and_resume_autoconnect);
    RUN_TEST(test_wifi_manager_sta_connect_async_should_resume_autoconnect_on_event);

    // sta enable/disable/is_enable test cases
    RUN_TEST(test_wifi_manager_sta_enable);
    RUN_TEST(test_wifi_manager_sta_disable);
    RUN_TEST(test_wifi_manager_sta_is_enable);
    
    // disconnect callback test cases
    RUN_TEST(test_wifi_manager_disconnect_callback_when_connected_should_trigger);
    RUN_TEST(test_wifi_manager_disconnect_callback_when_not_connected_should_not_trigger);
    RUN_TEST(test_wifi_mgr_sta_disconnect_should_block_autoconnect_to_same_ap);
    RUN_TEST(test_autoconnect_should_skip_empty_ssid_candidates);
    RUN_TEST(test_autoconnect_should_respect_saved_bssid_when_present);
    RUN_TEST(test_manual_disconnect_without_bssid_should_blacklist_ssid);
    RUN_TEST(test_wifi_manager_connection_failed_should_trigger_callback);
    RUN_TEST(test_wifi_manager_multiple_disconnect_events_should_trigger_once);
    RUN_TEST(test_wifi_manager_callbacks_should_not_share_sta_info_buffer);
    RUN_TEST(test_wifi_manager_connected_event_null_data_should_not_crash);
    RUN_TEST(test_wifi_manager_scan_failed_event_null_data_should_not_crash);
    RUN_TEST(test_wifi_manager_add_callback_before_init_should_fail);
    RUN_TEST(test_public_apis_before_init_should_fail);
    RUN_TEST(test_wifi_manager_sta_get_status_before_init_should_return_disconnected);
    RUN_TEST(test_wifi_manager_auto_connect_start_queue_push_fail_should_free_payload);
    RUN_TEST(test_wifi_manager_deinit_should_remove_callback_and_deinit);
    RUN_TEST(test_wifi_storage_save_ap_should_reject_empty_ssid);
    RUN_TEST(test_wifi_manager_init_storage_fail_should_cleanup_resources);
    RUN_TEST(test_wifi_manager_init_should_fail_when_wifi_ops_init_fails);
    RUN_TEST(test_wifi_manager_init_should_fail_when_add_callback_fails);
    RUN_TEST(test_wifi_manager_init_should_fail_when_thread_create_fails);
    RUN_TEST(test_wifi_manager_deinit_should_not_call_ops_while_locked);
    RUN_TEST(test_wifi_manager_scan_done_event_null_data_should_not_crash);
    RUN_TEST(test_wifi_manager_scan_failed_event_null_data_should_report_error);
    RUN_TEST(test_wifi_manager_event_after_deinit_should_be_ignored);

    // storage search test cases
    RUN_TEST(test_wifi_manager_storage_search_ap_query_count_should_work);
    RUN_TEST(test_wifi_manager_storage_search_ap_with_buffer_should_copy_data);
    RUN_TEST(test_wifi_manager_storage_search_ap_with_small_buffer_should_truncate);
    RUN_TEST(test_wifi_manager_storage_search_ap_internal_fail);

    return UNITY_END();
}
