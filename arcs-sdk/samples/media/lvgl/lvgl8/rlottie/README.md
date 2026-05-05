# LVGL8 Rlottie 动画演示示例

## 功能说明

演示 LVGL 8.x 中 `Rlottie` 动画控件的最小显示流程。

本示例使用 LVGL 自带的 `lv_example_rlottie_approve` 动画数据，在屏幕中央循环显示动画。为适配 `arcs_mini` 的 `240x240` 屏幕，sample 仅保留动画显示功能，不包含标题、按钮、状态文本和触摸交互。

## 依赖说明

本示例依赖预编译 `rlottie` 静态库，默认从当前目录下的 `rlottie-riscv/` 查找头文件和 `librlottie.a`。

首次使用请先执行：

```bash
cd arcs-sdk/samples/media/lvgl/lvgl8/rlottie
./tools/build_rlottie.sh
```

脚本会拉取固定 commit 的 `Samsung/rlottie`，应用裸机适配 patch，并以 `LOTTIE_THREAD=OFF` 方式生成适用于当前工程的预编译库。

## 硬件连接

### LCD 显示屏（SPI 接口，默认 `arcs_mini`）

| 信号 | 引脚 | 说明 |
|------|------|------|
| SPI_CLK | PAD_A[25] | SPI0 时钟 |
| SPI_DATA | PAD_A[24] | SPI0 MOSI |
| CS | PAD_A[22] | SPI0 片选 |
| DC | PAD_A[23] | 数据/命令选择 |
| RST | PAD_B[9] | 复位 |
| PWM | PAD_A[21] | 背光 PWM |

若构建目标切换为 `arcs_evb`，sample 会在源码中使用另一套 LCD 引脚配置。

## 示例步骤

1. 初始化 LVGL 库
2. 配置显示设备并初始化 LVGL 显示驱动
3. 创建 Rlottie 动画对象并居中显示
4. 在 UI 任务中循环调用 `lv_task_handler()` 刷新动画

## 编译

首次构建前，请先确保已生成当前目录下的 `rlottie-riscv/` 预编译库。

```{eval-rst}
.. include:: /sample_build.rst
```

若需使用其它预编译库路径，可在构建前设置：

```bash
RLOTTIE_ROOT=/your/path/to/rlottie-riscv
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**终端输出：**

```
LVGL8 rlottie start
```

**屏幕显示：**

- 深色背景
- 居中显示 Rlottie 动画

## 核心 API

| API | 说明 |
|-----|------|
| `lv_init()` | 初始化 LVGL 库 |
| `lisa_display_attach_bus()` | 配置显示设备总线 |
| `lv_port_disp_init()` | 初始化 LVGL 显示驱动 |
| `lv_rlottie_create_from_raw()` | 从内嵌动画数据创建动画对象 |
| `lv_task_handler()` | 周期刷新动画 |

## 关键代码

```c
extern const uint8_t lv_example_rlottie_approve[];

lv_obj_t *lottie = lv_rlottie_create_from_raw(lv_scr_act(),
                                              lottie_size,
                                              lottie_size,
                                              (const char *)lv_example_rlottie_approve);
lv_obj_center(lottie);
```

## 配置说明

### 必需的配置项（`prj.conf`）

```
CONFIG_SDK_MODULE_LVGL8=y
CONFIG_LV_BUILD_EXAMPLES=y
CONFIG_LV_USE_RLOTTIE=y
CONFIG_LISA_TOUCH_DEVICE=n
```

## 注意事项

1. `arcs_mini` 不启用触摸输入，本 sample 仅初始化显示。
2. `Rlottie` 会额外占用动画缓冲区内存，需保证 PSRAM 堆足够。
3. 若要替换动画，可将自定义 JSON 转为数组后替换 `lv_example_rlottie_approve`。
