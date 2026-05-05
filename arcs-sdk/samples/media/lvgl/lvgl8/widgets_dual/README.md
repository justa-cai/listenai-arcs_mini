# LVGL8 Widgets Dual 双屏控件演示示例

## 功能说明

演示 LVGL 8.x 在双屏场景下的 Widgets 控件显示能力。

本示例基于 LISA Display 多实例显示适配层，为两块 ST7789P3 屏幕分别注册独立的 `lv_disp_t`，并在两块屏上分别启动 `lv_demo_widgets()`。当前版本保留单路触摸输入：

- `display`：绑定 `touch_cst328`，支持触摸交互
- `display1`：仅显示，不绑定触摸输入

此外，示例中保留了 `i2c1` 的 pinmux 入口，作为后续第二路触摸多实例支持时的硬件预留。

## 硬件连接

### LCD0 显示屏（`display` / SPI1）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SPI_CLK | PB3 | SPI1 时钟 |
| SPI_DATA | PB1 | SPI1 数据 |
| CS | PB5 | LCD0 片选 |
| DC | PB0 | LCD0 数据/命令选择 |
| RST | PA1 | LCD0 复位 |
| PWM | PA0 | LCD0 背光 PWM |

### LCD1 显示屏（`display1` / SPI0）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SPI_CLK | PA15 | SPI0 时钟 |
| SPI_DATA | PA14 | SPI0 数据 |
| CS | PA12 | LCD1 片选 |
| DC | PA13 | LCD1 数据/命令选择 |
| PWM | PA11 | LCD1 背光 PWM |

### Touch0 触摸屏（仅绑定 `display`）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SDA | PA22 | I2C0 数据 |
| SCL | PA23 | I2C0 时钟 |
| INT | PA24 | 触摸中断 |
| RST | PA25 | 触摸复位 |

### Touch1 预留（当前未启用）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SDA | PA18 | I2C1 数据预留 |
| SCL | PA19 | I2C1 时钟预留 |

## 示例步骤

1. 初始化 LVGL 库
2. 配置双屏 GPIO/SPI/PWM 引脚复用
3. 初始化 `display` 和 `display1`，分别绑定各自 SPI 总线
4. 为两块屏分别注册 LVGL 显示驱动
5. 初始化 `touch_cst328`，并将输入设备绑定到 `display`
6. 打开两块屏背光并设置亮度
7. 在 `display` 和 `display1` 上分别运行 `lv_demo_widgets()`
8. 在主循环中持续调用 `lv_timer_handler()` 处理刷新

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
LVGL8 dual widgets start
```

**屏幕显示：**
- LCD0 显示 LVGL Widgets Demo，支持触摸操作
- LCD1 显示 LVGL Widgets Demo，仅显示不响应触摸
- 左上角可显示性能监控信息（启用 `CONFIG_LV_USE_PERF_MONITOR=y` 时）

## 核心 API

| API | 说明 |
|-----|------|
| `lv_init()` | 初始化 LVGL 库 |
| `lisa_device_get()` | 获取设备句柄 |
| `lisa_display_attach_bus()` | 配置显示设备总线 |
| `lv_port_disp_register()` | 为指定显示设备注册一个 `lv_disp_t` |
| `lisa_touch_attach_bus()` | 配置触摸设备总线 |
| `lv_port_indev_register()` | 为指定显示设备注册输入设备 |
| `lisa_display_blanking_off()` | 打开显示输出 |
| `lisa_display_set_brightness()` | 设置背光亮度 |
| `lv_demo_widgets()` | 启动 widgets 控件演示 |
| `lv_timer_handler()` | LVGL 定时刷新入口 |

## 关键代码

### 双屏显示初始化

```c
disp0 = lv_port_disp_register(display0);
disp1 = lv_port_disp_register(display1);

lisa_display_blanking_off(display0);
lisa_display_blanking_off(display1);
lisa_display_set_brightness(display0, 80);
lisa_display_set_brightness(display1, 80);
```

### 单路触摸绑定到屏0

```c
if (init_touch0() != 0) {
    return -1;
}

indev0 = lv_port_indev_register(touch0_dev, display0, disp0);
if (!indev0) {
    return -1;
}
```

### 两块屏分别创建 Widgets Demo

```c
lv_disp_set_default(disp0);
lv_demo_widgets();

lv_disp_set_default(disp1);
lv_demo_widgets();
```

## 配置说明

### 必需的配置项（`prj.conf`）

```kconfig
CONFIG_LISA_DEVICE=y
CONFIG_LISA_GPIO_DEVICE=y
CONFIG_LISA_GPIOA=y
CONFIG_LISA_GPIOB=y
CONFIG_LISA_PWM=y
CONFIG_LISA_SPI0=y
CONFIG_LISA_SPI1=y
CONFIG_LISA_I2C=y
CONFIG_LISA_I2C0=y
CONFIG_LISA_I2C1=y

CONFIG_LISA_DUAL_DISPLAY=y
CONFIG_LISA_DISPLAY_DEVICE=y
CONFIG_LISA_DISPLAY_PANEL_ST7789P3=y
CONFIG_LISA_DISPLAY_BUS_SPI_4WIRE=y

CONFIG_LISA_TOUCH_DEVICE=y
CONFIG_LISA_TOUCH_ARCS_CST328=y

CONFIG_SDK_MODULE_LVGL8=y
CONFIG_LV_USE_DEMO_WIDGETS=y
CONFIG_LV_USE_GPU_CSK_DMA2D=y
CONFIG_LV_DOUBLE_VDB=y
CONFIG_LV_USE_PERF_MONITOR=y
```

### 关键显示配置

```kconfig
CONFIG_PANEL_ST7789P3_WIDTH=240
CONFIG_PANEL_ST7789P3_HEIGHT=320
CONFIG_LV_DRIVER_ROTATE_270=y
CONFIG_LISA_DISPLAY_COLOR_INVERT=y
CONFIG_LV_DRIVER_FULL_REFRESH=y
CONFIG_LV_REFR_UNCONDITIONAL_AREA_JOIN=y
CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y
```

## 注意事项

1. **当前触摸能力边界**：
   - 目前 sample 只启用一路触摸
   - `touch_cst328` 绑定到 `display`
   - `display1` 暂不支持独立触摸

2. **`i2c1` 仅为预留**：
   - `lisa_i2c1_pinmux()` 已保留
   - 但当前没有在 sample 中启用第二路 touch 初始化
   - 需要等 touch 驱动支持多实例后再接入

3. **双屏初始化顺序**：
   - 先 `lv_init()`
   - 再 attach 两块 display
   - 再注册两个 `lv_disp_t`
   - 最后再注册 `touch0 -> disp0`

4. **多屏 Demo 的创建方式**：
   - 本示例通过 `lv_disp_set_default()` 切换默认显示
   - 然后分别调用两次 `lv_demo_widgets()`
   - 若后续需要完全不同的双屏异显内容，建议改为分别创建独立 screen/root 组件

5. **性能与内存**：
   - 双屏渲染对显存和带宽要求更高
   - 建议保留 `CONFIG_LV_USE_GPU_CSK_DMA2D=y`、`CONFIG_LV_DOUBLE_VDB=y`
   - 当前 `CONFIG_PSRAM_HEAP_SIZE=0x200000` 用于保证 LVGL 内存充足
