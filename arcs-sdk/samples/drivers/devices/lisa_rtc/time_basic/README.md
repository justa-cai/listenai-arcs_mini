# RTC 基础时间示例

## 功能说明

本示例演示如何使用 LISA RTC 驱动进行时间读写操作。

## 硬件连接

无需外部连接，RTC 为芯片内部外设。

## 示例内容

1. 初始化 RTC 设备
2. 设置当前时间为 2025-01-15 12:30:00
3. 循环读取并显示当前时间

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
=== LISA RTC time example ===
rtc0 device ready
RTC time set: 2025-01-15 12:30:00
Start reading RTC time...
Current time: 2025-01-15 12:30:00
Current time: 2025-01-15 12:30:01
Current time: 2025-01-15 12:30:02
Current time: 2025-01-15 12:30:03
...
```

## 参数说明

- **设备名称**: rtc0
- **初始时间**: 2025-01-15 Wednesday 12:30:00
- **读取间隔**: 1秒

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 RTC 设备 |
| `lisa_rtc_set_time()` | 设置 RTC 时间 |
| `lisa_rtc_get_time()` | 读取 RTC 当前时间 |

## 关键代码

```c
/* 获取 RTC 设备 */
lisa_device_t *rtc_dev = lisa_device_get("rtc0");

/* 设置时间 */
lisa_rtc_time_t time = {
    .year = 25, .month = 1, .day = 15,
    .hour = 12, .minute = 30, .second = 0,
};
lisa_rtc_set_time(rtc_dev, &time);

/* 读取时间 */
lisa_rtc_time_t current;
lisa_rtc_get_time(rtc_dev, &current);
```

## 注意事项

1. **时间格式**: ARCS 使用 CALENDAR 外设，支持完整的年/月/日/星期/时/分/秒
2. **年份范围**: 0-127（建议使用两位数，如 25 代表 2025 年）
3. **时间精度**: 秒级精度
4. **自动校准**: 驱动会自动启用时钟校准功能

