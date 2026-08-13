///! ARCS SDK Ring Buffer FFI 绑定
///! 映射 ring_buffer.h

// ════════════════════════════════════════════════════════════════════
// Ring Buffer 结构体
// ════════════════════════════════════════════════════════════════════

pub const RingBuf = extern struct {
    buffer: [*]u8,
    put_head: i32,
    put_tail: i32,
    put_base: i32,
    get_head: i32,
    get_tail: i32,
    get_base: i32,
    size: u32,
};

// ════════════════════════════════════════════════════════════════════
// extern C 函数声明
// ════════════════════════════════════════════════════════════════════

/// Zig-native implementation of ring_buf_init (static inline in C header)
pub fn ring_buf_init(buf: *RingBuf, size: u32, data: [*]u8) void {
    buf.buffer = data;
    buf.size = size;
    buf.put_head = 0;
    buf.put_tail = 0;
    buf.put_base = 0;
    buf.get_head = 0;
    buf.get_tail = 0;
    buf.get_base = 0;
}

/// Zig-native implementation of ring_buf_reset (static inline in C header)
pub fn ring_buf_reset(buf: *RingBuf) void {
    buf.put_head = 0;
    buf.put_tail = 0;
    buf.put_base = 0;
    buf.get_head = 0;
    buf.get_tail = 0;
    buf.get_base = 0;
}
pub extern fn ring_buf_put(buf: *RingBuf, data: [*]const u8, size: u32) callconv(.C) u32;
pub extern fn ring_buf_get(buf: *RingBuf, data: [*]u8, size: u32) callconv(.C) u32;
pub extern fn ring_buf_peek(buf: *RingBuf, data: [*]u8, size: u32) callconv(.C) u32;
pub extern fn ring_buf_put_claim(buf: *RingBuf, data: *[*]u8, size: u32) callconv(.C) u32;
pub extern fn ring_buf_put_finish(buf: *RingBuf, size: u32) callconv(.C) i32;
pub extern fn ring_buf_get_claim(buf: *RingBuf, data: *[*]u8, size: u32) callconv(.C) u32;
pub extern fn ring_buf_get_finish(buf: *RingBuf, size: u32) callconv(.C) i32;
