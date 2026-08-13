# LISA PM 组件

LISA PM 是基于 ARCS HAL PM 的系统电源管理组件，负责把应用侧的电源策略、系统睡眠锁、设备 system PM、唤醒原因、睡眠统计、WiFi 省电协同和 AP/CP 双核协同统一到一组 `lisa_pm_*()` 接口中。

组件由公共接口、核心调度、设备 PM 分发、ARCS SoC 移植层和可选扩展组成，可用于应用侧自动轻睡眠、外设低功耗恢复、WiFi 省电协同以及 AP/CP 双核运行模式。

## 功能特性

- **系统策略**：通过 `lisa_pm_set_system_policy()` 在 `ACTIVE` 和 `AUTO_LIGHT_SLEEP` 之间切换。
- **系统睡眠锁**：`lisa_pm_lock_acquire()` / `lisa_pm_lock_release()` 使用引用计数封装 HAL `PM_LOCK_APP`。
- **设备 system PM**：自动发现 `lisa_device` 中挂载的 `lisa_pm_system_ops_t`，统一执行 idle 检查、睡眠前挂起和唤醒后恢复。
- **应用睡眠回调**：支持单实例 `before_sleep` / `after_wake`，其中 `after_wake` 延后到 `lisa_pm` 内部任务执行。
- **唤醒原因和统计**：把 ARCS AON `PMU_WAKEUP_*` 映射为 `lisa_pm_wakeup_cause_t`，可选统计睡眠次数、时长和占比。
- **WiFi 省电协同**：在 `CONFIG_LISA_PM_WIFI=y` 时提供 WiFi PS 模式和 WiFi PS 锁接口。
- **双核运行模式**：AP/CP 两侧均运行固件并初始化 `lisa_pm`，HAL PM 通过 AMP shared memory 协同进入和退出 light sleep。
- **远端 AP 睡眠锁**：在 CP 侧可通过 remote lock MRPC 请求 AP 持有或释放系统睡眠锁。

## 文件说明

| 路径 | 说明 |
|---|---|
| `components/lisa_pm/include/lisa_pm.h` | 对应用和驱动开放的 LISA PM 公共接口 |
| `components/lisa_pm/src/lisa_pm_core.c` | 系统策略、系统锁、初始化、应用睡眠回调和 after-wake 任务 |
| `components/lisa_pm/src/lisa_pm_device.c` | system PM 设备发现、复制注册和回调分发 |
| `components/lisa_pm/src/lisa_pm_stats.c` | 睡眠统计 hook 实现 |
| `components/lisa_pm/src/lisa_pm_wifi.c` | WiFi PS 模式和 WiFi PS 锁封装 |
| `components/lisa_pm/src/lisa_pm_remote_lock.c` | remote lock 公共 wrapper |
| `components/lisa_pm/src/porting/arcs/` | ARCS SoC porting、snapshot 默认区和 remote lock MRPC 适配 |
| `soc/arcs/hal/chip/arcs/pm_impl/` | 底层 HAL PM 实现，包含 `pm_can_sleep()`、`pm_light_sleep()` 和双核共享状态 |
| `drivers/lisa_device/lisa_device.h` | 设备 PM 描述、wakeup-source vtable 和 PM 版本注册宏 |

## 配置选项

启用组件：

```kconfig
CONFIG_LISA_PM=y
```

主要配置：

