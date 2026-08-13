#ifndef LISA_PM_H
#define LISA_PM_H

/**
 * @file lisa_pm.h
 * @brief LISA 电源管理框架公共接口
 *
 * 提供系统级电源策略、睡眠锁、唤醒原因查询，以及可选的 WiFi 扩展。
 * Phase 1 额外提供面向 system PM 的设备注册接口，用于统一睡眠前检查
 * 与唤醒后恢复调度。
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 系统级电源策略
 *
 * 用于区分“系统始终保持活跃”和“允许系统在满足条件时自动进入轻睡眠”
 * 两类核心行为。
 *
 * @note 在 ARCS SoC 当前实现中，`LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP`
 *       对应用暴露自动轻睡眠语义，但底层通过深度睡眠流程模拟；
 *       睡眠期间除 AON 域外其它模块会掉电。AON 域包括 GPIOB、
 *       SRAM 前 320K、PSRAM、RTC 和 WiFi。
 */
typedef enum {
    /** 活跃策略：不允许自动进入轻睡眠 */
    LISA_PM_SYSTEM_POLICY_ACTIVE = 0,
    /** 自动轻睡策略：空闲且无阻塞条件时允许进入轻睡眠 */
    LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP,
} lisa_pm_system_policy_t;

/**
 * @brief 设置当前系统电源策略
 *
 * 新代码应优先使用此接口管理系统级睡眠准入。
 * 如果启用了 WiFi 扩展能力，WiFi 侧的省电行为应通过
 * `lisa_pm_wifi_*()` 接口单独控制。
 *
 * @param policy 目标系统电源策略
 *
 * @retval 0 成功
 * @retval <0 设置失败
 */
int32_t lisa_pm_set_system_policy(lisa_pm_system_policy_t policy);

/**
 * @brief 获取当前记录的系统电源策略
 *
 * 返回值反映的是 `lisa_pm` 当前记录的策略，而不是实时向 HAL 重新查询
 * 的结果。
 *
 * @return 当前系统电源策略
 */
lisa_pm_system_policy_t lisa_pm_get_system_policy(void);

/**
 * @brief 获取系统睡眠锁引用计数
 *
 * 返回值大于 0 表示当前至少存在一个系统睡眠锁引用，系统轻睡眠会被阻塞。
 *
 * @return 当前系统睡眠锁引用计数
 */
int32_t lisa_pm_lock_get_count(void);

/**
 * @brief 获取一个系统睡眠锁引用
 *
 * 内部采用引用计数模型：
 * - 当计数从 0 变为 1 时，底层调用 `pm_lock_acquire(PM_LOCK_APP)`
 * - 后续重复获取仅增加内部计数，不重复向 HAL 申请锁
 *
 * @retval 0 成功
 * @retval <0 获取失败
 */
int32_t lisa_pm_lock_acquire(void);

/**
 * @brief 释放一个系统睡眠锁引用
 *
 * 内部采用引用计数模型：
 * - 当计数从 1 变为 0 时，底层调用 `pm_lock_release(PM_LOCK_APP)`
 * - 中间计数递减只更新内部状态，不重复向 HAL 释放锁
 *
 * @retval 0 成功
 * @retval <0 当前没有可释放的锁，或底层释放失败
 */
int32_t lisa_pm_lock_release(void);

/**
 * @brief 查询系统睡眠是否被阻塞
 *
 * @retval true 当前至少存在一个未释放的系统睡眠锁
 * @retval false 当前没有系统睡眠锁
 */
bool lisa_pm_is_sleep_blocked(void);

#if CONFIG_LISA_PM_REMOTE_LOCK_CLIENT
/**
 * @brief 远端 AP 电源锁状态快照
 */
typedef struct {
    /** AP 当前是否由 remote lock 持有系统睡眠锁 */
    uint32_t locked;
} lisa_pm_remote_lock_state_t;

/**
 * @brief CP 侧请求 AP 持有系统睡眠锁
 *
 * 该接口通过平台 remote lock transport 访问 AP 侧 lisa_pm remote lock
 * 服务。ARCS 当前实现使用 MRPC，AP 处于 pm_light_sleep/WFI_DEEP_SLEEP
 * 时可通过 MRPC 唤醒路径先唤醒 AP 再投递请求。
 *
 * @retval 0 成功
 * @retval <0 发送失败或 AP 侧处理失败
 */
int32_t lisa_pm_remote_lock_acquire(void);

/**
 * @brief CP 侧请求 AP 释放由 remote lock 持有的系统睡眠锁
 *
 * @retval 0 成功
 * @retval <0 发送失败或 AP 侧处理失败
 */
int32_t lisa_pm_remote_lock_release(void);

/**
 * @brief CP 侧读取 AP remote lock 状态
 *
 * 主要用于示例和调试验证。
 *
 * @param state 输出状态快照
 *
 * @retval 0 成功
 * @retval <0 发送失败、参数错误或 AP 侧处理失败
 */
