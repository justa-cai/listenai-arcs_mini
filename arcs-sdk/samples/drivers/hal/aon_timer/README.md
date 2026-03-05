# AON Timer 示例

本示例演示如何使用 AON Timer（Always-On Timer）的周期性计数模式，实现定时器中断功能。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **周期模式计数**：使用 AON Timer 的周期重载计数模式
- **时钟源配置**：配置使用 RC32K 作为定时器时钟源
- **中断处理**：实现定时器中断回调函数处理
- **定时器控制**：演示定时器的初始化、启动、停止等完整流程

### 硬件要求

- 支持 AON Timer 的 ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/aon_timer
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/aon_timer -C
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 和 `arcs.elf` 文件。

### 2. 烧录运行

将开发板连接到 PC，执行烧录命令：

```bash
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/arcs.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板，通过串口工具（如 minicom）查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### 定时器参数配置

在源码中可以配置以下参数：

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `AON_TIMER_RELOAD_COUNT` | 32000 | 定时器重载计数值 |
| 时钟源 | RC32K | 使用 32KHz RC 振荡器 |
| 工作模式 | Repeat | 周期重载模式 |
| 中断使能 | Enabled | 使能定时器中断 |

## 📋 代码解析

### 关键代码段

#### 1. 定时器初始化

```c
// 初始化定时器句柄
AON_TIMER_Init_Handler();

// 初始化定时器，注册回调函数
AON_TIMER_Initialize(AON_TIMER_Handler, AON_TIMER_EventCallback, NULL);

// 使能定时器电源
AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_FULL);
```

#### 2. 定时器配置

```c
// 配置定时器工作模式
AON_TIMER_Control(AON_TIMER_Handler,
                  HAL_AON_TIMER_MODE_Repeat |           // 周期模式
                  HAL_AON_TIMER_INTERRUPT_Enabled |     // 使能中断
                  HAL_AON_TIMER_CLK_SEL_Rc32k);         // RC32K 时钟源

// 设置定时器周期（计数值）
AON_TIMER_SetTimerPeriodByCount(AON_TIMER_Handler, AON_TIMER_RELOAD_COUNT);
```

#### 3. 定时器中断回调

```c
static void AON_TIMER_EventCallback(uint32_t event, void *workspace)
{
    aonTimerTriggered = 1;  // 设置触发标志
    
    printf("aon timer trigger\n");
    
    uint32_t counter;
    AON_TIMER_ReadTimerCount(AON_TIMER_Handler, &counter);  // 读取计数值
}
```

#### 4. 启动和停止定时器

```c
// 启动定时器
AON_TIMER_StartTimer(AON_TIMER_Handler);

// 等待定时器触发
while (aonTimerTriggered == 0);

// 停止定时器
AON_TIMER_StopTimer(AON_TIMER_Handler);

// 关闭电源并释放资源
AON_TIMER_PowerControl(AON_TIMER_Handler, CSK_POWER_OFF);
AON_TIMER_Uninitialize(AON_TIMER_Handler);
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! aon_timer
aon timer trigger
aon_timer end
```

输出说明：
- `Hello, world! aon_timer`：程序启动信息
- `aon timer trigger`：定时器中断触发，在回调函数中打印
- `aon_timer end`：定时器测试完成

## ⚠️ 注意事项

1. **时钟使能**：在使用 AON Timer 前，必须先使能 AON Timer 时钟：
   ```c
   IP_AON_CTRL->REG_AON_CLK_CTRL.bit.ENA_AON_TIMER_CLK = 1;
   ```

2. **计数周期计算**：
   - 使用 RC32K 时钟源（32768 Hz）
   - 重载值为 32000，触发周期约为：32000 / 32768 ≈ 0.976 秒

3. **中断回调**：中断回调函数在中断上下文中执行，应避免执行耗时操作

4. **电源管理**：使用完定时器后，应及时关闭电源以降低功耗

5. **工作模式**：
   - **Repeat 模式**：定时器到达计数值后自动重载，持续触发
   - **OneShot 模式**：定时器到达计数值后停止（本示例未使用）
