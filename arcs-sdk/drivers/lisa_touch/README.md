# Touch 驱动

基于 lisa_device 框架的 Touch 设备驱动，为 ARCS 平台提供统一的触摸屏接口。

## 功能特性

- **多芯片支持**: 支持 CST328、AXS15231B、FT5336、CST816D、ST77921、BL6133 等触摸芯片
- **总线接口**: I2C 接口通信，自动配置 I2C 总线参数
- **工作模式**: 支持轮询模式和中断模式
- **中断优化**: 中断模式下使用驱动内部任务处理 I2C 读取，回调在任务上下文中安全执行
- **线程安全**: 内部使用互斥锁保护并发访问，自动判断线程和中断上下文
- **灵活配置**: 通过 Kconfig 选择启用的芯片，节省资源

## 配置选项

在 `prj.conf` 中启用驱动并选择芯片：

```kconfig
CONFIG_LISA_TOUCH_DEVICE=y
CONFIG_LISA_TOUCH_ARCS_CST328=y       # 启用 CST328 芯片
# CONFIG_LISA_TOUCH_ARCS_AXS15231B=y  # 启用 AXS15231B 芯片
# CONFIG_LISA_TOUCH_ARCS_FT5336=y     # 启用 FT5336 芯片
# CONFIG_LISA_TOUCH_ARCS_CST816D=y    # 启用 CST816D 芯片
# CONFIG_LISA_TOUCH_ARCS_ST77921=y    # 启用 ST77921 芯片
# CONFIG_LISA_TOUCH_ARCS_BL6133=y     # 启用 BL6133 芯片

CONFIG_LISA_I2C=y                     # 依赖 I2C 驱动
CONFIG_LISA_GPIO_DEVICE=y             # 依赖 GPIO 驱动
```

根据需要选择启用一个或多个触摸芯片。每个芯片注册为独立设备，可同时使用。

## 支持的芯片

| 芯片型号 | 设备名称 | I2C地址 | 分辨率 | 状态 |
|---------|---------|---------|--------|------|
| CST328 | `touch_cst328` | 0x1D | 240x320 | ✅ 已验证 |
| AXS15231B | `touch_axs15231b` | 0x3B | 390x390 | ⚠️ 待验证 |
| FT5336 | `touch_ft5336` | 0x38 | 480x272 | ⚠️ 待验证 |
| CST816D | `touch_cst816d` | 0x15 | 240x240 | ⚠️ 待验证 |
| ST77921 | `touch_st77921` | 0x55 | 480x480 | ⚠️ 待验证 |
| BL6133 | `touch_bl6133` | 0x2C | 320x386 | ⚠️ 待验证 |

## API 接口

### 总线配置

```c
int lisa_touch_attach_bus(lisa_device_t *dev, const lisa_touch_bus_config_t *bus_config);
```

配置触摸设备的总线接口（I2C）。I2C 总线将自动配置为标准速度（100kHz）。

### 触摸控制

```c
int lisa_touch_enable(lisa_device_t *dev);
int lisa_touch_disable(lisa_device_t *dev);
int lisa_touch_get_capabilities(lisa_device_t *dev, lisa_touch_capabilities_t *caps);
```

启用/禁用触摸设备，获取设备能力信息（分辨率、最大触摸点数等）。

### 事件读取

```c
int lisa_touch_read_event(lisa_device_t *dev, lisa_touch_event_t *event);
```

读取触摸事件（按下、释放、坐标等）。

**事件类型说明**:
- `LISA_TOUCH_EVENT_NONE`: 无事件
- `LISA_TOUCH_EVENT_PRESS`: 按下事件（检测到触摸时返回，包含坐标信息）
- `LISA_TOUCH_EVENT_RELEASE`: 释放事件（检测到释放时返回）

**行为说明**:
- 检测到触摸时返回 `PRESS` 事件，包含当前坐标
- 检测到释放时返回 `RELEASE` 事件
- 轮询模式下，每次读取时如果触摸状态为按下，都会返回 `PRESS` 事件（坐标可能更新）

**返回值**:
- `LISA_DEVICE_OK (0)`: 成功读取事件
- `LISA_DEVICE_ERR_INVALID`: 参数无效
- `LISA_DEVICE_ERR_NOT_READY`: 设备未启用或 I2C 未就绪
- `LISA_DEVICE_ERR_NOT_SUPPORT`: 当前处于中断模式，不支持轮询读取
- `LISA_DEVICE_ERR_IO`: 读取失败（无有效触摸数据）

**重要提示**:
- ⚠️ **中断模式下禁止调用此函数**：如果已通过 `lisa_touch_set_int_mode(INTERRUPT)` 设置为中断模式，再调用 `lisa_touch_read_event()` 将返回 `LISA_DEVICE_ERR_NOT_SUPPORT`
- 这是为了避免轮询读取与中断任务产生资源竞争，导致状态混乱
- 中断模式下请使用回调函数接收触摸事件

