# UART 示例

本示例演示 UART 的收发功能，使用中断模式。程序会将接收到的数据原样发送回去。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **UART 收发**：使用 UART2 进行数据接收和发送
- **中断模式**：使用中断方式处理 UART 收发
- **回环测试**：接收到的数据原样发送回去
- **超时处理**：支持接收超时中断
- **引脚复用配置**：配置 PA20 和 PA21 为 UART2 功能

### UART 引脚

| 引脚 | UART 功能 | 备注 |
|------|-----------|------|
| PA20 | UART2_RX | UART2 接收 |
| PA21 | UART2_TX | UART2 发送 |

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具
- 硬件连接：PC 端需要通过串口板来连接芯片的 PA20 和 PA21 引脚

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/uart
./build.sh
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 2. 烧录运行

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

烧录完成后，复位开发板，使用串口软件连接 PA20/PA21，发送数据并查看是否收到相同的数据回显。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### UART 参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| UART 控制器 | UART2 | 使用 UART2 外设 |
| TX 引脚 | PA21 | UART2 发送引脚 |
| RX 引脚 | PA20 | UART2 接收引脚 |
| 波特率 | 115200 | 串口波特率 |
| 数据位 | 8位 | 数据位宽度 |
| 校验位 | 无 | 无校验 |
| 停止位 | 1位 | 停止位 |
| 流控 | 无 | 无流控 |
| 接收缓冲区 | 64 字节 | 接收缓冲区大小 |

### UART 控制配置

```c
UART_Control(UART_Handler, 
    CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |   // 异步模式，使能空闲超时中断
    CSK_UART_DATA_BITS_8 |                 // 8位数据位
    CSK_UART_PARITY_NONE |                 // 无校验位
    CSK_UART_STOP_BITS_1 |                 // 1位停止位
    CSK_UART_FLOW_CONTROL_NONE |           // 无流控
    CSK_UART_Function_CONTROL_Int |        // 中断模式
    CSK_UART_GPIO_CONTROL_DEFAULT,         // GPIO控制默认
    115200);                               // 波特率
```

## 📋 代码解析

### 关键代码段

#### 1. UART 引脚定义

```c
/* UART2 PIN*/
#define UART2_IO_TX_PAD          (CSK_IOMUX_PAD_A)
#define UART2_IO_TX_PIN          (21)
#define UART2_IO_TX_SEL          (CSK_IOMUX_FUNC_ALTER4)

#define UART2_IO_RX_PAD          (CSK_IOMUX_PAD_A)
#define UART2_IO_RX_PIN          (20)
#define UART2_IO_RX_SEL          (CSK_IOMUX_FUNC_ALTER4)

#define RX_BUFFER_SIZE           (64)
```

#### 2. 全局变量

```c
static void* UART_Handler = NULL;

static int rpos = 0; 
static uint8_t recv_buffer[RX_BUFFER_SIZE];
```

#### 3. UART 事件回调

```c
static void UART_EventCallback(uint32_t event, void* workspace)
{
    int ipos = 0;
    switch (event) {
    case CSK_UART_EVENT_RECEIVE_COMPLETE:   /* 接收完成中断 */
        if (rpos < RX_BUFFER_SIZE) {
            /* 把收到的数据发送出去 */
            UART_Send(UART_Handler, &recv_buffer[rpos], RX_BUFFER_SIZE - rpos);
        }

        rpos = 0;
        /* 再一次启动串口接收，接收数据量为 RX_BUFFER_SIZE */
        UART_Receive(UART_Handler, recv_buffer, RX_BUFFER_SIZE);
        break;
        
    case CSK_UART_EVENT_RX_TIMEOUT:         /* 接收超时中断 */
        if ((ipos = UART_GetRxCount(UART_Handler)) > rpos) {
            /* 把收到的数据发送出去 */
            UART_Send(UART_Handler, &recv_buffer[rpos], ipos-rpos);
        }
        rpos = ipos;

        break;
    default:
        break;
    }
}
```

#### 4. UART 初始化和配置

```c
static void UART_Interrupt_RXTX(void)
{
    /* 注册用户回调 */
    UART_Initialize(UART_Handler, UART_EventCallback, NULL);

    /* 使能 UART2 时钟，注册 UART2 中断回调、使能 UART2 中断 */
    UART_PowerControl(UART_Handler, CSK_POWER_FULL);

    /* UART 配置 */
    UART_Control(UART_Handler, 
        CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT |
        CSK_UART_DATA_BITS_8 |
        CSK_UART_PARITY_NONE |
        CSK_UART_STOP_BITS_1 |
        CSK_UART_FLOW_CONTROL_NONE |
        CSK_UART_Function_CONTROL_Int |
        CSK_UART_GPIO_CONTROL_DEFAULT, 
        115200);

    /* 使能串口发送 */
    UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);

    /* 使能串口接收 */
    UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

    /* 启动串口接收，接收数据量为 RX_BUFFER_SIZE */
    UART_Receive(UART_Handler, recv_buffer, RX_BUFFER_SIZE);
}
```

#### 5. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! UART RX TX\n");

    /* PA20 和 PA21 引脚配置为 UART2，具体 IOMUX 列表见芯片手册的 APPENDIX 章节 */
    IOMuxManager_PinConfigure(UART2_IO_TX_PAD, UART2_IO_TX_PIN, UART2_IO_TX_SEL);
    IOMuxManager_PinConfigure(UART2_IO_RX_PAD, UART2_IO_RX_PIN, UART2_IO_RX_SEL);

    /* 获取 UART 指针 */
    UART_Handler = UART2();

    /* 启动 UART 操作 */
    UART_Interrupt_RXTX();

    while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! UART RX TX
```

功能验证：
- 用户在 PC 端用串口软件发送数据
- 在串口软件的接收端查看是否收到相同的数据（回环）

## ⚠️ 注意事项

1. **UART2 配置**：
   - 使用 UART2 外设
   - 波特率：115200
   - 格式：8位数据位，无校验位，1位停止位，无流控

2. **引脚复用**：
   - TX 引脚 PA21 配置为功能 4（CSK_IOMUX_FUNC_ALTER4）
   - RX 引脚 PA20 配置为功能 4（CSK_IOMUX_FUNC_ALTER4）

3. **中断事件类型**：
   - `CSK_UART_EVENT_RECEIVE_COMPLETE`：接收完成中断
   - `CSK_UART_EVENT_RX_TIMEOUT`：接收超时中断

4. **接收缓冲区**：
   - 缓冲区大小为 64 字节
   - 使用 `rpos` 记录当前接收位置

5. **回环处理**：
   - 接收完成或超时时，将接收到的数据发送回去
   - 发送完成后重新启动接收

6. **超时机制**：
   - 使用 `CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT` 使能空闲超时中断
   - 当接收空闲一段时间后触发超时中断
   - 超时中断可以处理不满缓冲区的数据

7. **接收流程**：
   - 启动接收：`UART_Receive()`
   - 等待中断：接收完成或超时
   - 发送数据：`UART_Send()`
   - 重新启动接收
