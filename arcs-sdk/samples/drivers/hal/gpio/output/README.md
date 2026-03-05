# GPIO 输出示例

本示例演示 GPIO 引脚输出功能，周期性地将 PA20 引脚输出高电平和低电平。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **GPIO 输出配置**：配置 GPIO 引脚为输出模式
- **引脚复用配置**：设置引脚为 GPIO 功能
- **电平输出控制**：输出高电平和低电平
- **周期性翻转**：每秒翻转一次引脚电平
- **GPIO 状态查询**：查询引脚的方向配置

### 硬件要求

- ARCS 系列开发板
- 逻辑分析仪或示波器（用于观察 PA20 引脚电平变化）
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/gpio/output
./build.sh
```

或者在 SDK 根目录执行：

### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 2. 烧录运行

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

### 3. 查看输出

烧录完成后，复位开发板，通过串口工具查看日志输出，同时使用逻辑分析仪或示波器观察 PA20 引脚的电平变化。

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
| 引脚 | PA20（`CSK_GPIO_PIN20`） |
| 方向 | `CSK_GPIO_DIR_OUTPUT` |
| 消抖功能 | `CSK_GPIO_DEBOUNCE_DISABLE` |
| 延时周期 | `vTaskDelay(pdMS_TO_TICKS(1000))` |

## 📋 代码解析

### 关键代码段

#### 1. GPIO 句柄初始化

```c
static void* GPIOA_Handler = NULL;

int main(int argc, char **argv)
{
    printf("Hello, world! \n");
    
    GPIOA_Handler = GPIOA();
    
    gpio_output();
    
    return 0;
}
```

#### 2. 引脚复用配置

```c
/* 设置 PA20 引脚为 GPIO，具体 IOMUX 列表见芯片手册的 APPENDIX 章节 */
IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);
```

#### 3. GPIO 初始化

```c
/* 初始化 GPIOA 外设，包含使能 GPIOA 的时钟，注册 GPIOA 的中断回调，使能 GPIOA 的中断等 */
GPIO_Initialize(GPIOA_Handler, NULL, NULL);
```

#### 4. GPIO 状态查询

```c
_GPIO_ *status;
uint32_t size;

/* 获取 GPIOA 所有引脚（GPIOA 共 32 个引脚）的状态，包含方向，上下拉模式、中断模式等配置 */
GPIO_Status(GPIOA_Handler, &status, &size);
printf("PA20 direction: %d\n", status[20].dir);
```

#### 5. GPIO 控制配置

```c
/* 设置 PA20 引脚不使能消抖功能 
 * 此函数主要功能如下
 *  - 设置 GPIO 引脚的消抖功能
 *  - 设置 GPIO 输入引脚的中断模式
 *  - 设置 GPIO 引脚上拉/下拉模式
 */
GPIO_Control(GPIOA_Handler, CSK_GPIO_DEBOUNCE_DISABLE, CSK_GPIO_PIN20);
```

#### 6. 设置引脚方向

```c
/* 设置 PA20 为输出引脚 */
GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN20, CSK_GPIO_DIR_OUTPUT);

/* 再次查询状态，验证配置是否成功 */
GPIO_Status(GPIOA_Handler, &status, &size);
printf("PA20 direction: %d\n", status[20].dir);
```

#### 7. 周期性翻转引脚

```c
while(1) {
    /* PA20 引脚输出高电平 */
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN20, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    /* PA20 引脚输出低电平 */
    GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN20, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

#### 8. GPIO 去初始化

```c
/* GPIOA 外设逆初始化，包含关闭 GPIOA 的时钟，关闭 GPIOA 的中断等 */
GPIO_Uninitialize(GPIOA_Handler);
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! 
PA20 direction: 0
PA20 direction: 1
```

输出说明：
- `Hello, world!`：程序启动信息
- `PA20 direction: 0`：初始化后 PA20 方向为 0（输入模式）
- `PA20 direction: 1`：设置后 PA20 方向为 1（输出模式）

同时，PA20 引脚将周期性输出：
- 高电平（1）持续 1 秒
- 低电平（0）持续 1 秒
- 持续循环

## ⚠️ 注意事项

1. **引脚方向**：
   - 方向值 0：输入模式
   - 方向值 1：输出模式

2. **引脚复用**：
   - 使用 GPIO 功能前需要配置引脚复用
   - 使用 `CSK_IOMUX_FUNC_DEFAULT` 配置为默认 GPIO 功能
   - 具体复用配置见芯片手册 APPENDIX 章节

3. **消抖功能**：
   - 消抖功能主要用于输入引脚
   - 输出引脚通常禁用消抖功能

4. **GPIO_Control 函数**：
   - 该函数功能较多，可用于：
     - 设置消抖功能
     - 设置中断模式
     - 设置上拉/下拉模式

5. **电平输出**：
   - `GPIO_PinWrite(handler, pin, 1)`：输出高电平
   - `GPIO_PinWrite(handler, pin, 0)`：输出低电平

6. **无限循环**：
   - 本示例使用 `while(1)` 无限循环
   - 实际应用中可根据需要添加退出条件

7. **FreeRTOS 延时**：
   - 使用 `vTaskDelay(pdMS_TO_TICKS(1000))` 实现 1 秒延时
   - 不会阻塞其他任务的执行

8. **GPIO 状态结构**：
   - `GPIO_Status()` 返回所有引脚的状态数组
   - 可通过索引访问特定引脚的状态
   - GPIOA 共有 32 个引脚（索引 0-31）
