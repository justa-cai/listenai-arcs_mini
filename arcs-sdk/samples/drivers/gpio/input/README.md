# GPIO 输入示例

本示例演示 GPIO 引脚输入功能，通过 PA21 输出电平控制 PA20 读取输入值。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **GPIO 输入配置**：配置 GPIO 引脚为输入模式
- **GPIO 输出配置**：配置 GPIO 引脚为输出模式
- **电平读取**：读取输入引脚的电平状态
- **引脚环回测试**：PA21 输出到 PA20 输入，验证读取功能
- **周期性读取**：每秒读取一次输入电平

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
cd samples/drivers/gpio/input
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

| 参数 | 值 | 说明 |
|------|-----|------|
| GPIO 组 | GPIOA | 使用 GPIOA 外设 |
| 输入引脚 | PA20 | 配置为输入模式 |
| 输出引脚 | PA21 | 配置为输出模式 |
| 读取周期 | 1000ms | 每秒读取一次 |

## 📋 代码解析

### 关键代码段

#### 1. GPIO 句柄初始化

```c
static void* GPIOA_Handler = NULL;

int main(int argc, char **argv)
{
    printf("Hello, world! \n");
    
    GPIOA_Handler = GPIOA();
    
    gpio_input();
    
    return 0;
}
```

#### 2. 引脚复用配置

```c
/* 设置 PA20 和 PA21 引脚为 GPIO，具体 IOMUX 列表见芯片手册的 APPENDIX 章节 */
IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_DEFAULT);
IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_DEFAULT);
```

#### 3. GPIO 初始化

```c
GPIO_Initialize(GPIOA_Handler, NULL, NULL);
```

#### 4. 设置引脚方向

```c
/* 设置 PA20 为输入引脚，PA21 为输出引脚 */
GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN20, CSK_GPIO_DIR_INPUT);
GPIO_SetDir(GPIOA_Handler, CSK_GPIO_PIN21, CSK_GPIO_DIR_OUTPUT);
```

#### 5. 周期性读取输入

```c
void gpio_input(void)
{
    uint32_t value;
    
    printf("gpio input enter...\n");
    
    /* 配置引脚 */
    /* ... */
    
    while(1) {
        // PA21 输出高电平，PA20 读取
        GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 1);
        value = GPIO_PinRead(GPIOA_Handler, CSK_GPIO_PIN20);
        printf("PA21 -> PA20 value = %d\n", value);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // PA21 输出低电平，PA20 读取
        GPIO_PinWrite(GPIOA_Handler, CSK_GPIO_PIN21, 0);
        value = GPIO_PinRead(GPIOA_Handler, CSK_GPIO_PIN20);
        printf("PA21 -> PA20 value = %d\n", value);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    GPIO_Uninitialize(GPIOA_Handler);
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! 
gpio input enter...
PA21 -> PA20 value = 1
PA21 -> PA20 value = 0
PA21 -> PA20 value = 1
PA21 -> PA20 value = 0
PA21 -> PA20 value = 1
PA21 -> PA20 value = 0
PA21 -> PA20 value = 1
PA21 -> PA20 value = 0
PA21 -> PA20 value = 1
...
```

输出说明：
- `Hello, world!`：程序启动信息
- `gpio input enter...`：进入 GPIO 输入测试
- `PA21 -> PA20 value = 1`：PA21 输出高电平，PA20 读取到高电平（1）
- `PA21 -> PA20 value = 0`：PA21 输出低电平，PA20 读取到低电平（0）
- 持续交替输出

## ⚠️ 注意事项

1. **硬件连接必需**：
   - 必须使用导线连接 PA20 和 PA21 引脚
   - 未连接时 PA20 可能读取到不确定的值

2. **引脚方向**：
   - PA20：输入模式（`CSK_GPIO_DIR_INPUT`）
   - PA21：输出模式（`CSK_GPIO_DIR_OUTPUT`）

3. **电平读取**：
   - `GPIO_PinRead()` 返回引脚的当前电平状态
   - 返回值 1：高电平
   - 返回值 0：低电平

4. **环回测试**：
   - 本示例通过 PA21 输出控制 PA20 输入
   - 这是一种简单的自测试方法
   - 验证 GPIO 输入和输出功能正常

5. **浮空输入**：
   - 如果未连接外部电路，输入引脚处于浮空状态
   - 浮空状态下读取的值不确定
   - 可通过上拉/下拉电阻或内部上下拉设置固定电平

6. **引脚复用**：
   - 使用 GPIO 功能前需要配置引脚复用
   - 使用 `CSK_IOMUX_FUNC_DEFAULT` 配置为默认 GPIO 功能

7. **无限循环**：
   - 本示例使用 `while(1)` 无限循环

8. **读取时序**：
   - 先通过 PA21 输出电平（`GPIO_PinWrite`）
   - 再通过 PA20 读取电平（`GPIO_PinRead`）
   - 代码注释显示：`// PA21 -> PA20 value = 1` 和 `// PA21 -> PA20 value = 0`
