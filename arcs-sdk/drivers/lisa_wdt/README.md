# WDT 驱动

基于 lisa_device 框架的看门狗定时器（Watchdog Timer）设备驱动，为 ARCS 平台提供统一的看门狗管理接口。

## 功能特性

- **设备支持**: WDT0 看门狗定时器控制器
- **两阶段超时机制**: 支持中断阶段和复位阶段两阶段超时保护
- **灵活配置**: 支持毫秒级超时时间配置，自动转换为硬件可配置值
- **状态查询**: 支持查询看门狗运行状态和剩余时间
- **回调支持**: 支持超时中断回调函数，可在超时前执行恢复操作
- **线程安全**: 内部使用互斥锁保护并发访问

## 配置选项

在 Kconfig 中启用驱动：

```kconfig
CONFIG_LISA_WDT=y
```

## API 接口

### 配置接口

```c
int lisa_wdt_setup(lisa_device_t *dev, const lisa_wdt_config_t *config);
```

**重要**: 必须先调用 `lisa_wdt_setup()` 配置设备后，才能使用下面的功能 API。

### 控制接口

```c
int lisa_wdt_start(lisa_device_t *dev);
int lisa_wdt_stop(lisa_device_t *dev);
int lisa_wdt_feed(lisa_device_t *dev);
```

### 查询接口

```c
int lisa_wdt_get_state(lisa_device_t *dev, lisa_wdt_state_t *state);
int lisa_wdt_get_remaining_time(lisa_device_t *dev, uint32_t *remaining_ms);
```

### 回调接口

```c
int lisa_wdt_set_callback(lisa_device_t *dev, lisa_wdt_callback_t callback, void *user_data);
```

## 配置参数说明

### lisa_wdt_config_t 结构体

看门狗配置使用两阶段超时机制：

```c
typedef struct {
    uint32_t int_timeout_ms;    /* 中断超时时间（毫秒），从启动开始计算 */
    uint32_t rst_timeout_ms;    /* 复位超时时间（毫秒），硬件配置参数 */
} lisa_wdt_config_t;
```

**重要说明**:
- **中断即复位**: 中断触发时，系统已经进入复位流程。中断回调执行后，系统会立即或很快复位，**没有独立的复位阶段窗口**
- **必须在中断前喂狗**: 必须在中断发生之前，在非中断上下文中（如主循环、任务等）及时喂狗。**不能在中断回调中喂狗**，因为此时系统已经进入复位流程
- `int_timeout_ms`: 从启动开始计算，到达此时间未喂狗会触发中断
- `rst_timeout_ms`: 硬件复位时间配置参数，用于硬件内部复位时序控制

**示例**: 配置 `int_timeout_ms = 1000`, `rst_timeout_ms = 500`
- 启动后 1 秒未喂狗 → 触发中断（系统进入复位流程）
- 中断回调执行后 → 系统很快复位
- **必须在 1 秒内喂狗**，否则系统会复位

### 超时时间精度

**重要**: 由于硬件使用 2^N 倍时钟周期配置，实际超时时间可能与配置值存在偏差。

- 驱动会自动将配置的毫秒值转换为最接近的硬件可配置值（向上取整）
- 例如：配置 500ms，实际可能使用 512ms（2^14 / 32K ≈ 512ms）
- 实际超时时间总是**大于或等于**配置值，确保不会提前超时
- 建议在应用层预留足够的时间余量

**超时时间范围**（32K 时钟源，默认）:

**中断阶段可选值**（16个）:

| 枚举值 | 时钟周期数 (2^N) | 实际时间 (ms) |
|--------|-----------------|--------------|
| int_time_6 | 2^6 = 64 | 2.00 |
| int_time_8 | 2^8 = 256 | 8.00 |
| int_time_10 | 2^10 = 1,024 | 32.00 |
| int_time_11 | 2^11 = 2,048 | 64.00 |
| int_time_12 | 2^12 = 4,096 | 128.00 |
| int_time_13 | 2^13 = 8,192 | 256.00 |
| int_time_14 | 2^14 = 16,384 | 512.00 |
| int_time_15 | 2^15 = 32,768 | 1,024.00 |
| int_time_17 | 2^17 = 131,072 | 4,096.00 |
| int_time_19 | 2^19 = 524,288 | 16,384.00 |
| int_time_21 | 2^21 = 2,097,152 | 65,536.00 |
| int_time_23 | 2^23 = 8,388,608 | 262,144.00 |
| int_time_25 | 2^25 = 33,554,432 | 1,048,576.00 |
| int_time_27 | 2^27 = 134,217,728 | 4,194,304.00 |
| int_time_29 | 2^29 = 536,870,912 | 16,777,216.00 |
| int_time_31 | 2^31 = 2,147,483,648 | 67,108,864.00 |

