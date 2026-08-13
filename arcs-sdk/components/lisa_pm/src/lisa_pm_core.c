/**
 * @file lisa_pm_core.c
 * @brief LISA PM 核心能力实现
 */

#include "lisa_pm_internal.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#if CONFIG_LISA_PM_STATS
int32_t lisa_pm_stats_init(void);
#endif

#ifndef CONFIG_LISA_PM_AFTER_WAKE_TASK_STACK_SIZE
#define CONFIG_LISA_PM_AFTER_WAKE_TASK_STACK_SIZE 1024
#endif

#ifndef CONFIG_LISA_PM_AFTER_WAKE_TASK_PRIORITY
#define CONFIG_LISA_PM_AFTER_WAKE_TASK_PRIORITY 5
#endif

static volatile lisa_pm_wakeup_cause_t s_wakeup_cause = LISA_PM_WAKEUP_UNKNOWN;
static bool s_initialized;
static lisa_pm_system_policy_t s_system_policy = LISA_PM_SYSTEM_POLICY_ACTIVE;
static int32_t s_system_lock_count;
static SemaphoreHandle_t s_state_lock;
static lisa_pm_sleep_callback_t s_sleep_callback;
static bool s_sleep_callback_registered;
static QueueHandle_t s_after_wake_queue;
static TaskHandle_t s_after_wake_task;
static volatile bool s_after_wake_guard_acquired;

static int32_t lisa_pm_init_state_lock(void);
static int32_t lisa_pm_state_lock_acquire(void);
static void lisa_pm_state_lock_release(void);
static int32_t lisa_pm_init_after_wake_task(void);
static void lisa_pm_after_wake_task(void *arg);

void lisa_pm_internal_update_wakeup_cause(lisa_pm_wakeup_cause_t cause)
{
    s_wakeup_cause = cause;
}

static bool lisa_pm_sleep_callback_is_valid(const lisa_pm_sleep_callback_t *callback)
{
    return callback != NULL &&
           (callback->before_sleep != NULL || callback->after_wake != NULL);
}

static int32_t lisa_pm_init_state_lock(void)
{
    if (s_state_lock != NULL) {
        return 0;
    }

    vTaskSuspendAll();
    if (s_state_lock == NULL) {
        s_state_lock = xSemaphoreCreateMutex();
    }
    (void)xTaskResumeAll();

    return (s_state_lock != NULL) ? 0 : -1;
}

static int32_t lisa_pm_state_lock_acquire(void)
{
    if (lisa_pm_init_state_lock() != 0) {
        return -1;
    }

    return (xSemaphoreTake(s_state_lock, portMAX_DELAY) == pdTRUE) ? 0 : -1;
}

static void lisa_pm_state_lock_release(void)
{
    if (s_state_lock != NULL) {
        (void)xSemaphoreGive(s_state_lock);
    }
}

int32_t lisa_pm_framework_hook_enter(uint32_t sleep_time_us, void *arg)
{
    (void)sleep_time_us;
    (void)arg;

    return 0;
}

int32_t lisa_pm_framework_hook_exit(uint32_t sleep_time_us, void *arg)
{
    (void)sleep_time_us;
    lisa_pm_internal_update_wakeup_cause(
        lisa_pm_porting_map_wakeup_cause((uint32_t)(uintptr_t)arg));

    return 0;
}

int32_t lisa_pm_framework_device_check_idle(pm_mode_t mode)
{
    (void)mode;

    return lisa_pm_devices_check_idle();
}

int32_t lisa_pm_framework_device_on_enter(uint32_t sleep_time_us, void *arg)
{
    (void)sleep_time_us;
    (void)arg;

    lisa_pm_dispatch_app_before_sleep();

    return lisa_pm_devices_prepare_suspend();
}

int32_t lisa_pm_framework_device_on_wake(uint32_t sleep_time_us, void *arg)
{
    lisa_pm_wakeup_cause_t cause;
    int32_t ret;

    (void)sleep_time_us;
    cause = lisa_pm_porting_map_wakeup_cause((uint32_t)(uintptr_t)arg);
    lisa_pm_internal_update_wakeup_cause(cause);

    ret = lisa_pm_devices_resume_restore();
    lisa_pm_dispatch_app_after_wake(cause);

    return ret;
}