| 配置 | 说明 |
|---|---|
| `CONFIG_LISA_PM` | 启用 LISA PM；自动选择 `CONFIG_ARCS_HAL_PM` 和 `CONFIG_ARCS_HAL_VRTC` |
| `CONFIG_LISA_PM_CORE_PRIMARY` | 指定本固件在 HAL PM 中是否是 primary 核；CP 默认 `y`，AP 默认 `n` |
| `CONFIG_LISA_PM_DUAL_CORE` | CP 侧声明 AP 是运行中的 PM 对端核心；打开后不会选择 `CONFIG_ARCS_HAL_PM_CLOSE_AP` |
| `CONFIG_LISA_PM_STATS` | 启用睡眠统计，默认 `y` |
| `CONFIG_LISA_PM_SYSTEM_DEVICE_MAX` | `lisa_pm` 内部可复制注册的 system PM 设备数量，默认 16 |
| `CONFIG_LISA_PM_AFTER_WAKE_TASK_STACK_SIZE` | `after_wake` 内部任务栈大小，默认 2048 |
| `CONFIG_LISA_PM_AFTER_WAKE_TASK_PRIORITY` | `after_wake` 内部任务优先级，默认 5 |
| `CONFIG_LISA_PM_WIFI` | 启用 WiFi 省电扩展；依赖 `CONFIG_LISA_WIFI`，并选择 `CONFIG_ARCS_HAL_PM_KEEP_ALIVE` |
| `CONFIG_LISA_PM_REMOTE_LOCK_CLIENT` | CP 侧 remote lock client，使用 AP PM lock MRPC 服务 |
| `CONFIG_LISA_PM_REMOTE_LOCK_SERVER` | AP 侧 remote lock server，把远端请求映射为本地 `lisa_pm_lock_*()` |
| `CONFIG_LISA_PM_DEVICE_RUNTIME` | 预留的 Runtime PM 开关；当前 system PM 不依赖它 |
| `CONFIG_LISA_PM_SNAPSHOT_DEFAULT_ILM_DLM` | 存在 ILM 时，`lisa_pm_init()` 自动注册 ILM/DLM snapshot 区，默认 `y` |
| `CONFIG_LISA_PM_SNAPSHOT_DEFAULT_SRAM` | 存在 ILM 时，`lisa_pm_init()` 自动注册 ARCS 非 AON SRAM snapshot 区；按需显式打开 |

双核示例中的典型配置：

```ini
# AP remote 固件
CONFIG_ARCS_AP_CORE=y
CONFIG_ARCS_HAL_IPC=y
CONFIG_LISA_PM=y
CONFIG_LISA_PM_CORE_PRIMARY=n

# CP 主固件
CONFIG_ARCS_CP_CORE=y
CONFIG_ARCS_HAL_IPC=y
CONFIG_LISA_PM=y
CONFIG_LISA_PM_CORE_PRIMARY=y
CONFIG_LISA_PM_DUAL_CORE=y
```

`components/lisa_pm/CMakeLists.txt` 会根据 `CONFIG_LISA_PM_CORE_PRIMARY` 导出：

| 角色 | 编译定义 |
|---|---|
| primary 核 | `PM_CORE_PRIMARY=1`、`CFG_VRTC=1` |
| 非 primary 核 | `PM_CORE_PRIMARY=0`、`CFG_VRTC_PROXY=1`；必须启用 `CONFIG_ARCS_HAL_IPC` |

## API 接口

### 初始化

```c
int32_t lisa_pm_init(void);
```

`lisa_pm_init()` 是幂等入口。当前 ARCS porting 的主要初始化顺序如下：

1. 创建并获取 `lisa_pm` 内部状态锁。
2. 调用 `vrtc_init()`，再调用 `pm_init()`。
3. 存在 ILM 时按配置注册默认 snapshot 区域。
4. 将当前系统策略下发给 HAL PM；默认策略是 `LISA_PM_SYSTEM_POLICY_ACTIVE`。
5. 创建 after-wake 队列和 `lisa_pm_wake` 内部任务。
6. `CONFIG_LISA_PM_STATS=y` 时注册统计 hook，占用 `PM_HOOK_ID_0`。
7. 注册框架 common hook，占用 `PM_HOOK_ID_1`。
8. 注册 HAL managed device，占用 `PM_DEV_ID_UART`，其中 `.on_exit` 指向 `lisa_pm_framework_device_on_wake()`，`.on_wake = NULL`。
9. 遍历 `lisa_device`，把带 `system_ops` 的设备复制到 `lisa_pm` 内部表。
10. 初始化当前记录的唤醒原因。

应用不需要在使用 `lisa_pm` 前显式调用 `vrtc_init()`。

### 系统策略

```c
typedef enum {
    LISA_PM_SYSTEM_POLICY_ACTIVE = 0,
    LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP,
} lisa_pm_system_policy_t;

int32_t lisa_pm_set_system_policy(lisa_pm_system_policy_t policy);
lisa_pm_system_policy_t lisa_pm_get_system_policy(void);
```

在 ARCS 上，策略映射如下：

| LISA PM 策略 | HAL PM 配置 |
|---|---|
| `LISA_PM_SYSTEM_POLICY_ACTIVE` | `PM_MODE_ACTIVE` |
| `LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP` | `PM_MODE_LIGHT_SLEEP` + `PM_CLOCK_LEVEL0` |

