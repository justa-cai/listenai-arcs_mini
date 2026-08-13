///! ARCS SDK Audio 驱动 FFI 绑定
///! 映射 lisa_audio.h — 与 C 结构体严格对齐
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// Audio 枚举
// ════════════════════════════════════════════════════════════════════

pub const AudioStatus = enum(u32) {
    idle = 0,
    running = 1,
    paused = 2,
    err = 3,
};

pub const SampleRate = enum(u32) {
    rate_8k = 8000,
    rate_16k = 16000,
    rate_24k = 24000,
    rate_32k = 32000,
    rate_48k = 48000,
    rate_96k = 96000,
};

pub const AudioChannel = enum(u32) {
    left = 0x01,
    right = 0x02,
    stereo = 0x03,
};

pub const AudioBits = enum(u32) {
    bit_16 = 2,
    bit_24 = 3,
    bit_32 = 4,
};

// IOCTL 命令
pub const IOCTL_RECORD_START: u32 = 0x00;
pub const IOCTL_RECORD_STOP: u32 = 0x01;
pub const IOCTL_RECORD_PAUSE: u32 = 0x02;
pub const IOCTL_RECORD_RESUME: u32 = 0x03;
pub const IOCTL_RECORD_SET_GAIN: u32 = 0x05;
pub const IOCTL_RECORD_SET_CHANNEL_GAIN: u32 = 0x09;
pub const IOCTL_PLAY_START: u32 = 0x20;
pub const IOCTL_PLAY_STOP: u32 = 0x21;
pub const IOCTL_PLAY_SET_GAIN: u32 = 0x22;
pub const IOCTL_PLAY_FLUSH: u32 = 0x27;

// ════════════════════════════════════════════════════════════════════
// Audio 结构体 (与 C 严格匹配)
// ════════════════════════════════════════════════════════════════════

/// lisa_audio_format_t: 3 × uint32_t = 12 bytes
pub const AudioFormat = extern struct {
    sample_rate: u32 = 16000,
    channels: AudioChannel = .stereo,
    sample_bits: AudioBits = .bit_16,
};

/// lisa_audio_gain_t: 2 × int8_t = 2 bytes
pub const AudioGain = extern struct {
    analog_gain: i8 = 0,
    digital_gain: i8 = 0,
};

/// lisa_audio_record_channel_gain_t: left/right physical channel gains
pub const RecordChannelGain = extern struct {
    left: AudioGain = .{},
    right: AudioGain = .{},
};

/// lisa_audio_event_t
pub const AudioEvent = extern struct {
    record_buffer: ?*const anyopaque,
    echo_buffer: ?*const anyopaque,
    record_samples: u32,
    echo_samples: u32,
};

/// lisa_audio_record_config_t
pub const RecordConfig = extern struct {
    format: AudioFormat = .{},
    gain: AudioGain = .{},
    enable_hpf: bool = false,
    differential_input: bool = false,
};

/// lisa_audio_play_config_t — 与 C 严格匹配
/// C: format(12) + gain(2) + buffer_count(u8) + buffer_samples(u16)
pub const PlayConfig = extern struct {
    format: AudioFormat = .{},
    gain: AudioGain = .{},
    buffer_count: u8 = 4,
    buffer_samples: u16 = 320,
};

/// lisa_audio_phase_compensation_t
pub const PhaseCompensation = extern struct {
    record_skip_samples: u16 = 0,
    echo_skip_samples: u16 = 0,
};

pub const AudioCallback = *const fn (*const AudioEvent, ?*anyopaque) callconv(.C) void;

// ════════════════════════════════════════════════════════════════════
// Audio API 虚表 — 与 C lisa_audio_api_t 严格匹配
// ════════════════════════════════════════════════════════════════════

pub const AudioApi = extern struct {
    // 统一回调
    register_callback: ?*const fn (*device.Device, AudioCallback, ?*anyopaque) callconv(.C) i32,
    unregister_callback: ?*const fn (*device.Device, AudioCallback) callconv(.C) i32,
    // Record 操作
    record_config: ?*const fn (*device.Device, *const RecordConfig) callconv(.C) i32,
    record_control: ?*const fn (*device.Device, u32, ?*anyopaque) callconv(.C) i32,
    // Play 操作
    play_config: ?*const fn (*device.Device, *const PlayConfig) callconv(.C) i32,
    play_write: ?*const fn (*device.Device, ?*const anyopaque, u32) callconv(.C) i32,
    play_get_buffer: ?*const fn (*device.Device, *?*anyopaque, u32) callconv(.C) i32,
    play_control: ?*const fn (*device.Device, u32, ?*anyopaque) callconv(.C) i32,
    // 通用
    ioctl: ?*const fn (*device.Device, u8, ?*anyopaque) callconv(.C) i32,
};

// ════════════════════════════════════════════════════════════════════
// 内联辅助
// ════════════════════════════════════════════════════════════════════

pub fn registerCallback(dev: *device.Device, cb: AudioCallback, user_data: ?*anyopaque) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.register_callback orelse return -6;
    return func(dev, cb, user_data);
}

pub fn recordConfig(dev: *device.Device, config: *const RecordConfig) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.record_config orelse return -6;
    return func(dev, config);
}

pub fn recordControl(dev: *device.Device, cmd: u32, arg: ?*anyopaque) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.record_control orelse return -6;
    return func(dev, cmd, arg);
}

pub fn playConfig(dev: *device.Device, config: *const PlayConfig) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.play_config orelse return -6;
    return func(dev, config);
}

pub fn playWrite(dev: *device.Device, buffer: ?*const anyopaque, samples: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.play_write orelse return -6;
    return func(dev, buffer, samples);
}

pub fn playControl(dev: *device.Device, cmd: u32, arg: ?*anyopaque) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.play_control orelse return -6;
    return func(dev, cmd, arg);
}

fn getApi(dev: *device.Device) ?*const AudioApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
