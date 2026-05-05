# PM 接口参考

底层 PM 框架和 WiFi PS 模块向应用层暴露的全部接口。

## 目录

- [枚举类型](#枚举类型)
- [数据结构](#数据结构)
- [宏定义](#宏定义)
- [系统 PM 接口](#系统-pm-接口)
- [网络保活接口](#网络保活接口)
- [WiFi 省电接口](#wifi-省电接口)
- [设备 PM 回调接口](#设备-pm-回调接口)

---

## 枚举类型

### `pm_mode_t` — 低功耗模式

```c
typedef enum {
    PM_MODE_ACTIVE = 0,      // 正常运行，不睡眠
    PM_MODE_LIGHT_SLEEP,     // 轻睡眠：CPU 断电，RAM 保持，AON 域运行
    PM_MODE_DEEP_SLEEP,      // 深睡眠
    PM_MODE_MAX
} pm_mode_t;
```

### `pm_clock_level_t` — 时钟降频级别

```c
typedef enum {
    PM_CLOCK_LEVEL0,         // 最高频率
    PM_CLOCK_LEVEL1,
    PM_CLOCK_LEVEL2,
    PM_CLOCK_LEVEL_COUNT,
} pm_clock_level_t;
```

### `pm_wakeup_source_t` — 唤醒源

```c
typedef enum {
    PM_WAKEUP_TIMER = 0,    // 定时器唤醒
    PM_WAKEUP_RTC,           // RTC 唤醒
    PM_WAKEUP_BT,            // 蓝牙唤醒
    PM_WAKEUP_WIFI,          // WiFi 唤醒
    PM_WAKEUP_GPIO,          // GPIO 唤醒
    PM_WAKEUP_MAX
} pm_wakeup_source_t;
```

### `pm_gpio_wakeup_mode_t` — GPIO 唤醒电平模式

```c
typedef enum {
    PM_GPIO_MODE_LOW  = 0,   // 低电平唤醒
    PM_GPIO_MODE_HIGH,       // 高电平唤醒
} pm_gpio_wakeup_mode_t;
```

### `pm_lock_t` — 功耗锁类型

```c
typedef enum {
    PM_LOCK_NONE = 0,
    PM_LOCK_WIFI,            // WiFi 子系统持有
    PM_LOCK_BT,              // BT 子系统持有
    PM_LOCK_FLASH,           // Flash 操作期间持有
    PM_LOCK_APP,             // 应用层持有
    PM_LOCK_MAX = 32
} pm_lock_t;
```

持有任何一个锁时，`pm_can_sleep()` 返回 false，系统不会进入 Light Sleep。

### `pm_dev_id_t` — PM 设备 ID

```c
typedef enum {
    PM_DEV_ID_WIFI = 0,      // WiFi 设备
    PM_DEV_ID_UART,          // UART 设备
} pm_dev_id_t;
```

### `pm_hook_id_t` — PM 钩子 ID

```c
typedef enum {
    PM_HOOK_ID_0 = 0,
    PM_HOOK_ID_1,
} pm_hook_id_t;
```

### `wifi_ps_mode_e` — WiFi 省电模式

```c
typedef enum {
    WIFI_PS_MODE_OFF,        // WiFi PS 关闭 (始终活跃)
    WIFI_PS_MODE_DTIM,       // DTIM 模式：每个 DTIM beacon 间隔唤醒
    WIFI_PS_MODE_LISTEN,     // Listen 模式：按配置的 listen interval 唤醒
    WIFI_PS_MODE_MAX
} wifi_ps_mode_e;
```

### PM 调试级别

```c
enum {
    PM_DBG_OFF = 0,          // 关闭调试
    PM_DBG_CRT,              // 仅关键信息
    PM_DBG_INF,              // 一般信息
    PM_DBG_VRB,              // 详细信息
    PM_DBG_MAX
};
```

---

## 数据结构

### `pm_config_t` — PM 配置

```c
typedef struct {
    pm_mode_t        mode;         // 目标低功耗模式
    pm_clock_level_t clock_level;  // 时钟降频级别
    uint16_t         auto_mode;    // 自动模式
    uint16_t         dbg_level;    // 调试日志级别
} pm_config_t;
```

### `pm_sleep_config_t` — 睡眠参数配置

```c
typedef struct {
    uint32_t              wakeup_src_mask;  // 唤醒源位掩码
    uint32_t              time_us;          // 定时器唤醒时间 (µs)
    uint64_t              gpio_mask;        // GPIO 唤醒引脚掩码
    pm_gpio_wakeup_mode_t gpio_mode;        // GPIO 唤醒电平模式 (高/低)
    uint32_t              retention_bits;   // RAM bank 保持位掩码
} pm_sleep_config_t;
```

### `pm_handler_ops_t` — PM 操作回调集

```c
typedef int32_t (*pm_handler_func_t)(uint32_t sleep_time_us, void *arg);

typedef struct {
    int32_t (*check_idle)(pm_mode_t mode);  // 检查是否允许睡眠
    pm_handler_func_t on_enter;              // 进入睡眠前回调
    pm_handler_func_t on_exit;               // 退出睡眠后回调
    pm_handler_func_t on_wake;               // 唤醒时回调
} pm_handler_ops_t;
```

### `pm_reg_info` — 寄存器保存/恢复条目

```c
struct pm_reg_info {
    volatile uint32_t *addr;    // 寄存器地址
    uint32_t value;             // 保存的值
};
```

### `wifi_ps_state_t` — WiFi PS 状态

```c
typedef struct wifi_ps_state {
    bool     state;             // 当前 PS 是否激活
    bool     enable;            // PS 是否使能
    bool     event_pending;     // 是否有待处理事件
    uint32_t lock_state;        // 锁状态 (WIFI_PS_LOCK_BIT_*)
    uint32_t prevent;           // 阻止 PS 的原因
    uint32_t vif_prevent;       // VIF 级别的阻止原因
    uint32_t tx_cnt;            // 待发送帧计数
    uint32_t timer_prevent;     // 定时器阻止标志
} wifi_ps_state_t;
```

### `wifi_ps_hw_ops` — WiFi PS 硬件操作接口

```c
struct wifi_ps_hw_ops {
    int32_t (*suspend)(uint32_t sleep_time, int32_t suspend);  // 睡前硬件操作
    int32_t (*resume)(int32_t suspend);                        // 唤醒硬件操作
    int32_t (*check_idle)(void);                               // 检查硬件是否空闲
    void    (*aon_wakup_isr)(void);                            // AON 唤醒中断处理
    void    (*mac_wakup_isr)(void);                            // MAC 唤醒中断处理
    void    (*set_beacon_intv)(uint16_t bcn_intv);             // 设置 beacon 间隔
    void    (*set_listen_intv)(uint16_t listen_intv);          // 设置 listen 间隔
    void    (*set_dtim)(uint8_t dtim_period);                  // 设置 DTIM 周期
    void    (*enable_dtim_wakeup)(bool enable);                // 使能/禁用 DTIM 唤醒
    void    (*set_wakeup_time)(uint32_t time);                 // 设置唤醒时间
};
```

---

## 宏定义

### PM 常量

```c
#define PM_GPIO_PIN_MAX              10        // 最大 GPIO 唤醒引脚数
#define PM_UART_IDLE_TIME            1000000   // UART 空闲检测时间 (µs)
#define PM_GPIO_IDLE_TIME            1000000   // GPIO 空闲检测时间 (µs)
#define PM_RAM_RETENTION_BIT_MASK    0xFFFF    // RAM retention 位掩码
```

### 网络保活常量

```c
#define NET_KEEP_ALIVE_PERIOD        30000     // ARP 保活周期 (ms)，定义在 net_al.h
```

### WiFi PS 锁位

```c
#define WIFI_PS_LOCK_BIT_APP           0x00000001  // 应用层锁
#define WIFI_PS_LOCK_BIT_FHOST_CNTRL   0x00000002  // FHOST 控制锁
#define WIFI_PS_LOCK_BIT_FHOST_RX      0x00000004  // FHOST 接收锁
#define WIFI_PS_LOCK_BIT_LWIP          0x00000008  // LWIP 协议栈锁
```

### WiFi PS 默认配置

```c
#define WIFI_PS_DEFAULT_TYPE           WIFI_PS_MODE_DTIM
```

---

## 系统 PM 接口

### 初始化

#### `pm_init`

```c
int32_t pm_init(void);
```

初始化电源管理模块。配置 AON 电源域时序寄存器。应在系统启动时调用一次。

- **返回值**: 0 成功

**Demo 用法**:

```c
// main() 最开始调用
pm_init();
```

---

### 配置

#### `pm_set_config`

```c
int32_t pm_set_config(pm_config_t *config);
```

配置并切换到指定的低功耗模式。此调用不会立即进入睡眠，而是"武装"PM 框架——
当 FreeRTOS idle task 判断可以睡眠时，才会真正进入 Light Sleep。

- **参数**: `config` — 低功耗配置
- **返回值**: 0 成功, <0 失败

内部行为：
1. 验证 `mode < PM_MODE_MAX`
2. 如果 `clock_level` 变化，切换 PLL 时钟源
3. 存储配置到内部 `pm_env.config`

**Demo 用法**:

```c
pm_config_t config = {
    .mode = PM_MODE_LIGHT_SLEEP,
};
pm_set_config(&config);
```

---

### 睡眠参数配置

#### `pm_get_sleep_config`

```c
int32_t pm_get_sleep_config(pm_sleep_config_t *sleep_config);
```

获取当前睡眠参数配置（唤醒源、GPIO 掩码、retention 位等）。

- **参数**: `sleep_config` — 输出配置
- **返回值**: 0 成功

#### `pm_set_sleep_config`

```c
int32_t pm_set_sleep_config(pm_sleep_config_t *sleep_config);
```

设置睡眠参数配置。

- **参数**: `sleep_config` — 输入配置
- **返回值**: 0 成功

---

### 功耗锁

功耗锁用于临时阻止系统进入 Light Sleep。持有任何锁时 `pm_can_sleep()` 返回 false。

#### `pm_lock_acquire`

```c
int32_t pm_lock_acquire(pm_lock_t lock);
```

获取功耗锁，阻止进入低功耗。

- **参数**: `lock` — 锁类型 (`PM_LOCK_APP` 等)
- **返回值**: 0 成功

**使用场景**: 在需要持续 CPU 运算或外设访问的代码段前获取，完成后释放。

```c
pm_lock_acquire(PM_LOCK_APP);
// ... 需要 CPU 活跃的操作 ...
pm_lock_release(PM_LOCK_APP);
```

#### `pm_lock_release`

```c
int32_t pm_lock_release(pm_lock_t lock);
```

释放功耗锁，允许进入低功耗。

- **参数**: `lock` — 锁类型
- **返回值**: 0 成功

---

### 唤醒源配置

#### `pm_enable_gpio_wakeup`

```c
int32_t pm_enable_gpio_wakeup(uint64_t mask, pm_gpio_wakeup_mode_t mode);
```

使能 GPIO 唤醒，指定引脚掩码和触发电平模式。

- **参数**: `mask` — GPIO 引脚位掩码; `mode` — `PM_GPIO_MODE_LOW` 或 `PM_GPIO_MODE_HIGH`
- **返回值**: 0 成功

#### `pm_disable_gpio_wakeup`

```c
int32_t pm_disable_gpio_wakeup(uint64_t mask);
```

禁用指定 GPIO 引脚的唤醒功能。

#### `pm_enable_timer_wakeup`

```c
int32_t pm_enable_timer_wakeup(uint32_t time_in_us);
```

使能定时器唤醒，指定唤醒时间（微秒）。

#### `pm_disable_timer_wakeup`

```c
int32_t pm_disable_timer_wakeup(void);
```

禁用定时器唤醒。

---

### AP 域电源控制

仅在 `CONFIG_PM_CLOSE_AP` 使能时可用。

#### `pm_force_ap_off`

```c
void pm_force_ap_off(void);
```

强制关闭 AP 域电源。

#### `pm_force_ap_on`

```c
void pm_force_ap_on(void);
```

强制开启 AP 域电源。

---

### 设备注册

#### `pm_device_register`

```c
int32_t pm_device_register(int32_t dev_id, pm_handler_ops_t *ops);
```

注册设备的 PM 回调。注册后，PM 框架在每次睡眠/唤醒循环中都会调用该设备的
`check_idle`、`on_enter`、`on_exit`、`on_wake` 回调。

- **参数**: `dev_id` — 设备 ID (`PM_DEV_ID_WIFI` 等); `ops` — PM 操作回调集
- **返回值**: 0 成功

#### `pm_device_unregister`

```c
int32_t pm_device_unregister(int32_t dev_id);
```

注销设备的 PM 回调。

- **参数**: `dev_id` — 之前注册的设备 ID
- **返回值**: 0 成功

---

### 钩子注册

#### `pm_hook_register`

```c
int32_t pm_hook_register(pm_hook_id_t hook_id, pm_handler_func_t enter, pm_handler_func_t exit);
```

注册 PM 钩子函数，在睡眠进入/退出时被调用。钩子独立于设备注册，用于轻量级的通用睡眠/唤醒处理。

- **参数**: `hook_id` — 钩子 ID; `enter` — 进入睡眠回调; `exit` — 退出睡眠回调
- **返回值**: 0 成功

#### `pm_hook_unregister`

```c
int32_t pm_hook_unregister(pm_hook_id_t hook_id);
```

注销 PM 钩子。

---

## 网络保活接口

ARP 保活功能由网络层管理，通过 `net_al.h` 暴露接口。

#### `net_enable_keep_alive`

```c
int32_t net_enable_keep_alive(void);
```

启动 ARP 保活功能。使能后每 30 秒（`NET_KEEP_ALIVE_PERIOD`）发送一次免费 ARP，
防止路由器在设备睡眠期间清除 ARP 表项。

- **返回值**: 0 成功

**Demo 用法**:

```c
// 开启 WiFi 省电时同步启动保活
wifi_ps_mode_set(WIFI_PS_MODE_DTIM);
net_enable_keep_alive();
```

#### `net_disable_keep_alive`

```c
int32_t net_disable_keep_alive(void);
```

停止 ARP 保活功能。

- **返回值**: 0 成功

**Demo 用法**:

```c
// 关闭 WiFi 省电时同步停止保活
wifi_ps_mode_set(WIFI_PS_MODE_OFF);
net_disable_keep_alive();
```

---

## WiFi 省电接口

以下接口通过 MRPC IPC 发送到 CP 核执行，调用者在 AP 核上。

### 模式控制

#### `wifi_ps_mode_set`

```c
ls_err_t wifi_ps_mode_set(wifi_ps_mode_e mode);
```

设置 WiFi 省电模式。

- `WIFI_PS_MODE_OFF` — 关闭省电，WiFi 射频始终活跃
- `WIFI_PS_MODE_DTIM` — DTIM 省电，每个 DTIM beacon 间隔唤醒
- `WIFI_PS_MODE_LISTEN` — Listen 省电，按配置的 listen interval 唤醒
- **返回值**: `LS_OK` 成功

**Demo 用法**:

```c
wifi_ps_mode_set(WIFI_PS_MODE_DTIM);   // 进入 DTIM 省电
wifi_ps_mode_set(WIFI_PS_MODE_OFF);    // 退出省电
```

#### `wifi_sta_ps_enter`

```c
ls_err_t wifi_sta_ps_enter(void);
```

手动进入 WiFi 省电模式。

#### `wifi_sta_ps_exit`

```c
ls_err_t wifi_sta_ps_exit(void);
```

手动退出 WiFi 省电模式。

---

### Listen Interval

#### `wifi_sta_set_listen_itv`

```c
ls_err_t wifi_sta_set_listen_itv(uint8_t listen_itv);
```

设置 STA 的 listen interval（值必须 < 20）。表示设备每隔多少个 DTIM 周期
醒来监听一次 beacon。值越大越省电，但响应延迟越高。

- **参数**: `listen_itv` — 间隔值 (1-19)
- **返回值**: `LS_OK` 成功

**Demo 用法**: WiFi 初始化完成后设置

```c
wifi_sta_set_listen_itv(10);   // 每 10 个 DTIM 醒一次
```

#### `wifi_sta_get_listen_itv`

```c
ls_err_t wifi_sta_get_listen_itv(uint8_t *listen_itv);
```

获取当前 listen interval 值。

---

### WiFi PS 锁

WiFi PS 锁独立于系统 PM 锁，用于临时阻止 WiFi 射频进入省电模式。

#### `wifi_ps_lock_acquire`

```c
ls_err_t wifi_ps_lock_acquire(uint32_t lock, bool force);
```

获取 WiFi PS 锁，阻止 WiFi 进入省电。

- **参数**: `lock` — `WIFI_PS_LOCK_BIT_*` 掩码; `force` — 是否强制
- **返回值**: `LS_OK` 成功

**使用场景**: 在需要持续 WiFi 活跃的操作（如大量数据传输）前获取。

```c
wifi_ps_lock_acquire(WIFI_PS_LOCK_BIT_APP, false);
// ... WiFi 密集操作 ...
wifi_ps_lock_release(WIFI_PS_LOCK_BIT_APP);
```

#### `wifi_ps_lock_release`

```c
ls_err_t wifi_ps_lock_release(uint32_t lock);
```

释放 WiFi PS 锁。

---

### Keep-Alive

#### `wifi_sta_keepalive_time_set`

```c
ls_err_t wifi_sta_keepalive_time_set(uint8_t time_seconds);
```

设置 STA 模式的 WiFi 层 keep alive 时间（固件默认 30 秒）。

---

### BCMC (广播/组播)

#### `wifi_sta_set_dont_wait_bcmc`

```c
ls_err_t wifi_sta_set_dont_wait_bcmc(uint8_t dont_wait_bcmc);
```

设置是否跳过等待广播/组播帧。设为 1 时设备不等待 DTIM 后的 BCMC 帧，更省电但可能丢失广播数据。

#### `wifi_sta_get_dont_wait_bcmc`

```c
ls_err_t wifi_sta_get_dont_wait_bcmc(uint8_t *dont_wait_bcmc);
```

获取当前 dont_wait_bcmc 标志值。

---

### TWT (Target Wake Time)

WiFi 6 的 TWT 机制，允许设备与 AP 协商精确的唤醒时间。

#### `wifi_twt_setup`

```c
ls_err_t wifi_twt_setup(uint8_t setup_type, uint16_t mantissa, uint8_t min_twt);
```

建立 TWT 会话。

- **参数**:
  - `setup_type` — 1=suggest (STA 提供参数), 2=demand (使用 AP 参数)
  - `mantissa` — 唤醒间隔 (ms)
  - `min_twt` — 最小 TWT 持续时间 (最大 255ms)
- **返回值**: `LS_OK` 成功

#### `wifi_twt_teardown`

```c
ls_err_t wifi_twt_teardown(void);
```

拆除 TWT 会话。

---

### 调试

#### `wifi_ps_dbg_level_set`

```c
ls_err_t wifi_ps_dbg_level_set(uint8_t level);
```

设置 WiFi PS 模块调试日志级别。

---

## 设备 PM 回调接口

设备驱动通过 `pm_device_register()` 注册 `pm_handler_ops_t` 回调集，参与 PM 的睡眠/唤醒协商。

### `check_idle` — 空闲检查

```c
int32_t (*check_idle)(pm_mode_t mode);
```

PM 框架在每次尝试睡眠前调用所有注册设备的此回调。**任何一个**返回 0 (busy)，
本轮睡眠就会被跳过。

- **参数**: `mode` — 将要进入的低功耗模式
- **返回值**: >0 允许睡眠, 0 不允许 (跳过本轮), <0 错误
- **调用时机**: `pm_can_sleep()` → 遍历设备链表

### `on_enter` — 睡前回调

```c
int32_t (*on_enter)(uint32_t sleep_time_us, void *arg);
```

`check_idle` 全部通过后，PM 框架调用此回调让设备执行挂起操作（关电源、保存状态等）。

- **参数**: `sleep_time_us` — 预期睡眠时长 (µs); `arg` — 用户参数
- **返回值**: 0 成功

### `on_exit` — 唤醒后回调

```c
int32_t (*on_exit)(uint32_t sleep_time_us, void *arg);
```

唤醒后 PM 框架调用此回调，让设备重新初始化或恢复状态。

- **参数**: `sleep_time_us` — 实际睡眠时长 (µs); `arg` — 用户参数
- **返回值**: 0 成功

### `on_wake` — 唤醒时回调

```c
int32_t (*on_wake)(uint32_t sleep_time_us, void *arg);
```

在唤醒恢复流程中调用，用于需要更早时机处理的设备。

### 注册示例

```c
static int32_t my_dev_check_idle(pm_mode_t mode) {
    if (my_dev_is_busy())
        return 0;  // 正在工作，不允许睡眠
    return 1;      // 空闲，允许睡眠
}

static int32_t my_dev_on_enter(uint32_t sleep_time_us, void *arg) {
    my_dev_save_regs();
    my_dev_power_off();
    return 0;
}

static int32_t my_dev_on_exit(uint32_t sleep_time_us, void *arg) {
    my_dev_power_on();
    my_dev_restore_regs();
    return 0;
}

static pm_handler_ops_t my_dev_ops = {
    .check_idle = my_dev_check_idle,
    .on_enter   = my_dev_on_enter,
    .on_exit    = my_dev_on_exit,
    .on_wake    = NULL,
};

void my_dev_init(void) {
    pm_device_register(PM_DEV_ID_UART, &my_dev_ops);
}
```

---

## 本 Demo 使用的接口汇总

| 接口 | 调用位置 | 用途 |
|------|---------|------|
| `pm_init()` | `main()` 开头 | 初始化 PM 框架 |
| `vrtc_init()` | `main()` 开头 | 初始化虚拟 RTC (PM 时间补偿依赖) |
| `pm_set_config()` | `enable/disable_system_low_power()` | 配置 Light Sleep / Active 模式 |
| `net_enable_keep_alive()` | `enable_wifi_power_save()` | 启动网络层 ARP 保活 |
| `net_disable_keep_alive()` | `disable_wifi_power_save()` | 停止网络层 ARP 保活 |
| `wifi_ps_mode_set()` | `enable/disable_wifi_power_save()` | 开关 WiFi DTIM PS |
| `wifi_sta_set_listen_itv()` | `cb_lisa_wifi_init_done()` | 设置 listen interval=10 |

Demo 中**未使用**但可用的接口：

| 接口 | 适用场景 |
|------|---------|
| `pm_lock_acquire/release()` | 消息处理期间防止进入睡眠 |
| `wifi_ps_lock_acquire/release()` | WiFi 密集操作期间保持射频活跃 |
| `pm_device_register()` | 自定义设备的 PM 适配 |
| `pm_hook_register/unregister()` | 轻量级睡眠/唤醒钩子 |
| `pm_enable_gpio_wakeup()` | GPIO 引脚唤醒配置 |
| `pm_enable_timer_wakeup()` | 定时器唤醒配置 |
| `pm_get/set_sleep_config()` | 读写睡眠参数 |
| `pm_force_ap_off/on()` | AP 域电源控制 (需 CONFIG_PM_CLOSE_AP) |
| `wifi_twt_setup/teardown()` | WiFi 6 TWT 节能 (需 AP 支持) |
| `wifi_sta_set_dont_wait_bcmc()` | 跳过 BCMC 帧进一步省电 |
