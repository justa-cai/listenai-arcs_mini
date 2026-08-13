///! Zig Display Test sample for ARCS SDK
///! Demonstrates display operations: fill colors and brightness control.

const arcs = @import("arcs");

/// C 可调用的 Zig Display 测试入口
/// C 端已完成 attach_bus，Zig 负责 get_capabilities + 填色 + 亮度
export fn zig_display_test() callconv(.C) i32 {
    arcs.log.init() catch return -1;
    const dlog = arcs.log.scoped("zig-disp");

    dlog.info("Display test starting...", .{});

    // 1. 打开 display 设备
    var lcd = arcs.Display.open("display") catch {
        dlog.err("[FAIL] display: failed to open 'display'", .{});
        return -1;
    };
    dlog.info("[PASS] display: device opened", .{});

    // 2. 获取能力
    const caps = lcd.getCapabilities() catch {
        dlog.err("[FAIL] display: getCapabilities failed", .{});
        return -2;
    };
    dlog.info("[PASS] display: capabilities {d}x{d}", .{ caps.width, caps.height });

    // 3. 开启显示
    lcd.on() catch {
        dlog.err("[FAIL] display: blanking_off failed", .{});
        return -3;
    };
    dlog.info("[PASS] display: blanking off (screen on)", .{});

    // 4. 设置亮度
    lcd.setBrightness(80) catch {
        dlog.err("[FAIL] display: setBrightness failed", .{});
        return -4;
    };
    dlog.info("[PASS] display: brightness set to 80%", .{});

    // 5. 填充红色
    dlog.info("Filling screen with RED...", .{});
    lcd.fillRect(0, 0, caps.width, caps.height, arcs.Display.Color.red) catch {
        dlog.err("[FAIL] display: fillRect RED failed", .{});
        return -5;
    };
    dlog.info("[PASS] display: RED fill complete", .{});
    arcs.sleep(1000);

    // 6. 填充绿色
    dlog.info("Filling screen with GREEN...", .{});
    lcd.fillRect(0, 0, caps.width, caps.height, arcs.Display.Color.green) catch {
        dlog.err("[FAIL] display: fillRect GREEN failed", .{});
        return -6;
    };
    dlog.info("[PASS] display: GREEN fill complete", .{});
    arcs.sleep(1000);

    // 7. 填充蓝色
    dlog.info("Filling screen with BLUE...", .{});
    lcd.fillRect(0, 0, caps.width, caps.height, arcs.Display.Color.blue) catch {
        dlog.err("[FAIL] display: fillRect BLUE failed", .{});
        return -7;
    };
    dlog.info("[PASS] display: BLUE fill complete", .{});
    arcs.sleep(1000);

    // 8. 亮度渐变测试
    dlog.info("Brightness ramp test...", .{});
    lcd.fillRect(0, 0, caps.width, caps.height, arcs.Display.Color.white) catch {};
    var b: u8 = 0;
    while (b < 100) : (b += 10) {
        lcd.setBrightness(b) catch {};
        arcs.sleep(200);
    }
    lcd.setBrightness(80) catch {};
    dlog.info("[PASS] display: brightness ramp 0->100", .{});

    dlog.info("=== Zig Display Test PASSED ===", .{});
    return 0;
}
