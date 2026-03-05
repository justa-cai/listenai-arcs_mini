# RTC 驱动

基于 lisa_device 框架的 RTC 设备驱动，为 ARCS 平台提供统一的实时时钟管理接口。

## 功能特性

- **时间管理**：支持完整的年/月/日/星期/时/分/秒设置与读取
- **闹钟功能**：提供多字段匹配的闹钟配置能力
- **周期中断**：支持秒、分钟、小时级别的周期性事件
- **事件回调**：统一的事件通知机制
- **时钟校准**：驱动初始化时自动启用校准功能
- **线程安全**：内部使用互斥锁保证并发访问安全

## 配置选项

在 `Kconfig` 中启用驱动：

```kconfig
CONFIG_LISA_RTC=y
```

驱动依赖芯片内置的 CALENDAR 外设，无需额外硬件配置。

## API 接口

### 时间读写

```c
int lisa_rtc_set_time(lisa_device_t *dev, const lisa_rtc_time_t *time);
int lisa_rtc_get_time(lisa_device_t *dev, lisa_rtc_time_t *time);
```

### 闹钟管理

```c
int lisa_rtc_set_alarm(lisa_device_t *dev, uint8_t alarm_id, const lisa_rtc_alarm_t *alarm);
int lisa_rtc_get_alarm(lisa_device_t *dev, uint8_t alarm_id, lisa_rtc_alarm_t *alarm);
int lisa_rtc_enable_alarm(lisa_device_t *dev, uint8_t alarm_id, bool enable);
```

### 周期性中断与回调

```c
int lisa_rtc_set_periodic_int(lisa_device_t *dev, lisa_rtc_event_t event, bool enable);
int lisa_rtc_set_callback(lisa_device_t *dev, lisa_rtc_callback_t callback, void *user_data);
int lisa_rtc_get_capabilities(lisa_device_t *dev, lisa_rtc_capabilities_t *caps);
```

## 使用示例

### 设置与读取时间

```c
#include "lisa_rtc.h"

// 获取 RTC 设备
lisa_device_t *rtc = lisa_device_get("rtc0");
if (!lisa_device_ready(rtc)) {
    return -1;
}

// 设置时间到 2025-01-01 12:30:00
lisa_rtc_time_t time = {
    .year = 25,      // 25 代表 2025 年
    .month = 1,
    .day = 1,
    .weekday = 3,    // 0=Sunday
    .hour = 12,
    .minute = 30,
    .second = 0,
};
if (lisa_rtc_set_time(rtc, &time) != LISA_DEVICE_OK) {
    return -1;
}

// 读取当前时间
lisa_rtc_time_t current;
if (lisa_rtc_get_time(rtc, &current) == LISA_DEVICE_OK) {
    printf("Current time: 20%02u-%02u-%02u %02u:%02u:%02u\n",
           current.year, current.month, current.day,
           current.hour, current.minute, current.second);
}
```

### 配置闹钟

```c
// 闹钟事件回调
void rtc_event_handler(uint32_t event, void *user_data)
{
    if (event & LISA_RTC_EVENT_ALARM) {
        printf("Alarm triggered\n");
    }
}

// 注册回调函数
lisa_rtc_set_callback(rtc, rtc_event_handler, NULL);

// 配置闹钟时间为 2025-01-15 13:00:00
lisa_rtc_alarm_t alarm = {
    .year = 25,
    .month = 1,
    .day = 15,
    .weekday = 0,
    .hour = 13,
    .minute = 0,
    .second = 0
};
lisa_rtc_set_alarm(rtc, 0, &alarm);

// 启用闹钟
lisa_rtc_enable_alarm(rtc, 0, true);
```

### 周期性中断

```c
// 周期性事件回调
void rtc_tick_handler(uint32_t event, void *user_data)
{
    if (event & LISA_RTC_EVENT_SECOND) {
        printf("Second tick\n");
    }
    if (event & LISA_RTC_EVENT_MINUTE) {
        printf("Minute tick\n");
    }
}

// 注册回调并启用秒/分钟周期事件
lisa_rtc_set_callback(rtc, rtc_tick_handler, NULL);
lisa_rtc_set_periodic_int(rtc, LISA_RTC_EVENT_SECOND, true);
lisa_rtc_set_periodic_int(rtc, LISA_RTC_EVENT_MINUTE, true);
```

## 硬件配置

### 时间字段范围

| 字段 | 说明 | 范围 |
|------|------|------|
| `year` | 年份（偏移值） | 0-127 |
| `month` | 月份 | 1-12 |
| `day` | 日期 | 1-31 |
| `weekday` | 星期（0=Sunday） | 0-6 |
| `hour` | 小时 | 0-23 |
| `minute` | 分钟 | 0-59 |
| `second` | 秒 | 0-59 |

### 闹钟字段范围

| 字段 | 说明 | 范围 |
|------|------|------|
| `year` | 年份匹配 | 0-127 |
| `month` | 月份匹配 | 1-12 |
| `day` | 日期匹配 | 1-31 |
| `hour` | 小时匹配 | 0-23 |
| `minute` | 分钟匹配 | 0-59 |
| `second` | 秒匹配 | 0-59 |

### 事件类型

- `LISA_RTC_EVENT_ALARM`：闹钟事件
- `LISA_RTC_EVENT_SECOND`：秒中断事件
- `LISA_RTC_EVENT_MINUTE`：分钟中断事件
- `LISA_RTC_EVENT_HOUR`：小时中断事件

## 返回值说明

- `LISA_DEVICE_OK (0)`：成功
- `LISA_DEVICE_ERR_INVALID`：参数无效
- `LISA_DEVICE_ERR_RANGE`：闹钟 ID 超出范围
- `LISA_DEVICE_ERR_NOT_READY`：设备未就绪
- `LISA_DEVICE_ERR_NOT_SUPPORT`：不支持的操作
- `LISA_DEVICE_ERR_IO`：底层 IO 错误

## 注意事项

1. 驱动基于芯片 CALENDAR 外设实现，闹钟匹配所有字段，硬件不支持通配符。
2. 如需每日或每小时重复提醒，可在回调中重新设置闹钟，或使用周期性中断替代。
3. 当前实现仅支持 1 个闹钟（`alarm_id` 固定为 0）。
4. 回调函数在中断上下文中执行，应保持逻辑简短。
5. 设置时间和闹钟前需确保设备已正确初始化并上电。
6. 线程安全已在驱动内部实现，可直接在多线程环境中调用。

## 文件说明

- `lisa_rtc.h` —— 驱动头文件，包含 API 与类型定义
- `lisa_rtc_arcs.c` —— ARCS 平台适配实现
- `CMakeLists.txt` —— 构建配置
- `Kconfig` —— 配置选项
