///! Zig Blinky sample for ARCS SDK
///! Demonstrates GPIO output and LED toggling.

const arcs = @import("arcs");

/// C 可调用的 Zig Blinky 入口
/// 在 C 代码中声明: extern int zig_blinky_main(void);
export fn zig_blinky_main() callconv(.C) i32 {
    arcs.log.init() catch return -1;
    const blog = arcs.log.scoped("zig-blinky");

    blog.info("Blinky starting — opening gpiob...", .{});

    var gpio = arcs.Gpio.open("gpiob") catch {
        blog.err("Failed to open gpiob", .{});
        return -2;
    };
    blog.info("gpiob opened successfully", .{});

    const LED_PIN: u32 = 9; // arcs_evb: PAD_B[9] = LED

    gpio.configOutput(LED_PIN, .{ .init_high = false }) catch {
        blog.err("Failed to configure LED pin", .{});
        return -3;
    };
    blog.info("LED pin {d} configured as output", .{LED_PIN});

    var count: u32 = 0;
    while (count < 20) : (count += 1) {
        gpio.toggle(LED_PIN) catch |err| {
            blog.err("Failed to toggle LED pin on iteration {d}: {}", .{ count, err });
            return -4;
        };
        if (count % 2 == 0) {
            blog.info("LED ON  (toggle #{d})", .{count});
        } else {
            blog.info("LED OFF (toggle #{d})", .{count});
        }
        arcs.sleep(500); // 500ms
    }

    blog.info("Blinky done — {d} toggles completed", .{count});
    return 0;
}
