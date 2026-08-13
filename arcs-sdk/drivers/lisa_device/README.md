# LISA轻量级驱动设备框架

## 一、框架概述

LISA 驱动设备框架提供一套轻量级的统一设备抽象层，实现设备的自动注册、状态管理和访问控制。

---

## 二、框架层次结构

```
┌─────────────────────────────────────────────────────────┐
│  应用层 (Application Layer)                              │
│  • 业务逻辑                                               │
│  • 调用设备 API                                           │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  设备驱动接口层 (Device API Layer)                        │
│  • lisa_gpio.h                                           │
│  • lisa_uart.h                                           │
│  • lisa_spi.h                                            │
│  └─ 设备专用 API 定义 (如 lisa_gpio_api_t)                │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  驱动实现层 (Driver Implementation Layer)                 │
│                                                          │
│  ┌──────────────────────┐   ┌──────────────────────┐   │
│  │  设备框架            │   │  平台适配            │   │
│  │  • lisa_device.h/c   │   │  • lisa_gpio_arcs.c  │   │
│  │  • lisa_device_lock  │   │  • lisa_uart_arcs.c  │   │
│  │  • lisa_device_debug │   │  • API 实现          │   │
│  │  • 注册机制          │   │  • 设备实例          │   │
│  │  • 状态管理          │   │  • 初始化函数        │   │
│  └──────────────────────┘   └──────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  硬件抽象层 (HAL Layer)                                   │
│  • Driver_GPIO.h                                         │
│  • Driver_UART.h                                         │
│  └─ 芯片厂商提供的硬件驱动接口                             │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  硬件层 (Hardware Layer)                                 │
│  • GPIO 控制器                                            │
│  • UART 控制器                                            │
│  • 外设寄存器                                             │
└─────────────────────────────────────────────────────────┘
```

---

## 三、核心组件

### 3.1 设备基类 (`lisa_device_t`)

```c
typedef struct lisa_device {
    const char *name;              // 设备名称（唯一标识）
    lisa_device_state_t state;     // 当前状态
    lisa_device_stats_t stats;     // 统计信息
    void *api;                     // 设备专用 API
    void *priv_data;               // 设备私有数据
    void *user_data;               // 用户自定义数据
#if CONFIG_LISA_PM
    const lisa_device_pm_t *pm;    // 可选 PM 能力描述（system_ops / wakeup_ops）
#endif
    struct lisa_device *next;      // 链表节点
} lisa_device_t;
```

**关键字段：**
- `name`: 设备唯一标识符，用于查找设备
- `api`: 指向设备特定的 API 结构体
- `priv_data`: 硬件平台私有数据（如 HAL 句柄）
- `pm`: 兼容旧 PM 注册宏的可选 PM 能力描述；新驱动推荐使用独立 PM attach 段，`CONFIG_LISA_PM=n` 时字段不存在

### 3.2 设备状态

```c
typedef enum {
    LISA_DEVICE_STATE_UNINITIALIZED,  // 未初始化
    LISA_DEVICE_STATE_INITIALIZED,    // 已初始化（可用）
    LISA_DEVICE_STATE_ERROR,          // 错误状态
} lisa_device_state_t;
```

### 3.3 统一错误码

```c
LISA_DEVICE_OK              // 成功
LISA_DEVICE_ERR_INVALID     // 无效参数
LISA_DEVICE_ERR_NOT_FOUND   // 设备未找到
LISA_DEVICE_ERR_EXISTS      // 设备已存在
LISA_DEVICE_ERR_NOT_READY   // 设备未就绪
LISA_DEVICE_ERR_RANGE       // 参数超出范围
LISA_DEVICE_ERR_IO          // IO 错误
LISA_DEVICE_ERR_INIT_FAIL   // 初始化失败
```

### 3.4 设备优先级

```c
LISA_DEVICE_PRIORITY_CRITICAL   // 0   - 核心系统设备
LISA_DEVICE_PRIORITY_HIGH       // 10  - 重要外设
LISA_DEVICE_PRIORITY_NORMAL     // 50  - 普通设备（默认）
LISA_DEVICE_PRIORITY_LOW        // 90  - 非关键设备
LISA_DEVICE_PRIORITY_LOWEST     // 99  - 可选功能
```

数值越小越先初始化，推荐使用预定义宏。可在标准优先级基础上微调（如 `NORMAL + 1`）。

---

## 四、设备注册机制

### 4.1 静态注册宏

```c
LISA_DEVICE_REGISTER(name, api_ptr, priv_data_ptr, user_data_ptr, init_fn, level, priority)
```

