///! ARCS SDK RTC 驱动 FFI 绑定
///! 映射 lisa_rtc.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// RTC 枚举和常量
// ════════════════════════════════════════════════════════════════════

pub const RtcEvent = enum(u32) {
    alarm = 1 << 0,
    second = 1 << 1,
    minute = 1 << 2,
    hour = 1 << 3,
};

pub const Weekday = struct {
    pub const sunday: u8 = 0;
    pub const monday: u8 = 1;
    pub const tuesday: u8 = 2;
    pub const wednesday: u8 = 3;
    pub const thursday: u8 = 4;
    pub const friday: u8 = 5;
    pub const saturday: u8 = 6;
};

// ════════════════════════════════════════════════════════════════════
// RTC 结构体
// ════════════════════════════════════════════════════════════════════

pub const RtcTime = extern struct {
    year: u16 = 0, // 0-127, 基准年 2000
    month: u8 = 1, // 1-12
    day: u8 = 1, // 1-31
    weekday: u8 = 0, // 0-6
    hour: u8 = 0, // 0-23
    minute: u8 = 0, // 0-59
    second: u8 = 0, // 0-59
};

pub const RtcAlarm = RtcTime;

pub const RtcCapabilities = extern struct {
    has_alarm: bool,
    alarm_count: u8,
    min_year: u16,
    max_year: u16,
};

pub const RtcCallback = *const fn (u32, ?*anyopaque) callconv(.C) void;

// ════════════════════════════════════════════════════════════════════
// RTC API 虚表
// ════════════════════════════════════════════════════════════════════

pub const RtcApi = extern struct {
    set_time: ?*const fn (*device.Device, *const RtcTime) callconv(.C) i32,
    get_time: ?*const fn (*device.Device, *RtcTime) callconv(.C) i32,
    set_alarm: ?*const fn (*device.Device, u8, *const RtcAlarm) callconv(.C) i32,
    get_alarm: ?*const fn (*device.Device, u8, *RtcAlarm) callconv(.C) i32,
    enable_alarm: ?*const fn (*device.Device, u8, bool) callconv(.C) i32,
    set_periodic_int: ?*const fn (*device.Device, RtcEvent, bool) callconv(.C) i32,
    get_capabilities: ?*const fn (*device.Device, *RtcCapabilities) callconv(.C) i32,
    set_callback: ?*const fn (*device.Device, RtcCallback, ?*anyopaque) callconv(.C) i32,
};

// ════════════════════════════════════════════════════════════════════
// 内联辅助
// ════════════════════════════════════════════════════════════════════

pub fn setTime(dev: *device.Device, time: *const RtcTime) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_time orelse return -6;
    return func(dev, time);
}

pub fn getTime(dev: *device.Device, time: *RtcTime) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.get_time orelse return -6;
    return func(dev, time);
}

pub fn setAlarm(dev: *device.Device, alarm_id: u8, alarm: *const RtcAlarm) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_alarm orelse return -6;
    return func(dev, alarm_id, alarm);
}

pub fn enableAlarm(dev: *device.Device, alarm_id: u8, enable: bool) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.enable_alarm orelse return -6;
    return func(dev, alarm_id, enable);
}

pub fn getCapabilities(dev: *device.Device, caps: *RtcCapabilities) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.get_capabilities orelse return -6;
    return func(dev, caps);
}

pub fn setCallback(dev: *device.Device, cb: RtcCallback, user_data: ?*anyopaque) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_callback orelse return -6;
    return func(dev, cb, user_data);
}

fn getApi(dev: *device.Device) ?*const RtcApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
