# cskburn 烧录工具

## 简介

`cskburn` 是用于 ARCS 芯片的固件烧录工具，支持通过串口烧录固件到 Flash 或 eMMC/T卡 存储器。

## 基本语法

```bash
cskburn -C arcs -s <串口设备> [选项] <地址1> <文件1> [<地址2> <文件2> ...]
```

## 串口烧录选项

### 基本参数

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `-C, --chip arcs` | 指定 ARCS 芯片系列（必需） | - |
| `-s, --serial <端口>` | 指定串口设备（必需） | - |
| `-b, --baud <波特率>` | 串口波特率 | 3000000 |
| `-v, --verbose` | 打印详细日志 | - |
| `--verify-all` | 烧录后验证所有分区 | - |

### 存储类型选项

| 选项 | 说明 | 使用场景 |
|------|------|----------|
| （默认） | 烧录到 Flash | 默认存储类型 |
| `--emmc` | 烧录到 eMMC/T卡 | 使用 eMMC/T卡 存储的设备 |

**eMMC/T卡 硬件接口说明：**
- **接口类型**：SDIO 接口
- **引脚定义**：PA4 ~ PA9（6个引脚）
  - PA6: SDIO_CLK（时钟线）
  - PA7: SDIO_CMD（命令线）
  - PA5: SDIO_D0（数据线0）
  - PA4: SDIO_D1（数据线1）
  - PA9: SDIO_D2（数据线2）
  - PA8: SDIO_D3（数据线3）
- **烧录方式**：通过内置程序使用 SDIO 接口访问 eMMC/T卡
- **支持设备**：eMMC 芯片、TF卡（MicroSD卡）

### 超时和重试选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--probe-timeout <毫秒>` | 探测设备的超时时间 | 10000 ms |
| `--reset-attempts <次数>` | 探测期间重置设备的尝试次数 | 4 |
| `--reset-delay <毫秒>` | 复位线保持低电平的延迟时间 | 500 ms |
| `--timeout <毫秒>` | 操作超时时间（-1:无限制, 0:默认, n>0:n毫秒） | 0 |

### 高级操作

| 选项 | 说明 |
|------|------|
| `--flash-index <索引>` | 设置 Flash 索引（默认：0） |
| `--erase <地址:大小>` | 擦除指定区域 |
| `--erase-all` | 擦除整个存储器 |
| `--verify <地址:大小>` | 验证指定区域 |
| `--chip-id` | 读取芯片唯一 ID |

## Flash 烧录

### 1. 基本 Flash 烧录

```bash
# 烧录单个文件
cskburn -C arcs -s /dev/ttyUSB0 0x0 app.bin

# 烧录多个文件到不同地址
cskburn -C arcs -s /dev/ttyUSB0 0x0 bootloader.bin 0x10000 app.bin 0x100000 resources.bin
```

### 2. Flash 烧录并验证

```bash
# 烧录后自动验证所有分区
cskburn -C arcs -s /dev/ttyUSB0 --verify-all 0x0 app.bin 0x100000 res.bin
```

### 3. 高速 Flash 烧录

```bash
# 使用更高波特率加快烧录速度
cskburn -C arcs -s /dev/ttyUSB0 -b 6000000 --verify-all 0x0 firmware.bin
```

### 4. Flash 擦除操作

```bash
# 擦除指定区域（从 0x0 开始擦除 1MB）
cskburn -C arcs -s /dev/ttyUSB0 --erase 0x0:0x100000

# 擦除多个区域
cskburn -C arcs -s /dev/ttyUSB0 --erase 0x0:0x10000 --erase 0x100000:0x200000

# 擦除整个 Flash
cskburn -C arcs -s /dev/ttyUSB0 --erase-all
```

### 5. Flash 验证操作

```bash
# 验证指定区域
cskburn -C arcs -s /dev/ttyUSB0 --verify 0x0:0x100000

# 验证多个区域
cskburn -C arcs -s /dev/ttyUSB0 --verify 0x0:0x10000 --verify 0x100000:0x200000
```

### 6. 完整 Flash 工作流程

```bash
# 步骤 1：擦除旧固件
cskburn -C arcs -s /dev/ttyUSB0 --erase-all

# 步骤 2：烧录新固件并验证
cskburn -C arcs -s /dev/ttyUSB0 -b 3000000 --verify-all \
    0x0 bootloader.bin \
    0x10000 app.bin \
    0x100000 resources.bin
```