当前 HAL `PM_MODE_LIGHT_SLEEP` 使用深睡恢复路径承载自动轻睡语义。除 AON/保留域和 snapshot 覆盖区域外，外设寄存器和普通内存状态需要通过 driver 的 `resume_restore()` 或应用重配置恢复。

### 系统睡眠锁

```c
int32_t lisa_pm_lock_acquire(void);
int32_t lisa_pm_lock_release(void);
int32_t lisa_pm_lock_get_count(void);
bool lisa_pm_is_sleep_blocked(void);
```

锁使用引用计数：计数从 0 到 1 时调用 HAL `pm_lock_acquire(PM_LOCK_APP)`，计数从 1 到 0 时调用 `pm_lock_release(PM_LOCK_APP)`。计数大于 0 时，HAL `pm_can_sleep()` 会因为 `pm_env.lock_bits != 0` 放弃 deep sleep，本轮 idle 只执行普通 WFI。

### 应用睡眠回调

```c
typedef struct {
    void (*before_sleep)(void *user_data);
    void (*after_wake)(void *user_data, lisa_pm_wakeup_cause_t cause);
    void *user_data;
} lisa_pm_sleep_callback_t;

int32_t lisa_pm_sleep_callback_register(const lisa_pm_sleep_callback_t *callback);
int32_t lisa_pm_sleep_callback_unregister(void);
```

- 当前只支持一个应用回调实例。
- `before_sleep` 和 `after_wake` 可只填一个，但不能同时为 `NULL`。
- `before_sleep` 运行在 HAL PM enter 关键路径中，必须短小、确定、不可阻塞。
- `after_wake` 被投递到 `lisa_pm_wake` 任务，已离开 PM exit 关键路径，可执行普通任务上下文允许的恢复动作。
- `after_wake` 投递前会临时直接持有一个 HAL `PM_LOCK_APP`，任务取到消息后先释放该临时锁，再执行用户回调；这个临时锁不计入 `lisa_pm_lock_get_count()`。

### 唤醒原因

```c
typedef enum {
    LISA_PM_WAKEUP_TIMER = 0,
    LISA_PM_WAKEUP_RTC,
    LISA_PM_WAKEUP_BT,
    LISA_PM_WAKEUP_WIFI,
    LISA_PM_WAKEUP_GPIO,
    LISA_PM_WAKEUP_UNKNOWN,
} lisa_pm_wakeup_cause_t;

lisa_pm_wakeup_cause_t lisa_pm_get_wakeup_cause(void);
```

ARCS porting 使用 AON `PMU_WAKEUP_*` ISR 位做归一化映射：WiFi、GPIOB_00..09、Timer、RTC、BT 分别映射到对应的 `LISA_PM_WAKEUP_*`。注意 `PMU_WAKEUP_*` 是 AON 唤醒状态位，和 `pm_sleep_config_t.wakeup_src_mask` 使用的 `PM_WAKEUP_*` 不是同一个 namespace。

### 睡眠统计

```c
typedef struct {
    uint32_t sleep_count;
    uint32_t sleep_abort_count;
    uint64_t total_sleep_us;
    uint64_t total_active_us;
    uint32_t last_sleep_us;
    uint32_t max_sleep_us;
    uint32_t wakeup_cause_count[LISA_PM_WAKEUP_UNKNOWN + 1];
} lisa_pm_stats_t;

int32_t lisa_pm_get_stats(lisa_pm_stats_t *stats);
int32_t lisa_pm_reset_stats(void);
uint32_t lisa_pm_get_sleep_ratio(void);
```

`lisa_pm_get_sleep_ratio()` 返回万分比，例如 5234 表示 52.34%。当前统计由 HAL common hook 更新，`sleep_abort_count` 字段保留在结构体中，当前实现未在 abort 路径累加。

### WiFi 省电扩展

