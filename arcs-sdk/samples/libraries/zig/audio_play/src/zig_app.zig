///! Zig Audio Play sample for ARCS SDK
///! Demonstrates audio playback with generated sine wave tones.

const std = @import("std");
const arcs = @import("arcs");

/// C 可调用的 Zig Audio Play 入口
/// 在 C 代码中声明: extern int zig_audio_play_main(void);
export fn zig_audio_play_main() callconv(.C) i32 {
    arcs.log.init() catch return -1;
    const alog = arcs.log.scoped("zig-audio");

    alog.info("Audio play demo starting...", .{});

    // ── 打开音频设备 ──────────────────────────────────
    var audio = arcs.Audio.open("audio0") catch {
        alog.err("Failed to open audio0 device", .{});
        return -2;
    };
    alog.info("audio0 device opened", .{});

    // ── 配置播放参数 ──────────────────────────────────
    // 16kHz, 16-bit, Mono (与 C sample 一致)
    const play_cfg = arcs.Audio.PlayConfig{
        .format = .{
            .sample_rate = 16000,
            .channels = .left, // mono
            .sample_bits = .bit_16,
        },
        .gain = .{
            .analog_gain = 0,
            .digital_gain = -12,
        },
        .buffer_count = 12,
        .buffer_samples = 256,
    };

    audio.configPlay(play_cfg) catch {
        alog.err("Failed to configure play", .{});
        return -3;
    };
    alog.info("Play configured: 16kHz/16bit/Mono", .{});

    // ── 生成音频数据 ──────────────────────────────────
    // 编译时生成 1kHz 正弦波 + 500Hz 正弦波 + 静音
    // 16kHz 采样率, 2秒总时长
    const SAMPLE_RATE = 16000;
    const DURATION_SEC = 2;
    const TOTAL_SAMPLES = SAMPLE_RATE * DURATION_SEC;

    // 1kHz 正弦波查找表 (16 个采样点 = 16kHz / 1kHz)
    // sin(2pi * n/16) * 16000, n = 0..15
    const sine_1k = [16]i16{
        0,      6180,   11585,  15137,
        16000,  15137,  11585,  6180,
        0,      -6180,  -11585, -15137,
        -16000, -15137, -11585, -6180,
    };

    // 500Hz 正弦波查找表 (32 个采样点 = 16kHz / 500Hz)
    const sine_500 = [32]i16{
        0,      3121,   6180,   9102,
        11585,  13623,  15137,  15956,
        16000,  15956,  15137,  13623,
        11585,  9102,   6180,   3121,
        0,      -3121,  -6180,  -9102,
        -11585, -13623, -15137, -15956,
        -16000, -15956, -15137, -13623,
        -11585, -9102,  -6180,  -3121,
    };

    // 在栈上按段生成并播放（避免大数组）
    const CHUNK_SAMPLES = 256;
    var pcm_buf: [CHUNK_SAMPLES]i16 = undefined;

    // ── 启动播放 ──────────────────────────────────────
    audio.startPlay() catch {
        alog.err("Failed to start play", .{});
        return -4;
    };
    alog.info("Play started — generating tones...", .{});

    // 段 1: 1kHz 正弦波 (0.8秒, "嘟"音)
    const seg1_samples = SAMPLE_RATE * 8 / 10; // 0.8s = 12800 samples
    alog.info("Segment 1: 1kHz tone (0.8s, {d} samples)", .{seg1_samples});
    var written: u32 = 0;
    while (written < seg1_samples) {
        const remain = seg1_samples - written;
        const chunk: u32 = if (remain < CHUNK_SAMPLES) remain else CHUNK_SAMPLES;
        var i: u32 = 0;
        while (i < chunk) : (i += 1) {
            pcm_buf[i] = sine_1k[(written + i) % 16];
        }
        const byte_ptr: [*]const u8 = @ptrCast(&pcm_buf);
        const byte_slice = byte_ptr[0 .. chunk * 2];
        audio.writePlayData(byte_slice, chunk) catch |err| {
            alog.err("Segment 1 write failed at sample {d}: {}", .{ written, err });
            return -5;
        };
        written += chunk;
    }

    // 段 2: 静音间隔 (0.2秒)
    const seg2_samples = SAMPLE_RATE * 2 / 10; // 0.2s = 3200 samples
    alog.info("Segment 2: silence (0.2s)", .{});
    @memset(std.mem.asBytes(&pcm_buf), 0);
    written = 0;
    while (written < seg2_samples) {
        const remain = seg2_samples - written;
        const chunk: u32 = if (remain < CHUNK_SAMPLES) remain else CHUNK_SAMPLES;
        const byte_ptr: [*]const u8 = @ptrCast(&pcm_buf);
        audio.writePlayData(byte_ptr[0 .. chunk * 2], chunk) catch |err| {
            alog.err("Segment 2 write failed at sample {d}: {}", .{ written, err });
            return -6;
        };
        written += chunk;
    }

    // 段 3: 500Hz 正弦波 (0.8秒, 低音)
    const seg3_samples = SAMPLE_RATE * 8 / 10;
    alog.info("Segment 3: 500Hz tone (0.8s, {d} samples)", .{seg3_samples});
    written = 0;
    while (written < seg3_samples) {
        const remain = seg3_samples - written;
        const chunk: u32 = if (remain < CHUNK_SAMPLES) remain else CHUNK_SAMPLES;
        var i: u32 = 0;
        while (i < chunk) : (i += 1) {
            pcm_buf[i] = sine_500[(written + i) % 32];
        }
        const byte_ptr: [*]const u8 = @ptrCast(&pcm_buf);
        audio.writePlayData(byte_ptr[0 .. chunk * 2], chunk) catch |err| {
            alog.err("Segment 3 write failed at sample {d}: {}", .{ written, err });
            return -7;
        };
        written += chunk;
    }

    // 段 4: 静音 + 结束
    const seg4_samples = SAMPLE_RATE * 2 / 10;
    alog.info("Segment 4: silence (0.2s)", .{});
    @memset(std.mem.asBytes(&pcm_buf), 0);
    written = 0;
    while (written < seg4_samples) {
        const remain = seg4_samples - written;
        const chunk: u32 = if (remain < CHUNK_SAMPLES) remain else CHUNK_SAMPLES;
        const byte_ptr: [*]const u8 = @ptrCast(&pcm_buf);
        audio.writePlayData(byte_ptr[0 .. chunk * 2], chunk) catch |err| {
            alog.err("Segment 4 write failed at sample {d}: {}", .{ written, err });
            return -8;
        };
        written += chunk;
    }

    _ = TOTAL_SAMPLES; // suppress unused

    // ── 等待播放完成并停止 ────────────────────────────
    alog.info("Flushing audio...", .{});
    audio.flushPlay() catch |err| {
        alog.err("flushPlay failed: {}", .{err});
        return -9;
    };

    alog.info("Stopping play...", .{});
    audio.stopPlay() catch |err| {
        alog.err("stopPlay failed: {}", .{err});
        return -10;
    };

    alog.info("Audio play demo completed! Total duration: {d}s", .{DURATION_SEC});
    return 0;
}
