# PWM 示例

本示例演示 PWM 输出功能，PWM0 输出 1kHz 占空比 50% 的波形，PWM1 输出 2kHz 占空比 70% 的波形。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **PWM 输出配置**：配置 GPT0_PWM 的两个通道
- **频率和占空比设置**：设置不同的 PWM 频率和占空比
- **引脚复用配置**：配置 PA20 和 PA21 为 PWM 输出
- **多通道 PWM**：同时使用 PWM0 和 PWM1 两个通道

### PWM 输出参数

| 通道 | 引脚 | 频率 | 占空比 |
|------|------|------|--------|
| PWM0 | PA20 | 1kHz | 50% |
| PWM1 | PA21 | 2kHz | 70% |

### 硬件要求

- ARCS 系列开发板
- 逻辑分析仪或示波器（用于观察 PA20 和 PA21 引脚的 PWM 输出）
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/pwm
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

烧录完成后，复位开发板：
- 通过串口工具查看日志输出
- 使用逻辑分析仪或示波器观察 PA20 和 PA21 引脚的 PWM 波形

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### PWM 参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| PWM 控制器 | GPT0_PWM | 使用 GPT0_PWM 外设 |
| PWM0 引脚 | PA20 | PWM 通道 0 输出 |
| PWM1 引脚 | PA21 | PWM 通道 1 输出 |
| 引脚复用 | 功能 12 | IOMUX 功能选择 |

### PWM 控制配置

代码中配置的 PWM 参数：

```c
CSK_GPT_PWM_MODE |                      // PWM 模式
CSK_GPT_PWM_CLKSRC_PCLK |               // 时钟源：PCLK
CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED |      // 输出模式：边沿对齐
CSK_GPT_PWM_OUTPOLARITY_LOW |           // 输出极性：低
CSK_GPT_PWM_CLKDIV_1 |                  // 时钟分频：1
CSK_GPT_PWM_OPERATION_MODE_PWM          // 操作模式：PWM
```

## 📋 代码解析

### 关键代码段

#### 1. 引脚复用配置

```c
#define PWM_CH0_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH0_PIN           (20)
#define PWM_CH0_SEL           (12)

#define PWM_CH1_PAD           (CSK_IOMUX_PAD_A)
#define PWM_CH1_PIN           (21)
#define PWM_CH1_SEL           (12)

/* 设置 PA20 和 PA21 引脚为 PWM 输出，具体 IOMUX 列表见芯片手册的 APPENDIX 章节 */
IOMuxManager_PinConfigure(PWM_CH0_PAD, PWM_CH0_PIN, PWM_CH0_SEL);
IOMuxManager_PinConfigure(PWM_CH1_PAD, PWM_CH1_PIN, PWM_CH1_SEL);
```

#### 2. PWM 初始化

```c
/* 初始化 GPT0_PWM */
HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);

/* 使能 GPT0_PWM */
ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
if(ret != CSK_DRIVER_OK) {
    printf("line: %d, Error = %d\n", __LINE__, ret);
    goto error;
}
```

#### 3. PWM 通道配置

```c
for (int ch = 0; ch < 2; ch++) {
    /* 配置 PWM 的 ch 通道的时钟源为 PCLK，时钟分频，设置 PWM 输出模式 */
    ret = HAL_GPT_PWMControl(GPT0_PWM(), 
                    CSK_GPT_PWM_MODE | 
                    CSK_GPT_PWM_CLKSRC_PCLK | 
                    CSK_GPT_PWM_OUTMODE_EDGE_ALIGNED | 
                    CSK_GPT_PWM_OUTPOLARITY_LOW |
                    CSK_GPT_PWM_CLKDIV_1 | 
                    CSK_GPT_PWM_OPERATION_MODE_PWM, ch);
    if(ret != CSK_DRIVER_OK) {
        printf("line: %d, ch: %d, Error = %d\n", __LINE__, ch, ret);
        goto error;
    }
}
```

#### 4. 设置频率和占空比

```c
for (int ch = 0; ch < 2; ch++) {
    /* 设置 PWM 的 ch 通道的频率和占空比 */
    ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), ch, (ch + 1) * 1000, 50 + ch * 20);
    if(ret != CSK_DRIVER_OK) {
        printf("line: %d, ch: %d, Error = %d\n", __LINE__, ch, ret);
        goto error;
    }
    
    /* 启动 ch 通道的 PWM 输出 */
    HAL_GPT_EnablePWM(GPT0_PWM(), ch);
}
```

频率和占空比计算：
- ch=0: 频率 = (0+1) * 1000 = 1000Hz，占空比 = 50 + 0*20 = 50%
- ch=1: 频率 = (1+1) * 1000 = 2000Hz，占空比 = 50 + 1*20 = 70%

#### 5. 错误处理

```c
error:
    /* 关闭 GPT0_PWM */
    HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_OFF);
    
    /* 反初始化 GPT0_PWM */
    HAL_GPT_PWMUninitialize(GPT0_PWM());
    
    return;
```

#### 6. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! PWM\n");
    
    GPT_PWM_Output();
    
    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Hello, world! PWM
```

同时：
- PA20 引脚输出 1kHz 频率、50% 占空比的 PWM 波形
- PA21 引脚输出 2kHz 频率、70% 占空比的 PWM 波形

## ⚠️ 注意事项

1. **引脚复用**：
   - PA20 和 PA21 需要配置为功能 12（PWM 功能）
   - 具体 IOMUX 列表见芯片手册 APPENDIX 章节

2. **PWM 通道**：
   - 使用 GPT0_PWM 的两个通道（ch0 和 ch1）
   - ch0 对应 PWM0（PA20）
   - ch1 对应 PWM1（PA21）

3. **频率和占空比设置**：
   - 使用 `HAL_GPT_SetPWMFreqDuty()` 设置
   - 参数：通道号、频率（Hz）、占空比（%）

4. **PWM 配置参数**：
   - 时钟源：PCLK
   - 输出模式：边沿对齐
   - 输出极性：低
   - 时钟分频：1

5. **错误处理**：
   - 每个配置步骤都检查返回值
   - 如果出错，跳转到 error 标签进行清理

6. **PWM 使能**：
   - 配置完成后需要调用 `HAL_GPT_EnablePWM()` 启动输出
   - 每个通道独立使能

7. **观察波形**：
   - 需要使用逻辑分析仪或示波器
   - 连接到 PA20 和 PA21 引脚
   - 验证频率和占空比是否正确