**复位阶段可选值**（8个）:

| 枚举值 | 时钟周期数 (2^N) | 实际时间 (ms) |
|--------|-----------------|--------------|
| rst_time_7 | 2^7 = 128 | 4.00 |
| rst_time_8 | 2^8 = 256 | 8.00 |
| rst_time_9 | 2^9 = 512 | 16.00 |
| rst_time_10 | 2^10 = 1,024 | 32.00 |
| rst_time_11 | 2^11 = 2,048 | 64.00 |
| rst_time_12 | 2^12 = 4,096 | 128.00 |
| rst_time_13 | 2^13 = 8,192 | 256.00 |
| rst_time_14 | 2^14 = 16,384 | 512.00 |

## 使用示例

### 复位模式示例

```c
#include "lisa_wdt.h"
#include "lisa_device.h"

// 1. 获取 WDT 设备
lisa_device_t *wdt = lisa_device_get("wdt0");
if (!lisa_device_ready(wdt)) {
    return -1;  // 设备未就绪
}

// 2. 配置看门狗：中断超时 300ms，复位超时 200ms
lisa_wdt_config_t config = {
    .int_timeout_ms = 300,  // 中断超时时间（必须在此时限内喂狗）
    .rst_timeout_ms = 200,  // 硬件复位时间配置参数
};
int ret = lisa_wdt_setup(wdt, &config);
if (ret != LISA_DEVICE_OK) {
    return -1;  // 配置失败
}

// 3. 启动看门狗
ret = lisa_wdt_start(wdt);
if (ret != LISA_DEVICE_OK) {
    return -1;  // 启动失败
}

// 4. 在主循环中定期喂狗
while (1) {
    // 执行主要任务
    do_main_task();
    
    // 喂狗（每 200ms 喂一次，确保在 300ms 中断超时之前）
    // 注意：实际超时时间可能略大于配置值，已预留余量
    lisa_wdt_feed(wdt);
    
    lisa_os_sleep_ms(200);
}
```

### 中断模式示例

```c
#include "lisa_wdt.h"
#include "lisa_device.h"

static lisa_device_t *g_wdt = NULL;

// 超时回调函数（在中断上下文中执行）
// 注意：中断触发时系统已进入复位流程，回调执行后系统会很快复位
// 不能在回调中喂狗，必须在中断发生之前在其他地方（如主循环）及时喂狗
void wdt_timeout_callback(void *user_data)
{
    printf("WDT timeout! System will reset soon...\n");
    
    // 执行最后的恢复操作：保存关键数据、记录日志等
    // 注意：操作必须非常快速，因为系统很快会复位
    save_critical_data();
    log_error();
    
    // 警告：不能在回调中喂狗，此时系统已进入复位流程
}

int main(void)
{
    // 1. 获取 WDT 设备
    g_wdt = lisa_device_get("wdt0");
    if (!lisa_device_ready(g_wdt)) {
        return -1;
    }

    // 2. 配置看门狗：中断超时 1000ms，复位超时 500ms
    lisa_wdt_config_t config = {
        .int_timeout_ms = 1000,  // 1 秒后触发中断（必须在此时间内喂狗）
        .rst_timeout_ms = 500,   // 硬件复位时间配置参数
    };
    int ret = lisa_wdt_setup(g_wdt, &config);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // 3. 设置超时回调
    ret = lisa_wdt_set_callback(g_wdt, wdt_timeout_callback, g_wdt);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // 4. 启动看门狗
    ret = lisa_wdt_start(g_wdt);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // 5. 在主循环中定期喂狗（必须在中断发生之前喂狗）
    while (1) {
        do_main_task();
        
        // 每 800ms 喂一次狗，小于 1000ms 中断超时时间
        // 必须在中断发生之前喂狗，不能在中断回调中喂狗
        lisa_wdt_feed(g_wdt);
        lisa_os_sleep_ms(800);
    }
}
```

