/*
 * lisa_pm tests - application sleep callback API.
 *
 * These cases cover the public single-instance callback contract and the
 * initial integration point in the managed-device PM hook path.
 */

#include "test_common.h"

#include <stdint.h>
#include <stdio.h>

#include "lisa_pm.h"
#include "unity.h"

/* Internal entry points are intentionally not public API; tests use forward
 * declarations to verify the PM hook wiring without exposing extra headers. */
void lisa_pm_dispatch_app_before_sleep(void);
void lisa_pm_dispatch_app_after_wake(lisa_pm_wakeup_cause_t cause);
void lisa_pm_dispatch_app_after_wake_in_task(lisa_pm_wakeup_cause_t cause);
int32_t lisa_pm_framework_device_on_enter(uint32_t sleep_time_us, void *arg);
int32_t lisa_pm_framework_device_on_wake(uint32_t sleep_time_us, void *arg);
int32_t lisa_pm_device_register(lisa_pm_device_t *dev);
int32_t lisa_pm_device_unregister(lisa_pm_device_t *dev);

typedef enum {
    TEST_SLEEP_EVENT_BEFORE = 1,
    TEST_SLEEP_EVENT_AFTER,
    TEST_SLEEP_EVENT_DEVICE_PREPARE,
    TEST_SLEEP_EVENT_DEVICE_RESUME,
} test_sleep_event_t;

#define TEST_SLEEP_EVENT_MAX 8U

static test_sleep_event_t g_events[TEST_SLEEP_EVENT_MAX];
static uint32_t g_event_count;
static uint32_t g_before_count;
static uint32_t g_after_count;
static uint32_t g_alt_before_count;
static uint32_t g_alt_after_count;
static void *g_before_user_data;
static void *g_after_user_data;
static void *g_alt_before_user_data;
static void *g_alt_after_user_data;

static void sleep_callback_reset_capture(void)
{
    uint32_t i;

    for (i = 0; i < TEST_SLEEP_EVENT_MAX; ++i) {
        g_events[i] = 0;
    }

    g_event_count = 0;
    g_before_count = 0;
    g_after_count = 0;
    g_alt_before_count = 0;
    g_alt_after_count = 0;
    g_before_user_data = NULL;
    g_after_user_data = NULL;
    g_alt_before_user_data = NULL;
    g_alt_after_user_data = NULL;
}

static void sleep_callback_record_event(test_sleep_event_t event)
{
    if (g_event_count < TEST_SLEEP_EVENT_MAX) {
        g_events[g_event_count] = event;
    }
    g_event_count++;
}

static void sleep_callback_cleanup(void)
{
    (void)lisa_pm_sleep_callback_unregister();
}

static void sleep_before_cb(void *user_data)
{
    g_before_count++;
    g_before_user_data = user_data;
    sleep_callback_record_event(TEST_SLEEP_EVENT_BEFORE);
}

static void sleep_after_cb(void *user_data, lisa_pm_wakeup_cause_t cause)
{
    (void)cause;
    g_after_count++;
    g_after_user_data = user_data;
    sleep_callback_record_event(TEST_SLEEP_EVENT_AFTER);
}

static void sleep_alt_before_cb(void *user_data)
{
    g_alt_before_count++;
    g_alt_before_user_data = user_data;
    sleep_callback_record_event(TEST_SLEEP_EVENT_BEFORE);
}

static void sleep_alt_after_cb(void *user_data, lisa_pm_wakeup_cause_t cause)
{
    (void)cause;
    g_alt_after_count++;
    g_alt_after_user_data = user_data;
    sleep_callback_record_event(TEST_SLEEP_EVENT_AFTER);
}

static int32_t sleep_test_device_prepare(void *ctx)
{
    (void)ctx;
    sleep_callback_record_event(TEST_SLEEP_EVENT_DEVICE_PREPARE);
    return 0;
}

static int32_t sleep_test_device_resume(void *ctx)
{
    (void)ctx;
    sleep_callback_record_event(TEST_SLEEP_EVENT_DEVICE_RESUME);
    return 0;
}

static const lisa_pm_system_ops_t g_sleep_test_device_ops = {
    .prepare_suspend = sleep_test_device_prepare,
    .resume_restore = sleep_test_device_resume,
};

static lisa_pm_device_t g_sleep_test_device = {
    .name = "sleep_callback_test_device",
    .ctx = NULL,
    .system_ops = &g_sleep_test_device_ops,
};

static void test_sleep_callback_rejects_invalid_inputs(void)
{
    lisa_pm_sleep_callback_t empty_callback = {0};

    sleep_callback_cleanup();
    sleep_callback_reset_capture();

    TEST_ASSERT_EQUAL_INT32(-1, lisa_pm_sleep_callback_register(NULL));
    TEST_ASSERT_EQUAL_INT32(-1, lisa_pm_sleep_callback_register(&empty_callback));
    TEST_ASSERT_EQUAL_INT32(-1, lisa_pm_sleep_callback_unregister());
}

static void test_sleep_callback_single_instance_and_unregister(void)
{
    int marker_a;
    int marker_b;
    lisa_pm_sleep_callback_t first = {
        .before_sleep = sleep_before_cb,
        .after_wake = sleep_after_cb,
        .user_data = &marker_a,
    };
    lisa_pm_sleep_callback_t second = {
        .before_sleep = sleep_alt_before_cb,
        .after_wake = sleep_alt_after_cb,
        .user_data = &marker_b,
    };

    sleep_callback_cleanup();
    sleep_callback_reset_capture();

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_sleep_callback_register(&first));
    TEST_ASSERT_EQUAL_INT32(-2, lisa_pm_sleep_callback_register(&second));

    lisa_pm_dispatch_app_before_sleep();
    lisa_pm_dispatch_app_after_wake_in_task(LISA_PM_WAKEUP_UNKNOWN);

    TEST_ASSERT_EQUAL_UINT32(1U, g_before_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_after_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_alt_before_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_alt_after_count);
    TEST_ASSERT_EQUAL_PTR(&marker_a, g_before_user_data);
    TEST_ASSERT_EQUAL_PTR(&marker_a, g_after_user_data);

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_sleep_callback_unregister());
    TEST_ASSERT_EQUAL_INT32(-1, lisa_pm_sleep_callback_unregister());
}

