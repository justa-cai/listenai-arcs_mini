# ARCS Touch 组件

## 概述

Touch组件是ARCS SDK中用于触摸屏控制的统一接口层，提供了标准的触摸屏驱动抽象和通用API。该组件支持多种触摸屏芯片，并提供统一的编程接口。

## 特性

- 🎯 **统一接口** - 为不同触摸屏芯片提供统一的API接口
- 🔧 **多芯片支持** - 支持6种主流触摸屏控制器芯片
- ⚡ **中断支持** - 支持触摸中断处理机制
- 📐 **坐标变换** - 支持X/Y轴反转和坐标轴交换
- 🎛️ **可配置** - 通过Kconfig系统进行灵活配置

## 支持的触摸屏芯片

| 芯片型号 | 配置选项 | 描述 |
|---------|---------|------|
| AXS15231B | `CONFIG_LISA_TOUCH_AXS15231B` | AXS15231B触摸控制器 |
| ST77921 | `CONFIG_LISA_TOUCH_ST77921` | ST77921触摸控制器（默认） |
| CST816D | `CONFIG_LISA_TOUCH_CST816D` | CST816D触摸控制器 |
| FT5336 | `CONFIG_LISA_TOUCH_FT5336` | FT5336触摸控制器 |
| BL6133 | `CONFIG_LISA_TOUCH_BL6133` | BL6133触摸控制器 |
| CST328 | `CONFIG_LISA_TOUCH_CST328` | CST328触摸控制器 |


## 核心数据结构

### 硬件配置结构体

```c
typedef struct {
    void *i2c_dev;              // I2C设备句柄
    
    // I2C引脚配置
    struct {
        struct {
            uint8_t sda_pad;        // SDA引脚pad
            uint8_t sda_pin;        // SDA引脚号
            uint8_t sda_func;       // SDA引脚功能
        } sda;
        struct {
            uint8_t scl_pad;        // SCL引脚pad
            uint8_t scl_pin;        // SCL引脚号
            uint8_t scl_func;       // SCL引脚功能
        } scl;
    } i2c_pins;
    
    // GPIO引脚配置
    struct {
        struct {
            uint8_t reset_pad;      // 复位引脚pad
            uint8_t reset_pin;      // 复位引脚号
            uint8_t reset_func;     // 复位引脚功能
        } reset;
        struct {
            uint8_t int_pad;        // 中断引脚pad
            uint8_t int_pin;        // 中断引脚号
            uint8_t int_func;       // 中断引脚功能
        } intr;
    } gpio_pins;
} touch_hw_config_t;
```

### 触摸设备结构体

```c
struct touch_device {
    const char *name;                                              // 设备名称
    int (*device_init)(const touch_hw_config_t *config);         // 设备初始化函数
    const struct touch_driver_api *api;                          // 设备API接口
};
```

### 触摸驱动API接口

```c
struct touch_driver_api {
    int (*read_coordinates)(uint16_t *x, uint16_t *y, bool *pressed);  // 读取坐标
    void (*set_int_callback)(lisa_touch_callback_t cb);                // 设置中断回调
    int (*set_enable)(bool enable);                                    // 启用/禁用触摸
    int (*set_inverted_x)(bool inverted);                             // X轴反转
    int (*set_inverted_y)(bool inverted);                             // Y轴反转
    int (*set_swap_xy)(bool swap);                                     // XY轴交换
};
```

## API接口

### 设备管理

#### `lisa_touch_create()`
```c
void *lisa_touch_create(touch_hw_config_t *config);
```
创建并初始化触摸设备实例。

**参数:**
- `config`: 硬件配置结构体指针

**返回值:**
- 成功: 触摸设备结构体指针
- 失败: NULL

### 坐标读取

#### `lisa_touch_read_coordinates()`
```c
int lisa_touch_read_coordinates(const struct touch_device *dev, uint16_t *x, uint16_t *y, bool *pressed);
```
读取当前触摸坐标。

**参数:**
- `dev`: 触摸设备指针
- `x`: X坐标存储指针
- `y`: Y坐标存储指针
- `pressed`: 触摸状态存储指针

**返回值:**
- `0`: 成功
- `<0`: 失败

### 中断处理

#### `lisa_touch_set_int_callback()`
```c
void lisa_touch_set_int_callback(const struct touch_device *dev, lisa_touch_callback_t cb);
```
设置触摸中断回调函数。

**参数:**
- `dev`: 触摸设备指针
- `cb`: 中断回调函数