**参数：**
- `name`: 设备标识符（自动转为设备名称字符串）
- `api_ptr`: API 结构体指针
- `priv_data_ptr`: 私有数据指针
- `user_data_ptr`: 用户数据（可选）
- `init_fn`: 初始化函数（返回 0 表示成功）
- `level`: 初始化级别（`LISA_DEVICE_LEVEL_EARLY` / `LISA_DEVICE_LEVEL_NORMAL`）
- `priority`: 初始化优先级（使用 LISA_DEVICE_PRIORITY_* 宏）

当设备需要参与 `lisa_pm` 的 PM 能力（system PM 调度或作为 wakeup-source）时，推荐保持普通设备注册不变，并在同一 C 文件中追加独立 PM 能力声明：

```c
LISA_DEVICE_REGISTER_DEINIT(name, api_ptr, priv_data_ptr, user_data_ptr,
                            init_fn, deinit_fn, level, priority);

#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(name, system_ops, wakeup_ops, ctx);
#endif
```

- `system_ops`: 指向 `const lisa_pm_system_ops_t` 的指针；不支持 system PM 时填 `NULL`。
- `wakeup_ops`: 指向 `const lisa_pm_wakeup_ops_t` 的指针；不支持作唤醒源时填 `NULL`。
- `ctx`: 透传给两组 ops 的所有回调，通常填设备私有数据指针。

`LISA_DEVICE_PM_ATTACH(...)` 只在 `CONFIG_LISA_PM=y` 时生成 PM attach 条目；`CONFIG_LISA_PM=n` 时为空宏，不影响设备本体注册和驱动可用性。现有代码仍可使用兼容宏 `LISA_DEVICE_REGISTER_PM(...)` / `LISA_DEVICE_REGISTER_PM_DEINIT(...)`。

### 4.2 注册原理

使用链接器段机制（`.lisa_device_registry`），系统初始化时自动扫描并按级别、
优先级注册设备。若启用了 `CONFIG_LISA_PM`，`lisa_pm_init()` 会同时扫描设备自带 PM 描述和独立 PM attach 段，自动接管带 `system_ops` 的设备参与 system PM 调度。

---

## 五、核心 API

### 5.1 初始化

```c
int lisa_device_init(void);
```
- 扫描注册段，按优先级调用各设备初始化函数
- 返回成功注册的设备数量
- **必须在系统启动时调用，且仅调用一次**

### 5.2 获取设备

```c
lisa_device_t *lisa_device_get(const char *name);
```
- 通过名称查找设备
- 自动增加引用计数
- 返回 `NULL` 表示未找到

### 5.3 检查设备就绪

```c
bool lisa_device_ready(const lisa_device_t *dev);
```
- 检查设备是否处于 `INITIALIZED` 状态
- **推荐在使用设备前调用**

### 5.4 遍历设备

```c
int lisa_device_foreach(lisa_device_iterator_cb callback, void *user_data);
```
- 遍历所有已注册设备
- 回调返回非 0 时停止遍历

### 5.5 PM 能力（`CONFIG_LISA_PM`）

`CONFIG_LISA_PM=y` 时，设备可声明两类互相独立的 PM 能力：

```c
LISA_DEVICE_PM_ATTACH(name, system_ops, wakeup_ops, ctx)
```

推荐用法是先通过 `LISA_DEVICE_REGISTER()` 或 `LISA_DEVICE_REGISTER_DEINIT()` 完成设备本体注册，再在同一 C 文件中使用 `LISA_DEVICE_PM_ATTACH()` 追加 PM 能力。`name` 必须与前面的设备注册名一致。

该宏会生成独立的 `lisa_device_pm_t` attach 条目：

```c
typedef struct {
    const lisa_pm_system_ops_t *system_ops;
    const lisa_pm_wakeup_ops_t *wakeup_ops;
    void *ctx;
} lisa_device_pm_t;
```

`system_ops` 和 `wakeup_ops` 是两个独立能力槽位：

| 字段 | 回答的问题 | 调用方 | 典型内容 | 不负责 |
|------|------------|--------|----------|--------|
| `system_ops` | 设备当前是否允许系统睡眠，以及睡前/醒后如何快速处理 | `lisa_pm` 系统睡眠流程 | `check_idle()`、睡前停止硬件、唤醒后恢复 HAL/寄存器/pinmux 基础状态 | 配置唤醒条件、使能唤醒源 |
| `wakeup_ops` | 设备能否作为唤醒源，以及如何启停硬件 wakeup | `lisa_device_wakeup_*()` | 缓存唤醒条件、统一下发/撤销硬件唤醒配置 | 判断设备是否忙、睡前挂起、醒后恢复 |

