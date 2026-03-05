# LVGL7 Benchmark 性能测试示例

## 功能说明

演示 LVGL 7.x 图形库的 Benchmark 性能测试功能，用于评估图形渲染性能和帧率。

本示例使用 LISA Display 和 LISA Touch 设备框架，通过 SPI 接口驱动 ST7789P3 LCD 屏幕，通过 I2C 接口驱动 CST328 电容触摸屏。Benchmark 会自动运行一系列图形测试场景，并显示每个场景的 FPS（帧率）和 CPU 占用率。

## 硬件连接

### LCD 显示屏（SPI 接口）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SPI_CLK | PB3 | SPI 时钟 |
| SPI_DATA | PB1 | SPI 数据 |
| CS | PB5 | 片选 |
| DC | PB (LCD_CD_PIN) | 数据/命令选择 |
| RST | PA (LCD_RST_PIN) | 复位 |
| PWM | PA (LCD_PWM_PIN) | 背光 PWM |

### 触摸屏（I2C 接口）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SDA | PA22 | I2C 数据 |
| SCL | PA23 | I2C 时钟 |
| INT | PA24 | 中断 |
| RST | PA25 | 复位 |

## 示例步骤

1. 初始化 LVGL 库
2. 配置 GPIO 引脚复用
3. 初始化 Display 设备并配置 SPI 总线
4. 初始化 Touch 设备并配置 I2C 总线
5. 启动 LVGL benchmark 性能测试
6. 在 UI 任务中循环调用 `lv_task_handler()` 处理界面刷新

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
LVGL benchmark start
```

**屏幕显示：**
- 自动运行多个图形测试场景
- 每个场景显示测试名称、FPS 和 CPU 占用率
- 测试场景包括：
  - 矩形填充
  - 边框绘制
  - 阴影效果
  - 图片解码
  - 文字渲染
  - 线条绘制
  - 弧形绘制
  - 等等

## 核心 API

| API | 说明 |
|-----|------|
| `lv_init()` | 初始化 LVGL 库 |
| `lisa_device_get()` | 获取设备句柄 |
| `lisa_display_attach_bus()` | 配置显示设备总线 |
| `lv_port_disp_init()` | 初始化 LVGL 显示驱动 |
| `lisa_touch_attach_bus()` | 配置触摸设备总线 |
| `lv_port_indev_init()` | 初始化 LVGL 输入设备驱动 |
| `lv_task_handler()` | LVGL 任务处理函数 |
| `lv_demo_benchmark()` | 启动 benchmark 性能测试 |

## 关键代码

### Display 设备配置

```c
lisa_display_config_t display_config = {
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .bus_config = {.spi_4wire = {
        .spi_dev = lisa_device_get("spi1"),
        .cs_gpio = gpiob_dev,
        .cs_pin = LCD_CS_PIN,
        .dc_gpio = gpiob_dev,
        .dc_pin = LCD_CD_PIN,
        .spi_freq = 50 * 1000 * 1000,
    }},
    .backlight = {
        .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
        .config.pwm = {.channel = 0, .dev = lisa_device_get("pwm0"), .freq = 2000}
    },
    .rst_gpio = gpioa_dev,
    .rst_pin = LCD_RST_PIN,
};

lisa_device_t *display_device = lisa_device_get("display");
lisa_display_attach_bus(display_device, &display_config);
lv_port_disp_init(display_device);
```

### Touch 设备配置

```c
lisa_touch_bus_config_t bus_config = {
    .bus_type = LISA_TOUCH_BUS_I2C,
    .config = {
        .i2c = {
            .i2c_dev = i2c_dev,
            .int_gpio = gpioa_dev,
            .int_pin = LISA_TOUCH_I2C_INT_PIN,
            .rst_gpio = gpioa_dev,
            .rst_pin = LISA_TOUCH_I2C_RST_PIN,
        }
    }
};

lisa_device_t *touch_dev = lisa_device_get(TOUCH_DEVICE);
lisa_touch_attach_bus(touch_dev, &bus_config);
lv_port_indev_init(touch_dev);
```

### UI 任务

```c
static void task_ui(void *pvParameters)
{
    lv_demo_benchmark();
    LISA_LOGI(LOG_TAG, "LVGL benchmark start");

    while (1) {
        uint32_t wait_time = lv_task_handler();
        lisa_thread_mdelay(wait_time);
    }
}
```

## 配置说明

### 必需的配置项（prj.conf）

```
# 设备框架
CONFIG_LISA_DEVICE=y
CONFIG_LISA_GPIO_DEVICE=y
CONFIG_LISA_GPIOA=y
CONFIG_LISA_GPIOB=y
CONFIG_LISA_PWM=y
CONFIG_LISA_SPI1=y

# 显示设备
CONFIG_LISA_DISPLAY_DEVICE=y
CONFIG_LISA_DISPLAY_PANEL_ST7789P3=y
CONFIG_LISA_DISPLAY_BUS_SPI_4WIRE=y

# 触摸设备
CONFIG_LISA_TOUCH_DEVICE=y
CONFIG_LISA_I2C=y
CONFIG_LISA_I2C0=y
CONFIG_LISA_TOUCH_ARCS_CST328=y

# LVGL
CONFIG_SDK_MODULE_LVGL=y
CONFIG_LV_USE_DEMO_BENCHMARK=y
CONFIG_LV_USE_GPU_CSK_DMA2D=y
CONFIG_LV_DOUBLE_VDB=y
CONFIG_LV_USE_PERF_MONITOR=y
```

### 性能优化配置

```
CONFIG_LV_USE_GPU_CSK_DMA2D=y         # 启用 DMA2D 硬件加速
CONFIG_LV_DOUBLE_VDB=y                 # 双缓冲
CONFIG_LV_DRIVER_FULL_REFRESH=n        # 局部刷新（提高帧率）
CONFIG_LV_REFR_UNCONDITIONAL_AREA_JOIN=y  # 区域合并优化
```

## 注意事项

1. **引脚复用配置**：必须在设备初始化前正确配置 GPIO 引脚复用功能

2. **设备初始化顺序**：
   - 先调用 `lv_init()` 初始化 LVGL
   - 再初始化 Display 设备
   - 然后初始化 Touch 设备
   - 最后创建 UI 任务

3. **屏幕旋转**：通过 `CONFIG_LV_DRIVER_ROTATE_270` 配置屏幕旋转角度

4. **内存配置**：确保 PSRAM 堆大小足够（`CONFIG_PSRAM_HEAP_SIZE=614400`）

5. **性能监控**：启用 `CONFIG_LV_USE_PERF_MONITOR=y` 可在屏幕左上角显示实时 FPS 和 CPU 占用率

6. **刷新模式**：Benchmark 建议使用局部刷新（`CONFIG_LV_DRIVER_FULL_REFRESH=n`）以获得更准确的性能数据