# TRNG 示例

本示例演示 TRNG（True Random Number Generator）的真随机数生成功能，启动 TRNG 后进入中断回调函数生成随机数，共生成 5 次。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **真随机数生成**：使用 TRNG 硬件模块生成真随机数
- **中断模式**：使用中断方式获取随机数
- **事件回调**：注册回调函数处理随机数生成事件
- **TRNG 参数配置**：配置 coldtime、hottime、delaytime 参数

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/trng
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

### TRNG 参数配置

在源码中定义的参数：

| 参数 | 值 | 说明 |
|------|-----|------|
| `TRNGCOUNT` | 5 | 生成随机数次数 |
| coldtime | `CSK_TRNG_COLDTIME_2_23` | Cold time 配置 |
| hottime | `CSK_TRNG_HOTTIME_2_17` | Hot time 配置 |
| delaytime | `CSK_TRNG_DELAYTIME_2_11` | Delay time 配置 |

## 📋 代码解析

### 关键代码段

#### 1. 全局变量定义

```c
#define TRNGCOUNT 5 // 只打印5次

volatile uint32_t testCnt = 0;
```

#### 2. TRNG 数据生成事件回调

```c
static void TRNG_DataGenerate_Event(void *param)
{
    uint32_t trngdata;
    testCnt++;
    
    // 获取 TRNG 数据
    trngdata = HAL_TRNG_GetData(TRNG());
    printf("trng data is 0x%x\n", trngdata);
    
    // 重新启动 TRNG
    HAL_TRNG_Enable(TRNG());
    
    if (testCnt >= TRNGCOUNT) {
        HAL_TRNG_Disable(TRNG());
        printf("trng interrupt test end!!!!");
    }
}
```

#### 3. TRNG 中断模式测试

```c
void TRNG_Test_Interrupt(void)
{
    printf("TRNG test interrupt modes, test begin");
    testCnt = 0;

    // 清理之前的状态
    HAL_TRNG_Uninitialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_OFF);

    // 初始化 TRNG
    HAL_TRNG_Initialize(TRNG());
    HAL_TRNG_PowerControl(TRNG(), CSK_POWER_FULL);
    
    // 配置 TRNG 参数
    HAL_TRNG_Control(TRNG(), 
                     CSK_TRNG_COLDTIME_2_23 | 
                     CSK_TRNG_HOTTIME_2_17 | 
                     CSK_TRNG_DELAYTIME_2_11);

    // 注册回调函数
    HAL_TRNG_RegisterCallback(TRNG(), TRNG_DataGenerate_Event);
    
    // 使能 TRNG 中断
    HAL_TRNG_InterruptEnable(TRNG());

    printf("trng data is below:\n");

    // 启动 TRNG
    HAL_TRNG_Enable(TRNG());
}
```

#### 4. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! TRNG\n");
    
    // 使能 TRNG 时钟
    __HAL_CRM_TRNG_CLK_ENABLE();
    
    TRNG_Test_Interrupt();
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! TRNG
TRNG test interrupt modes, test begintrng data is below:
trng data is 0xbb209e5e
...
trng interrupt test end!!!!
```

输出说明：
- `Hello, world! TRNG`：程序启动信息
- `TRNG test interrupt modes, test begin`：开始中断模式测试
- `trng data is below:`：开始输出随机数
- `trng data is 0xbb209e5e`：生成的随机数（每次运行值不同）
- `...`：省略中间的随机数输出
- `trng interrupt test end!!!!`：生成 5 次后测试结束

## ⚠️ 注意事项

1. **时钟使能**：
   - 使用 TRNG 前必须先使能 TRNG 时钟
   - 调用 `__HAL_CRM_TRNG_CLK_ENABLE()`

2. **中断回调**：
   - 每次生成随机数后会触发中断
   - 在回调函数中读取随机数
   - 读取后需要重新启动 TRNG 以生成下一个随机数

3. **生成次数控制**：
   - 本示例生成 5 次随机数（`TRNGCOUNT = 5`）
   - 达到次数后自动停止 TRNG

4. **TRNG 参数**：
   - coldtime、hottime、delaytime 影响随机数生成的时序
   - 本示例配置为 `CSK_TRNG_COLDTIME_2_23`、`CSK_TRNG_HOTTIME_2_17`、`CSK_TRNG_DELAYTIME_2_11`

5. **随机数值**：
   - 每次运行生成的随机数都不同
   - 输出为 32 位无符号整数（0x 开头的十六进制）

6. **初始化顺序**：
   - 先反初始化和关闭电源（清理之前的状态）
   - 再重新初始化和配置
   - 确保 TRNG 处于干净状态
