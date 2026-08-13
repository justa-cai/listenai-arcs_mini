///! ARCS SDK Zig RTC 驱动封装
///!
///! 用法:
///!   var rtc = try arcs.Rtc.open("rtc0");
///!   try rtc.setTime(.{ .year = 24, .month = 6, .day = 15, .hour = 10, .minute = 30 });
///!   const now = try rtc.getTime();
///!   log.info("Time: 20{d:0>2}-{d:0>2}-{d:0>2} {d:0>2}:{d:0>2}:{d:0>2}",
///!       .{now.year, now.month, now.day, now.hour, now.minute, now.second});
const c_rtc = @import("../bindings/rtc.zig");
const c_device = @import("../bindings/device.zig");

pub const Rtc = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub const Time = c_rtc.RtcTime;
    pub const Alarm = c_rtc.RtcAlarm;
    pub const Capabilities = c_rtc.RtcCapabilities;
    pub const Event = c_rtc.RtcEvent;
    pub const Weekday = c_rtc.Weekday;

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    /// 设置 RTC 时间
    pub fn setTime(self: Self, time: Time) !void {
        const ret = c_rtc.setTime(self.dev, &time);
        if (ret != 0) return error.SetTimeFailed;
    }

    /// 获取 RTC 时间
    pub fn getTime(self: Self) !Time {
        var time: Time = .{};
        const ret = c_rtc.getTime(self.dev, &time);
        if (ret != 0) return error.GetTimeFailed;
        return time;
    }

    /// 设置闹钟
    pub fn setAlarm(self: Self, alarm_id: u8, alarm: Alarm) !void {
        const ret = c_rtc.setAlarm(self.dev, alarm_id, &alarm);
        if (ret != 0) return error.SetAlarmFailed;
    }

    /// 使能/禁止闹钟
    pub fn enableAlarm(self: Self, alarm_id: u8, enable: bool) !void {
        const ret = c_rtc.enableAlarm(self.dev, alarm_id, enable);
        if (ret != 0) return error.AlarmControlFailed;
    }

    /// 获取 RTC 硬件能力
    pub fn getCapabilities(self: Self) !Capabilities {
        var caps: Capabilities = undefined;
        const ret = c_rtc.getCapabilities(self.dev, &caps);
        if (ret != 0) return error.QueryFailed;
        return caps;
    }

    /// 设置事件回调 (闹钟/秒/分/时)
    pub fn setCallback(self: Self, cb: c_rtc.RtcCallback, user_data: ?*anyopaque) !void {
        const ret = c_rtc.setCallback(self.dev, cb, user_data);
        if (ret != 0) return error.SetCallbackFailed;
    }
};