```c
typedef enum {
    LISA_PM_WIFI_PS_OFF = 0,
    LISA_PM_WIFI_PS_DTIM,
    LISA_PM_WIFI_PS_LISTEN,
} lisa_pm_wifi_ps_mode_t;

typedef struct {
    uint16_t listen_interval;
} lisa_pm_wifi_ps_config_t;

int32_t lisa_pm_wifi_set_ps_mode(lisa_pm_wifi_ps_mode_t mode,
                                 const lisa_pm_wifi_ps_config_t *config);
lisa_pm_wifi_ps_mode_t lisa_pm_wifi_get_ps_mode(void);
int32_t lisa_pm_wifi_lock_acquire(void);
int32_t lisa_pm_wifi_lock_release(void);
int32_t lisa_pm_wifi_lock_get_count(void);
bool lisa_pm_wifi_is_power_save_blocked(void);
```

`LISA_PM_WIFI_PS_LISTEN` 的 `listen_interval` 有效范围是 1 到 19，未传配置时默认使用 10。对当前 WiFi 库而言，LISTEN 参数必须在 WiFi 发起连接前设置，否则监听周期不会生效。WiFi PS 锁只控制 WiFi 省电，不等同于系统睡眠锁。

### 远端 AP 睡眠锁

```c
typedef struct {
    uint32_t locked;
} lisa_pm_remote_lock_state_t;

int32_t lisa_pm_remote_lock_acquire(void);
int32_t lisa_pm_remote_lock_release(void);
int32_t lisa_pm_remote_lock_get_state(lisa_pm_remote_lock_state_t *state);
```

这些接口仅在 `CONFIG_LISA_PM_REMOTE_LOCK_CLIENT=y` 时对 CP 侧开放。ARCS 当前实现通过 AP PM lock MRPC 服务访问 AP 侧 `lisa_pm_lock_*()`，用于 CP 业务在需要时阻止 AP 进入或停留在低功耗等待路径。

## System PM 设备接入

### lisa_device 注册

推荐驱动先使用普通设备注册宏完成设备本体注册，再用 `LISA_DEVICE_PM_ATTACH()` 独立声明 PM 能力：

```c
LISA_DEVICE_REGISTER_DEINIT(name, api_ptr, priv_data_ptr, user_data_ptr,
                            init_fn, deinit_fn, level, priority);

#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(name, system_ops, wakeup_ops, ctx);
#endif
```

`LISA_DEVICE_PM_ATTACH()` 会生成独立的 `lisa_device_pm_t` attach 条目，`name` 必须与前面的设备注册名一致：

```c
typedef struct {
    const lisa_pm_system_ops_t *system_ops;
    const lisa_pm_wakeup_ops_t *wakeup_ops;
    void *ctx;
} lisa_device_pm_t;
```

`system_ops` 和 `wakeup_ops` 是两个独立能力槽位：

| 字段 | 回答的问题 | 调用方 | 典型内容 | 不负责 |
|---|---|---|---|---|
| `system_ops` | 这个设备当前是否允许系统睡眠，以及睡前/醒后如何快速处理 | `lisa_pm` 系统睡眠流程 | `check_idle()`、关闭传输、保存/恢复寄存器、恢复 HAL 基础状态 | 配置唤醒条件、使能唤醒源 |
| `wakeup_ops` | 这个设备能否作为唤醒源，以及如何把唤醒条件下发到硬件 | `lisa_device_wakeup_*()` 总闸接口 | 缓存 GPIO/RTC 等唤醒条件、启用/禁用硬件 wakeup | 判断设备是否忙、睡前挂起、醒后恢复 |

- 两个槽位互不依赖：设备可以只提供 `system_ops`、只提供 `wakeup_ops`，也可以两个都提供；不支持的能力填 `NULL`。
- `ctx` 只透传给 `system_ops` 回调，通常填设备私有数据指针；`wakeup_ops` 回调直接接收 `lisa_device_t *dev`，再从 device 中取私有数据。
- `CONFIG_LISA_PM=n` 时，`LISA_DEVICE_PM_ATTACH()` 为空宏，不影响设备本体注册和驱动可用性。
- `lisa_pm_init()` 会扫描设备自带 PM 描述和独立 PM attach 段，只自动接管 `system_ops`；wakeup-source 的启停由应用通过 `lisa_device_wakeup_enable()` 显式控制。
- `LISA_DEVICE_REGISTER_PM()` / `LISA_DEVICE_REGISTER_PM_DEINIT()` 仍保留兼容，现有驱动可继续使用。

### system_ops 语义

```c
typedef struct {
    int32_t (*check_idle)(void *ctx);
    int32_t (*prepare_suspend)(void *ctx);
    int32_t (*resume_restore)(void *ctx);
} lisa_pm_system_ops_t;
```

