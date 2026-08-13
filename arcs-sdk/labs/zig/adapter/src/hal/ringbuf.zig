///! ARCS SDK Zig Ring Buffer 封装
///! 零拷贝环形缓冲区, 适用于生产者-消费者模式
///!
///! 用法:
///!   var storage: [1024]u8 = undefined;
///!   var rb = arcs.RingBuffer.init(&storage);
///!   const written = rb.put("hello");
///!   var buf: [64]u8 = undefined;
///!   const read_len = rb.get(&buf);
const c_rb = @import("../bindings/ringbuf.zig");

pub const RingBuffer = struct {
    raw: c_rb.RingBuf,

    const Self = @This();

    /// 用已有缓冲区初始化环形缓冲
    pub fn init(storage: []u8) Self {
        var self: Self = undefined;
        c_rb.ring_buf_init(&self.raw, @intCast(storage.len), storage.ptr);
        return self;
    }

    /// 重置缓冲区 (清空所有数据)
    pub fn reset(self: *Self) void {
        c_rb.ring_buf_reset(&self.raw);
    }

    /// 写入数据, 返回实际写入字节数
    pub fn put(self: *Self, data: []const u8) u32 {
        return c_rb.ring_buf_put(&self.raw, data.ptr, @intCast(data.len));
    }

    /// 读取数据, 返回实际读取字节数
    pub fn get(self: *Self, buf: []u8) u32 {
        return c_rb.ring_buf_get(&self.raw, buf.ptr, @intCast(buf.len));
    }

    /// 窥视数据 (不消费), 返回实际窥视字节数
    pub fn peek(self: *Self, buf: []u8) u32 {
        return c_rb.ring_buf_peek(&self.raw, buf.ptr, @intCast(buf.len));
    }

    // ── 零拷贝写入 (两阶段提交) ──────────────────────────────────

    /// 零拷贝写入: 获取写入缓冲区指针
    pub fn putClaim(self: *Self, size: u32) ?[]u8 {
        var ptr: [*]u8 = undefined;
        const claimed = c_rb.ring_buf_put_claim(&self.raw, &ptr, size);
        if (claimed == 0) return null;
        return ptr[0..claimed];
    }

    /// 零拷贝写入: 确认已写入字节数
    pub fn putFinish(self: *Self, size: u32) !void {
        const ret = c_rb.ring_buf_put_finish(&self.raw, size);
        if (ret != 0) return error.PutFinishFailed;
    }

    // ── 零拷贝读取 (两阶段提交) ──────────────────────────────────

    /// 零拷贝读取: 获取可读缓冲区指针
    pub fn getClaim(self: *Self, size: u32) ?[]u8 {
        var ptr: [*]u8 = undefined;
        const claimed = c_rb.ring_buf_get_claim(&self.raw, &ptr, size);
        if (claimed == 0) return null;
        return ptr[0..claimed];
    }

    /// 零拷贝读取: 确认已读取字节数
    pub fn getFinish(self: *Self, size: u32) !void {
        const ret = c_rb.ring_buf_get_finish(&self.raw, size);
        if (ret != 0) return error.GetFinishFailed;
    }

    // ── 状态查询 ─────────────────────────────────────────────────

    /// 缓冲区总容量
    pub fn capacity(self: *Self) u32 {
        return self.raw.size;
    }
};
