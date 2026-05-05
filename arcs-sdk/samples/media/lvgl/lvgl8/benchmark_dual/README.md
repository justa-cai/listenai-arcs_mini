# LVGL8 Dual Benchmark 双屏性能测试示例

## 功能说明

演示 LVGL 8.x 图形库在双屏场景下运行 Benchmark 性能测试的能力，用于评估两块屏幕同时渲染时的图形性能和稳定性。

本示例基于 LISA Display 多显示框架和 LVGL 多显示移植层实现：

- `display` 使用 SPI1 驱动第一块 ST7789P3 屏幕
- `display1` 使用 SPI0 驱动第二块 ST7789P3 屏幕
- 两块屏幕分别启动各自独立的 benchmark 实例
- 第一块屏默认接入 CST328 触摸屏
- 第二块触摸屏为可选，若设备不存在会跳过，不影响 benchmark 运行

与单屏 `benchmark` 示例不同，`benchmark_dual` 不是直接对两块屏重复调用 `lv_demo_benchmark()`，而是在 sample 内部为每块屏创建独立的 benchmark 实例，避免原始 demo 的全局静态状态在双屏场景下互相覆盖。

## 硬件连接

### LCD0 显示屏（SPI1）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SPI_CLK | PB3 | SPI1 时钟 |
| SPI_DATA | PB1 | SPI1 数据 |
| CS | PB5 | LCD0 片选 |
| DC | PB0 | LCD0 数据/命令选择 |
| RST | PA1 | LCD0 复位 |
| PWM | PA0 | LCD0 背光 PWM |

### LCD1 显示屏（SPI0）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SPI_CLK | PA15 | SPI0 时钟 |
| SPI_DATA | PA14 | SPI0 数据 |
| CS | PA12 | LCD1 片选 |
| DC | PA13 | LCD1 数据/命令选择 |
| PWM | PA11 | LCD1 背光 PWM |

### Touch0 触摸屏（I2C0，可选但推荐）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SDA | PA22 | I2C0 数据 |
| SCL | PA23 | I2C0 时钟 |
| INT | PA24 | Touch0 中断 |
| RST | PA25 | Touch0 复位 |

### Touch1 触摸屏（I2C1，可选）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SDA | PA18 | I2C1 数据 |
| SCL | PA19 | I2C1 时钟 |
| INT | PA27 | Touch1 中断 |
| RST | PA26 | Touch1 复位 |

说明：
- 第二块触摸屏当前为可选硬件。如果 `touch_cst328_1` 设备不存在，示例会打印 warning 并跳过 display1 的输入设备注册。
- 第二块触摸屏的 `RST/INT` 引脚定义目前采用 sample 内占位值，如实际硬件不同，请同步修改 `samples/modules/lvgl/lvgl8/benchmark_dual/src/main.c`。

## 示例步骤

1. 初始化 LVGL 库
2. 初始化两块显示屏对应的 GPIO、SPI、PWM 资源
3. 注册 `display` 和 `display1` 到 LVGL 多显示驱动层
4. 初始化第一块触摸屏，并在设备存在时尝试初始化第二块触摸屏
5. 打开两块屏幕背光并设置亮度
6. 分别在 `disp0` 和 `disp1` 上启动独立的 benchmark 实例
7. 在主循环中持续调用 `lv_timer_handler()` 处理两块屏幕的刷新与动画

## 编译

```bash
bash "samples/modules/lvgl/lvgl8/benchmark_dual/build.sh" \
  -S "samples/modules/lvgl/lvgl8/benchmark_dual" \
  -B "build_benchmark_dual" \
  -DBOARD=arcs_evb
```

## 烧录

请根据当前开发板烧录流程，将 `build_benchmark_dual/app.bin` 烧录到目标设备。

如果你已经有统一的烧录脚本，也可以直接使用该脚本对 `build_benchmark_dual/app.bin` 进行烧录。

## 预期输出

**终端输出：**

```
LVGL8 dual benchmark start
```

如果第二块触摸设备不存在，可能额外看到如下 warning，这属于预期行为：

```
Touch device touch_cst328_1 not found, skip display1 touch
```

**屏幕显示：**

- 两块屏幕都会进入 benchmark 测试场景
- 两块屏幕都会自动切换矩形、边框、阴影、图片、文字、线条、弧形等测试项目
- 每块屏幕都会独立刷新自己的 FPS 结果
- benchmark 结束后，两块屏幕都会各自进入结果汇总页

## 核心 API