static void test_sleep_callback_register_copies_descriptor(void)
{
    int marker_a;
    int marker_b;
    lisa_pm_sleep_callback_t callback = {
        .before_sleep = sleep_before_cb,
        .user_data = &marker_a,
    };

    sleep_callback_cleanup();
    sleep_callback_reset_capture();

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_sleep_callback_register(&callback));

    callback.before_sleep = sleep_alt_before_cb;
    callback.after_wake = sleep_alt_after_cb;
    callback.user_data = &marker_b;

    lisa_pm_dispatch_app_before_sleep();
    lisa_pm_dispatch_app_after_wake_in_task(LISA_PM_WAKEUP_UNKNOWN);

    TEST_ASSERT_EQUAL_UINT32(1U, g_before_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_after_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_alt_before_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_alt_after_count);
    TEST_ASSERT_EQUAL_PTR(&marker_a, g_before_user_data);

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_sleep_callback_unregister());
}

static void test_sleep_callback_accepts_one_sided_callbacks(void)
{
    int marker_before;
    int marker_after;
    lisa_pm_sleep_callback_t before_only = {
        .before_sleep = sleep_before_cb,
        .user_data = &marker_before,
    };
    lisa_pm_sleep_callback_t after_only = {
        .after_wake = sleep_after_cb,
        .user_data = &marker_after,
    };

    sleep_callback_cleanup();
    sleep_callback_reset_capture();

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_sleep_callback_register(&before_only));
    lisa_pm_dispatch_app_before_sleep();
    lisa_pm_dispatch_app_after_wake_in_task(LISA_PM_WAKEUP_UNKNOWN);
    TEST_ASSERT_EQUAL_UINT32(1U, g_before_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_after_count);
    TEST_ASSERT_EQUAL_PTR(&marker_before, g_before_user_data);
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_sleep_callback_unregister());

    sleep_callback_reset_capture();

    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_sleep_callback_register(&after_only));
    lisa_pm_dispatch_app_before_sleep();
    lisa_pm_dispatch_app_after_wake_in_task(LISA_PM_WAKEUP_UNKNOWN);
    TEST_ASSERT_EQUAL_UINT32(0U, g_before_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_after_count);
    TEST_ASSERT_EQUAL_PTR(&marker_after, g_after_user_data);
    TEST_ASSERT_EQUAL_INT32(0, lisa_pm_sleep_callback_unregister());
}

static void test_sleep_callback_framework_hook_order(void)
{
    int marker;
    int32_t register_ret;
    int32_t device_register_ret = -1;
    int32_t enter_ret = -1;
    int32_t wake_ret = -1;
    lisa_pm_sleep_callback_t callback = {
        .before_sleep = sleep_before_cb,
        .after_wake = sleep_after_cb,
        .user_data = &marker,
    };

    sleep_callback_cleanup();
    (void)lisa_pm_device_unregister(&g_sleep_test_device);
    sleep_callback_reset_capture();

    register_ret = lisa_pm_sleep_callback_register(&callback);
    if (register_ret == 0) {
        device_register_ret = lisa_pm_device_register(&g_sleep_test_device);
    }
    if (register_ret == 0 && device_register_ret == 0) {
        enter_ret = lisa_pm_framework_device_on_enter(0, NULL);
        wake_ret = lisa_pm_framework_device_on_wake(0, NULL);
        lisa_pm_dispatch_app_after_wake_in_task(LISA_PM_WAKEUP_UNKNOWN);
    }

    (void)lisa_pm_device_unregister(&g_sleep_test_device);
    sleep_callback_cleanup();

    printf("[SLEEP_CB] hook_order register=%ld device=%ld enter=%ld wake=%ld events=%lu\n",
           (long)register_ret,
           (long)device_register_ret,
           (long)enter_ret,
           (long)wake_ret,
           (unsigned long)g_event_count);

    TEST_ASSERT_EQUAL_INT32(0, register_ret);
    TEST_ASSERT_EQUAL_INT32(0, device_register_ret);
    TEST_ASSERT_EQUAL_UINT32(4U, g_event_count);
    TEST_ASSERT_EQUAL_INT(TEST_SLEEP_EVENT_BEFORE, g_events[0]);
    TEST_ASSERT_EQUAL_INT(TEST_SLEEP_EVENT_DEVICE_PREPARE, g_events[1]);
    TEST_ASSERT_EQUAL_INT(TEST_SLEEP_EVENT_DEVICE_RESUME, g_events[2]);
    TEST_ASSERT_EQUAL_INT(TEST_SLEEP_EVENT_AFTER, g_events[3]);
}

void run_sleep_callback_tests(void)
{
    RUN_TEST(test_sleep_callback_rejects_invalid_inputs);
    RUN_TEST(test_sleep_callback_single_instance_and_unregister);
    RUN_TEST(test_sleep_callback_register_copies_descriptor);
    RUN_TEST(test_sleep_callback_accepts_one_sided_callbacks);
    RUN_TEST(test_sleep_callback_framework_hook_order);
}
