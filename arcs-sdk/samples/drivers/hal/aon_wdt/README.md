# AON WDT 示例

本示例演示如何使用 AON WDT（Always-On Watchdog Timer）的中断喂狗功能，在看门狗计数器到达 0 时触发中断并执行喂狗操作。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **中断模式喂狗**：看门狗计数器到 0 时触发中断而不是复位
- **计数器重载**：从配置的重载值开始递减计数
- **喂狗操作**：在中断回调中执行喂狗，刷新计数器
- **日志系统集成**：展示如何使用 EasyLogger 进行日志输出

### 硬件要求

- 支持 AON WDT 的 ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/aon_wdt
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/aon_wdt -C
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

### 看门狗参数配置

在源码中可以配置以下参数：

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `AON_WDT_Reload` | 20000 | 看门狗计数器重载值 |
| 工作模式 | 中断模式 | 计数到 0 触发中断而非复位 |
| 中断使能 | Enabled | 使能看门狗中断 |
| 复位域 | PMU Domain | 配置复位范围 |

## 📋 代码解析

### 关键代码段

#### 1. 看门狗初始化

```c
void AON_WDT_Init(void)
{
    // 获取看门狗句柄
    AON_WDT_Handler = AON_WDT();
    
    // 初始化看门狗，注册中断回调函数
    AON_WDT_Initialize(AON_WDT_Handler, AON_WDT_Feed_EventCallback, NULL);
    
    // 使能看门狗电源
    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_FULL);
    
    // 配置看门狗计数初始值
    AON_WDT_Control(AON_WDT_Handler, HAL_AON_WDT_TIME_CFG, AON_WDT_Reload);
}
```

#### 2. 中断模式配置

```c
void AON_WDT_INT_Feed_Sample(void)
{
    // 配置看门狗工作模式
    AON_WDT_Control(AON_WDT_Handler, 
                    HAL_AON_WDT_INTERRUPT_EN |      // 使能中断
                    HAL_AON_WDT_CTRL_INT_MODE |     // 中断模式
                    HAL_AON_WDT_RST_PMU_DOMAIN,     // PMU域复位
                    1);
    
    // 使能看门狗
    AON_WDT_Enable(AON_WDT_Handler);
    
    // 等待看门狗中断触发
    while (!AON_WDT_Trigger);
    
    AON_WDT_Trigger = 0;
}
```

#### 3. 中断回调喂狗

```c
void AON_WDT_Feed_EventCallback(void *workspace)
{
    LOGI("Aon_Wdt trigger,feed dog");
    
    // 喂狗操作，刷新看门狗计数器
    AON_WDT_Refresh(AON_WDT_Handler);
    
    // 设置触发标志
    AON_WDT_Trigger = 1;
}
```

#### 4. 看门狗去初始化

```c
void AON_WDT_UnInit(void)
{
    // 禁用看门狗
    AON_WDT_Disable(AON_WDT_Handler);
    
    // 关闭看门狗电源
    AON_WDT_PowerControl(AON_WDT_Handler, CSK_POWER_OFF);
    
    // 释放看门狗资源
    AON_WDT_Uninitialize(AON_WDT_Handler);
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
I/elog            [00:00:00.000 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/Aon_Wdt Sample  [00:00:00.001 1 main] Hello, world! Aon_Wdt
I/Aon_Wdt Sample  [00:00:00.734 1 isr] Aon_Wdt trigger,feed dog
I/Aon_Wdt Sample  [00:00:00.734 1 main] Aon_Wdt end
```

输出说明：
- 第 1 行：日志系统初始化成功（EasyLogger V2.2.99）
- 第 2 行：程序启动信息
- 第 3 行：看门狗计数器到 0，触发中断，执行喂狗操作
- 第 4 行：示例程序执行完成

## ⚠️ 注意事项

1. **中断模式与复位模式**：
   - **中断模式**（本示例）：计数到 0 时触发中断，不会导致系统复位
   - **复位模式**：计数到 0 时直接复位系统，用于系统异常恢复

2. **喂狗时机**：
   - 必须在看门狗计数器到 0 之前调用 `AON_WDT_Refresh()` 喂狗
   - 中断模式下，可以在中断回调中喂狗
   - 实际应用中，通常在主循环或定时任务中定期喂狗

3. **计数周期计算**：
   - 使用 RC32K 时钟源（32768 Hz）
   - 重载值为 20000，触发周期约为：20000 / 32768 ≈ 0.61 秒

4. **日志系统**：
   - 本示例使用 EasyLogger 日志系统，需要启用相应配置
   - 日志标签通过 `LOG_TAG` 宏定义
   - 使用 `LOGI()` 宏输出信息级别日志

5. **复位域配置**：
   - `HAL_AON_WDT_RST_PMU_DOMAIN`：复位 PMU 电源域
   - 可根据实际需求选择不同的复位范围

6. **实际应用建议**：
   - 生产环境中通常使用复位模式，确保系统异常时自动恢复
   - 合理设置看门狗超时时间，避免误触发
   - 在关键任务执行前后进行喂狗操作
