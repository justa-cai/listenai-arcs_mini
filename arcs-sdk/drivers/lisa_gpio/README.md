# GPIO 驱动

基于 lisa_device 框架的 GPIO 设备驱动，为 ARCS 平台提供统一的 GPIO 操作接口。

## 功能特性

- **设备支持**: GPIOA、GPIOB 两个 GPIO 控制器
- **引脚配置**: 支持输入/输出模式、上下拉电阻、去抖动
- **IO 操作**: 引脚电平读写
- **中断支持**: 多种触发模式（上升沿、下降沿、双边沿、高低电平）

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_GPIO_DEVICE=y
CONFIG_LISA_GPIOA=y        # 启用 GPIOA 设备
CONFIG_LISA_GPIOB=y        # 启用 GPIOB 设备
```

根据需要选择启用 GPIOA 或 GPIOB 设备。

## API 接口

### 引脚配置

```c
int lisa_gpio_configure(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t flags);
int lisa_gpio_get_config(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t *flags);
```

### IO 操作

```c
int lisa_gpio_read_pin(lisa_device_t *dev, uint32_t pin);   // 返回 LISA_GPIO_LOW/HIGH
int lisa_gpio_write_pin(lisa_device_t *dev, uint32_t pin, uint32_t value);
```

### 中断管理

```c
int lisa_gpio_configure_irq(lisa_device_t *dev, uint32_t pin, lisa_gpio_irq_mode_t mode, 
                            lisa_gpio_irq_callback_t callback, void *user_data);
int lisa_gpio_enable_irq(lisa_device_t *dev, uint32_t pin);
int lisa_gpio_disable_irq(lisa_device_t *dev, uint32_t pin);
```

## 使用示例

### 输出模式

```c
#include "lisa_gpio.h"

// 获取 GPIOA 设备
lisa_device_t *gpioa = lisa_device_get_by_name("gpioa");
if (!gpioa) {
    return -1;
}

// 配置引脚 5 为输出模式，初始电平为高
lisa_gpio_configure(gpioa, 5, LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_HIGH);

// 设置高电平
lisa_gpio_write_pin(gpioa, 5, LISA_GPIO_HIGH);

// 设置低电平
lisa_gpio_write_pin(gpioa, 5, LISA_GPIO_LOW);
```

### 输入模式

```c
// 配置引脚 3 为输入上拉模式
lisa_gpio_configure(gpioa, 3, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);

// 配置引脚 4 为输入下拉模式，启用去抖动
lisa_gpio_configure(gpioa, 4, LISA_GPIO_INPUT | LISA_GPIO_PULL_DOWN | LISA_GPIO_DEBOUNCE);

// 读取引脚电平
int level = lisa_gpio_read_pin(gpioa, 3);
if (level == LISA_GPIO_HIGH) {
    // 高电平处理
}
```

### 中断模式

```c
// 中断回调函数
void gpio_irq_handler(uint32_t pin, void *user_data)
{
    // 中断处理逻辑（在中断上下文中执行，应尽量简短）
    printf("GPIO Pin %d interrupt triggered\n", pin);
}

// 配置引脚 10 为输入上拉模式
lisa_gpio_configure(gpioa, 10, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);

// 配置上升沿中断
lisa_gpio_configure_irq(gpioa, 10, LISA_GPIO_IRQ_EDGE_RISING,
                        gpio_irq_handler, NULL);

// 启用中断
lisa_gpio_enable_irq(gpioa, 10);
```

## 配置标志位

驱动使用标志位方式进行配置，可以通过按位或（`|`）组合多个标志：

### 方向标志
- `LISA_GPIO_INPUT` - 输入模式（默认）
- `LISA_GPIO_OUTPUT` - 输出模式

### 上下拉标志
- `LISA_GPIO_PULL_UP` - 启用上拉电阻
- `LISA_GPIO_PULL_DOWN` - 启用下拉电阻

### 去抖动标志
- `LISA_GPIO_DEBOUNCE` - 启用去抖动

### 初始电平标志（仅用于输出模式）
- `LISA_GPIO_OUTPUT_INIT_LOW` - 输出初始化为低电平
- `LISA_GPIO_OUTPUT_INIT_HIGH` - 输出初始化为高电平

### 示例组合
```c
// 输入模式 + 上拉
LISA_GPIO_INPUT | LISA_GPIO_PULL_UP

// 输入模式 + 下拉 + 去抖动
LISA_GPIO_INPUT | LISA_GPIO_PULL_DOWN | LISA_GPIO_DEBOUNCE

// 输出模式 + 初始高电平
LISA_GPIO_OUTPUT | LISA_GPIO_OUTPUT_INIT_HIGH
```

## 中断触发模式

- `LISA_GPIO_IRQ_EDGE_RISING` - 上升沿触发
- `LISA_GPIO_IRQ_EDGE_FALLING` - 下降沿触发
- `LISA_GPIO_IRQ_EDGE_BOTH` - 双边沿触发
- `LISA_GPIO_IRQ_LEVEL_HIGH` - 高电平触发
- `LISA_GPIO_IRQ_LEVEL_LOW` - 低电平触发

## 硬件配置

### 引脚复用配置

GPIO 驱动在初始化时会自动调用板型目录中定义的引脚复用函数，用于配置 GPIO 引脚的复用功能。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_gpioa_pinmux()` 和 `lisa_gpiob_pinmux()` 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明 `void lisa_gpioa_pinmux()` 和 `void lisa_gpiob_pinmux()`
- **调用时机**: GPIOA/GPIOB 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):
```c
void lisa_gpioa_pinmux()
{
    // 配置 PA20 为 GPIO 功能（功能码 1）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, 1);
    // 配置 PA21 为 GPIO 功能（功能码 0，默认 GPIO）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, 0);
}

void lisa_gpiob_pinmux()
{
    // 配置 PB08 为 GPIO 功能（功能码 0）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 8, 0);
}
```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 只需配置实际使用的 GPIO 引脚
- 功能码需根据芯片手册确定，通常 GPIO 功能对应功能码 0 或 1

## 注意事项

1. **引脚范围**: 每个 GPIO 控制器支持 0-31 共 32 个引脚
2. **中断回调**: 在中断上下文执行，应保持简短快速
3. **线程安全**: 驱动内部使用互斥锁保护中断配置操作
4. **设备初始化**: 使用前需确保设备已正确初始化

## 文件说明

- `lisa_gpio.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_gpio_arcs.c` - ARCS 平台适配实现
- `CMakeLists.txt` - 构建配置
- `Kconfig` - 配置选项