## eMMC/T卡 烧录

eMMC/T卡 烧录通过芯片内置程序访问 SDIO 接口（PA4~PA9）进行数据读写。

**硬件连接说明：**
- 确保 eMMC 芯片或 TF卡已正确连接到 SDIO 引脚（PA4~PA9）
- 供电正常，eMMC/T卡已正确初始化

### 1. 基本 eMMC/T卡 烧录

```bash
# 烧录单个文件到 eMMC/T卡
cskburn -C arcs -s /dev/ttyUSB0 --emmc 0x0 app.bin

# 烧录多个文件到 eMMC/T卡 不同地址
cskburn -C arcs -s /dev/ttyUSB0 --emmc 0x0 bootloader.bin 0x10000 app.bin 0x100000 resources.bin
```

### 2. eMMC/T卡 烧录并验证

```bash
# 烧录到 eMMC/T卡 并验证所有分区
cskburn -C arcs -s /dev/ttyUSB0 --emmc --verify-all 0x0 app.bin 0x100000 res.bin
```

### 3. 高速 eMMC/T卡 烧录

```bash
# 使用高波特率烧录 eMMC/T卡（通过串口加快传输速度）
cskburn -C arcs -s /dev/ttyUSB0 --emmc -b 6000000 --verify-all 0x0 firmware.bin
```

### 4. eMMC/T卡 擦除操作

```bash
# 擦除 eMMC/T卡 指定区域
cskburn -C arcs -s /dev/ttyUSB0 --emmc --erase 0x0:0x100000

# 擦除整个 eMMC/T卡
cskburn -C arcs -s /dev/ttyUSB0 --emmc --erase-all
```

### 5. eMMC/T卡 验证操作

```bash
# 验证 eMMC/T卡 指定区域
cskburn -C arcs -s /dev/ttyUSB0 --emmc --verify 0x0:0x100000
```

### 6. 完整 eMMC/T卡 工作流程

```bash
# 步骤 1：擦除 eMMC/T卡
cskburn -C arcs -s /dev/ttyUSB0 --emmc --erase-all

# 步骤 2：烧录固件到 eMMC/T卡 并验证
cskburn -C arcs -s /dev/ttyUSB0 --emmc -b 3000000 --verify-all \
    0x0 bootloader.bin \
    0x10000 app.bin \
    0x100000 resources.bin
```

**注意事项：**
- eMMC/T卡 烧录是通过串口发送数据到芯片，再由芯片内置程序通过 SDIO 接口（PA4~PA9）写入存储器
- 串口波特率影响数据传输速度，但实际写入速度还受 SDIO 接口和存储器性能限制
- 确保 SDIO 引脚连接正确且无短路或接触不良

## 其他常用操作

### 读取芯片 ID

```bash
cskburn -C arcs -s /dev/ttyUSB0 --chip-id
```

### 详细日志输出

```bash
# 查看详细的烧录过程信息
cskburn -C arcs -s /dev/ttyUSB0 -v 0x0 app.bin
```

### 自定义超时设置

```bash
# 增加超时时间以适应不稳定的连接
cskburn -C arcs -s /dev/ttyUSB0 --probe-timeout 20000 --timeout 30000 0x0 app.bin

# 增加复位尝试次数
cskburn -C arcs -s /dev/ttyUSB0 --reset-attempts 10 0x0 app.bin
```


## 常见问题

### 1. 串口设备未找到或权限不足

**问题现象：**
```
Error: Cannot open /dev/ttyUSB0
```

**解决方案：**
```bash
# 检查串口设备是否存在
ls -l /dev/ttyUSB*

# 将用户添加到 dialout 组（需要重新登录）
sudo usermod -a -G dialout $USER

# 或者临时修改权限
sudo chmod 666 /dev/ttyUSB0
```

### 2. 烧录超时或失败

**解决方案：**
```bash
# 降低波特率
cskburn -C arcs -s /dev/ttyUSB0 -b 921600 0x0 app.bin

# 增加超时时间
cskburn -C arcs -s /dev/ttyUSB0 --probe-timeout 20000 --timeout 30000 0x0 app.bin

# 增加复位尝试次数和延迟
cskburn -C arcs -s /dev/ttyUSB0 --reset-attempts 10 --reset-delay 1000 0x0 app.bin
```

