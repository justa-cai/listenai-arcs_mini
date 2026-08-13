<!-- type: worker -->
<!-- 开发规则: README.md#开发规则 -->

# ARCS 样例代码生成

生成符合 ARCS SDK 规范的完整示例项目，覆盖驱动、HAL、模块、网络、蓝牙、算法等各类场景。

---

## Step 1: 需求确认

向用户确认以下信息（不明确时询问）：

| 信息 | 默认值 | 说明 |
|------|--------|------|
| **功能目标** | — | 要演示什么：哪个驱动/组件/模块，什么场景 |
| **样例路径** | — | 建议放在哪个分类下（见下方分类规则） |
| **样例名称** | — | 目录名（snake_case，简洁描述功能） |
| **目标板型** | arcs_evb | arcs_evb 或 arcs_mini |

### 样例分类规则

```
samples/
├── drivers/devices/{lisa_xxx}/{feature}/   # LISA 设备驱动类（lisa_gpio、lisa_uart 等）
├── drivers/hal/{peripheral}/{feature}/     # HAL 层直接使用类（gpio、uart、spi、rtc 等）
├── modules/{module_name}/{feature}/        # 模块类（cjson、sqlite3、lvgl、mbedtls、app_player、usb 等）
├── network/{feature}/                      # 网络类（http、mqtt/{variant}、wifi、websocket 等）
├── bluetooth/ble/{feature}/                # BLE 类（broadcaster、peripheral）
├── bluetooth/classic/{feature}/            # 经典蓝牙类（a2dp_source、hfp_source）
├── algorithms/{feature}/                   # 算法类（face_detect、wake_up）
├── cpp/{feature}/                          # C++ 特性类（simple_use、threads、regex、io_stream）
└── helloworld/                             # 最简骨架
```

**注意：** 蓝牙分 `ble/` 和 `classic/` 两级；MQTT 等有多变体的在 `mqtt/` 下按协议分子目录（tcp、ssl、ws、wss、agent）。

---

## Step 2: 读取参考样例

在生成代码前，**必须先读取**最相关的现有样例作为参考。

### 2.1 动态搜索（优先）

按以下优先级搜索参考样例：

1. **精确匹配**：`Glob "samples/**/*{module_name}*/"` — 找到与目标模块同名的样例目录
2. **同类匹配**：`Glob "samples/{category}/*/"` — 找到同分类下的其他样例（category 由 Step 1 确定）
3. **静态回退**：若上述均无结果，使用 2.2 节的预置映射表

找到参考样例后，Read 以下文件（并行执行）：
- `CMakeLists.txt` — 了解构建结构和依赖
- `prj.conf` — 了解配置依赖链
- `src/main.c` — 了解代码模式和初始化流程
- `sample.yaml` — 了解测试配置方式

### 2.2 预置参考映射（回退用）

当动态搜索无结果时，使用以下映射表：

| 目标功能 | 参考样例路径 |
|----------|------------|
| GPIO 操作 | `samples/drivers/devices/lisa_gpio/output_basic/` |
| UART 收发 | `samples/drivers/devices/lisa_uart/poll_out/` 或 `recv_sync_int/` |
| SPI 通信 | `samples/drivers/devices/lisa_spi/master/` |
| I2C 通信 | `samples/drivers/devices/lisa_i2c/basic_write_read/` |
| Flash 读写 | `samples/drivers/devices/lisa_flash/single_core/` |
| ADC 采样 | `samples/drivers/devices/lisa_adc/read_basic/` |
| PWM 输出 | `samples/drivers/devices/lisa_pwm/output_basic/` |
| 硬件定时器 | `samples/drivers/devices/lisa_hwtimer/gpt_timer/` |
| RTC 时钟 | `samples/drivers/devices/lisa_rtc/time_basic/` |
| WDT 看门狗 | `samples/drivers/devices/lisa_wdt/basic/` |
| SDMMC/SD卡 | `samples/drivers/devices/lisa_sdmmc/simple_use/` |
| I2S 音频接口 | `samples/drivers/devices/lisa_i2s/master_tx/` |
| 音频录放 | `samples/drivers/devices/lisa_audio/record_playback/` |
| 显示屏 | `samples/drivers/devices/lisa_display/display_flush/` |
| 触摸 | `samples/drivers/devices/lisa_touch/polling_mode/` |
| 摄像头 | `samples/drivers/devices/lisa_camera/` |
| DVP 接口 | `samples/drivers/devices/lisa_dvp/normal_mode/` |
| RGB LED | `samples/drivers/devices/lisa_rgb/rgb_bounce_buffer/` |
| QSPI LCD | `samples/drivers/devices/lisa_qspilcd/basic/` |
| HAL GPIO | `samples/drivers/hal/gpio/output/` |
| HAL UART | `samples/drivers/hal/uart/` |
| BLE 功能 | `samples/bluetooth/ble/peripheral/` |
| 经典蓝牙 | `samples/bluetooth/classic/a2dp_source/` |
| WiFi/HTTP | `samples/network/http/` |
| MQTT | `samples/network/mqtt/tcp/` |
| WebSocket | `samples/network/websocket/` |
| 第三方模块 | `samples/modules/cjson/` |
| LVGL UI | `samples/modules/lvgl/lvgl8/widgets/` |
| mbedTLS 加密 | `samples/modules/mbedtls/self_check/` |
| 文件系统 | `samples/modules/fs/lsfs/` |
| USB 设备 | `samples/modules/usb/device/msc/` |
| 播放器 | `samples/modules/app_player/local_fs/` |
| 算法 | `samples/algorithms/face_detect/` |
| 最简骨架 | `samples/helloworld/` |

