///! Zig Hello World sample for ARCS SDK
///! Demonstrates basic Zig-to-C interop and logging.

const arcs = @import("arcs");

/// C 可调用的 Zig Hello World 入口
/// 在 C 代码中声明: extern int zig_hello_main(void);
export fn zig_hello_main() callconv(.C) i32 {
    arcs.log.init() catch return -1;
    arcs.log.info("Hello from Zig on ARCS SDK!", .{});
    arcs.log.info("Zig language support is working!", .{});

    const mylog = arcs.log.scoped("zig-hello");
    mylog.info("Zig version running on RISC-V ARCS SoC", .{});
    mylog.info("Log system initialized successfully", .{});

    return 0;
}
