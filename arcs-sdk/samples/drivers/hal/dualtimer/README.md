# Dual Timer 示例

本示例演示如何使用 Dual Timer 的周期性（Periodic）模式进行计数，定时器从 DUAL_TIMER_RELOAD_COUNT 周期装载计数值并触发中断。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **周期模式计数**：使用 Dual Timer 的周期重载计数模式
- **32 位定时器**：配置为 32 位计数器模式
- **中断处理**：实现定时器中断回调函数处理
- **定时器控制**：演示定时器的初始化、启动、停止等完整流程
- **计数值读取**：读取定时器当前计数值

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/dualtimer
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/dualtimer -C
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

```shell
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
| `DUAL_TIMER_TEST_CHANNEL` | `CSK_TIMER_CHANNEL_1` | 使用的定时器通道 |
| `DUAL_TIMER_RELOAD_COUNT` | 16000 | 定时器重载计数值 |
| 预分频 | `Divide_1` | 不分频 |
| 计数器位宽 | 32位 | 使用 32 位计数器 |
| 工作模式 | Periodic | 周期重载模式 |
| 中断使能 | Enabled | 使能定时器中断 |

## 📋 代码解析

### 关键代码段

#### 1. 定时器句柄初始化

```c
static void *DUAL_TIMER_Handler = NULL;

static void DUAL_TIMER_Init_Handler()
{
    DUAL_TIMER_Handler = DUALTIMERS1();
}
```

#### 2. 定时器中断回调

```c
volatile uint32_t intFlag = 0;

static void DUAL_TIMER_EventCallback(uint32_t event, void *workspace)
{
    printf("Trigger dual timer interrupt: %d\n", event);
    intFlag = 1;
}
```

#### 3. 周期模式定时器配置

```c
static void DUAL_TIMER_Interrupt_Periodic(void)
{
    printf("DUAL_TIMER_Interrupt_Periodic begin\n");
    
    // 初始化定时器
    DUALTIMERS_Initialize(DUAL_TIMER_Handler);
    
    // 使能定时器电源
    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_FULL);
    
    // 配置定时器模式
    DUALTIMERS_Control(DUAL_TIMER_Handler,
                       CSK_TIMER_PRESCALE_Divide_1 |      // 预分频：不分频
                       CSK_TIMER_SIZE_32Bit |              // 32位计数器
                       CSK_TIMER_MODE_Periodic |           // 周期模式
                       CSK_TIMER_INTERRUPT_Enabled,        // 使能中断
                       DUAL_TIMER_TEST_CHANNEL);
    
    // 设置中断回调函数
    DUALTIMERS_SetTimerCallback(DUAL_TIMER_Handler, 
                                DUAL_TIMER_TEST_CHANNEL, 
                                DUAL_TIMER_EventCallback, NULL);
    
    // 设置定时器周期（计数值）
    DUALTIMERS_SetTimerPeriodByCount(DUAL_TIMER_Handler, 
                                     DUAL_TIMER_TEST_CHANNEL, 
                                     DUAL_TIMER_RELOAD_COUNT);
    
    // 启动定时器
    DUALTIMERS_StartTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);
    
    // 等待定时器中断触发
    while (intFlag == 0);
    
    intFlag = 0;
    
    // 读取定时器当前计数值
    uint32_t count = 0;
    DUALTIMERS_ReadTimerCount(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL, &count);
    
    // 停止定时器
    DUALTIMERS_StopTimer(DUAL_TIMER_Handler, DUAL_TIMER_TEST_CHANNEL);
    
    // 关闭电源
    DUALTIMERS_PowerControl(DUAL_TIMER_Handler, CSK_POWER_OFF);
    
    // 释放定时器资源
    DUALTIMERS_Uninitialize(DUAL_TIMER_Handler);
    
    printf("DUAL_TIMER_Interrupt_Periodic end");
}
```

#### 4. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! dualtimer\n");
    
    // 初始化定时器句柄
    DUAL_TIMER_Init_Handler();
    
    // 周期性定时器中断模式（按设定周期重复触发中断）
    DUAL_TIMER_Interrupt_Periodic();
    
    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! dualtimer
DUAL_TIMER_Interrupt_Periodic begin
Trigger dual timer interrupt: 2
DUAL_TIMER_Interrupt_Periodic end
```

输出说明：
- `Hello, world! dualtimer`：程序启动信息
- `DUAL_TIMER_Interrupt_Periodic begin`：开始周期模式定时器测试
- `Trigger dual timer interrupt: 2`：定时器中断触发，事件值为 2
- `DUAL_TIMER_Interrupt_Periodic end`：定时器测试完成

## ⚠️ 注意事项

1. **Dual Timer 通道**：
   - 本示例使用通道 1（`CSK_TIMER_CHANNEL_1`）
   - Dual Timer 通常有 2 个独立通道可用

2. **计数器位宽**：
   - 配置为 32 位计数器（`CSK_TIMER_SIZE_32Bit`）
   - 也可配置为 16 位计数器（`CSK_TIMER_SIZE_16Bit`）

3. **预分频设置**：
   - `CSK_TIMER_PRESCALE_Divide_1`：不分频
   - 可选其他分频比（Divide_16, Divide_256）

4. **工作模式**：
   - **Periodic 模式**：定时器到达 0 后自动重载，持续触发
   - **OneShot 模式**：定时器到达 0 后停止（本示例未使用）

5. **中断回调**：
   - 中断回调函数在中断上下文中执行
   - 应避免在回调中执行耗时操作
   - 使用标志位（intFlag）通知主程序中断已触发

6. **计数方向**：
   - Dual Timer 为递减计数器
   - 从重载值开始递减到 0 时触发中断
   - 然后自动重载并继续计数

7. **电源管理**：
   - 使用前需要使能电源（`CSK_POWER_FULL`）
   - 使用完成后关闭电源（`CSK_POWER_OFF`）以降低功耗

8. **资源释放**：
   - 使用完定时器后应调用 `DUALTIMERS_Uninitialize()` 释放资源
   - 确保定时器已停止后再释放
