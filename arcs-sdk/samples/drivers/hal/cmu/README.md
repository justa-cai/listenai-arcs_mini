# CMU 示例

本示例演示如何使用 CMU（Clock Management Unit）读取并打印系统关键时钟频率和配置信息。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **系统时钟频率查询**：读取 CPU、HCLK、APB、FLASH 时钟频率
- **外设时钟频率查询**：读取 UART0/1/2、SPI0、I2C0 等外设时钟频率
- **时钟配置信息读取**：获取 HCLK、APB、UART0 的时钟源和分频配置
- **时钟源识别**：将时钟源枚举转换为可读的字符串

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/cmu
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/cmu -C
```

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

## 📋 代码解析

### 关键代码段

#### 1. 打印系统时钟频率

```c
static void print_cmu_summary(void)
{
    uint32_t cpu_hz   = CRM_GetCpuFreq();
    uint32_t hclk_hz  = CRM_GetHclkFreq();
    uint32_t apb_hz   = CRM_GetAp_peri_pclkFreq();
    uint32_t flash_hz = CRM_GetFlashFreq();
    
    printf("CPU: %u Hz\n", cpu_hz);
    printf("HCLK: %u Hz\n", hclk_hz);
    printf("APB: %u Hz\n", apb_hz);
    printf("FLASH: %u Hz\n", flash_hz);
    
    // 常用外设频率
    printf("UART0: %u Hz\n", CRM_GetUart0Freq());
    printf("UART1: %u Hz\n", CRM_GetUart1Freq());
    printf("UART2: %u Hz\n", CRM_GetUart2Freq());
    printf("SPI0: %u Hz\n", CRM_GetSpi0Freq());
    printf("I2C0: %u Hz\n", CRM_GetI2c0Freq());
}
```

#### 2. 时钟源枚举转换

```c
static const char* crm_src_to_str(clock_src_name_t src)
{
    switch (src) {
    case CRM_IpSrcInvalide:      return "Invalide";
    case CRM_IpSrcCoreClk:       return "CoreClk";
    case CRM_IpSrcPsramClk:      return "PsramClk";
    case CRM_IpSrcXtalClk:       return "XtalClk";
    case CRM_IpSrcPeriClk:       return "PeriClk";
    case CRM_IpSrcFlashClk:      return "FlashClk";
    case CRM_IpSrcCmn32kClk:     return "Cmn32k";
    case CRM_IpSrcAon32kClk:     return "Aon32k";
    case CRM_IpSrcBBPLLCoreClk:  return "BBPLLCore";
    default:                     return "Unknown";
    }
}
```

#### 3. 打印时钟配置信息

```c
static void print_cmu_config_usage(void)
{
    clock_src_name_t src = 0;
    uint32_t div_n = 0, div_m = 0;
    
    // HCLK：源 + 分频 n/m
    HAL_CRM_GetHclkClkConfig(&src, &div_n, &div_m);
    printf("[CMU] HCLK cfg: src=%u(%s) n=%u m=%u\n",
           (unsigned)src, crm_src_to_str(src), div_n, div_m);
    
    // APB：分频 n/m（源由上游 HCLK 决定）
    HAL_CRM_GetAp_peri_pclkClkConfig(&div_n, &div_m);
    printf("[CMU] APB cfg: n=%u m=%u\n", div_n, div_m);
    
    // UART0：源 + 分频 n/m
    HAL_CRM_GetUart0ClkConfig(&src, &div_n, &div_m);
    printf("[CMU] UART0 cfg: src=%u(%s) n=%u m=%u\n",
           (unsigned)src, crm_src_to_str(src), div_n, div_m);
}
```

#### 4. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! CMU\n");
    
    // 打印时钟频率摘要
    print_cmu_summary();
    
    // 打印时钟配置详情
    print_cmu_config_usage();
    
    printf("CMU check success\n");
    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
********Arcs SDK@test_release-31-g6edf105a-dirty-@v0.0.22********
Running on hart-id: 1
Hello, world! CMU
CPU: 300000000 Hz
HCLK: 300000000 Hz
APB: 100000000 Hz
FLASH: 100000000 Hz
UART0: 3686400 Hz
UART1: 0 Hz
UART2: 0 Hz
SPI0: 0 Hz
I2C0: 0 Hz
[CMU] HCLK cfg: src=1(CoreClk) n=1 m=1
[CMU] APB cfg: n=1 m=3
[CMU] UART0 cfg: src=3(XtalClk) n=96 m=625
CMU check success
```

输出说明：
- `Arcs SDK@...`：SDK 版本信息
- `Running on hart-id: 1`：程序运行在 HART 1（CP 核）
- `Hello, world! CMU`：程序启动信息
- `CPU/HCLK/APB/FLASH`：系统核心时钟频率
- `UART0/1/2, SPI0, I2C0`：外设时钟频率（未使能的外设频率为 0）
- `[CMU] HCLK cfg`：HCLK 时钟源和分频配置
- `[CMU] APB cfg`：APB 分频配置
- `[CMU] UART0 cfg`：UART0 时钟源和分频配置
- `CMU check success`：示例执行成功

## ⚠️ 注意事项

1. **时钟频率值**：
   - 示例日志中显示 CPU 和 HCLK 均为 300MHz
   - FLASH 和 APB 为 100MHz

2. **时钟源类型**：
   - `CoreClk`：CPU 核心时钟
   - `XtalClk`：外部晶振时钟
   - `PsramClk`：PSRAM 时钟
   - `PeriClk`：外设时钟
   - 其他时钟源见 `crm_src_to_str()` 函数

3. **分频配置**：
   - `n` 和 `m` 参数用于时钟分频
   - 示例中 HCLK 配置为 n=1, m=1
   - APB 配置为 n=1, m=3

4. **外设时钟**：
   - 未使能的外设时钟频率显示为 0
   - 只有 UART0 在本示例中有时钟配置
   - UART0 使用晶振时钟源（XtalClk）

5. **只读操作**：
   - 本示例仅读取和显示时钟信息（使用 `CRM_Get*` 和 `HAL_CRM_Get*` 函数）
   - 不修改任何时钟配置