### 中断模式

```c
int lisa_touch_set_callback(lisa_device_t *dev, lisa_touch_callback_t callback, void *user_data);
int lisa_touch_set_int_mode(lisa_device_t *dev, lisa_touch_int_mode_t mode);
```

设置触摸中断回调函数和中断模式（中断模式/轮询模式）。

## 使用方法

### 基本步骤

1. **配置引脚**: 将对应的 GPIO 引脚配置为 I2C 和 GPIO 功能
2. **获取设备**: 通过 `lisa_device_get()` 获取触摸设备、I2C 设备和 GPIO 设备
3. **配置总线**: 调用 `lisa_touch_attach_bus()` 配置 I2C 总线
4. **设置模式**: 调用 `lisa_touch_set_int_mode()` 设置工作模式（轮询/中断）
5. **启用设备**: 调用 `lisa_touch_enable()` 启用设备
6. **读取事件**: 调用 `lisa_touch_read_event()` 读取触摸事件（轮询模式）或设置回调（中断模式）

### 轮询模式

```c
#include "lisa_touch.h"
#include "lisa_gpio.h"
#include "IOMuxManager.h"
#include "board.h"
#include "pinmux.h"

// 1. 配置 I2C 和 GPIO 引脚（通过 pinmux 重定向或直接配置）
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_i2c0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 8);  // SDA
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 8);  // SCL
}

void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, 0);  // RST
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, 0);  // INT
}
#endif

// 2. 获取设备
lisa_device_t *touch = lisa_device_get("touch_cst328");
lisa_device_t *i2c_dev = lisa_device_get("i2c0");
lisa_device_t *gpio_dev = lisa_device_get("gpioa");

if (!touch || !i2c_dev || !gpio_dev) {
    return -1;
}

// 3. 配置总线
lisa_touch_bus_config_t bus_config = {
    .bus_type = LISA_TOUCH_BUS_I2C,
    .config = {
        .i2c = {
            .i2c_dev = i2c_dev,
            .int_gpio = gpio_dev,
            .int_pin = 24,
            .rst_gpio = gpio_dev,
            .rst_pin = 25,
        }
    }
};
lisa_touch_attach_bus(touch, &bus_config);

// 4. 设置轮询模式（可选，默认就是轮询模式）
lisa_touch_set_int_mode(touch, LISA_TOUCH_INT_MODE_POLLING);

// 5. 启用设备
lisa_touch_enable(touch);

// 6. 获取设备能力
lisa_touch_capabilities_t caps;
lisa_touch_get_capabilities(touch, &caps);
printf("Touch: max_x=%d, max_y=%d, max_points=%d\n",
       caps.max_x, caps.max_y, caps.max_points);

// 7. 循环读取触摸事件
while (1) {
    lisa_touch_event_t event;
    int ret = lisa_touch_read_event(touch, &event);
    
    if (ret == LISA_DEVICE_OK) {
        switch (event.type) {
            case LISA_TOUCH_EVENT_PRESS:
                // 按下事件
                if (event.point_count > 0) {
                    printf("Touch: x=%4d, y=%4d, type=PRESS\n",
                           event.points[0].x, event.points[0].y);
                }
                break;
            case LISA_TOUCH_EVENT_RELEASE:
                printf("Touch: RELEASE\n");
                break;
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));  // 50ms 采样间隔
}
```

### 中断模式

```c
// 触摸事件回调函数（在任务上下文中调用，由驱动内部任务触发）
void touch_event_callback(const lisa_touch_event_t *event, void *user_data)
{
    (void)user_data;
    
    // 此回调在驱动内部的任务上下文中调用，可以安全使用 printf
    switch (event->type) {
        case LISA_TOUCH_EVENT_PRESS:
            // 按下事件
            if (event->point_count > 0) {
                printf("Touch: x=%4d, y=%4d, type=PRESS\n",
                       event->points[0].x, event->points[0].y);
            }
            break;
        case LISA_TOUCH_EVENT_RELEASE:
            // 释放事件
            printf("Touch: type=RELEASE\n");
            break;
    }
}

int main(void)
{
    // ... 配置引脚和总线（同上）...
    
    // 设置触摸事件回调函数
    lisa_touch_set_callback(touch, touch_event_callback, NULL);
    
    // 设置中断模式
    lisa_touch_set_int_mode(touch, LISA_TOUCH_INT_MODE_INTERRUPT);
    
    // 启用设备
    lisa_touch_enable(touch);

    // 主循环：触摸事件通过回调自动处理
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("System running, waiting for touch events...\n");
    }
}
```

**中断模式说明**:
- 驱动内部创建专用任务处理 I2C 读取
- GPIO 中断回调只发送任务通知，不执行耗时操作
- 用户回调在任务上下文中调用，可以安全使用 `printf`、`malloc` 等操作
- 回调直接收到完整的触摸数据（包括坐标），无需再次读取
- ⚠️ **中断模式下不能调用 `lisa_touch_read_event()`**，会返回 `LISA_DEVICE_ERR_NOT_SUPPORT` 错误

