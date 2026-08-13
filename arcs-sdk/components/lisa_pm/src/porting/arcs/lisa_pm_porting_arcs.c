/**
 * @file lisa_pm_porting_arcs.c
 * @brief LISA PM ARCS porting implementation
 */

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>

#include "PowerManager.h"
#include "vrtc.h"
#include "../../lisa_pm_internal.h"
#include "../../lisa_pm_porting.h"

#if CONFIG_LISA_PM_REMOTE_LOCK_SERVER
int32_t lisa_pm_porting_arcs_remote_lock_server_init(void);
#endif
#if CONFIG_LISA_PM_REMOTE_LOCK_CLIENT
int32_t lisa_pm_porting_arcs_remote_lock_acquire(void);
int32_t lisa_pm_porting_arcs_remote_lock_release(void);
int32_t lisa_pm_porting_arcs_remote_lock_get_state(lisa_pm_remote_lock_state_t *state);
#endif

#if CONFIG_MEM_ILM_SIZE > 0

#if CONFIG_LISA_PM_SNAPSHOT_DEFAULT_ILM_DLM
extern char _itcm_code_start[];
extern char _itcm_code_end[];
extern char _dtcm_data_start[];
extern char _dtcm_bss_end[];
#endif

#define LISA_PM_ARCS_SRAM_SNAPSHOT_START 0x20050000U
#define LISA_PM_ARCS_SRAM_SNAPSHOT_END   0x200BFFFFU

static void lisa_pm_porting_arcs_register_snapshot_region(uintptr_t start, uintptr_t end)
{
    if (end <= start) {
        return;
    }

    int32_t ret = pm_register_snapshot_region(
        (uint32_t)start, (uint32_t)(end - start),
        PM_SNAPSHOT_REGION_VALID | PM_SNAPSHOT_REGION_RESTORE_EN);
    /* 默认 snapshot 区域是 SoC 基础恢复路径，注册失败属于配置 bug：
     * PSRAM 快照区误配 / 槽位被异常占用 / region 重叠等，
     * 静默吞错会让深度睡眠唤醒后内存损坏，必须早暴露。 */
    assert(ret == PM_SNAPSHOT_OK);
    (void)ret;  /* NDEBUG 下 assert 会被编出去，避免 unused 警告 */
}

static void lisa_pm_porting_arcs_register_default_snapshots(void)
{
#if CONFIG_LISA_PM_SNAPSHOT_DEFAULT_ILM_DLM
    lisa_pm_porting_arcs_register_snapshot_region((uintptr_t)_itcm_code_start,
                                                  (uintptr_t)_itcm_code_end);
    lisa_pm_porting_arcs_register_snapshot_region((uintptr_t)_dtcm_data_start,
                                                  (uintptr_t)_dtcm_bss_end);
#endif

#if CONFIG_LISA_PM_SNAPSHOT_DEFAULT_SRAM
    lisa_pm_porting_arcs_register_snapshot_region(LISA_PM_ARCS_SRAM_SNAPSHOT_START,
                                                  LISA_PM_ARCS_SRAM_SNAPSHOT_END);
#endif
}

#endif

/*
 * 注意: AON wakeup ISR (IP_AON_CTRL->REG_WAKEUP_ISR) 用 HAL 的 PMU_WAKEUP_*
 * 位编码 (PowerManager.h), 与 lisa_pm 自己的 PM_WAKEUP_* (pm.h, 仅用于
 * sleep_cfg.wakeup_src_mask 配置) 是两个独立 namespace, 位值不同。
 * 归一化匹配必须用 PMU_WAKEUP_*, 否则永远 miss → 返回 UNKNOWN。
 * GPIO 在 AON ISR 中占用 PMU_WAKEUP_GPIOB_00..09 共 10 个位, 用一个掩码覆盖。
 */
static const struct {
    uint32_t mask;
    lisa_pm_wakeup_cause_t cause;
} s_arcs_wakeup_map[] = {
    {1U << PMU_WAKEUP_WIFI,            LISA_PM_WAKEUP_WIFI},
    {0x3FFU << PMU_WAKEUP_GPIOB_00,    LISA_PM_WAKEUP_GPIO},
    {1U << PMU_WAKEUP_TIMER,           LISA_PM_WAKEUP_TIMER},
    {1U << PMU_WAKEUP_RTC,             LISA_PM_WAKEUP_RTC},
    {1U << PMU_WAKEUP_BT,              LISA_PM_WAKEUP_BT},
};

