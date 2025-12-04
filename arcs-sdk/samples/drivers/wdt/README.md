# WDT 示例

本示例演示看门狗（WDT）外设的功能。示例中设置看门狗中断时间为 1s，复位阶段为 0.5s，然后刷新了 10 次看门狗，最后等待看门狗触发中断并复位芯片。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **看门狗配置**：配置看门狗的时钟源、中断时间、复位时间
- **看门狗刷新**：定期刷新看门狗，防止复位
- **看门狗复位**：停止刷新后触发中断和系统复位
- **回调函数**：注册看门狗回调函数

### WDT 配置参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 时钟源 | 32k | 外部时钟 32k |
| 中断时间 | 1s | 2^15 / 32k = 1s |
| 复位时间 | 0.5s | 2^14 / 32k = 0.5s |
| 刷新次数 | 10 | 刷新 10 次后停止 |
| 刷新间隔 | 800ms | 每 800ms 刷新一次 |

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/wdt
./build.sh
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 2. 烧录运行

将开发板连接到 PC，执行烧录命令：

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

烧录完成后，复位开发板，通过串口工具查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### WDT 配置结构

```c
hal_driver_wdt_cfg_t wdt_cfg = {
    /* WDT 有两个时钟源可以选择：
     * hal_driver_wdt_clk_src_32k: 外部时钟 32k
     * hal_driver_wdt_clk_src_apb: 内部 APB clock
     */
    .clk_src = hal_driver_wdt_clk_src_32k,
    
    /* 中断阶段为 2^15 / 32k = 1s，中断产生后就开始进入复位阶段
     * 如果中断阶段内 WDT 没有刷新，就会触发中断，然后进入芯片复位阶段
     * hal_driver_wdt_int_time_15: 2^15 / 32k = 1s (interrupt stage)
     */
    .int_time = hal_driver_wdt_int_time_15,
    
    /* 复位阶段为 2^14 / 32k = 0.5s
     * hal_driver_wdt_rst_time_14: 2^14 / 32k = 0.5s (reset stage)
     */
    .rst_time = hal_driver_wdt_rst_time_14,
};
```

## 📋 代码解析

### 关键代码段

#### 1. 看门狗回调函数

```c
static void WDT_Callback_hook(void* workspace){
    // printf("Resetting\n");
}
```

#### 2. WDT 初始化

```c
static void* WDT_Handler = NULL;
WDT_Handler = WDT();

/* 初始化看门狗 */
WDT_Initialize(WDT_Handler, WDT_Callback_hook, NULL);
WDT_PowerControl(WDT_Handler, CSK_POWER_FULL);
```

#### 3. WDT 配置

```c
/* 配置看门狗的时钟源、中断时间、复位时间 */
hal_driver_wdt_cfg_t wdt_cfg = {
    .clk_src = hal_driver_wdt_clk_src_32k,
    .int_time = hal_driver_wdt_int_time_15,
    .rst_time = hal_driver_wdt_rst_time_14,
};
WDT_Control(WDT_Handler, &wdt_cfg);
```

#### 4. 启动看门狗

```c
/* 启动看门狗 */
WDT_Enable(WDT_Handler);
```

#### 5. 刷新看门狗和触发复位

```c
uint32_t i = 0;
while(1){
    vTaskDelay(pdMS_TO_TICKS(800));
    if (++i <= 10) {
        /* 刷新看门狗 */
        WDT_Refresh(WDT_Handler);
        printf("refresh\n");
    } else {
        printf("waiting WDT interrupt trigger\n");
    }
}
```

循环逻辑：
- 每 800ms 执行一次
- 前 10 次刷新看门狗并打印 "refresh"
- 第 11 次及以后不再刷新，打印 "waiting WDT interrupt trigger"
- 等待看门狗中断触发并复位系统

#### 6. 资源清理

```c
/* 关闭看门狗 */
WDT_PowerControl(WDT_Handler, CSK_POWER_OFF);
WDT_Uninitialize(WDT_Handler);
```

#### 7. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! WDT\n");

    wdt_test();

    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
********Arcs SDK@V0.0.9-5-g9273928a-dirty-@v0.0.9********
Running on hart-id: 1
Hello, world! WDT
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
refresh
waiting WDT interrupt trigger

********Arcs SDK@V0.0.9-5-g9273928a-dirty-@v0.0.9********
Running on hart-id: 1
Hello, world! WDT
refresh
...
```

输出说明：
- `Hello, world! WDT`：程序启动信息
- `refresh`：打印 10 次，表示刷新看门狗 10 次
- `waiting WDT interrupt trigger`：停止刷新，等待看门狗触发
- 然后系统复位，重新启动程序（从头开始）
- 循环往复

## ⚠️ 注意事项

1. **时钟源选项**：
   - `hal_driver_wdt_clk_src_32k`：外部时钟 32k
   - `hal_driver_wdt_clk_src_apb`：内部 APB clock

2. **中断阶段**：
   - 中断时间为 2^15 / 32k = 1s
   - 如果中断阶段内 WDT 没有刷新，就会触发中断，然后进入芯片复位阶段

3. **复位阶段**：
   - 复位时间为 2^14 / 32k = 0.5s
   - 中断触发后进入复位阶段，0.5s 后系统复位

4. **刷新时机**：
   - 本示例每 800ms 刷新一次
   - 刷新间隔（800ms）小于中断时间（1s），前 10 次不会触发中断

5. **刷新次数**：
   - 前 10 次循环刷新看门狗
   - 第 11 次及以后停止刷新，等待复位

6. **系统复位**：
   - 看门狗触发复位后，系统重新启动
   - 程序从头开始执行
   - 形成循环：刷新 10 次 → 停止刷新 → 触发中断 → 复位 → 重新启动

7. **回调函数**：
   - 看门狗回调函数被注册但注释掉了打印语句
