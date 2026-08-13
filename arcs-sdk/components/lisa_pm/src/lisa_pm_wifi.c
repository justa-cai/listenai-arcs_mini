/**
 * @file lisa_pm_wifi.c
 * @brief LISA PM WiFi 扩展能力实现
 */

#include "lisa_pm.h"

#if CONFIG_LISA_PM_WIFI

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "wifi_api.h"

#define LISA_PM_WIFI_LISTEN_INTERVAL_DEFAULT 10U
#define LISA_PM_WIFI_LISTEN_INTERVAL_MIN     1U
#define LISA_PM_WIFI_LISTEN_INTERVAL_MAX     19U

static lisa_pm_wifi_ps_mode_t s_wifi_ps_mode = LISA_PM_WIFI_PS_OFF;
static int32_t s_wifi_lock_count;
static SemaphoreHandle_t s_wifi_state_lock;

/**
 * @brief 初始化 WiFi 扩展内部状态锁
 *
 * @retval 0 成功
 * @retval <0 初始化失败
 */
static int32_t lisa_pm_wifi_init_state_lock(void)
{
    if (s_wifi_state_lock != NULL) {
        return 0;
    }

    vTaskSuspendAll();
    if (s_wifi_state_lock == NULL) {
        s_wifi_state_lock = xSemaphoreCreateMutex();
    }
    (void)xTaskResumeAll();

    return (s_wifi_state_lock != NULL) ? 0 : -1;
}

/**
 * @brief 获取 WiFi 扩展内部状态锁
 *
 * @retval 0 成功
 * @retval <0 获取失败
 */
static int32_t lisa_pm_wifi_state_lock_acquire(void)
{
    if (lisa_pm_wifi_init_state_lock() != 0) {
        return -1;
    }

    return (xSemaphoreTake(s_wifi_state_lock, portMAX_DELAY) == pdTRUE) ? 0 : -1;
}

/**
 * @brief 释放 WiFi 扩展内部状态锁
 */
static void lisa_pm_wifi_state_lock_release(void)
{
    if (s_wifi_state_lock != NULL) {
        (void)xSemaphoreGive(s_wifi_state_lock);
    }
}

/**
 * @brief 校验 LISTEN 模式监听周期是否合法
 *
 * @param interval 监听周期，单位 beacon interval
 *
 * @retval 0 合法
 * @retval <0 非法
 */
static int32_t lisa_pm_wifi_validate_listen_interval(uint16_t interval)
{
    if ((interval < LISA_PM_WIFI_LISTEN_INTERVAL_MIN) ||
        (interval > LISA_PM_WIFI_LISTEN_INTERVAL_MAX)) {
        return -1;
    }

    return 0;
}

/**
 * @brief 设置 WiFi 省电模式
 *
 * @param mode 目标省电模式
 * @param config LISTEN 模式配置；其它模式可传 `NULL`
 *
 * @retval 0 成功
 * @retval <0 参数错误或底层设置失败
 */
int32_t lisa_pm_wifi_set_ps_mode(lisa_pm_wifi_ps_mode_t mode,
                                 const lisa_pm_wifi_ps_config_t *config)
{
    int32_t ret = 0;

    if (lisa_pm_wifi_state_lock_acquire() != 0) {
        return -1;
    }

    switch (mode) {
    case LISA_PM_WIFI_PS_OFF:
        ret = wifi_ps_mode_set(WIFI_PS_MODE_OFF);
        break;
    case LISA_PM_WIFI_PS_DTIM:
        ret = wifi_ps_mode_set(WIFI_PS_MODE_DTIM);
        break;
    case LISA_PM_WIFI_PS_LISTEN: {
        uint16_t listen_interval = LISA_PM_WIFI_LISTEN_INTERVAL_DEFAULT;

        if (config != NULL) {
            listen_interval = config->listen_interval;
        }

        if (lisa_pm_wifi_validate_listen_interval(listen_interval) != 0) {
            ret = -1;
            break;
        }

        ret = wifi_sta_set_listen_itv((uint8_t)listen_interval);
        if (ret != 0) {
            break;
        }

        ret = wifi_ps_mode_set(WIFI_PS_MODE_LISTEN);
        break;
    }
    default:
        ret = -1;
        break;
    }

    if (ret == 0) {
        s_wifi_ps_mode = mode;
    }

    lisa_pm_wifi_state_lock_release();
    return ret;
}

/**
 * @brief 获取当前记录的 WiFi 省电模式
 *
 * @return 当前记录的 WiFi 省电模式
 */
lisa_pm_wifi_ps_mode_t lisa_pm_wifi_get_ps_mode(void)
{
    lisa_pm_wifi_ps_mode_t mode = s_wifi_ps_mode;

    if (lisa_pm_wifi_state_lock_acquire() != 0) {
        return mode;
    }

    mode = s_wifi_ps_mode;
    lisa_pm_wifi_state_lock_release();

    return mode;
}

/**
 * @brief 获取一个 WiFi 省电锁引用
 *
 * @retval 0 成功
 * @retval <0 获取失败
 */
int32_t lisa_pm_wifi_lock_acquire(void)
{
    int32_t ret = 0;

    if (lisa_pm_wifi_state_lock_acquire() != 0) {
        return -1;
    }

    if (s_wifi_lock_count == 0) {
        ret = wifi_ps_lock_acquire(WIFI_PS_LOCK_BIT_APP, false);
        if (ret == 0) {
            s_wifi_lock_count = 1;
        }
    } else {
        s_wifi_lock_count++;
    }

    lisa_pm_wifi_state_lock_release();
    return ret;
}

/**
 * @brief 释放一个 WiFi 省电锁引用
 *
 * @retval 0 成功
 * @retval <0 释放失败
 */
int32_t lisa_pm_wifi_lock_release(void)
{
    int32_t ret = 0;

    if (lisa_pm_wifi_state_lock_acquire() != 0) {
        return -1;
    }

    if (s_wifi_lock_count == 0) {
        ret = -1;
    } else if (s_wifi_lock_count == 1) {
        ret = wifi_ps_lock_release(WIFI_PS_LOCK_BIT_APP);
        if (ret == 0) {
            s_wifi_lock_count = 0;
        }
    } else {
        s_wifi_lock_count--;
    }

    lisa_pm_wifi_state_lock_release();
    return ret;
}

/**
 * @brief 获取当前 WiFi 省电锁引用计数
 *
 * @return 当前 WiFi 省电锁计数
 */
int32_t lisa_pm_wifi_lock_get_count(void)
{
    int32_t count = s_wifi_lock_count;

    if (lisa_pm_wifi_state_lock_acquire() != 0) {
        return count;
    }

    count = s_wifi_lock_count;
    lisa_pm_wifi_state_lock_release();

    return count;
}

/**
 * @brief 查询 WiFi 省电是否被阻塞
 *
 * @retval true 当前 WiFi 省电被阻塞
 * @retval false 当前 WiFi 省电未被阻塞
 */
bool lisa_pm_wifi_is_power_save_blocked(void)
{
    return lisa_pm_wifi_lock_get_count() > 0;
}

#endif