static lisa_pm_wakeup_cause_t lisa_pm_porting_arcs_map_wakeup_cause(uint32_t cause)
{
    for (size_t i = 0; i < sizeof(s_arcs_wakeup_map) / sizeof(s_arcs_wakeup_map[0]); ++i) {
        if (cause & s_arcs_wakeup_map[i].mask) {
            return s_arcs_wakeup_map[i].cause;
        }
    }

    return LISA_PM_WAKEUP_UNKNOWN;
}
static int32_t lisa_pm_porting_arcs_apply_policy(lisa_pm_system_policy_t policy)
{
    pm_config_t config = {
        .clock_level = PM_CLOCK_LEVEL0,
    };

    switch (policy) {
    case LISA_PM_SYSTEM_POLICY_ACTIVE:
        config.mode = PM_MODE_ACTIVE;
        break;
    case LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP:
        config.mode = PM_MODE_LIGHT_SLEEP;
        break;
    default:
        return -1;
    }

    return pm_set_config(&config);
}

static int32_t lisa_pm_porting_arcs_init(void)
{
    int32_t ret;

    /* vrtc 启动 AON timer 并初始化 vrtc_reg；pm_impl 在 light_sleep 进出时
     * 调用 vrtc_get_time_us() / vrtc_is_allow_sleep()，没有 vrtc_init 会
     * NULL 解引用崩溃。lisa_pm_init 是 idempotent 入口，因此这里只在第一
     * 次进 porting init 时跑 vrtc_init；外部已经调过 vrtc_init 的 SoC HAL
     * demo 路径不会经过 lisa_pm，不会双初始化。 */
    static bool s_vrtc_initialized = false;
    if (!s_vrtc_initialized) {
        int32_t vret = vrtc_init();
        if (vret != 0) {
            return vret;
        }
        s_vrtc_initialized = true;
    }

    ret = pm_init();
    if (ret != 0) {
        return ret;
    }

    lisa_pm_porting_arcs_register_default_snapshots();

#if CONFIG_LISA_PM_REMOTE_LOCK_SERVER
    ret = lisa_pm_porting_arcs_remote_lock_server_init();
    if (ret != 0) {
        return ret;
    }
#endif

    return ret;
}

static int32_t lisa_pm_porting_arcs_acquire_lock(void)
{
    return pm_lock_acquire(PM_LOCK_APP);
}

static int32_t lisa_pm_porting_arcs_release_lock(void)
{
    return pm_lock_release(PM_LOCK_APP);
}

static int32_t lisa_pm_porting_arcs_register_sleep_hooks(void)
{
    return pm_hook_register(PM_HOOK_ID_1,
                            lisa_pm_framework_hook_enter,
                            lisa_pm_framework_hook_exit);
}

static int32_t lisa_pm_porting_arcs_unregister_sleep_hooks(void)
{
    return pm_hook_unregister(PM_HOOK_ID_1);
}

static int32_t lisa_pm_porting_arcs_register_managed_device(void)
{
    pm_handler_ops_t ops = {
        .check_idle = lisa_pm_framework_device_check_idle,
        .on_enter = lisa_pm_framework_device_on_enter,
        .on_exit = lisa_pm_framework_device_on_wake,
        .on_wake = NULL,
    };

    return pm_device_register(PM_DEV_ID_UART, &ops);
}

static int32_t lisa_pm_porting_arcs_unregister_managed_device(void)
{
    return pm_device_unregister(PM_DEV_ID_UART);
}

static lisa_pm_wakeup_cause_t lisa_pm_porting_arcs_get_wakeup_cause(void)
{
    return LISA_PM_WAKEUP_UNKNOWN;
}

const lisa_pm_porting_ops_t g_lisa_pm_porting_ops = {
    .init = lisa_pm_porting_arcs_init,
    .apply_policy = lisa_pm_porting_arcs_apply_policy,
    .acquire_lock = lisa_pm_porting_arcs_acquire_lock,
    .release_lock = lisa_pm_porting_arcs_release_lock,
    .register_sleep_hooks = lisa_pm_porting_arcs_register_sleep_hooks,
    .unregister_sleep_hooks = lisa_pm_porting_arcs_unregister_sleep_hooks,
    .register_managed_device = lisa_pm_porting_arcs_register_managed_device,
    .unregister_managed_device = lisa_pm_porting_arcs_unregister_managed_device,
    .get_wakeup_cause = lisa_pm_porting_arcs_get_wakeup_cause,
    .map_wakeup_cause = lisa_pm_porting_arcs_map_wakeup_cause,
#if CONFIG_LISA_PM_REMOTE_LOCK_CLIENT
    .remote_lock_acquire = lisa_pm_porting_arcs_remote_lock_acquire,
    .remote_lock_release = lisa_pm_porting_arcs_remote_lock_release,
    .remote_lock_get_state = lisa_pm_porting_arcs_remote_lock_get_state,
#endif
};