int32_t lisa_pm_remote_lock_get_state(lisa_pm_remote_lock_state_t *state);
#endif

/**
 * @brief 归一化后的唤醒原因
 */
typedef enum {
    /** 定时器唤醒 */
    LISA_PM_WAKEUP_TIMER = 0,
    /** RTC 唤醒 */
    LISA_PM_WAKEUP_RTC,
    /** 蓝牙相关事件唤醒 */
    LISA_PM_WAKEUP_BT,
    /** WiFi 相关事件唤醒 */
    LISA_PM_WAKEUP_WIFI,
    /** GPIO 唤醒 */
    LISA_PM_WAKEUP_GPIO,
    /** 未知唤醒原因或尚未记录 */
    LISA_PM_WAKEUP_UNKNOWN,
} lisa_pm_wakeup_cause_t;

/**
 * @brief System PM 设备回调集合
 *
 * system_ops 描述“设备如何配合一次系统睡眠”。lisa_pm 在每轮系统
 * 睡眠决策和睡眠/唤醒关键路径中调用这些回调：
 * - check_idle: 查询设备当前是否允许系统睡眠
 * - prepare_suspend: 系统即将睡眠前，执行快速硬件挂起动作
 * - resume_restore: 系统唤醒后，执行快速硬件基础恢复动作
 *
 * system_ops 不负责配置唤醒源；唤醒源启停由 lisa_device 的
 * wakeup_ops 和 lisa_device_wakeup_enable() 负责。
 */
typedef struct {
    int32_t (*check_idle)(void *ctx);
    int32_t (*prepare_suspend)(void *ctx);
    int32_t (*resume_restore)(void *ctx);
} lisa_pm_system_ops_t;

/**
 * @brief LISA PM 设备描述对象
 *
 * 该对象用于 system PM 设备注册：
 * - `check_idle()` 决定当前设备是否允许系统进入睡眠
 * - `prepare_suspend()` 在真正进入睡眠前执行
 * - `resume_restore()` 在唤醒后执行硬件基础恢复
 */
typedef struct lisa_pm_device {
    const char *name;
    void *ctx;

    const lisa_pm_system_ops_t *system_ops;
} lisa_pm_device_t;

/**
 * @brief 应用级自动睡眠回调集合
 *
 * 该回调用于应用在自动轻睡眠真正发生前后执行板级或业务操作。
 *
 * @note `before_sleep` 处于 PM enter 关键路径，应保持短小、确定、
 *       不可阻塞，不应等待 mutex / event / 任务调度，不应调用文件系统、
 *       网络或大块动态内存分配等可能阻塞的 API。
 *       `after_wake` 会被延后到 lisa_pm 内部任务中执行，已经离开 PM
 *       exit 关键路径，可调用普通任务上下文允许的业务 API。
 */
typedef struct {
    /** 睡眠前回调，可为 NULL */
    void (*before_sleep)(void *user_data);
    /** 唤醒后回调，可为 NULL；在 lisa_pm 内部任务上下文执行 */
    void (*after_wake)(void *user_data, lisa_pm_wakeup_cause_t cause);
    /** 透传给回调的用户上下文 */
    void *user_data;
} lisa_pm_sleep_callback_t;

/**
 * @brief 注册应用级自动睡眠回调
 *
 * 当前仅支持单实例注册；注册时会拷贝 `callback` 描述对象本身，
 * 但不会拷贝 `user_data` 指向的用户数据。
 *
 * @param callback 回调描述对象；`before_sleep` 和 `after_wake` 不能同时为 NULL
 *
 * @retval 0 注册成功
 * @retval -1 参数错误
 * @retval -2 已存在已注册的应用级睡眠回调
 */
int32_t lisa_pm_sleep_callback_register(const lisa_pm_sleep_callback_t *callback);

/**
 * @brief 注销应用级自动睡眠回调
 *
 * @retval 0 注销成功
 * @retval -1 当前没有已注册的应用级睡眠回调
 */
int32_t lisa_pm_sleep_callback_unregister(void);

/**
 * @brief 初始化 LISA PM 框架
 *
 * 该接口负责底层 HAL PM 初始化，以及 phase-1 system PM 所需的 framework
 * hook / 设备调度注册。
 *
 * 在 arcs SoC 上，本函数内部还会自动调用 `vrtc_init()` 启动 AON timer，
 * 应用层无需也不应该再显式调用 `vrtc_init()`。本函数 idempotent，重复
 * 调用安全。
 *
 * @retval 0 成功
 * @retval <0 初始化失败
 */
int32_t lisa_pm_init(void);

#if CONFIG_LISA_PM_WIFI
/**
 * @brief WiFi 省电模式
 *
 * `LISA_PM_WIFI_PS_DTIM` 仅作为模式标识，不携带额外参数。
 * `LISA_PM_WIFI_PS_LISTEN` 需要通过
 * `lisa_pm_wifi_ps_config_t::listen_interval` 指定监听周期。
 */