**注意:**
- 回调函数在中断上下文中执行
- 回调函数应保持简短，避免阻塞操作

### 坐标变换

#### `lisa_touch_set_inverted_x()`
```c
int lisa_touch_set_inverted_x(const struct touch_device *dev, bool inverted);
```
设置X轴坐标是否反转。

#### `lisa_touch_set_inverted_y()`
```c
int lisa_touch_set_inverted_y(const struct touch_device *dev, bool inverted);
```
设置Y轴坐标是否反转。

#### `lisa_touch_set_swap_xy()`
```c
int lisa_touch_set_swap_xy(const struct touch_device *dev, bool swap);
```
设置是否交换X和Y坐标轴。

### 设备控制

#### `lisa_touch_set_enable()`
```c
int lisa_touch_set_enable(const struct touch_device *dev, bool enable);
```
启用或禁用触摸屏。

## 配置选项

### Kconfig配置

```kconfig
# 启用触摸支持
CONFIG_LISA_TOUCH=y

# 选择触摸芯片（仅能选择一个）
CONFIG_LISA_TOUCH_ST77921=y
# CONFIG_LISA_TOUCH_AXS15231B=y
# CONFIG_LISA_TOUCH_CST816D=y
# CONFIG_LISA_TOUCH_FT5336=y
# CONFIG_LISA_TOUCH_BL6133=y
# CONFIG_LISA_TOUCH_CST328=y

# 启用中断支持
CONFIG_LISA_TOUCH_INTERRUPT=y

# 设置触摸读取频率
CONFIG_LISA_TOUCH_READ_FREQUENCY=20
```

### 硬件配置示例

```c
touch_hw_config_t touch_config = {
    .i2c_dev = I2C0(),
    .i2c_pins = {
        .sda = {
            .sda_pad = CSK_IOMUX_PAD_A,
            .sda_pin = 11,
            .sda_func = 2
        },
        .scl = {
            .scl_pad = CSK_IOMUX_PAD_A,
            .scl_pin = 10,
            .scl_func = 2
        }
    },
    .gpio_pins = {
        .reset = {
            .reset_pad = CSK_IOMUX_PAD_A,
            .reset_pin = 12,
            .reset_func = 0
        },
        .intr = {
            .int_pad = CSK_IOMUX_PAD_A,
            .int_pin = 13,
            .int_func = 0
        }
    }
};
```

## 使用示例

### 基本使用

```c
#include "lisa_touch.h"

// 定义硬件配置
touch_hw_config_t config = {
    // ... 硬件配置参数
};

// 创建触摸设备
void *touch_dev = lisa_touch_create(&config);
if (touch_dev == NULL) {
    // 初始化失败处理
    return -1;
}

// 读取触摸坐标
uint16_t x, y;
bool pressed;
int ret = lisa_touch_read_coordinates(touch_dev, &x, &y, &pressed);
if (ret == 0 && pressed) {
    // 处理触摸事件
    printf("Touch at (%d, %d)\n", x, y);
}
```

### 中断处理示例

```c
#include "lisa_touch.h"

static void touch_interrupt_handler(void)
{
    // 中断处理逻辑
    // 注意：此函数在中断上下文中执行，应保持简短
    touch_event_flag = true;
}

int main(void)
{
    // 创建触摸设备
    void *touch_dev = lisa_touch_create(&config);
    
    // 设置中断回调
    lisa_touch_set_int_callback(touch_dev, touch_interrupt_handler);
    
    // 主循环
    while (1) {
        if (touch_event_flag) {
            uint16_t x, y;
            bool pressed;
            lisa_touch_read_coordinates(touch_dev, &x, &y, &pressed);
            // 处理触摸事件
            touch_event_flag = false;
        }
    }
}
```

### 坐标变换示例

```c
// 适配不同的屏幕方向
lisa_touch_set_inverted_x(touch_dev, true);    // X轴反转
lisa_touch_set_inverted_y(touch_dev, false);   // Y轴正常
lisa_touch_set_swap_xy(touch_dev, true);       // 交换XY轴
```

## 注意事项

1. **单一芯片支持**: 同一时间只能配置一种触摸芯片
2. **中断上下文**: 中断回调函数在中断上下文中执行，应避免长时间阻塞操作
3. **硬件初始化**: 使用前必须正确配置硬件引脚和I2C接口
4. **线程安全**: API接口设计为线程安全，但具体实现取决于底层驱动
5. **坐标系统**: 坐标原点通常在屏幕左上角，X轴向右，Y轴向下

