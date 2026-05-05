# LISA RTC 闹钟示例

## 功能说明

本示例演示如何使用 LISA RTC 驱动的闹钟功能。

## 硬件连接

无需外部连接，RTC 为芯片内部外设。

## 示例内容

1. 初始化 RTC 设备
2. 设置当前时间为 2025-01-15 12:00:00
3. 配置闹钟在 12:00:03 触发（3秒后）
4. 注册事件回调函数
5. 等待闹钟触发

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```
=== LISA RTC alarm example ===
rtc0 device ready
RTC time set: 2025-01-15 12:00:00
Alarm set after 3 s -> 12:00:03
Alarm enabled, waiting for trigger...
Current time: 2025-01-15 12:00:00
Current time: 2025-01-15 12:00:01
Current time: 2025-01-15 12:00:02
Current time: 2025-01-15 12:00:03
Alarm triggered!
Alarm test completed!
```

## 参数说明

- **设备名称**: rtc0
- **初始时间**: 2025-01-15 Wednesday 12:00:00
- **闹钟时间**: 12:00:03 (3秒后)
- **闹钟匹配**: 支持年/月/日/时/分/秒多级匹配

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 RTC 设备 |
| `lisa_rtc_set_time()` | 设置 RTC 时间 |
| `lisa_rtc_get_time()` | 读取 RTC 当前时间 |
| `lisa_rtc_set_alarm()` | 设置闹钟时间 |
| `lisa_rtc_set_callback()` | 注册闹钟事件回调函数 |
| `lisa_rtc_enable_alarm()` | 使能闹钟 |

## 关键代码

```c
/* 获取 RTC 设备 */
lisa_device_t *rtc_dev = lisa_device_get("rtc0");

/* 设置当前时间 */
lisa_rtc_time_t time = {
    .year = 25, .month = 1, .day = 15,
    .hour = 12, .minute = 0, .second = 0,
};
lisa_rtc_set_time(rtc_dev, &time);

static volatile bool alarm_triggered = false;

static void alarm_callback(uint32_t event, void *user_data)
{
    (void)user_data;

    if (event & LISA_RTC_EVENT_ALARM) {
        printf("Alarm triggered!\n");
        alarm_triggered = true;
    }
}

/* 设置闹钟（3秒后） */
lisa_rtc_alarm_t alarm = {
    .year = 25, .month = 1, .day = 15,
    .hour = 12, .minute = 0, .second = 3,
};
lisa_rtc_set_alarm(rtc_dev, 0, &alarm);
lisa_rtc_set_callback(rtc_dev, alarm_callback, NULL);
lisa_rtc_enable_alarm(rtc_dev, 0, true);

while (!alarm_triggered) {
    /* 等待中断回调置位标志 */
}

printf("Alarm triggered!\n");
```

## 注意事项

1. **闹钟匹配**: **ARCS CALENDAR 闹钟需要所有字段都匹配**
   - 必须设置完整的年/月/日/时/分/秒
   - **不支持忽略字段**（即使设为 0 也会参与匹配）
   - 如果只想在每天某个时间触发，需要每天重新设置闹钟
2. **年份范围**: 0-127
3. **中断回调**: 回调在中断上下文中执行，应保持简短
4. **闹钟ID**: 当前仅支持 1 个闹钟，ID 固定为 0