### 模式切换

轮询模式和中断模式可以动态切换，但同一时间只能使用一种模式：

```c
// 从轮询模式切换到中断模式
lisa_touch_set_callback(touch, touch_event_callback, NULL);  // 先设置回调
lisa_touch_set_int_mode(touch, LISA_TOUCH_INT_MODE_INTERRUPT);  // 切换到中断模式
// 此后不能再调用 lisa_touch_read_event()

// 从中断模式切换回轮询模式
lisa_touch_set_int_mode(touch, LISA_TOUCH_INT_MODE_POLLING);  // 切换到轮询模式
// 此后可以调用 lisa_touch_read_event()，回调不再触发
```

**重要提示**:
- 中断模式下禁止调用 `lisa_touch_read_event()`，避免资源竞争
- 轮询模式下回调函数不会被调用（即使已设置）
- 切换模式前无需禁用设备，可以动态切换

## 硬件配置

### 引脚复用配置

Touch 驱动依赖 I2C 和 GPIO 驱动，引脚复用配置由 I2C 和 GPIO 驱动在初始化时自动完成。

**配置位置**:
- **I2C 引脚**: `boards/<板型名>/pinmux.c` 中的 `lisa_i2c0_pinmux()` 或 `lisa_i2c1_pinmux()` 函数
- **GPIO 引脚**: `boards/<板型名>/pinmux.c` 中的 `lisa_gpioa_pinmux()` 或 `lisa_gpiob_pinmux()` 函数
- **调用时机**: I2C 和 GPIO 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):

```c
void lisa_i2c0_pinmux()
{
    /* I2C0: PA22=SDA, PA23=SCL, 功能码 8 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 8);
}

void lisa_gpioa_pinmux()
{
    /* GPIO: PA25=RST, PA24=INT, 功能码 0 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, 0);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, 0);
}
```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 如需自定义引脚配置，可在 sample 中重写 pinmux 函数（参考 `samples/drivers/devices/lisa_touch/interrupt_mode/src/main.c`）
- 触摸驱动需要以下引脚：
  - I2C SDA/SCL：用于 I2C 通信
  - INT：触摸中断引脚（中断模式需要）
  - RST：芯片复位引脚

### 芯片特性

不同芯片的 I2C 地址、分辨率、触发方式可能不同，请参考具体芯片手册和"支持的芯片"表格。

## 工作模式对比

| 特性 | 轮询模式 | 中断模式 |
|------|---------|---------|
| **响应速度** | 取决于轮询间隔（通常 50ms） | 立即响应（< 1ms） |
| **CPU 占用** | 高（持续轮询） | 低（事件驱动） |
| **功耗** | 高 | 低 |
| **实现复杂度** | 简单 | 中等 |
| **适用场景** | 简单应用、对响应时间不敏感 | 需要快速响应、低功耗应用 |
| **回调上下文** | 任务上下文（安全） | 任务上下文（安全） |

## 注意事项

1. **I2C 自动配置**: `lisa_touch_attach_bus()` 会自动配置 I2C 总线为标准速度（100kHz），无需手动调用 `lisa_i2c_configure()`

2. **引脚配置**: 使用前需配置对应的 GPIO 引脚，驱动初始化时会自动调用 pinmux 函数，或通过 pinmux 重定向自定义引脚

3. **设备依赖**: 需要确保 I2C 和 GPIO 设备已就绪（通过 `lisa_device_ready()` 检查）

4. **中断回调**: 中断模式下，用户回调在驱动内部任务上下文中调用，可以安全使用 `printf`、`malloc` 等操作

5. **线程安全**: 驱动内部已实现线程保护，可在多线程环境中使用

6. **芯片特性**: 不同芯片的坐标系、分辨率、触发方式可能不同，请参考具体芯片手册

7. **设备选择**: 使用前需在 `prj.conf` 中启用对应的触摸芯片和依赖驱动（I2C、GPIO）

8. **多芯片使用**: 每个芯片注册为独立设备，可同时使用多个触摸芯片

## 文件说明

- `lisa_touch.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_touch_cst328.c` - CST328 芯片驱动实现
- `lisa_touch_axs15231b.c` - AXS15231B 芯片驱动实现
- `lisa_touch_ft5336.c` - FT5336 芯片驱动实现
- `lisa_touch_cst816d.c` - CST816D 芯片驱动实现
- `lisa_touch_st77921.c` - ST77921 芯片驱动实现
- `lisa_touch_bl6133.c` - BL6133 芯片驱动实现
- `Kconfig` - 主配置选项
- `Kconfig.arcs.*` - 各芯片配置选项
- `CMakeLists.txt` - 构建配置
- `README.md` - 驱动使用说明