若目标模块有 experience 文件，读取 `references/experience/{module}.md` 获取历史经验。

**多模块协同场景**：涉及网络类（HTTP/WebSocket/MQTT）时，额外 Read `samples/network/http/src/main.c` 了解 WiFi 初始化 + 网络就绪的完整模式。

---

## Step 3: 生成文件

按以下顺序生成所有文件。

### 3.1 CMakeLists.txt

**模式 A: 单源文件（最常见，驱动类/简单模块）**
```cmake
cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
```

**模式 B: 多源文件（含 include 目录）**
```cmake
cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

listenai_include_directories(src)

target_sources(${PROJECT_NAME} PRIVATE
    src/main.c
    src/xxx.c
)
```

**模式 C: 子目录 + 外部库（网络/复杂模块类常用）**
```cmake
cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(./)

add_subdirectory(src)

listenai_add_executable(${PROJECT_NAME})
listenai_link_libraries(mbedtls)   # 按需添加
```

配合 `src/CMakeLists.txt`：
```cmake
listenai_library_named(app)

listenai_library_sources(
    main.c
    other_file.c
)

listenai_include_directories(
    ./
)
```

**选择规则：** 源文件 ≤ 2 个用模式 A；有 include 子目录或多源用模式 B；需要链接第三方库（mbedtls 等）或项目结构复杂用模式 C。

---

### 3.2 Kconfig

所有样例统一内容：

```kconfig
osource "$ARCS_BASE/Kconfig"


```

---

### 3.3 prj.conf

**推导原则：读参考样例，不要猜。**

```
1. 读取 Step 2 选定的参考样例的 prj.conf（直接复制为基础）
2. 若参考样例不完整，Read drivers/lisa_{xxx}/Kconfig 查看 depends on 链
3. 网络/BLE 类：WebFetch 对应组件文档获取依赖配置
   - 网络: components/lisa_wifi/README.html + components/lisa_net/README.html
   - BLE:  components/lisa_bluetooth/README.html
```

**驱动类（lisa_device 层）最小配置规律（从 Kconfig 依赖链提取）:**
- 所有驱动均需: `CONFIG_LISA_DEVICE=y`
- 驱动主开关: `CONFIG_LISA_{DRIVER}_DEVICE=y`（如 `CONFIG_LISA_GPIO_DEVICE=y`）
- 实例开关: `CONFIG_LISA_{DRIVER}{N}=y`（如 `CONFIG_LISA_GPIOA=y`）
- 具体 CONFIG 名称以 `Read drivers/lisa_{xxx}/Kconfig` 为准

**HAL 类最小配置：** 通常只需 `CONFIG_MEM_CONFIG=y`，具体以参考样例为准。

**模块类典型配置层次：**
- 基础: `CONFIG_MODULE_FREERTOS=y` + `CONFIG_MODULE_HEAP=y` + `CONFIG_LISA_OS=y`
- 文件系统: `CONFIG_FILE_SYSTEM=y` + `CONFIG_FATFS_FILESYSTEM=y` + `CONFIG_LSFS=y`
- 网络: `CONFIG_LWIP=y` + `CONFIG_WIFI=y` + `CONFIG_LISA_WIFI=y` + `CONFIG_LISA_NETWORK=y`

---

### 3.4 sample.yaml

两种模式：

**模式 A: 标准测试（可上板验证）**
```yaml
tests:
  samples.{category}.{feature}:
    programmer: arcs
    runner: uart
    log_analyzer:
      type: "regex"
      pattern: "{首条关键日志}"
```

**模式 B: 仅构建（需外部设备/网络等不便自动测试）**
```yaml
tests:
  samples.{name}:
    build_only: true
```

**命名规则：** 全小写，点分隔。实际项目中命名风格不完全统一，优先参考同分类下现有样例的命名模式：
- 驱动类: `samples.lisa_gpio_output` 或 `samples.lisa_uart.poll_out`
- 模块类: `samples.cjson`、`samples.sqlite3`
- 网络类: `samples.http`（通常用 build_only）