| API | 说明 |
|-----|------|
| `lv_init()` | 初始化 LVGL 库 |
| `lisa_display_attach_bus()` | 绑定显示设备与总线 |
| `lv_port_disp_register()` | 注册 LVGL 多显示设备 |
| `lisa_touch_attach_bus()` | 绑定触摸设备与总线 |
| `lv_port_indev_register()` | 注册 LVGL 多输入设备 |
| `lv_demo_benchmark_disp0_bind()` | 绑定 display0 的 benchmark 实例 |
| `lv_demo_benchmark_disp0()` | 启动 display0 的 benchmark |
| `lv_demo_benchmark_disp1_bind()` | 绑定 display1 的 benchmark 实例 |
| `lv_demo_benchmark_disp1()` | 启动 display1 的 benchmark |
| `lv_timer_handler()` | 处理 LVGL 定时器和刷新 |

## 关键实现

### 双屏注册

```c
lv_disp_t *disp0 = lv_port_disp_register(display0);
lv_disp_t *disp1 = lv_port_disp_register(display1);
```

### 双实例 benchmark 绑定与启动

```c
lv_demo_benchmark_disp0_bind(disp0);
lv_demo_benchmark_disp1_bind(disp1);

lv_demo_benchmark_disp0();
lv_demo_benchmark_disp1();
```

### 第二块触摸屏的降级处理

```c
if (!touch_dev) {
    LISA_LOGW(LOG_TAG, "Touch device %s not found, skip display1 touch", TOUCH1_DEVICE);
    return 1;
}
```

## 配置说明

### 必需配置项（prj.conf）

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
CONFIG_LV_USE_DEMO_BENCHMARK=y
CONFIG_LV_USE_GPU_CSK_DMA2D=y
CONFIG_LV_DOUBLE_VDB=y
CONFIG_LV_USE_PERF_MONITOR=y
```

### 性能相关配置

```kconfig
CONFIG_LV_USE_GPU_CSK_DMA2D=y
CONFIG_LV_DOUBLE_VDB=y
CONFIG_LV_DRIVER_FULL_REFRESH=y
CONFIG_LV_REFR_UNCONDITIONAL_AREA_JOIN=y
CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y
```

这些配置用于提升双屏 benchmark 的刷新效率，减少渲染瓶颈对测试结果的干扰。

## 与单屏 benchmark 的区别

| 项目 | `benchmark` | `benchmark_dual` |
|------|-------------|------------------|
| 显示屏数量 | 1 | 2 |
| 显示驱动 | 单显示移植层 | 多显示移植层 |
| benchmark 实例数量 | 1 | 2 |
| 触摸设备 | 1 个必需 | 第 2 个可选 |
| 运行目标 | 单屏性能测试 | 双屏同时渲染性能与稳定性测试 |

## 注意事项

1. **不要直接在双屏场景中对两块屏重复调用原始 `lv_demo_benchmark()`**
   - 原始 benchmark demo 内部包含全局静态状态
   - 在双屏环境中直接复用会导致状态互相覆盖，可能在场景切换或结果汇总阶段触发异常
   - 本 sample 已在本地做了双实例隔离处理

2. **第二块触摸屏不是运行 benchmark 的前置条件**
   - 即使 `touch_cst328_1` 不存在，display1 的 benchmark 仍可正常运行

3. **请确认双屏硬件连接与板级 pinmux 一致**
   - 尤其是 SPI0/SPI1、PWM、I2C1 相关引脚

4. **内存占用高于单屏 benchmark**
   - 双屏同时运行会增加显存与对象内存占用
   - 请保留足够 PSRAM 空间，当前配置使用 `CONFIG_PSRAM_HEAP_SIZE=0x200000`

5. **稳定性验证建议持续观察至少 1 分钟以上**
   - 重点确认两块屏都能顺利切换场景并完成结果页展示

6. **若第二块屏幕方向或触摸映射不正确**
   - 请检查 `CONFIG_LV_DRIVER_ROTATE_270` 与板级接线方向是否匹配

## 相关文件

- `samples/modules/lvgl/lvgl8/benchmark_dual/src/main.c`
- `samples/modules/lvgl/lvgl8/benchmark_dual/src/benchmark_dual.h`
- `samples/modules/lvgl/lvgl8/benchmark_dual/src/benchmark_dual_instance_impl.h`
- `samples/modules/lvgl/lvgl8/benchmark_dual/src/benchmark_dual_disp0.c`
- `samples/modules/lvgl/lvgl8/benchmark_dual/src/benchmark_dual_disp1.c`
- `samples/modules/lvgl/lvgl8/benchmark_dual/prj.conf`
- `samples/modules/lvgl/lvgl8/benchmark_dual/sample.yaml`
