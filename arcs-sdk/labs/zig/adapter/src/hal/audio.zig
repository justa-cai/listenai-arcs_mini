///! ARCS SDK Zig Audio 驱动封装
///!
///! 用法:
///!   var audio = try arcs.Audio.open("audio0");
///!
///!   // 播放
///!   try audio.configPlay(.{ .format = .{ .sample_rate = 16000, .channels = .left } });
///!   try audio.startPlay();
///!   try audio.writePlayData(pcm_buffer, samples);
///!   try audio.flushPlay();
///!   try audio.stopPlay();
const c_audio = @import("../bindings/audio.zig");
const c_device = @import("../bindings/device.zig");

pub const Audio = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub const Format = c_audio.AudioFormat;
    pub const Gain = c_audio.AudioGain;
    pub const RecordChannelGain = c_audio.RecordChannelGain;
    pub const RecordConfig = c_audio.RecordConfig;
    pub const PlayConfig = c_audio.PlayConfig;
    pub const Callback = c_audio.AudioCallback;
    pub const SampleRate = c_audio.SampleRate;
    pub const Channel = c_audio.AudioChannel;
    pub const Bits = c_audio.AudioBits;

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    // ── 回调 ─────────────────────────────────────────────────────

    pub fn registerCallback(self: Self, cb: Callback, user_data: ?*anyopaque) !void {
        const ret = c_audio.registerCallback(self.dev, cb, user_data);
        if (ret != 0) return error.RegisterCallbackFailed;
    }

    // ── 录音 ─────────────────────────────────────────────────────

    pub fn configRecord(self: Self, config: RecordConfig) !void {
        const ret = c_audio.recordConfig(self.dev, &config);
        if (ret != 0) return error.ConfigFailed;
    }

    pub fn startRecord(self: Self) !void {
        const ret = c_audio.recordControl(self.dev, c_audio.IOCTL_RECORD_START, null);
        if (ret != 0) return error.StartFailed;
    }

    pub fn stopRecord(self: Self) !void {
        const ret = c_audio.recordControl(self.dev, c_audio.IOCTL_RECORD_STOP, null);
        if (ret != 0) return error.StopFailed;
    }

    pub fn setRecordChannelGain(self: Self, gain: RecordChannelGain) !void {
        var mutable_gain = gain;
        const ret = c_audio.recordControl(
            self.dev,
            c_audio.IOCTL_RECORD_SET_CHANNEL_GAIN,
            @ptrCast(&mutable_gain),
        );
        if (ret != 0) return error.SetGainFailed;
    }

    // ── 播放 ─────────────────────────────────────────────────────

    pub fn configPlay(self: Self, config: PlayConfig) !void {
        const ret = c_audio.playConfig(self.dev, &config);
        if (ret != 0) return error.ConfigFailed;
    }

    pub fn startPlay(self: Self) !void {
        const ret = c_audio.playControl(self.dev, c_audio.IOCTL_PLAY_START, null);
        if (ret != 0) return error.StartFailed;
    }

    pub fn stopPlay(self: Self) !void {
        const ret = c_audio.playControl(self.dev, c_audio.IOCTL_PLAY_STOP, null);
        if (ret != 0) return error.StopFailed;
    }

    pub fn flushPlay(self: Self) !void {
        const ret = c_audio.playControl(self.dev, c_audio.IOCTL_PLAY_FLUSH, null);
        if (ret != 0) return error.FlushFailed;
    }

    /// 写入 PCM 数据到播放缓冲区
    pub fn writePlayData(self: Self, buffer: []const u8, samples: u32) !void {
        const ret = c_audio.playWrite(self.dev, buffer.ptr, samples);
        if (ret < 0) return error.WriteFailed;
    }
};
