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
    struct lisa_device *next;      // 链表节点
} lisa_device_t;
```

**关键字段：**
- `name`: 设备唯一标识符，用于查找设备
- `api`: 指向设备特定的 API 结构体
- `priv_data`: 硬件平台私有数据（如 HAL 句柄）

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
LISA_DEVICE_REGISTER(name, api_ptr, priv_data_ptr, user_data_ptr, init_fn, priority)
```

**参数：**
- `name`: 设备标识符（自动转为设备名称字符串）
- `api_ptr`: API 结构体指针
- `priv_data_ptr`: 私有数据指针
- `user_data_ptr`: 用户数据（可选）
- `init_fn`: 初始化函数（返回 0 表示成功）
- `priority`: 初始化优先级（使用 LISA_DEVICE_PRIORITY_* 宏）

### 4.2 注册原理

使用链接器段机制（`.lisa_device_registry`），系统初始化时自动扫描并按优先级注册设备。

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

