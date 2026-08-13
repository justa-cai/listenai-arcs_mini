///! ARCS SDK Zig 线程封装
///! 将 FreeRTOS 线程模型包装为惯用 Zig 接口
///!
///! 用法:
///!   const thread = try arcs.Thread.spawn(.{
///!       .name = "worker",
///!       .stack_size = 4096,
///!       .priority = .normal,
///!   }, myWorkerFn, .{arg1, arg2});
///!
///!   arcs.Thread.sleep(100); // 毫秒
///!   arcs.Thread.yield();
const std = @import("std");
const c = @import("../bindings/thread.zig");
const c_types = @import("../bindings/c_types.zig");

// ════════════════════════════════════════════════════════════════════
// 线程配置
// ════════════════════════════════════════════════════════════════════
pub const SpawnConfig = struct {
    name: [:0]const u8 = "zig_thread",
    stack_size: u32 = 4096,
    priority: c_types.Priority = .normal,
};

// ════════════════════════════════════════════════════════════════════
// 线程句柄
// ════════════════════════════════════════════════════════════════════
pub const Thread = struct {
    handle: c.ThreadHandle,

    const Self = @This();

    /// 创建并启动线程
    /// entry_fn: 线程入口函数 (Zig 闭包或函数指针)
    ///
    /// 参数通过堆分配传递给新线程。线程入口包装器负责在执行完用户函数后
    /// 自动释放参数内存, 确保无内存泄漏。
    pub fn spawn(config: SpawnConfig, comptime entry_fn: anytype, args: anytype) !Self {
        const Args = @TypeOf(args);
        const mem = @import("../bindings/mem.zig");

        const Wrapper = struct {
            fn entrypoint(ctx: ?*anyopaque) callconv(.C) void {
                if (ctx) |ptr| {
                    const args_ptr: *Args = @ptrCast(@alignCast(ptr));
                    // 拷贝到栈上后立即释放堆内存
                    const local_args = args_ptr.*;
                    mem.lisa_mem_free(ptr);
                    @call(.auto, entry_fn, local_args);
                } else {
                    @call(.auto, entry_fn, .{});
                }
            }
        };

        // 在堆上存储参数 (线程启动后参数必须仍然有效)
        // 0 大小的参数类型无需堆分配
        var args_on_heap: ?*anyopaque = null;
        if (@sizeOf(Args) > 0) {
            args_on_heap = mem.lisa_mem_alloc(@sizeOf(Args));
            if (args_on_heap == null) return error.OutOfMemory;
            const typed_ptr: *Args = @ptrCast(@alignCast(args_on_heap.?));
            typed_ptr.* = args;
        }

        const attr = c.ThreadAttr{
            .name = config.name.ptr,
            .stack_size = config.stack_size,
            .priority = @intFromEnum(config.priority),
        };

        const handle = c.lisa_thread_create(&attr, Wrapper.entrypoint, args_on_heap);
        if (handle == null) {
            if (args_on_heap) |ptr| mem.lisa_mem_free(ptr);
            return error.ThreadCreateFailed;
        }

        return Self{ .handle = handle };
    }

    /// 终止线程
    pub fn join(self: *Self) void {
        _ = c.lisa_thread_delete(self.handle);
    }

    /// 设置线程优先级
    pub fn setPriority(self: *Self, priority: c_types.Priority) !void {
        const ret = c.lisa_thread_set_priority(self.handle, @intFromEnum(priority));
        if (ret != 0) return error.SetPriorityFailed;
    }

    // ── 静态方法 ──────────────────────────────────────────────────

    /// 休眠指定毫秒
    pub fn sleep(ms: u32) void {
        _ = c.lisa_thread_mdelay(ms);
    }

    /// 延迟指定 tick
    pub fn delay(ticks: u32) void {
        _ = c.lisa_thread_delay(ticks);
    }

    /// 让出 CPU 时间片
    pub fn yield() void {
        _ = c.lisa_thread_yield();
    }

    /// 获取当前线程名称
    pub fn currentName() ?[]const u8 {
        const name_ptr = c.lisa_thread_cur_thread_name();
        if (name_ptr == null) return null;
        return std.mem.span(@as([*:0]const u8, @ptrCast(name_ptr)));
    }
};
