# Touch 示例

触摸（CST328）示例，配合 ST7789P3 显示屏（SPI 四线，240×320）。无须先点亮 LCD 才能工作，可直接初始化触摸并在中断到来时打印坐标。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **触摸初始化**：初始化 CST328 触摸芯片
- **中断回调**：注册触摸中断回调函数
- **坐标读取**：读取触摸点的 x、y 坐标和按压状态
- **轮询模式**：主循环轮询中断标志

### 硬件配置

#### 显示（ST7789P3，SPI1）

| 信号 | 引脚 |
| --- | --- |
| LCD_CS | PB05 |
| LCD_CLK | PB03 |
| LCD_SDA(MOSI) | PB01 |
| LCD_DC | PB00 |
| LCD_RST | PA01 |
| LCD_BL(PWM) | PA00 |

#### 触摸（CST328，I2C0）

| 信号 | 引脚 |
| --- | --- |
| TS_SDA | PA22 |
| TS_SCL | PA23 |
| TS_RST | PA25 |
| TS_INT | PA24 |

### 硬件要求

- ARCS 系列开发板
- ST7789P3 显示屏（SPI 四线，240×320）
- CST328 触摸芯片

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/touch
./build.sh
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 2. 烧录运行

将开发板连接到 PC，执行烧录命令：

```bash
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/app.bin
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/app.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板，触摸屏幕可以看到串口输出坐标信息。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_LISA_DISPLAY=y
CONFIG_LISA_DISPLAY_ST7789P3=y
CONFIG_LISA_DISPLAY_INTERFACE_SPI_4_LINE=y
CONFIG_DISPLAY_ST7789P3_WIDTH=240
CONFIG_DISPLAY_ST7789P3_HEIGHT=320

CONFIG_LISA_TOUCH=y
CONFIG_LISA_TOUCH_CST328=y

CONFIG_SYSLOG_UART_TX_PIN=3
CONFIG_SYSLOG_UART_TX_PIN_FUNC_SEL=2
CONFIG_SYSLOG_UART_PORT=0
CONFIG_SYSLOG_UART_BAUDRATE=921600
```

### 触摸配置

若实际连线不同，请在 `src/main.c` 的 `touch_hw_config_t` 中修改 PAD/PIN/功能复用后重新编译。

## 📋 代码解析

### 关键代码段

#### 1. 全局变量

```c
void *touch_device = NULL;
static volatile bool touch_int_trigger = false;
```

#### 2. 触摸中断回调

```c
/**
    该函数在中断上下文
 */
static void touch_int_callback(void)
{
    touch_int_trigger = true;
}
```

#### 3. 触摸硬件配置

```c
touch_hw_config_t config = {
    .i2c_dev = I2C0(),
    .i2c_pins = {
        .sda = {
            .sda_pad = CSK_IOMUX_PAD_A,
            .sda_pin = 22,
            .sda_func = CSK_IOMUX_FUNC_ALTER8,
        },
        .scl = {
            .scl_pad = CSK_IOMUX_PAD_A,
            .scl_pin = 23,
            .scl_func = CSK_IOMUX_FUNC_ALTER8,
        },
    },
    .gpio_pins.reset.reset_pad = CSK_IOMUX_PAD_A,
    .gpio_pins.reset.reset_pin = 25,
    .gpio_pins.reset.reset_func = CSK_IOMUX_FUNC_DEFAULT,
    .gpio_pins.intr.int_pad = CSK_IOMUX_PAD_A,
    .gpio_pins.intr.int_pin = 24,
    .gpio_pins.intr.int_func = CSK_IOMUX_FUNC_DEFAULT,
};
```

#### 4. 创建触摸设备

```c
touch_device = lisa_touch_create(&config);
if (touch_device == NULL) {
    LOGE("lisa_touchy_create fail.\r\n");
    return -1;
}
```

#### 5. 设置中断回调

```c
lisa_touch_set_int_callback(touch_device, touch_int_callback);
```

#### 6. 主循环读取坐标

```c
while (1) {
    if (touch_int_trigger == true) {
        touch_int_trigger = false;
        bool pressed = false;
        lisa_touch_read_coordinates(touch_device, &x, &y, &pressed);
        LOGI("touch: x=%d, y=%d, pressed=%d.", x, y, pressed);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
touch sample start.
```

触摸屏幕时，会输出坐标与按压状态：

```
touch: x=120, y=200, pressed=1
```

松开时：

```
touch: x=120, y=200, pressed=0
```

输出说明：
- `touch sample start.`：程序启动信息
- `touch: x=120, y=200, pressed=1/0`：触摸坐标和按压状态
  - `x`、`y`：触摸点坐标
  - `pressed=1`：按下
  - `pressed=0`：松开
- 不触摸时无持续输出（主循环 100ms 轮询中断标志）

## ⚠️ 注意事项

1. **触摸初始化**：
   - 对于 ST7789P3 方案，触摸初始化无需先点亮 LCD，可直接初始化触摸（代码注释第 27 行）

2. **中断上下文**：
   - 触摸中断回调函数在中断上下文中执行（代码注释第 14 行）

3. **引脚配置**：
   - I2C0 用于触摸通信（SDA: PA22, SCL: PA23）
   - GPIO 用于复位和中断（RST: PA25, INT: PA24）

4. **轮询周期**：
   - 主循环每 100ms 检查一次中断标志（`vTaskDelay(pdMS_TO_TICKS(100))`）

5. **坐标读取**：
   - 使用 `lisa_touch_read_coordinates()` 读取坐标
   - 返回 x、y 坐标和 pressed 状态

6. **引脚修改**：
   - 若实际连线不同，请在 `src/main.c` 的 `touch_hw_config_t` 中修改 PAD/PIN/功能复用后重新编译