| 回调 | 执行时机 | 返回值语义 |
|---|---|---|
| `check_idle()` | HAL `pm_can_sleep()` 中逐设备检查 | 返回 `1` 表示设备空闲、允许睡眠；返回 `0` 表示设备忙、阻止本轮 deep sleep |
| `prepare_suspend()` | HAL enter handler 中，真正睡眠前 | 返回 `0` 表示处理成功；非 0 会被记录为本轮回调错误 |
| `resume_restore()` | HAL exit handler 中，唤醒后 | 返回 `0` 表示恢复成功；非 0 会被记录为本轮回调错误 |

实现要求：

1. **不能阻塞**：PM 关键路径中不要等待 mutex/event，不要访问文件系统、网络或大块动态内存分配。
2. **只做快速硬件动作**：`prepare_suspend()` / `resume_restore()` 可能在关中断或调度受限阶段执行，应只处理寄存器、HAL 状态和 pinmux 等必要动作。
3. **忙态显式阻止睡眠**：设备正在传输或处于不可断电阶段时，`check_idle()` 必须返回 `0`。
4. **区分驱动恢复和应用恢复**：`resume_restore()` 只负责驱动基础状态；业务通道、IRQ、callback、USB 枚举、音频流等应用态配置需要由应用在唤醒后重新配置。
5. **需要显式释放的设备**：如果设备驱动的 `deinit_fn` / `init_fn` 会释放或创建 FreeRTOS 对象、堆内存、USB/Audio 等复杂资源，应用应在进入 `AUTO_LIGHT_SLEEP` 前的正常任务上下文调用 `lisa_device_destroy()` 或对应 stop/deinit 接口；唤醒后在 `after_wake` 或普通任务上下文调用 `lisa_device_reinit()` 或对应 start/init 接口，再重新完成业务配置。
6. **禁止在 PM 临界区重建资源**：不要在 `prepare_suspend()` / `resume_restore()` 中调用 `lisa_device_destroy()`、`lisa_device_reinit()` 或会创建/删除 OS 对象的 API；这些接口应放在 shell 命令、业务任务或 app `after_wake` 任务上下文中执行。

### wakeup_ops 语义

`wakeup_ops` 只描述设备作为唤醒源时的配置缓存和硬件启停，不参与 `check_idle()` 判断，也不替代 `prepare_suspend()` / `resume_restore()`。`lisa_device` 提供与源类型无关的 wakeup-source 总闸：

```c
bool lisa_device_wakeup_is_capable(lisa_device_t *dev);
int32_t lisa_device_wakeup_enable(lisa_device_t *dev, bool enable);
bool lisa_device_wakeup_is_enabled(lisa_device_t *dev);
```

驱动侧 vtable：

```c
typedef struct {
    int32_t (*configure)(struct lisa_device *dev, uint32_t sub_idx, uint32_t trigger);
    int32_t (*clear)(struct lisa_device *dev, uint32_t sub_idx);
    int32_t (*set_enabled)(struct lisa_device *dev, bool enable);
} lisa_pm_wakeup_ops_t;
```

当前约定是强一致缓存模型：

- driver 的 `configure()` / `clear()` 只更新内部 wakeup 缓存，不下发硬件。
- `lisa_device_wakeup_enable(dev, true)` 调用 driver `set_enabled(true)`，一次性把缓存下发到硬件。
- `lisa_device_wakeup_enable(dev, false)` 撤销该 device 已下发的所有 wakeup 配置。
- 在线修改触发条件必须按 `enable(false) -> configure/clear -> enable(true)` 顺序执行。
- 缓存为空时 `enable(true)` 约定返回 0，但不会实际打开任何硬件唤醒源。

以 GPIO 为例：

```c
lisa_device_t *gpiob = lisa_device_get("gpiob");
lisa_gpio_configure(gpiob, 7, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
lisa_gpio_configure_wakeup(gpiob, 7, LISA_GPIO_WAKEUP_LEVEL_LOW);
lisa_device_wakeup_enable(gpiob, true);
```

ARCS PMU GPIO 唤醒仅支持 GPIOB_00..GPIOB_09。

## HAL PM 睡眠和唤醒流程

`lisa_pm` 不直接执行硬件睡眠，而是把策略、锁、设备回调和应用回调接入 ARCS HAL PM。HAL PM 在 FreeRTOS idle 的 tickless 路径中决定本轮是否进入低功耗：

