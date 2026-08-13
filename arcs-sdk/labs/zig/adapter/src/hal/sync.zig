///! ARCS SDK Zig 同步原语封装
///! 提供 Mutex、Semaphore、Channel (基于 Queue) 的惯用 Zig 接口
///!
///! 用法:
///!   var mtx = try arcs.Mutex.init();
///!   defer mtx.deinit();
///!   mtx.lock();
///!   defer mtx.unlock();
///!
///!   var sem = try arcs.Semaphore.init(1);
///!   try sem.acquire(.{ .timeout_ms = 1000 });
///!   sem.release();
///!
///!   var ch = try arcs.Channel(u32).init(16);
///!   try ch.send(42);
///!   const val = try ch.recv();
const std = @import("std");
const c_sync = @import("../bindings/sync.zig");
const c_types = @import("../bindings/c_types.zig");

// ════════════════════════════════════════════════════════════════════
// Mutex
// ════════════════════════════════════════════════════════════════════
pub const Mutex = struct {
    handle: c_sync.MutexHandle,

    const Self = @This();

    pub fn init() !Self {
        const handle = c_sync.lisa_mutex_create();
        if (handle == null) return error.MutexCreateFailed;
        return Self{ .handle = handle };
    }

    pub fn deinit(self: *Self) void {
        _ = c_sync.lisa_mutex_delete(self.handle);
    }

    /// 加锁 (永久等待)
    pub fn lock(self: *Self) void {
        _ = c_sync.lisa_mutex_lock(self.handle, @bitCast(c_types.WAIT_FOREVER));
    }

    /// 尝试加锁 (非阻塞)
    pub fn tryLock(self: *Self) bool {
        return c_sync.lisa_mutex_lock(self.handle, @bitCast(c_types.NO_WAIT)) == 0;
    }

    /// 带超时加锁
    pub fn lockTimeout(self: *Self, timeout_ms: u32) !void {
        const ret = c_sync.lisa_mutex_lock(self.handle, @intCast(timeout_ms));
        if (ret != 0) return error.MutexTimeout;
    }

    /// 解锁
    pub fn unlock(self: *Self) void {
        _ = c_sync.lisa_mutex_unlock(self.handle);
    }

    /// RAII 式锁守卫
    pub fn acquire(self: *Self) Held {
        self.lock();
        return .{ .mutex = self };
    }

    pub const Held = struct {
        mutex: *Mutex,

        pub fn release(self: Held) void {
            self.mutex.unlock();
        }
    };
};

// ════════════════════════════════════════════════════════════════════
// Semaphore
// ════════════════════════════════════════════════════════════════════
pub const Semaphore = struct {
    handle: c_sync.SemaphoreHandle,

    const Self = @This();

    pub fn init(initial_count: u32) !Self {
        const handle = c_sync.lisa_semaphore_create(initial_count);
        if (handle == null) return error.SemaphoreCreateFailed;
        return Self{ .handle = handle };
    }

    pub fn deinit(self: *Self) void {
        _ = c_sync.lisa_semaphore_delete(self.handle);
    }

    pub const AcquireOptions = struct {
        timeout_ms: ?u32 = null, // null = 永久等待
    };

    /// 获取信号量
    pub fn acquire(self: *Self, opts: AcquireOptions) !void {
        const block_time: i32 = if (opts.timeout_ms) |ms|
            @intCast(ms)
        else
            @bitCast(c_types.WAIT_FOREVER);

        const ret = c_sync.lisa_semaphore_take(self.handle, block_time);
        if (ret != 0) return error.SemaphoreTimeout;
    }

    /// 尝试获取 (非阻塞)
    pub fn tryAcquire(self: *Self) bool {
        return c_sync.lisa_semaphore_take(self.handle, @bitCast(c_types.NO_WAIT)) == 0;
    }

    /// 释放信号量
    pub fn release(self: *Self) void {
        _ = c_sync.lisa_semaphore_give(self.handle);
    }

    /// 清空信号量计数
    pub fn reset(self: *Self) void {
        _ = c_sync.lisa_semaphore_clear(self.handle);
    }
};

// ════════════════════════════════════════════════════════════════════
// Channel (类型安全的消息队列)
// ════════════════════════════════════════════════════════════════════
pub fn Channel(comptime T: type) type {
    return struct {
        handle: c_sync.QueueHandle,
        count: u32,
        item_size: u32,

        const Self = @This();

        pub fn init(cap: u32) !Self {
            const handle = c_sync.lisa_queue_create(cap, "zig_ch", @sizeOf(T));
            if (handle == null) return error.QueueCreateFailed;
            return Self{ .handle = handle, .count = cap, .item_size = @sizeOf(T) };
        }

        pub fn deinit(self: *Self) void {
            _ = c_sync.lisa_queue_delete(self.handle);
        }

        /// 发送消息 (永久等待)
        pub fn send(self: *Self, item: T) !void {
            var val = item;
            const ret = c_sync.lisa_queue_push(self.handle, @ptrCast(&val), @sizeOf(T), c_types.WAIT_FOREVER);
            if (ret != 0) return error.QueueSendFailed;
        }

        /// 尝试发送 (非阻塞)
        pub fn trySend(self: *Self, item: T) bool {
            var val = item;
            return c_sync.lisa_queue_push(self.handle, @ptrCast(&val), @sizeOf(T), c_types.NO_WAIT) == 0;
        }

        /// 发送到队列前端 (高优先级)
        pub fn sendUrgent(self: *Self, item: T) !void {
            var val = item;
            const ret = c_sync.lisa_queue_push_front(self.handle, @ptrCast(&val), @sizeOf(T), c_types.WAIT_FOREVER);
            if (ret != 0) return error.QueueSendFailed;
        }

        /// 接收消息 (永久等待)
        pub fn recv(self: *Self) !T {
            var val: T = undefined;
            const ret = c_sync.lisa_queue_pop(self.handle, @ptrCast(&val), @sizeOf(T), c_types.WAIT_FOREVER);
            if (ret != 0) return error.QueueRecvFailed;
            return val;
        }

        /// 尝试接收 (非阻塞)
        pub fn tryRecv(self: *Self) ?T {
            var val: T = undefined;
            if (c_sync.lisa_queue_pop(self.handle, @ptrCast(&val), @sizeOf(T), c_types.NO_WAIT) == 0) {
                return val;
            }
            return null;
        }

        /// 带超时接收
        pub fn recvTimeout(self: *Self, timeout_ms: u32) !T {
            var val: T = undefined;
            const ret = c_sync.lisa_queue_pop(self.handle, @ptrCast(&val), @sizeOf(T), timeout_ms);
            if (ret != 0) return error.QueueTimeout;
            return val;
        }

        /// 队列是否已满
        pub fn isFull(self: *Self) bool {
            return c_sync.lisa_queue_full(self.handle);
        }

        /// 等待中的消息数量
        pub fn pending(self: *Self) u32 {
            return c_sync.lisa_queue_waiting(self.handle);
        }

        /// 队列容量
        pub fn capacity(self: *Self) u32 {
            return c_sync.lisa_queue_size(self.handle);
        }

        /// 清空队列
        pub fn clear(self: *Self) void {
            _ = c_sync.lisa_queue_clear(self.handle);
        }
    };
}
