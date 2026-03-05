# DMA 示例

本示例演示如何使用 DMA 进行内存到内存（M2M）数据传输，包括初始化源和目标缓冲区、配置 DMA 通道、传输数据并验证传输结果。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **内存到内存传输**：使用 DMA 将数据从源缓冲区传输到目标缓冲区
- **DMA 通道配置**：配置 DMA 传输宽度、突发大小、地址递增等参数
- **事件回调机制**：注册并处理 DMA 传输完成事件
- **传输验证**：使用内存比较验证数据传输的正确性
- **缓存同步**：支持 DMA 传输的缓存一致性管理

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/dma
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/dma -C
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x1000 build/arcs.bin -C arcs
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x1000`：烧录起始地址
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

### DMA 参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| `MaxLen` | 1024 | 缓冲区最大长度（单位：32位字） |
| `SourceLen` | 1023 | 源缓冲区大小 |
| `ReceiveLen` | 1023 | 目标缓冲区大小 |
| `UnitLen` | 4 | 单位长度（字节） |
| `SRC_MASTER_SEL` | 0 | 源主设备选择（AP 仅支持 Master 0） |
| `DST_MASTER_SEL` | 0 | 目标主设备选择（AP 仅支持 Master 0） |

### DMA 传输配置

本示例配置的 DMA 传输参数：

```c
DMA_configure_check(&ch, 
                    DMA_WIDTH_WORD,      // 源宽度：32位字
                    DMA_BSIZE_16,        // 源突发大小：16
                    DMA_WIDTH_BYTE,      // 目标宽度：字节
                    DMA_BSIZE_64,        // 目标突发大小：64
                    false,               // 地址递增模式
                    DMA_CACHE_SYNC_AUTO); // 自动缓存同步
```

## 📋 代码解析

### 关键代码段

#### 1. DMA 初始化

```c
int main(int argc, char **argv)
{
    // 初始化 DMA 控制器
    dma_initialize();
    printf("Hello, world! dma\n");
    
    // 配置 DMA 传输
    uint8_t ch = 0;  // ch可取0-DMA_NUMBER_OF_CHANNELS
    DMA_configure_check(&ch, DMA_WIDTH_WORD, DMA_BSIZE_16, 
                        DMA_WIDTH_BYTE, DMA_BSIZE_64, 
                        false, DMA_CACHE_SYNC_AUTO);
    return 0;
}
```

#### 2. 缓冲区初始化

```c
void DMA_mem_int(uint32_t total_bytes)
{
    DMA_Src_Buf_Gen((uint8_t *)SourceBuf, total_bytes);
    DMA_Dst_Buf_Gen((uint8_t *)DestinBuf, total_bytes);
}

// 生成源缓冲区内容（顺序填充0~255）
static void DMA_Src_Buf_Gen(uint8_t *buffer, uint32_t size)
{
    int i;
    for (i = 0; i < size; i++)
    {
        buffer[i] = i % 256;
    }
}

// 生成目的缓冲区内容（填充平方值 % 256）
static void DMA_Dst_Buf_Gen(uint8_t *buffer, uint32_t size)
{
    int i = 0;
    for (i = 0; i < size; i++)
    {
        buffer[i] = (i * i) % 256;
    }
}
```

#### 3. DMA 事件回调

```c
static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    printf("[%s]: event = %d, channel = %d, xfer_bytes = %d\r\n", __func__,
           event_info & 0xFF, (event_info >> 8) & 0xFF, xfer_bytes);
    DMAEvent = event_info & 0xFF;
}
```

#### 4. 等待 DMA 完成

```c
static void DMA_Waiting(void)
{
    while (1)
    {
        if (DMAEvent & DMA_EVENT_TRANSFER_COMPLETE)
        {
            DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
            break;
        }
    }
}
```

#### 5. DMA 通道配置

```c
// 申请 DMA 通道
uint8_t ch = dma_channel_select(pch, DMA_DrvEvent, 0, cache_sync);
if (ch == DMA_CHANNEL_ANY)
{
    printf("[FAILED] NO free DMA channel!!\r\n");
    return false;
}

// 配置 DMA 通道
stat = dma_channel_configure(ch, src_addr, dst_addr, 
                             total_bytes / WIDTH_BYTES(src_width),
                             control, config_low, config_high, 0, 0);

if (stat == -1)
{
    printf("[FAILED] dma_channel_configure error.\r\n");
    dma_channel_disable(ch, true);
    return false;
}
```

#### 6. 传输验证

```c
// 比较内存，判断是否传输一致
if (memcmp(SourceBuf, DestinBuf, total_bytes) == 0)
{
    printf("Memory compare success\n");
    return true;
}
else
{
    printf("[%s FAILED] channel %d, Memory compare error !!!!!\r\n", 
           __func__, ch);
    return false;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! dma
SourceBuf: DD CC BB AA 
DestinBuf (Before): 00 99 88 77 
[DMA_DrvEvent]: event = 1, channel = 0, xfer_bytes = 4092
After DMA transfer:
DestinBuf (After ): DD CC BB AA 
Memory compare success
```

输出说明：
- `Hello, world! dma`：程序启动信息
- `SourceBuf`：源缓冲区数据（传输前）
- `DestinBuf (Before)`：目标缓冲区数据（传输前）
- `[DMA_DrvEvent]`：DMA 传输完成事件回调
  - `event = 1`：传输完成事件
  - `channel = 0`：使用的 DMA 通道号
  - `xfer_bytes = 4092`：传输的字节数（1023 × 4）
- `DestinBuf (After)`：目标缓冲区数据（传输后）
- `Memory compare success`：内存比较成功，数据传输正确

## ⚠️ 注意事项

1. **烧录地址**：
   - 本示例的烧录地址为 `0x1000`

2. **DMA 传输宽度**：
   - 源宽度：`DMA_WIDTH_WORD`（32位字）
   - 目标宽度：`DMA_WIDTH_BYTE`（字节）

3. **突发大小**：
   - 源突发大小：`DMA_BSIZE_16`（16）
   - 目标突发大小：`DMA_BSIZE_64`（64）

4. **地址模式**：
   - `addr_dec = false`：使用地址递增模式
   - `addr_dec = true`：使用地址递减模式（本示例未使用）

5. **缓存同步**：
   - `DMA_CACHE_SYNC_AUTO`：自动处理缓存一致性
   - DMA 传输时需要确保 CPU 缓存与内存数据同步

6. **传输大小计算**：
   - 总传输字节数：`SourceLen × UnitLen = 1023 × 4 = 4092` 字节
   - DMA 传输单元数：`total_bytes / WIDTH_BYTES(src_width)`

7. **通道管理**：
   - 使用前需要通过 `dma_channel_select()` 申请通道
   - 使用后可以通过 `dma_channel_disable()` 释放通道
   - 通道号范围：0 到 `DMA_NUMBER_OF_CHANNELS - 1`

8. **错误处理**：
   - 检查通道申请是否成功（是否返回 `DMA_CHANNEL_ANY`）
   - 检查通道配置是否成功（是否返回 -1）
   - 传输完成后验证数据正确性
