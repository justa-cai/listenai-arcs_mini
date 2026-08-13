/**
 * @file lisa_pm_remote_lock_server_arcs.c
 * @brief ARCS MRPC implementation for AP-side LISA PM remote lock service.
 */

#define LOG_TAG "lisa_pm_rlock"
#include <lisa_log.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lisa_pm.h"
#include "mrpc_ap_pm_lock_api_server.h"

#ifndef LISA_PM_REMOTE_LOCK_SERVER_USE_LOCK
#define LISA_PM_REMOTE_LOCK_SERVER_USE_LOCK 0
#endif

#if LISA_PM_REMOTE_LOCK_SERVER_USE_LOCK
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static SemaphoreHandle_t s_remote_lock_mutex;
#endif

static bool s_remote_lock_initialized;
static bool s_remote_locked;
static uint32_t s_remote_acquire_count;
static uint32_t s_remote_release_count;

#if LISA_PM_REMOTE_LOCK_SERVER_USE_LOCK
static int32_t lisa_pm_remote_lock_take(void)
{
    if (s_remote_lock_mutex == NULL) {
        return -1;
    }

    return (xSemaphoreTake(s_remote_lock_mutex, portMAX_DELAY) == pdTRUE) ? 0 : -1;
}

static void lisa_pm_remote_lock_give(void)
{
    if (s_remote_lock_mutex != NULL) {
        (void)xSemaphoreGive(s_remote_lock_mutex);
    }
}
#else
static int32_t lisa_pm_remote_lock_take(void)
{
    return 0;
}

static void lisa_pm_remote_lock_give(void)
{
}
#endif

int32_t lisa_pm_porting_arcs_remote_lock_server_init(void)
{
    if (s_remote_lock_initialized) {
        return 0;
    }

#if LISA_PM_REMOTE_LOCK_SERVER_USE_LOCK
    vTaskSuspendAll();
    if (s_remote_lock_mutex == NULL) {
        s_remote_lock_mutex = xSemaphoreCreateMutex();
    }
    (void)xTaskResumeAll();

    if (s_remote_lock_mutex == NULL) {
        LOGE("remote lock mutex create failed");
        return -1;
    }
#endif

    s_remote_lock_initialized = true;
    LOGI("remote lock server initialized");
    return 0;
}

int32_t ap_pm_remote_lock_server_acquire(void)
{
    int32_t ret = 0;

    if (lisa_pm_remote_lock_take() != 0) {
        return -1;
    }

    if (!s_remote_locked) {
        ret = lisa_pm_lock_acquire();
        if (ret != 0) {
            lisa_pm_remote_lock_give();
            LOGE("remote lock acquire failed: %ld", (long)ret);
            return ret;
        }
        s_remote_locked = true;
        LOGI("remote lock acquired");
    }

    s_remote_acquire_count++;
    LOGI("remote lock acquire count=%lu release=%lu",
         (unsigned long)s_remote_acquire_count,
         (unsigned long)s_remote_release_count);

    lisa_pm_remote_lock_give();
    return 0;
}

int32_t ap_pm_remote_lock_server_release(void)
{
    int32_t ret = 0;

    if (lisa_pm_remote_lock_take() != 0) {
        return -1;
    }

    if (s_remote_locked) {
        ret = lisa_pm_lock_release();
        if (ret != 0) {
            lisa_pm_remote_lock_give();
            LOGE("remote lock release failed: %ld", (long)ret);
            return ret;
        }
        s_remote_locked = false;
        LOGI("remote lock released");
    }

    s_remote_release_count++;
    LOGI("remote lock release count=%lu acquire=%lu",
         (unsigned long)s_remote_release_count,
         (unsigned long)s_remote_acquire_count);

    lisa_pm_remote_lock_give();
    return 0;
}

int32_t ap_pm_remote_lock_server_get_state(uint32_t *locked)
{
    if (locked == NULL) {
        return -1;
    }

    if (lisa_pm_remote_lock_take() != 0) {
        return -1;
    }

    *locked = s_remote_locked ? 1U : 0U;

    lisa_pm_remote_lock_give();
    return 0;
}