- 两个槽位互不依赖：设备可以只提供 `system_ops`、只提供 `wakeup_ops`，也可以两个都提供；不支持的能力填 `NULL`。
- `ctx` 只透传给 `system_ops` 回调，通常填设备私有数据指针；`wakeup_ops` 回调直接接收 `lisa_device_t *dev`。
- `CONFIG_LISA_PM=n` 时，`LISA_DEVICE_PM_ATTACH()` 为空宏，设备结构中没有 `pm` 字段。
- `LISA_DEVICE_REGISTER_PM()` / `LISA_DEVICE_REGISTER_PM_DEINIT()` 仍保留兼容，现有驱动可继续使用。

#### System PM 调度

`lisa_pm_init()` 会遍历已注册的 `lisa_device`，自动复制带 `system_ops` 的设备到 `lisa_pm` 内部表。驱动侧回调类型为：

```c
typedef struct {
    int32_t (*check_idle)(void *ctx);
    int32_t (*prepare_suspend)(void *ctx);
    int32_t (*resume_restore)(void *ctx);
} lisa_pm_system_ops_t;
```

| 回调 | 语义 |
|------|------|
| `check_idle()` | 只读检查；返回 `1` 表示空闲、允许本轮 deep sleep，返回 `0` 表示忙、阻止本轮 deep sleep |
| `prepare_suspend()` | 睡眠前快速关闭或保存硬件状态；返回 `0` 表示成功 |
| `resume_restore()` | 唤醒后快速恢复 HAL、寄存器、pinmux 等基础状态；返回 `0` 表示成功 |

这些回调运行在 PM 关键路径中，不能等待 mutex/event，不能访问文件系统、网络或执行大块动态内存分配。需要业务级重连、重新配置 IRQ 或恢复 UI 时，应由应用在 `lisa_pm` 的 `after_wake` 回调或普通任务上下文完成。

#### Wakeup-source 控制

`wakeup_ops` 只处理“把这个设备配置成唤醒源”这一件事，不参与 system PM 的 idle 判断、睡前挂起或唤醒恢复。框架提供与源类型无关的总闸接口，配合驱动自己的 configure API 使用：

```c
bool    lisa_device_wakeup_is_capable(lisa_device_t *dev);
int32_t lisa_device_wakeup_enable    (lisa_device_t *dev, bool enable);
bool    lisa_device_wakeup_is_enabled(lisa_device_t *dev);
```

- `lisa_device_wakeup_is_capable()`：等价于检查 `dev->pm != NULL && dev->pm->wakeup_ops != NULL`。
- `lisa_device_wakeup_enable(dev, true)`：调用 driver `set_enabled(true)`，把 driver 内部缓存的 wakeup 配置下发到硬件，并记录 enabled 状态。
- `lisa_device_wakeup_enable(dev, false)`：撤销该 device 已下发的所有硬件 wakeup 配置。
- `lisa_device_wakeup_is_enabled()`：查询设备框架维护的 enabled 状态，不实时向 driver 查询硬件。

驱动侧 `lisa_pm_wakeup_ops_t` 遵循缓存模型：`configure()` / `clear()` 只更新 driver 内部缓存，不下发硬件；只有 `set_enabled(true)` 才统一下发。在线变更触发条件必须按 `enable(false) -> configure/clear -> enable(true)` 顺序执行。缓存为空时 `enable(true)` 返回 0，但不会打开任何硬件唤醒源。

**典型调用顺序**（以 GPIO 为例）：

```c
lisa_device_t *gpiob = lisa_device_get("gpiob");
lisa_gpio_configure(gpiob, 7, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
lisa_gpio_configure_wakeup(gpiob, 7, LISA_GPIO_WAKEUP_LEVEL_LOW);
lisa_device_wakeup_enable(gpiob, true);
```

框架内部用 `CONFIG_LISA_DEVICE_WAKEUP_STATE_POOL_SIZE`（默认 8）大小的静态节点池记录 enabled 状态，受 `uint32_t` 位图约束，上限为 32。

---

## 六、设备驱动开发指南

### 6.1 定义设备 API

每种设备类型需定义专用 API 结构体：

```c
typedef struct {
    int (*config_dir)(lisa_device_t *dev, uint32_t pin, lisa_gpio_dir_t dir);
    int (*set_level)(lisa_device_t *dev, uint32_t pin, lisa_gpio_level_t level);
    int (*get_level)(lisa_device_t *dev, uint32_t pin);
} lisa_gpio_api_t;
```

### 6.2 实现 API 接口函数