### 3. 设备未检测到

**解决方案：**
- 检查串口连接线是否正常
- 确认芯片供电正常
- 检查芯片是否处于烧录模式
- 使用 `--reset-attempts` 增加重试次数

### 4. 验证失败

**解决方案：**
```bash
# 先完全擦除再烧录
cskburn -C arcs -s /dev/ttyUSB0 --erase-all
cskburn -C arcs -s /dev/ttyUSB0 --verify-all 0x0 app.bin

# 降低波特率重试
cskburn -C arcs -s /dev/ttyUSB0 -b 1500000 --verify-all 0x0 app.bin
```

### 5. eMMC/T卡 烧录失败

**可能原因及解决方案：**
- **SDIO 引脚连接问题**：
  - 检查 PA4~PA9 引脚是否正确连接
  - 确认无虚焊、短路或接触不良
  - 使用万用表测量引脚连通性

- **存储器未初始化**：
  - 确认 eMMC 芯片或 TF卡供电正常（通常 3.3V）
  - TF卡需正确插入卡槽，检查卡槽弹簧接触
  - 尝试更换 eMMC 芯片或 TF卡测试

- **内置烧录程序问题**：
  - 确认芯片固件包含 SDIO 烧录支持
  - 检查芯片是否正确进入烧录模式

```bash
# 增加超时时间重试 eMMC/T卡 烧录
cskburn -C arcs -s /dev/ttyUSB0 --emmc --probe-timeout 20000 --timeout 60000 0x0 app.bin
```

## 重要说明

1. **必须指定 `-C arcs`**：烧录 ARCS 芯片时必须使用此参数

2. **波特率选择**：
   - 推荐：1500000 - 3000000
   - 开发/测试：可尝试更高速率（6000000）
   - 生产环境：建议使用较低波特率确保稳定性

3. **地址格式**：支持十六进制（0x 前缀）和十进制

4. **Flash vs eMMC/T卡**：
   - Flash 烧录：默认行为，无需额外参数
   - eMMC/T卡 烧录：必须添加 `--emmc` 参数，通过 SDIO 接口（PA4~PA9）访问
   - 两者不能同时使用

5. **验证操作**：生产环境务必使用 `--verify-all` 确保烧录成功

6. **擦除操作**：烧录新固件前建议先使用 `--erase-all` 完全擦除

## 完整示例脚本

### Flash 烧录脚本

```bash
#!/bin/bash
SERIAL_PORT="/dev/ttyUSB0"
BAUD_RATE=3000000

echo "开始烧录 ARCS 芯片 Flash..."

# 擦除 Flash
cskburn -C arcs -s $SERIAL_PORT --erase-all
if [ $? -ne 0 ]; then
    echo "擦除失败"
    exit 1
fi

# 烧录固件
cskburn -C arcs -s $SERIAL_PORT -b $BAUD_RATE --verify-all \
    0x0 bootloader.bin \
    0x10000 app.bin \
    0x100000 resources.bin

if [ $? -eq 0 ]; then
    echo "Flash 烧录成功！"
else
    echo "Flash 烧录失败！"
    exit 1
fi
```

### eMMC/T卡 烧录脚本

```bash
#!/bin/bash
SERIAL_PORT="/dev/ttyUSB0"
BAUD_RATE=3000000

echo "开始烧录 ARCS 芯片 eMMC/T卡..."
echo "确保 SDIO 引脚（PA4~PA9）连接正确"

# 擦除 eMMC/T卡
cskburn -C arcs -s $SERIAL_PORT --emmc --erase-all
if [ $? -ne 0 ]; then
    echo "擦除失败，请检查 SDIO 连接和存储器状态"
    exit 1
fi

# 烧录固件到 eMMC/T卡
cskburn -C arcs -s $SERIAL_PORT --emmc -b $BAUD_RATE --verify-all \
    0x0 bootloader.bin \
    0x10000 app.bin \
    0x100000 resources.bin

if [ $? -eq 0 ]; then
    echo "eMMC/T卡 烧录成功！"
else
    echo "eMMC/T卡 烧录失败，请检查硬件连接！"
    exit 1
fi
```
