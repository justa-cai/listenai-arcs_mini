#include "unity.h"
#include "lisa_device.h"

#include <string.h>

static int s_attach_ctx;
static int s_attach_init_count;
static int s_attach_deinit_count;

static int pm_attach_test_init(void)
{
    s_attach_init_count++;
    return 0;
}

static int pm_attach_test_deinit(void)
{
    s_attach_deinit_count++;
    return 0;
}

static int32_t pm_attach_check_idle(void *ctx)
{
    TEST_ASSERT_EQUAL_PTR(&s_attach_ctx, ctx);
    return 1;
}

static const lisa_pm_system_ops_t s_attach_pm_ops = {
    .check_idle = pm_attach_check_idle,
    .prepare_suspend = NULL,
    .resume_restore = NULL,
};

static int32_t pm_attach_wakeup_configure(struct lisa_device *dev, uint32_t sub_idx, uint32_t trigger)
{
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_UINT32(1U, sub_idx);
    TEST_ASSERT_EQUAL_UINT32(0U, trigger);
    return 0;
}

static int32_t pm_attach_wakeup_clear(struct lisa_device *dev, uint32_t sub_idx)
{
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_UINT32(1U, sub_idx);
    return 0;
}

static int32_t pm_attach_wakeup_set_enabled(struct lisa_device *dev, bool enable)
{
    TEST_ASSERT_NOT_NULL(dev);
    (void)enable;
    return 0;
}

static const lisa_pm_wakeup_ops_t s_attach_wakeup_ops = {
    .configure = pm_attach_wakeup_configure,
    .clear = pm_attach_wakeup_clear,
    .set_enabled = pm_attach_wakeup_set_enabled,
};

LISA_DEVICE_REGISTER_DEINIT(pm_attach_test,
                            NULL,
                            &s_attach_ctx,
                            NULL,
                            pm_attach_test_init,
                            pm_attach_test_deinit,
                            LISA_DEVICE_LEVEL_NORMAL,
                            LISA_DEVICE_PRIORITY_LOWEST);

LISA_DEVICE_PM_ATTACH(pm_attach_test, &s_attach_pm_ops, &s_attach_wakeup_ops, &s_attach_ctx);

typedef struct {
    int count;
    bool found;
} pm_attach_scan_t;

static int pm_attach_scan_cb(lisa_device_t *dev, const lisa_device_pm_t *pm, void *user_data)
{
    pm_attach_scan_t *scan = (pm_attach_scan_t *)user_data;

    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_NOT_NULL(pm);

    scan->count++;
    if (strcmp(dev->name, "pm_attach_test") == 0) {
        scan->found = true;
        TEST_ASSERT_EQUAL_PTR(pm, dev->pm);
        TEST_ASSERT_EQUAL_PTR(&s_attach_pm_ops, pm->system_ops);
        TEST_ASSERT_EQUAL_PTR(&s_attach_wakeup_ops, pm->wakeup_ops);
        TEST_ASSERT_EQUAL_PTR(&s_attach_ctx, pm->ctx);
        TEST_ASSERT_EQUAL_INT(1, pm->system_ops->check_idle(pm->ctx));
    }

    return 0;
}

void test_device_pm_attach_registry_is_discoverable(void)
{
    pm_attach_scan_t scan = {0};

    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, lisa_device_init());
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, lisa_device_pm_foreach(pm_attach_scan_cb, &scan));
    TEST_ASSERT_TRUE(scan.found);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, scan.count);
    TEST_ASSERT_EQUAL_INT(1, s_attach_init_count);
    TEST_ASSERT_EQUAL_INT(0, s_attach_deinit_count);
}

void test_device_pm_attach_binds_to_device_pm_fast_path(void)
{
    lisa_device_t *dev;

    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, lisa_device_init());
    dev = lisa_device_get("pm_attach_test");
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_NOT_NULL(dev->pm);
    TEST_ASSERT_EQUAL_PTR(&s_attach_pm_ops, dev->pm->system_ops);
    TEST_ASSERT_EQUAL_PTR(&s_attach_wakeup_ops, dev->pm->wakeup_ops);
    TEST_ASSERT_TRUE(lisa_device_wakeup_is_capable(dev));
    TEST_ASSERT_EQUAL_INT32(0, dev->pm->wakeup_ops->configure(dev, 1U, 0U));
    TEST_ASSERT_EQUAL_INT32(0, dev->pm->wakeup_ops->clear(dev, 1U));
}

void run_device_pm_attach_tests(void)
{
    RUN_TEST(test_device_pm_attach_registry_is_discoverable);
    RUN_TEST(test_device_pm_attach_binds_to_device_pm_fast_path);
}