```c
static int arcs_gpio_config_dir(lisa_device_t *dev, uint32_t pin, lisa_gpio_dir_t dir)
{
    // 1. 检查设备状态
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    
    // 2. 验证参数
    if (check_pin_valid(dev, pin) != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }
    
    // 3. 获取私有数据并调用底层 HAL
    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    int ret = GPIO_SetDir(priv->hal_handler, pin_to_mask(pin), hal_dir);
    
    return (ret != 0) ? LISA_DEVICE_ERR_IO : LISA_DEVICE_OK;
}
```

### 6.3 定义私有数据和 API

```c
static lisa_gpio_priv_t gpioa_priv;  // 私有数据

static const lisa_gpio_api_t arcs_gpio_api = {  // API 实例
    .config_dir = arcs_gpio_config_dir,
    .set_level = arcs_gpio_set_level,
    .get_level = arcs_gpio_get_level,
};

// 注：不再需要手动定义 lisa_device_t 结构，
// LISA_DEVICE_REGISTER 宏会自动创建设备实例
```

### 6.4 实现初始化函数

```c
static int arcs_gpioa_init(void)
{
    // 1. 获取 HAL 句柄
    gpioa_priv.hal_handler = GPIOA();
    if (!gpioa_priv.hal_handler) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    
    // 2. 初始化硬件
    if (GPIO_Initialize(gpioa_priv.hal_handler, NULL, NULL) != 0) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    
    // 3. 配置私有数据
    gpioa_priv.max_pins = 32;
    
    return LISA_DEVICE_OK;  // 返回 0 表示成功
}
```

### 6.5 注册设备

```c
// 宏会自动创建设备实例并初始化所有成员
// 使用 gpioa 作为标识符，宏会自动生成设备名称字符串 "gpioa"
LISA_DEVICE_REGISTER(gpioa,                         // 设备名称标识符（自动转为字符串）
                     &arcs_gpio_api,                // API 指针
                     &gpioa_priv,                   // 私有数据指针
                     NULL,                          // 用户数据（可选）
                     arcs_gpioa_init,               // 初始化函数
                     LISA_DEVICE_PRIORITY_NORMAL);  // 优先级（使用标准宏）
```

---

## 七、设备使用示例

### 7.1 GPIO 使用流程

```c
// 1. 获取设备
lisa_device_t *gpio = lisa_device_get("gpioa");
if (!gpio) {
    // 设备未找到
    return;
}

// 2. 检查设备就绪
if (!lisa_device_ready(gpio)) {
    // 设备未初始化或处于错误状态
    return;
}

// 3. 配置引脚方向
int ret = lisa_gpio_config_dir(gpio, 5, LISA_GPIO_DIR_OUTPUT);
if (ret != LISA_DEVICE_OK) {
    // 配置失败
    return;
}

// 4. 设置输出电平
ret = lisa_gpio_set_level(gpio, 5, LISA_GPIO_LEVEL_HIGH);
```

### 7.2 便捷的 inline 封装

```c
static inline int lisa_gpio_set_level(lisa_device_t *dev, uint32_t pin, lisa_gpio_level_t level)
{
    if (!dev || !dev->api) {
        return LISA_DEVICE_ERR_INVALID;
    }
    
    lisa_gpio_api_t *api = (lisa_gpio_api_t *)dev->api;
    if (!api->set_level) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }
    
    return api->set_level(dev, pin, level);
}
```

---

## 八、线程安全

框架通过 `LISA_OS` 组件的互斥锁保护：
- 设备注册、查找、遍历
- 设备 API 调用不加锁，由驱动层按需实现

---

## 九、调试支持

### 9.1 调试接口（`CONFIG_LISA_DEVICE_DEBUG`）

```c
void lisa_device_print_registry(void);        // 打印注册表
void lisa_device_print_info(const lisa_device_t *dev);  // 打印设备信息
void lisa_device_print_all(void);             // 打印所有设备
int lisa_device_verify_registry(void);        // 验证注册表完整性
```

### 9.2 统计信息

```c
typedef struct {
    uint32_t ref_count;      // 引用次数
    int init_result;         // 初始化返回值
    uint32_t init_time;      // 初始化耗时 (ms)
    uint32_t init_timestamp; // 初始化时间戳 (ms)
} lisa_device_stats_t;
```

框架依赖 `LISA_OS` 组件，自动提供互斥锁和时间戳支持。

---

## 十、注意事项

1. `LISA_DEVICE_REGISTER` 自动创建设备实例
2. 初始化函数必须提供，返回 0 表示成功
3. 设备名称必须唯一
4. 使用前建议检查 `lisa_device_ready()`
5. 引用计数仅用于统计