int32_t lisa_pm_init(void)
{
    int32_t ret;

    if (lisa_pm_state_lock_acquire() != 0) {
        return -1;
    }

    if (s_initialized) {
        lisa_pm_state_lock_release();
        return 0;
    }

    ret = lisa_pm_porting_init();
    if (ret != 0) {
        lisa_pm_state_lock_release();
        return ret;
    }

    ret = lisa_pm_porting_apply_policy(s_system_policy);
    if (ret != 0) {
        lisa_pm_state_lock_release();
        return ret;
    }

    ret = lisa_pm_init_after_wake_task();
    if (ret != 0) {
        lisa_pm_state_lock_release();
        return ret;
    }

#if CONFIG_LISA_PM_STATS
    ret = lisa_pm_stats_init();
    if (ret != 0) {
        lisa_pm_state_lock_release();
        return ret;
    }
#endif

    ret = lisa_pm_porting_register_sleep_hooks();
    if (ret != 0 && ret != -2) {
        lisa_pm_state_lock_release();
        return ret;
    }

    ret = lisa_pm_porting_register_managed_device();
    if (ret != 0 && ret != -2) {
        (void)lisa_pm_porting_unregister_sleep_hooks();
        lisa_pm_state_lock_release();
        return ret;
    }

    ret = lisa_pm_register_discovered_devices();
    if (ret != 0 && ret != -2) {
        (void)lisa_pm_porting_unregister_managed_device();
        (void)lisa_pm_porting_unregister_sleep_hooks();
        lisa_pm_state_lock_release();
        return ret;
    }

    s_wakeup_cause = lisa_pm_porting_get_wakeup_cause();
    s_initialized = true;
    lisa_pm_state_lock_release();
    return 0;
}

lisa_pm_wakeup_cause_t lisa_pm_get_wakeup_cause(void)
{
    return s_wakeup_cause;
}

int32_t lisa_pm_set_system_policy(lisa_pm_system_policy_t policy)
{
    int32_t ret;

    if (lisa_pm_state_lock_acquire() != 0) {
        return -1;
    }

    if (s_system_policy == policy) {
        lisa_pm_state_lock_release();
        return 0;
    }

    ret = lisa_pm_porting_apply_policy(policy);
    if (ret != 0) {
        lisa_pm_state_lock_release();
        return ret;
    }

    s_system_policy = policy;
    lisa_pm_state_lock_release();
    return 0;
}

lisa_pm_system_policy_t lisa_pm_get_system_policy(void)
{
    lisa_pm_system_policy_t policy = s_system_policy;

    if (lisa_pm_state_lock_acquire() != 0) {
        return policy;
    }

    policy = s_system_policy;
    lisa_pm_state_lock_release();

    return policy;
}

int32_t lisa_pm_lock_acquire(void)
{
    int32_t ret = 0;

    if (lisa_pm_state_lock_acquire() != 0) {
        return -1;
    }

    if (s_system_lock_count == 0) {
        ret = lisa_pm_porting_acquire_lock();
        if (ret == 0) {
            s_system_lock_count = 1;
        }
    } else {
        s_system_lock_count++;
    }
    lisa_pm_state_lock_release();

    return ret;
}

int32_t lisa_pm_lock_release(void)
{
    int32_t ret = 0;

    if (lisa_pm_state_lock_acquire() != 0) {
        return -1;
    }

    if (s_system_lock_count == 0) {
        ret = -1;
    } else if (s_system_lock_count == 1) {
        ret = lisa_pm_porting_release_lock();
        if (ret == 0) {
            s_system_lock_count = 0;
        }
    } else {
        s_system_lock_count--;
    }
    lisa_pm_state_lock_release();

    return ret;
}

int32_t lisa_pm_lock_get_count(void)
{
    int32_t count = s_system_lock_count;

    if (lisa_pm_state_lock_acquire() != 0) {
        return count;
    }

    count = s_system_lock_count;
    lisa_pm_state_lock_release();

    return count;
}

bool lisa_pm_is_sleep_blocked(void)
{
    return lisa_pm_lock_get_count() > 0;
}