typedef enum {
    /** 关闭 WiFi 省电，保持链路活跃 */
    LISA_PM_WIFI_PS_OFF = 0,
    /** DTIM 省电模式 */
    LISA_PM_WIFI_PS_DTIM,
    /** LISTEN 省电模式 */
    LISA_PM_WIFI_PS_LISTEN,
} lisa_pm_wifi_ps_mode_t;

/**
 * @brief WiFi 省电配置
 *
 * 当前仅 `LISA_PM_WIFI_PS_LISTEN` 模式使用该配置，其它模式可传 `NULL`。
 */
typedef struct {
    /** LISTEN 模式下的监听周期，单位为 beacon interval */
    uint16_t listen_interval;
} lisa_pm_wifi_ps_config_t;

/**
 * @brief 设置 WiFi 省电模式
 *
 * @param mode 目标 WiFi 省电模式
 * @param config LISTEN 模式配置；其它模式可传 `NULL`
 *
 * @retval 0 成功
 * @retval <0 参数错误或底层设置失败
 *
 * @note 对当前 WiFi 库而言，LISTEN 模式相关配置必须在 WiFi 发起连接前完成，
 *       否则 `listen_interval` 不会生效。
 */
int32_t lisa_pm_wifi_set_ps_mode(lisa_pm_wifi_ps_mode_t mode,
                                 const lisa_pm_wifi_ps_config_t *config);

/**
 * @brief 获取当前记录的 WiFi 省电模式
 *
 * 返回值反映的是 `lisa_pm` 当前记录的模式，而不是实时向 HAL 重新查询结果。
 *
 * @return 当前 WiFi 省电模式
 */
lisa_pm_wifi_ps_mode_t lisa_pm_wifi_get_ps_mode(void);

/**
 * @brief 获取一个 WiFi 省电锁引用
 *
 * 内部采用引用计数模型：
 * - 当计数从 0 变为 1 时，底层调用 WiFi 省电锁接口
 * - 后续重复获取仅增加内部计数
 *
 * @retval 0 成功
 * @retval <0 获取失败
 */
int32_t lisa_pm_wifi_lock_acquire(void);

/**
 * @brief 释放一个 WiFi 省电锁引用
 *
 * 内部采用引用计数模型：
 * - 当计数从 1 变为 0 时，底层调用 WiFi 省电解锁接口
 * - 中间计数递减只更新内部状态
 *
 * @retval 0 成功
 * @retval <0 当前没有可释放的锁，或底层释放失败
 */
int32_t lisa_pm_wifi_lock_release(void);

/**
 * @brief 获取 WiFi 省电锁引用计数
 *
 * 返回值大于 0 表示当前 WiFi 省电被阻塞。
 *
 * @return 当前 WiFi 省电锁引用计数
 */
int32_t lisa_pm_wifi_lock_get_count(void);

/**
 * @brief 查询 WiFi 省电是否被阻塞
 *
 * @retval true 当前至少存在一个未释放的 WiFi 省电锁
 * @retval false 当前没有 WiFi 省电锁
 */
bool lisa_pm_wifi_is_power_save_blocked(void);
#endif

/**
 * @brief 获取最近一次归一化后的唤醒原因
 *
 * @return 最近一次唤醒原因；若未知则返回 `LISA_PM_WAKEUP_UNKNOWN`
 */
lisa_pm_wakeup_cause_t lisa_pm_get_wakeup_cause(void);

#if CONFIG_LISA_PM_STATS
/**
 * @brief 睡眠统计快照
 */
typedef struct {
    /** 成功进入睡眠的次数 */
    uint32_t sleep_count;
    /** 睡眠尝试后放弃的次数 */
    uint32_t sleep_abort_count;
    /** 累计睡眠时长，单位微秒 */
    uint64_t total_sleep_us;
    /** 累计活跃时长，单位微秒 */
    uint64_t total_active_us;
    /** 最近一次睡眠时长，单位微秒 */
    uint32_t last_sleep_us;
    /** 单次最大睡眠时长，单位微秒 */
    uint32_t max_sleep_us;
    /** 各唤醒原因对应的计数 */
    uint32_t wakeup_cause_count[LISA_PM_WAKEUP_UNKNOWN + 1];
} lisa_pm_stats_t;

/**
 * @brief 获取当前睡眠统计快照
 *
 * @param stats 输出统计结构体指针
 *
 * @retval 0 成功
 * @retval <0 获取失败
 */
int32_t lisa_pm_get_stats(lisa_pm_stats_t *stats);

/**
 * @brief 重置当前睡眠统计
 *
 * @retval 0 成功
 * @retval <0 重置失败
 */
int32_t lisa_pm_reset_stats(void);

/**
 * @brief 获取当前睡眠占比
 *
 * 返回值以万分比表示，例如 5234 表示 52.34%。
 *
 * @return 当前睡眠占比（万分比）
 */
uint32_t lisa_pm_get_sleep_ratio(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