```text
vPortSuppressTicksAndSleep()
  -> pm_can_sleep()
     -> 允许睡眠: pm_light_sleep(xExpectedIdleTime)
     -> 不允许睡眠: __WFI()
```

`pm_can_sleep()` 的核心准入条件：

- PM 环境已经初始化完成。
- `pm_env.config->mode == PM_MODE_LIGHT_SLEEP`。
- `pm_env.lock_bits == 0`；`lisa_pm_lock_acquire()` 会设置 `PM_LOCK_APP`。
- 双核模式下，本核心 `cross_core_lock == 0`。
- primary 核需要 `vrtc_is_allow_sleep()` 返回允许。
- 所有 HAL device handler 的 `check_idle(PM_MODE_LIGHT_SLEEP)` 都不能返回 0。

如果任一条件不满足，本轮 idle 只执行普通 `__WFI()`，不会进入深睡恢复路径，也不会触发 `lisa_pm` 的 `prepare_suspend()` / `resume_restore()`。

### 睡眠进入流程

满足准入条件后，HAL 进入 `pm_light_sleep()`：

```text
pm_light_sleep()
  -> 停止 SysTimer
  -> 关中断并 flush log
  -> eTaskConfirmSleepModeStatus() 二次确认
  -> pm_execute_enter_handler()
  -> pm_light_sleep_prepare()
  -> pm_hw_execute_sleep()
```

关键步骤如下：

1. **执行 enter handler**：HAL 按 handler 类型从后向前执行 enter handler。对 `lisa_pm` 来说，顺序表现为 `stats enter`、`framework enter`、`app before_sleep`、`lisa_device prepare_suspend`。
2. **保存上下文**：`pm_light_sleep_prepare()` 保存寄存器上下文；primary 核清除 AON wake cause、保存 BootClock、配置 AON 唤醒源和 RAM retention；非 primary 核调整 mailbox IRQ 优先级并清除 VRTC alert 状态。
3. **设置唤醒入口**：`pm_hw_execute_sleep()` 调用 `pm_set_wakeup_entry((uint32_t)__light_sleep_entry)`，把 `__light_sleep_entry` 登记为本次低功耗的 wakeup entry。AON 唤醒后，硬件/启动恢复路径会读取该入口地址并跳转执行，从而进入 HAL 的恢复流程。
4. **配置硬件深睡**：primary 核调用 `HAL_PMU_ConfigDeepSleepMode(PMU_SLEEPMODE_MODE2, PMU_HOLDENTRY_WFI)`，使用 PMU deep sleep mode2 承载本次 `PM_MODE_LIGHT_SLEEP`。
5. **保存 snapshot**：启用 `CONFIG_PM_PSRAM` 时，HAL 把已注册 snapshot 区域复制到 PSRAM，并记录 checksum；primary 核随后让 PSRAM 进入 sleep。
6. **进入低功耗**：执行 `__light_sleep_save()`，系统进入由 AON 唤醒源控制的低功耗状态。

### 唤醒恢复流程

AON 唤醒源触发后，系统从 `__light_sleep_entry` 进入 `pm_sleep_startup()`，再回到 `pm_light_sleep()` 后半段：

```text
AON wake source
  -> __light_sleep_entry
  -> pm_sleep_startup()
  -> 回到 pm_light_sleep()
  -> pm_light_sleep_restore()
  -> vTaskStepTick()
  -> pm_execute_exit_handler()
```

恢复路径的主要工作：

1. **恢复基础硬件**：primary 核恢复 RAM power、BootClock、cache、PSRAM、Flash 和中断控制相关状态；非 primary 核先进入 `PM_CORE_STATE_STARTUP`，等待 primary 核通过 IPC 唤醒后继续恢复。
2. **恢复 snapshot**：启用 `CONFIG_PM_PSRAM` 时，`pm_snapshot_restore()` 把 PSRAM 中的 snapshot 内容复制回原地址，并用 checksum 校验恢复结果。
3. **获取唤醒原因**：HAL 读取 AON wake cause，写入 `pm_wakeup_cause`；双核模式下 primary 核同时更新共享 `pm_data.last_wakeup_cause`。
4. **恢复寄存器和浅 WFI 模式**：回到 `pm_light_sleep()` 后执行 `pm_light_sleep_restore()`，恢复寄存器上下文，必要时唤醒对端核心，并把 WFI 模式切回 `WFI_SHALLOW_SLEEP`。
5. **修正系统 tick**：HAL 用 VRTC 统计的睡眠时长更新 SysTimer 和 FreeRTOS tick。
6. **执行 exit handler**：HAL 执行 exit handler。对 `lisa_pm` 来说，先进入 device `.on_exit`，执行 `lisa_device resume_restore` 并投递 app `after_wake`；随后 common hook 更新统计和最近一次唤醒原因；最后 `lisa_pm_wake` 任务释放临时 HAL 锁并执行 app `after_wake`。