**使用 `build_only` 的场景：** 需要 WiFi AP、外部传感器、特殊硬件等不便 CI 自动测试的样例。

---

### 3.5 src/main.c

#### 日志宏约定

统一使用短日志宏风格：

```c
#define LOG_TAG "sample"
#include <lisa_log.h>

LOGI("message %d", value);   // 自动使用 LOG_TAG
LOGW("warning");
LOGE("error");
LOGD("debug");
```

`#define LOG_TAG` 必须在 `#include <lisa_log.h>` **之前**定义。

#### 驱动类样例模板

```c
/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA {DRIVER} {功能}示例
 *
 * {可选：1-2行补充说明}
 */

#define LOG_TAG "sample"
#include <lisa_log.h>

#include <stdio.h>
#include <string.h>
#include "lisa_device.h"
#include "lisa_{driver}.h"
#include "IOMuxManager.h"
#include "FreeRTOS.h"
#include "task.h"

#define {DRIVER}_DEVICE    "{device_name}"   /* 如 "uart1", "spi0", "gpiob" */

/* 引脚定义（按实际硬件填写） */
#define {DRIVER}_TX_PAD    CSK_IOMUX_PAD_B
#define {DRIVER}_TX_PIN    2
#define {DRIVER}_FUNC      CSK_IOMUX_FUNC_ALTER3

/*
    为满足不同板型示例场景，重定向设备的 pinmux 配置
*/
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_{device_name}_pinmux()
{
    IOMuxManager_PinConfigure({DRIVER}_TX_PAD, {DRIVER}_TX_PIN, {DRIVER}_FUNC);
    /* 添加更多引脚... */
}
#endif

int main(int argc, char **argv)
{
    LOGI( "=== LISA {DRIVER} {功能} example ===");

    /* 1. 获取设备 */
    lisa_device_t *dev = lisa_device_get({DRIVER}_DEVICE);
    if (!lisa_device_ready(dev)) {
        LOGE( "Error: %s device not ready", {DRIVER}_DEVICE);
        return -1;
    }
    LOGI( "%s device ready", {DRIVER}_DEVICE);

    /* 2. 配置/使用设备（API 因驱动而异，以参考样例和头文件为准） */
    /* 例：GPIO 用 lisa_gpio_configure(dev, pin, flags)
     * 例：UART 用 lisa_uart_open(dev, &config) */

    /* 3. 主循环 */
    while (1) {
        /* TODO: 核心逻辑 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
```

#### HAL 类样例特点

HAL 类样例**不使用** `lisa_device_get()` / `lisa_device_ready()`，直接操作硬件寄存器/HAL API：

```c
#include <stdio.h>
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "FreeRTOS.h"
#include "task.h"

int main(int argc, char **argv)
{
    printf("Hello, world! \n");

    void *handler = GPIOA();
    GPIO_Initialize(handler, NULL, NULL);
    /* ... HAL 操作 ... */
}
```

#### 模块/网络类样例特点

- 通常**不涉及** `lisa_device` 层，直接调用模块 API
- 部分只用 `printf()` 输出（如 cjson）
- 网络类需要 WiFi 初始化回调和事件处理，参考 `samples/network/http/` 结构

#### main.c 关键约定

| 约定 | 规则 |
|------|------|
| LOG_TAG/TAG | 置于所有 include 之前（`#include <lisa_log.h>` 之前） |
| 首条日志 | 驱动类: `LOGI( "=== {Title} ===");` |
| 设备获取 | 驱动类: `lisa_device_get()` 后立即 `lisa_device_ready()` 检查 |
| 错误处理 | `return -1` 或打印错误后继续（视场景） |
| 延时 | `vTaskDelay(pdMS_TO_TICKS(ms))` |
| Pinmux 覆盖 | 驱动类: `#ifdef CONFIG_BOARD_ARCS_EVB` + `void lisa_{device}_pinmux()` |
| 日志宏 | LOGI / LOGE / LOGW / LOGD（需先定义 LOG_TAG） |

#### 回调/中断场景补充

```c
/* 回调函数（ISR 安全：不能使用阻塞 API，用 FromISR 版本） */
static void {driver}_event_callback({driver}_event_t event, void *user_data)
{
    if (event == LISA_{DRIVER}_EVENT_TX_DONE) {
        /* 通知主任务（使用 FromISR 变体） */
        lisa_semaphore_give_from_isr(tx_done_sem);
    }
}
```

---

### 3.6 README.md

**不在此处定义模板。** 生成 README 时，读取并遵循 `references/doc-writing.md` 中的文档编写流程（Step 3A: 新建文档），按其中规范生成示例文档。

