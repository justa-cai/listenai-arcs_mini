# RTC 示例

本 RTC 示例演示了 RTC 闹钟功能和分钟中断功能。通过设置 RTC 时间和闹钟时间，当 RTC 时间到达闹钟时间时会触发闹钟中断。同时 RTC 也会在整分钟时产生中断。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **RTC 时间设置**：设置初始 RTC 时间
- **RTC 闹钟功能**：设置闹钟时间并触发中断
- **分钟中断**：在整分钟时触发中断
- **时间读取**：读取并显示当前 RTC 时间
- **校准功能**：使能 RTC 校准以减少频偏

### RTC 配置参数

| 参数 | 值 |
|------|-----|
| 初始时间 | 1年6月6日14时48分55秒 |
| 闹钟时间 | 1年6月6日14时49分10秒 |
| 星期（`weekend`） | 5（代码注释：星期五） |
| 中断类型 | `CSK_CALENDAR_CTRL_MIN_INT` |

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/rtc
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

烧录完成后，复位开发板，通过串口工具查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### RTC 时间结构

```c
CSK_CALENDAR_TIME set_time = {
    .year = 1,      // 1年，注意：这个参数最大值为127，超过127会出错
    .month = 6,     // 6月
    .weekend = 5,   // 星期五，即6月6日是星期五
    .day = 6,       // 6日
    .hour = 14,     // 14时
    .min = 48,      // 48分
    .sec = 55,      // 55秒
};
```

### RTC 闹钟结构

```c
CSK_CALENDAR_ALARM alarm_time = {
    .year = 1,      // 1年，注意：这个参数最大值为127，超过127会出错
    .month = 6,     // 6月
    .day = 6,       // 6日
    .hour = 14,     // 14时
    .min = 49,      // 49分
    .sec = 55,      // 55秒
};
```

### 中断控制配置

| 控制类型 | 值 |
|----------|-----|
| 闹钟中断 | `CSK_CALENDAR_CTRL_ALARM_EN` |
| 分钟中断 | `CSK_CALENDAR_CTRL_MIN_INT` |
| 校准功能 | `CSK_CALENDAR_CTRL_CALIBRATION_EN` |

## 📋 代码解析

### 关键代码段

#### 1. RTC 初始化

```c
static void* CALENDAR_Handler = NULL;

static void CALENDAR_Init_Handler()
{
    CALENDAR_Handler = CALENDAR();
}
```

#### 2. 事件回调函数

```c
static void CALENDAR_EventCallback(uint32_t event, void* workspace)
{
    /* RTC 闹钟中断 */
    if (event & CSK_CALENDAR_EVENT_ALARM_INT) {
        printf("RTC Alarm interrupt trigger\n");
    }

    if (event & CSK_CALENDAR_EVENT_HOUR_INT) {
        printf("RTC Hour interrupt trigger\n");
    } else if (event & CSK_CALENDAR_EVENT_MIN_INT) {
        printf("RTC Minute interrupt trigger\n");
    } else if (event & CSK_CALENDAR_EVENT_SEC_INT) {
        printf("RTC Second interrupt trigger\n");
    }

    /* 读取并显示当前时间 */
    CSK_CALENDAR_TIME stime;
    CALENDAR_GetTime(CALENDAR_Handler, &stime);
    printf("Years: %d; Month: %d; Week: %d; Day: %d; Hour: %d; Min: %d; Sec: %d\n",
           stime.year, stime.month, stime.weekend, stime.day, stime.hour,
           stime.min, stime.sec);
}
```

#### 3. RTC 配置和启动

```c
static void CALENDAR_Test()
{
    /* 设置初始时间：1年6月6日14时48分55秒 */
    CSK_CALENDAR_TIME set_time = {
        .year = 1,      // 1年
        .month = 6,     // 6月
        .weekend = 5,   // 星期五
        .day = 6,       // 6日
        .hour = 14,     // 14时
        .min = 48,      // 48分
        .sec = 55,      // 55秒
    };

    /* 设置闹钟时间：1年6月6日14时49分10秒 */
    CSK_CALENDAR_ALARM alarm_time = {
        .year = 1,      // 1年
        .month = 6,     // 6月
        .day = 6,       // 6日
        .hour = 14,     // 14时
        .min = 49,      // 49分
        .sec = 55,      // 55秒
    };

    /* RTC 初始化，注册事件回调 */
    CALENDAR_Initialize(CALENDAR_Handler, CALENDAR_EventCallback, NULL);
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_FULL);

    /* 设置 RTC 时间 */
    CALENDAR_SetTime(CALENDAR_Handler, &set_time);

    /* 设置闹钟时间 */
    CALENDAR_SetAlarm(CALENDAR_Handler, &alarm_time);
    /* 使能闹钟中断 */
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_ALARM_EN, 1);

    /* 使能分钟中断 */
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_MIN_INT, 1);

    /* 使能校准 */
    CALENDAR_Control(CALENDAR_Handler, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);
}
```

#### 4. 主循环和清理

```c
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 关闭 RTC */
    CALENDAR_PowerControl(CALENDAR_Handler, CSK_POWER_OFF);
    CALENDAR_Uninitialize(CALENDAR_Handler);
```

#### 5. 主函数

```c
int main(int argc, char **argv)
{
    printf("Hello, world! RTC\n");

    CALENDAR_Init_Handler();
    CALENDAR_Test();

    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
Running on hart-id: 1
Hello, world! RTC
RTC Minute interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 49; Sec: 0
RTC Alarm interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 49; Sec: 10
RTC Minute interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 50; Sec: 0
RTC Minute interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 51; Sec: 0
RTC Minute interrupt trigger
Years: 1; Month: 6; Week: 5; Day: 6; Hour: 14; Min: 52; Sec: 0
...
```

输出说明：
- **RTC Minute interrupt trigger**：每分钟触发一次分钟中断
- **RTC Alarm interrupt trigger**：闹钟时间到达时触发闹钟中断
- **时间显示**：显示当前 RTC 的完整时间信息

## ⚠️ 注意事项

1. **年份限制**：
   - 年份参数最大值为 127，超过 127 会出错
   - 示例中使用年份为 1

2. **中断类型限制**：
   - 秒、分钟、小时的中断一次只能设置一个
   - 分钟、小时的中断只会在整分或整小时时触发
   - 示例中启用的是分钟中断

3. **校准功能**：
   - 如果不使能校准，由于 RTC 使用内部低精度时钟，运行一定时间后 RTC 会存在频偏，进而导致 RTC 时间存在偏移

4. **星期说明**：
   - 代码注释：星期五，即6月6日是星期五（`weekend = 5`）

5. **事件回调**：
   - 所有中断事件都在 `CALENDAR_EventCallback` 函数中处理
   - 通过 `event` 标志位判断中断类型

6. **资源管理**：
   - 代码中包含 `CALENDAR_PowerControl` 和 `CALENDAR_Uninitialize` 调用

7. **时间结构**：
   - `CSK_CALENDAR_TIME` 结构包含：year、month、weekend、day、hour、min、sec