### 状态查询示例

```c
#include "lisa_wdt.h"

// 查询看门狗状态
lisa_wdt_state_t state;
int ret = lisa_wdt_get_state(wdt, &state);
if (ret == LISA_DEVICE_OK) {
    switch (state) {
        case LISA_WDT_STATE_IDLE:
            printf("WDT is idle\n");
            break;
        case LISA_WDT_STATE_RUNNING:
            printf("WDT is running\n");
            break;
        case LISA_WDT_STATE_EXPIRED:
            printf("WDT has expired\n");
            break;
    }
}

// 获取剩余时间（注意：ARCS 硬件不支持直接读取，返回配置值）
uint32_t remaining_ms;
ret = lisa_wdt_get_remaining_time(wdt, &remaining_ms);
if (ret == LISA_DEVICE_OK) {
    printf("Remaining time: %u ms\n", remaining_ms);
}
```

## 硬件配置

### 引脚复用配置

WDT 为芯片内部外设，无需外部引脚配置，也无需配置引脚复用。

## 硬件特性

### ARCS 看门狗定时器

ARCS 平台的看门狗定时器具有以下特性：

1. **两阶段超时机制**:
   - **中断阶段**：连续 `int_timeout_ms` 未喂狗，触发中断并调用回调函数
   - **中断即复位**：中断触发时，系统已经进入复位流程。中断回调执行后，系统会立即或很快复位，没有独立的复位阶段窗口
   - **必须在中断前喂狗**：必须在中断发生之前，在非中断上下文中（如主循环、任务等）及时喂狗。不能在中断回调中喂狗
   - 时间线：启动 → [中断阶段] → 中断触发（系统进入复位流程）→ 系统复位

2. **时钟源**:
   - 默认使用 32K 外部时钟源
   - 时钟周期精度固定，超时时间使用 2^N 倍周期配置

3. **写保护机制**:
   - 配置寄存器需要先写入魔法值（0x5AA5）才能修改
   - 喂狗需要写入特定的重启值（0xCAFE）

4. **硬件限制**:
   - 硬件不支持直接读取剩余时间
   - 看门狗一旦启动，无法通过软件完全禁用（直到系统复位）

## 注意事项

1. **配置顺序**: 必须先调用 `lisa_wdt_setup()` 配置设备，再调用 `lisa_wdt_start()` 启动看门狗

2. **超时时间偏差**: 由于硬件使用 2^N 倍时钟周期配置，实际超时时间可能与配置值存在偏差。例如配置 500ms，实际可能使用 512ms（2^14 / 32K）。实际超时时间总是大于或等于配置值，建议预留足够时间余量

3. **喂狗频率**: 喂狗间隔必须小于中断超时时间（`int_timeout_ms`），建议在配置值的 80% 以内喂狗，确保考虑时间偏差后仍能及时喂狗

4. **中断即复位**: 中断触发时系统已进入复位流程，中断回调执行后系统会很快复位。**必须在中断发生之前，在非中断上下文中（如主循环、任务等）及时喂狗**。不能在中断回调中喂狗，因为此时系统已经进入复位流程

5. **中断回调限制**: 回调函数在中断上下文中执行，应尽量简短快速，避免执行耗时操作、阻塞操作或互斥锁操作。回调主要用于最后的紧急操作（如保存关键数据、记录日志），因为系统很快会复位

7. **剩余时间查询**: ARCS 硬件不支持直接读取剩余时间，`lisa_wdt_get_remaining_time()` 返回配置的中断超时时间，而非实际剩余时间

8. **状态管理**: 看门狗状态由驱动内部维护，超时后状态会更新为 `LISA_WDT_STATE_EXPIRED`

9. **停止限制**: 在超时之前，可以通过 `lisa_wdt_stop()` 停止看门狗。但一旦中断触发，系统已进入复位流程

10. **配置重新设置**: 如果看门狗正在运行，重新调用 `lisa_wdt_setup()` 会先停止看门狗，然后应用新配置

## 文件说明

- `lisa_wdt.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_wdt_arcs.c` - ARCS 平台适配实现
- `CMakeLists.txt` - 构建配置
- `Kconfig` - 配置选项
- `README.md` - 驱动使用说明
