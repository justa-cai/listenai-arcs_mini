# eFuse 示例

本示例演示如何使用 eFuse 驱动读取设备的 UUID 和 eFuse 存储器中的数据。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **UUID 读取**：读取设备的 64 位唯一标识符（UUID）
- **eFuse 数据读取**：读取指定地址的 eFuse 存储数据
- **数据格式化输出**：以十六进制格式打印 UUID 和 eFuse 内容

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/efuse
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/efuse -C
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

### eFuse 读取范围

本示例读取的 eFuse 地址范围：

| 项目 | 说明 |
|------|------|
| UUID | 64 位设备唯一标识符 |
| 地址范围 | 0x00 ~ 0x00（仅读取地址 0x0） |

## 📋 代码解析

### 关键代码段

#### 1. UUID 读取

```c
// 读取 64 位 UUID
uint64_t uuid = efuse_read_uuid();

// 打印 64 位 UUID，16 进制格式
uint32_t uuid_high = (uint32_t)(uuid >> 32);
uint32_t uuid_low  = (uint32_t)(uuid & 0xFFFFFFFF);
printf("Device UUID: 0x%08X%08X\n", uuid_high, uuid_low);
```

#### 2. eFuse 数据读取

```c
void efuse_dump_all(void)
{
    printf("Start dumping eFuse contents:\n");
    
    // 打印 eFuse 中从地址 0 至 0x1 中的内容
    for (uint8_t addr = 0; addr < 0x1; addr++) {
        uint32_t value = 0;
        if (efuse_read_word(addr, &value) == 0) {
            // 输出地址及所存值
            printf("eFuse[0x%02X] = 0x%08X\n", addr, value);
        }
    }
}
```

#### 3. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! efuse\n");
    
    // 读取 UUID
    uint64_t uuid = efuse_read_uuid();
    uint32_t uuid_high = (uint32_t)(uuid >> 32);
    uint32_t uuid_low  = (uint32_t)(uuid & 0xFFFFFFFF);
    printf("Device UUID: 0x%08X%08X\n", uuid_high, uuid_low);
    
    // 读取 eFuse 内容
    efuse_dump_all();
    
    printf("eFuse dump complete.\n");
    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! efuse
Device UUID: 0x0000000000000000
Start dumping eFuse contents:
eFuse[0x00] = 0x00000000
eFuse dump complete.
```

输出说明：
- `Hello, world! efuse`：程序启动信息
- `Device UUID: 0x0000000000000000`：设备的 64 位 UUID（未烧录时为全 0）
- `Start dumping eFuse contents:`：开始读取 eFuse 内容
- `eFuse[0x00] = 0x00000000`：地址 0x00 的 eFuse 数据（未烧录时为 0）
- `eFuse dump complete.`：eFuse 读取完成

## ⚠️ 注意事项

1. **烧录地址**：
   - 本示例的烧录地址为 `0x1000`，不同于常见的 `0x0`
   - 请确保烧录时使用正确的地址

2. **eFuse 特性**：
   - eFuse 是一次性可编程（OTP）存储器
   - 数据写入后不可修改或擦除
   - 通常用于存储设备 ID、密钥等关键信息

3. **UUID 说明**：
   - UUID 是设备的唯一标识符，每个芯片不同
   - 未烧录的芯片 UUID 可能为全 0
   - 出厂时通常会烧录唯一的 UUID

4. **只读操作**：
   - 本示例仅演示读取功能
   - 不涉及 eFuse 写入操作
   - 写入操作需要特别小心，因为不可逆

5. **读取范围**：
   - 示例代码仅读取地址 0x00
   - 可以修改循环范围读取更多地址
   - 具体可读取的地址范围取决于芯片型号

6. **数据格式**：
   - UUID 为 64 位数据
   - eFuse 数据以 32 位字（word）为单位读取
   - 输出格式为十六进制

7. **返回值检查**：
   - `efuse_read_word()` 返回 0 表示读取成功
   - 应检查返回值以确保读取操作成功
   - 读取失败时不会打印该地址的数据