static int32_t lisa_pm_init_after_wake_task(void)
{
    if (s_after_wake_queue != NULL && s_after_wake_task != NULL) {
        return 0;
    }

    if (s_after_wake_queue == NULL) {
        s_after_wake_queue = xQueueCreate(1, sizeof(lisa_pm_wakeup_cause_t));
        if (s_after_wake_queue == NULL) {
            return -1;
        }
    }

    if (s_after_wake_task == NULL) {
        BaseType_t ret = xTaskCreate(lisa_pm_after_wake_task,
                                     "lisa_pm_wake",
                                     CONFIG_LISA_PM_AFTER_WAKE_TASK_STACK_SIZE,
                                     NULL,
                                     CONFIG_LISA_PM_AFTER_WAKE_TASK_PRIORITY,
                                     &s_after_wake_task);
        if (ret != pdPASS) {
            vQueueDelete(s_after_wake_queue);
            s_after_wake_queue = NULL;
            return -1;
        }
    }

    return 0;
}

static void lisa_pm_after_wake_guard_release(void)
{
    if (s_after_wake_guard_acquired) {
        s_after_wake_guard_acquired = false;
        (void)lisa_pm_porting_release_lock();
    }
}

static void lisa_pm_after_wake_task(void *arg)
{
    lisa_pm_wakeup_cause_t cause;

    (void)arg;

    for (;;) {
        if (xQueueReceive(s_after_wake_queue, &cause, portMAX_DELAY) == pdPASS) {
            /* Drop the temporary HAL guard before user code may take lisa_pm locks. */
            lisa_pm_after_wake_guard_release();
            lisa_pm_dispatch_app_after_wake_in_task(cause);
        }
    }
}

int32_t lisa_pm_sleep_callback_register(const lisa_pm_sleep_callback_t *callback)
{
    int32_t ret = 0;

    if (!lisa_pm_sleep_callback_is_valid(callback)) {
        return -1;
    }

    if (lisa_pm_state_lock_acquire() != 0) {
        return -1;
    }

    taskENTER_CRITICAL();
    if (s_sleep_callback_registered) {
        ret = -2;
    } else {
        s_sleep_callback = *callback;
        s_sleep_callback_registered = true;
    }
    taskEXIT_CRITICAL();

    lisa_pm_state_lock_release();

    return ret;
}

int32_t lisa_pm_sleep_callback_unregister(void)
{
    int32_t ret = 0;

    if (lisa_pm_state_lock_acquire() != 0) {
        return -1;
    }

    taskENTER_CRITICAL();
    if (!s_sleep_callback_registered) {
        ret = -1;
    } else {
        s_sleep_callback.before_sleep = NULL;
        s_sleep_callback.after_wake = NULL;
        s_sleep_callback.user_data = NULL;
        s_sleep_callback_registered = false;
    }
    taskEXIT_CRITICAL();

    lisa_pm_state_lock_release();

    return ret;
}

void lisa_pm_dispatch_app_before_sleep(void)
{
    lisa_pm_sleep_callback_t callback = {0};
    bool registered;

    taskENTER_CRITICAL();
    registered = s_sleep_callback_registered;
    if (registered) {
        callback = s_sleep_callback;
    }
    taskEXIT_CRITICAL();

    if (registered && callback.before_sleep != NULL) {
        callback.before_sleep(callback.user_data);
    }
}

void lisa_pm_dispatch_app_after_wake(lisa_pm_wakeup_cause_t cause)
{
    if (s_after_wake_queue == NULL) {
        return;
    }

    (void)lisa_pm_porting_acquire_lock();
    s_after_wake_guard_acquired = true;

    if (xQueueOverwrite(s_after_wake_queue, &cause) != pdPASS) {
        lisa_pm_after_wake_guard_release();
    }
}

void lisa_pm_dispatch_app_after_wake_in_task(lisa_pm_wakeup_cause_t cause)
{
    lisa_pm_sleep_callback_t callback = {0};
    bool registered;

    taskENTER_CRITICAL();
    registered = s_sleep_callback_registered;
    if (registered) {
        callback = s_sleep_callback;
    }
    taskEXIT_CRITICAL();

    if (registered && callback.after_wake != NULL) {
        callback.after_wake(callback.user_data, cause);
    }
}