当前 `lisa_pm` 在 HAL managed device 中不使用 `.on_wake`，设备恢复统一走 `.on_exit`。

### 深睡模拟浅睡

`LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP` 对应用暴露的是“自动轻睡眠”语义：应用只需要声明系统允许在空闲时自动睡眠，唤醒后任务继续运行，FreeRTOS tick 会补偿睡眠时间，`lisa_pm` 会调度设备和应用恢复回调。

ARCS HAL 当前用 PMU deep sleep mode2 实现这层语义，而不是传统意义上只停时钟、不掉电的浅睡眠。也就是说：

- **接口层是 light sleep**：应用设置的是 `LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP`，porting 下发的是 `PM_MODE_LIGHT_SLEEP`。
- **硬件层走 deep sleep**：HAL 在 `pm_hw_execute_sleep()` 中配置 `PMU_SLEEPMODE_MODE2`，并通过 `__light_sleep_save()` 进入深睡保存/恢复入口。
- **连续性由软件重建**：HAL 通过 BootClock/cache/Flash/PSRAM 恢复、snapshot 还原、寄存器上下文恢复、FreeRTOS tick 补偿和 `lisa_pm` 设备回调，让应用看到接近 light sleep 的运行连续性。
- **掉电域需要应用配合重建**：除 AON/保留域和 snapshot 覆盖区域外，普通外设寄存器、HAL 状态和业务配置不能假设跨睡眠保持。`resume_restore()` 只覆盖 PM 关键路径允许的快速基础恢复；对会释放/创建 OS 对象、堆内存或复杂外设状态的设备，应用需要在睡前正常任务上下文显式调用 `lisa_device_destroy()` 或对应 stop/deinit 接口，唤醒后在 `after_wake` 或普通任务上下文调用 `lisa_device_reinit()` 或对应 start/init 接口，并重新完成业务配置。

常见保留和恢复来源：

| 来源 | 用途 |
|---|---|
| AON 域 | 保存 AON wake cause、GPIOB 唤醒状态、VRTC/RTC 等低功耗资源 |
| RAM retention | 保持配置的 RAM bank 供电 |
| PSRAM snapshot | 保存 ILM/DLM/非 AON SRAM 等注册区域，唤醒后复制回原地址 |
| HAL register context | 保存并恢复部分外设寄存器、IPC IRQ、GPIO retention 等上下文 |
| `lisa_pm_system_ops_t` | 让具体 driver 在睡眠前关闭硬件、唤醒后恢复 HAL/pinmux 基础状态 |
| app `after_wake` | 让应用恢复业务级状态，例如显示、电源控制、网络或外设通道重配置 |

因此，接入 `AUTO_LIGHT_SLEEP` 时不要把它理解成“所有 SRAM 和外设寄存器天然保持”的硬件浅睡；应按深睡恢复模型设计数据保留、设备恢复和业务重配置。

## AP/CP 双核运行模式

双核运行模式要求 AP remote 固件和 CP 主固件同时存在。AP 不是睡眠前被关闭、唤醒后再拉起的 boot-shim，而是作为 HAL PM 对端核心参与 shared state 协同。

典型职责：

| 核 | 典型职责 |
|---|---|
| AP remote 固件 | 引导 CP，初始化 IPC 和 `lisa_pm`，作为非 primary PM 对端保持运行 |
| CP 主固件 | 运行 WiFi/LWIP/业务逻辑，IP ready 后切到 `AUTO_LIGHT_SLEEP`，作为 primary 触发最终硬件睡眠 |

双核共享状态来自 `amp_shared_info.pm_data`：

