# GPIO 输入中断示例

本示例演示如何使用 GPIO 的输入中断功能，通过下降沿触发中断。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **GPIO 输入中断配置**：配置 GPIO 引脚的中断功能
- **下降沿触发**：配置为下降沿触发中断
- **中断回调处理**：注册并处理 GPIO 中断事件
- **中断使能/禁用**：在中断回调中禁用中断
- **环回测试**：PA21 输出触发 PA20 中断

### 硬件要求

- ARCS 系列开发板
- **硬件连接**：需要连接 PA20 到 PA21 引脚
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 硬件连接

**重要**：在烧录和运行前，需要用导线连接 PA20 和 PA21 引脚。

### 2. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/gpio/input_interrupt
./build.sh
```

或者在 SDK 根目录执行：

### ap固件烧录

### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 3. 烧录运行

将开发板连接到 PC，执行烧录命令：

```bash
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/gpio.bin
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/gpio.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 4. 查看输出

烧录完成后，复位开发板，通过串口工具查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### GPIO 参数配置

| 参数 | 值 |
|------|-----|
| GPIO 组 | GPIOA |
| 输入引脚 | PA20（`CSK_GPIO_DIR_INPUT`，`CSK_GPIO_INTR_ENABLE`） |
| 输出引脚 | PA21（`CSK_GPIO_DIR_OUTPUT`） |
| 中断模式 | `CSK_GPIO_SET_INTR_NEGATIVE_EDGE` |
| 消抖功能 | `CSK_GPIO_DEBOUNCE_DISABLE` |

## 📋 代码解析

### 关键代码段

#### 1. 中断事件标志和回调

```c
static volatile uint32_t GPIOA_Event = 0;

static void GPIOA_EventCallback_Negative(uint32_t event, void* workspace){
    printf("Trigger GPIOA Negative interrupt, event: 0x%x\n", event);
    
    // 在中断回调中禁用 PA20 的中断
    GPIO_Control(GPIOA_Handler, CSK_GPIO_INTR_DISABLE, CSK_GPIO_PIN20);
    
    GPIOA_Event |= event;
}
```

#### 2. GPIO 句柄初始化

```c
static void* GPIOA_Handler = NULL;

int main(int argc, char **argv)
{
    printf("Hello, world! \n");
    
    GPIOA_Handler = GPIOA();
    
    gpio_interrupt();
    
    return 0;
}
```

#### 3. 引脚复用配置

```c
/* 设置 PA20 和 PA21 引脚为 GPIO，具体 IOMUX 列表见芯片手册的 APPENDIX 章节 */
IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);
IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_DEFAULT);
```

#### 4. GPIO 初始化并注册回调

```c
// 初始化 GPIOA，注册中断回调函数
GPIO_Initialize(GPIOA_Handler, GPIOA_EventCallback_Negative, NULL);
```

#### 5. 输出引脚配置

```c
/* 设置 PA21 为输出引脚，并输出高电平 */
GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN21, CSK_GPIO_DIR_OUTPUT);
GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 1);
```

#### 6. 输入中断配置

```c
/* 设置 PA20 为输入引脚 */
GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN20, CSK_GPIO_DIR_INPUT);

/* 设置 PA20 为下降沿触发中断 */
GPIO_Control(GPIOA_Handler,
    CSK_GPIO_DEBOUNCE_DISABLE |           // 禁用消抖
    CSK_GPIO_SET_INTR_NEGATIVE_EDGE |     // 下降沿触发
    CSK_GPIO_INTR_ENABLE,                 // 使能中断
    CSK_GPIO_PIN20);
```

#### 7. 触发中断并等待

```c
void gpio_interrupt(void)
{
    /* 配置引脚和中断 */
    /* ... */
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    /* 开始触发中断：PA21 从高电平变为低电平 */
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 0); 
    
    /* 等待中断事件 */
    while(!(GPIOA_Event & CSK_GPIO_PIN20));
    GPIOA_Event = 0;
    printf("[GPIOA INT] PASS\n");
    
    GPIO_Uninitialize(GPIOA_Handler);
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! 
Trigger GPIOA Negative interrupt, event: 0x100000
[GPIOA INT] PASS
```

输出说明：
- `Hello, world!`：程序启动信息
- `Trigger GPIOA Negative interrupt, event: 0x100000`：触发 PA20 下降沿中断
  - `event: 0x100000`：表示 GPIO PIN20 触发中断（bit 20 置位）
- `[GPIOA INT] PASS`：中断测试通过

## ⚠️ 注意事项

1. **硬件连接必需**：
   - 必须使用导线连接 PA20 和 PA21 引脚
   - PA21 输出控制 PA20 输入，触发中断

2. **中断触发条件**：
   - 本示例配置为下降沿触发（`CSK_GPIO_SET_INTR_NEGATIVE_EDGE`）
   - PA21 从高电平（1）变为低电平（0）时触发 PA20 中断

3. **中断模式选项**：
   - `CSK_GPIO_SET_INTR_POSITIVE_EDGE`：上升沿触发
   - `CSK_GPIO_SET_INTR_NEGATIVE_EDGE`：下降沿触发
   - `CSK_GPIO_SET_INTR_BOTH_EDGE`：双边沿触发
   - `CSK_GPIO_SET_INTR_HIGH_LEVEL`：高电平触发
   - `CSK_GPIO_SET_INTR_LOW_LEVEL`：低电平触发

4. **中断回调**：
   - 中断回调函数在中断上下文中执行
   - 应避免在回调中执行耗时操作
   - 本示例在回调中禁用中断，避免重复触发

5. **中断使能控制**：
   - `CSK_GPIO_INTR_ENABLE`：使能中断
   - `CSK_GPIO_INTR_DISABLE`：禁用中断

6. **消抖功能**：
   - 本示例禁用消抖功能
   - 实际应用中可根据需要使能消抖，防止抖动误触发

7. **事件标志**：
   - `GPIOA_Event` 用于记录中断事件
   - 事件值的 bit 位对应引脚编号
   - bit 20 置位表示 PIN20 触发中断

8. **GPIO_Control 功能**：
   - 该函数可同时配置多个参数（使用位或运算）
   - 包括消抖、中断模式、中断使能等

9. **初始化时注册回调**：
   - 在 `GPIO_Initialize()` 时注册中断回调函数
   - 第二个参数为回调函数指针
   - 第三个参数为用户自定义参数（本示例为 NULL）