关键要点：
- 编译部分使用 `{eval-rst}` + `.. include:: /sample_build.rst`（与 Sphinx 在线文档统一）
- 预期输出与 main.c 实际日志一致
- 生成后需在对应 `index_zh.rst` 中注册（见 doc-writing.md Step 3A-4）

---

## Step 4: 质量检查

生成代码后逐项验证：

**CMakeLists.txt**
- [ ] `cmake_minimum_required(VERSION 3.13)` 首行
- [ ] `find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})`
- [ ] `listenai_add_executable(${PROJECT_NAME})` 使用正确宏
- [ ] 源文件路径正确
- [ ] 模式 C 时 src/CMakeLists.txt 使用 `listenai_library_named()` + `listenai_library_sources()`

**Kconfig**
- [ ] 仅含 `osource "$ARCS_BASE/Kconfig"`

**prj.conf**
- [ ] 驱动类：包含 `CONFIG_LISA_DEVICE=y`
- [ ] 设备实例 CONFIG 已启用（如 `CONFIG_LISA_UART1=y`）
- [ ] 网络类：LWIP / WiFi / LISA_WIFI 等基础配置完整
- [ ] BLE 类：BLE 依赖 WiFi 的 RF 初始化（`CONFIG_LISA_WIFI=y`）
- [ ] HAL 类：通常只需 `CONFIG_MEM_CONFIG=y`

**sample.yaml**
- [ ] test key 格式与同类现有样例一致
- [ ] 需外部资源的样例使用 `build_only: true`
- [ ] 非 build_only 的 pattern 与 main.c 中的日志输出匹配

**main.c**
- [ ] `LOG_TAG` 或 `TAG` 定义在 `#include <lisa_log.h>` 之前
- [ ] 版权头信息完整（Copyright 2025 LISTENAI）
- [ ] 驱动类：`lisa_device_ready()` 检查在 `lisa_device_get()` 之后
- [ ] 驱动类：`#ifdef CONFIG_BOARD_ARCS_EVB` pinmux 覆盖函数存在（涉及引脚时）
- [ ] 回调函数中不使用阻塞 FreeRTOS API
- [ ] 错误处理完整（每个 API 调用都检查返回值）

**README.md**（按 `doc-writing.md` 规范检查）
- [ ] 编译部分使用 `{eval-rst}` + `.. include:: /sample_build.rst`
- [ ] 预期输出与 main.c 实际日志一致
- [ ] 已在对应 `index_zh.rst` 中注册

---

## Step 4.5: 自动验证

质量检查完成后，执行以下自动验证（无需用户确认）：

### 4.5.1 头文件存在性

对 main.c 中每个 `#include "xxx.h"`（不含标准库 `<>` 头文件）：
- `Glob "**/xxx.h"` — 确认头文件存在于 SDK 中
- 不存在 → 标记为 **错误**，检查拼写或尝试 `Grep "xxx" **/*.h` 查找正确文件名

### 4.5.2 Kconfig 一致性

对 prj.conf 中每个 `CONFIG_XXX=y`：
- `Grep "config XXX$" **/Kconfig*` 或 `Grep "menuconfig XXX$" **/Kconfig*` — 确认配置项存在
- 不存在 → 标记为 **警告**，可能拼写错误或已更名

### 4.5.3 API 签名验证

对 main.c 中调用的关键 SDK 函数（`lisa_*`、`LISA_*`、`lisa_device_*`）：
- `Grep "函数名" {对应头文件}` — 确认函数声明存在
- 不存在 → 标记为 **错误**，`Grep "函数名" **/*.h` 查找正确位置

### 4.5.4 CMake 依赖完整性

对 CMakeLists.txt 中 `listenai_link_libraries()` 引用的每个库：
- `Glob "components/{lib}/"` 或 `Glob "modules/{lib}/"` — 确认库目录存在
- 不存在 → 标记为 **错误**

### 验证结果处理

- 有错误 → 自动修复后重新验证，直到无错误
- 仅警告 → 在 Step 5 输出说明中列出警告项
- 全部通过 → 在 Step 5 输出中标注"✓ 自动验证通过"

---

## Step 5: 输出说明

生成完成后告知用户：

1. **文件清单**：生成了哪些文件，放在哪里
2. **构建命令**：
   ```bash
   ./build.sh -C -S samples/{path} -DBOARD=arcs_evb
   ```
3. **build.sh 来源**：告知直接从任意相邻样例复制 `build.sh`（内容相同，无需修改）
4. **需要用户确认的信息**：引脚编号、设备实例号（uart0/uart1等）、WiFi 账号密码等用 `TODO` 标注的内容

---

## Step 6: 经验捕获

任务完成后，评估是否有值得记录的经验（如推导 prj.conf 依赖时遇到意外、参考样例与实际不一致、API 已变更等）。如有，调用 `experience-capture` skill。