```text
pm_data.config              当前 HAL PM 配置
pm_data.sleep_cfg           唤醒源、GPIO mask、timer 等睡眠配置
pm_data.core_ctx[]          每个核心的 state / cross_core_lock
pm_data.last_wakeup_cause   最近一次 AON 唤醒原因
```

当前示例约定：

- AP：`CONFIG_LISA_PM_CORE_PRIMARY=n`，对应 HAL `PM_CORE_PRIMARY=0`。
- CP：`CONFIG_LISA_PM_CORE_PRIMARY=y`，对应 HAL `PM_CORE_PRIMARY=1`。
- CP：必须启用 `CONFIG_LISA_PM_DUAL_CORE=y`，避免选择 `CONFIG_ARCS_HAL_PM_CLOSE_AP`。

简化时序：

```text
AP(non-primary) idle -> 设置自身 IDLE -> 清 CP cross_core_lock -> IPC_SIG_ENTER_IDLE -> WFI_DEEP_SLEEP
CP(primary) idle     -> 检查 cross_core_lock=0 -> 配置 AON/PMU/VRTC/snapshot -> __light_sleep_save()
AON wake source      -> CP 先恢复 -> 必要时 IPC_SIG_WAKEUP 唤醒 AP
AP 恢复              -> 重新置位 CP cross_core_lock -> AP/CP 回到 ACTIVE
```

参考示例：

- `samples/subsys/lisa_pm/dual_core/basic/`
- `samples/subsys/lisa_pm/dual_core/gpio_wakeup/`

## 使用示例

### 基础系统睡眠

```c
#include "lisa_pm.h"

int app_pm_start(void)
{
    int32_t ret = lisa_pm_init();
    if (ret != 0) {
        return ret;
    }

    return lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP);
}
```

### 睡眠锁保护关键事务

```c
void flash_update_begin(void)
{
    (void)lisa_pm_lock_acquire();
}

void flash_update_end(void)
{
    (void)lisa_pm_lock_release();
}
```

### 应用 after-wake 恢复

```c
static void board_after_wake(void *user_data, lisa_pm_wakeup_cause_t cause)
{
    (void)user_data;
    (void)cause;
    board_display_power_on();
}

static const lisa_pm_sleep_callback_t sleep_cb = {
    .before_sleep = NULL,
    .after_wake = board_after_wake,
    .user_data = NULL,
};

void board_pm_callback_init(void)
{
    (void)lisa_pm_sleep_callback_register(&sleep_cb);
}
```

### WiFi LISTEN 省电

```c
int app_wifi_pm_prepare(void)
{
    lisa_pm_wifi_ps_config_t ps_cfg = {
        .listen_interval = 10,
    };

    int32_t ret = lisa_pm_init();
    if (ret != 0) {
        return ret;
    }

    ret = lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_LISTEN, &ps_cfg);
    if (ret != 0) {
        return ret;
    }

    wifi_sta_connect(ssid, pwd);
    wait_until_ip_ready();
    net_enable_keep_alive();

    return lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP);
}
```

## 注意事项

1. **先初始化**：使用系统策略、统计、设备 system PM 或应用睡眠回调前，应先调用 `lisa_pm_init()`。
2. **区分锁类型**：`lisa_pm_lock_*()` 阻止系统 deep sleep；`lisa_pm_wifi_lock_*()` 只阻止 WiFi 进入省电。
3. **回调上下文受限**：`before_sleep` 和 device `system_ops` 属于 PM 关键路径，不能执行阻塞操作；复杂业务恢复应放到 `after_wake` 或普通任务。
4. **LISTEN 参数时机**：WiFi LISTEN interval 必须在 WiFi 发起连接前设置。
5. **双核不要关闭 AP**：AP/CP runtime 双核模式下 CP 必须启用 `CONFIG_LISA_PM_DUAL_CORE`，确保不会走 `CONFIG_ARCS_HAL_PM_CLOSE_AP` 单核 boot-shim 路径。
6. **GPIO 唤醒范围**：ARCS PMU GPIO 唤醒只支持 GPIOB_00..GPIOB_09，进入睡眠前引脚不能已经处于触发电平。
7. **snapshot 按需配置**：启用 ILM/DLM/SRAM snapshot 时需同时确认 `CONFIG_PM_PSRAM`、`CONFIG_PM_SNAPSHOT_PSRAM_SIZE` 和各固件 PSRAM 布局足够。
