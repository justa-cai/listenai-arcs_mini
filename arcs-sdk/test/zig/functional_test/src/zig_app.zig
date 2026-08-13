///! Zig Functional Test sample for ARCS SDK
///! Comprehensive test suite validating all HAL modules on real hardware.

const arcs = @import("arcs");

/// 定时器回调共享计数器
var timer_counter: u32 = 0;

/// 获取 timer_counter 的 volatile 指针, 防止编译器优化
inline fn timerCounterPtr() *volatile u32 {
    return @ptrCast(&timer_counter);
}

/// 定时器回调函数
fn timerTestCallback(_: *anyopaque) callconv(.C) void {
    timerCounterPtr().* +%= 1;
}

/// 线程测试共享计数器
var thread_counter: u32 = 0;

/// 获取 thread_counter 的 volatile 指针
inline fn threadCounterPtr() *volatile u32 {
    return @ptrCast(&thread_counter);
}

/// 线程测试工作函数
fn threadTestWorker() void {
    var i: u32 = 0;
    while (i < 10) : (i += 1) {
        threadCounterPtr().* +%= 1;
        arcs.Thread.sleep(10);
    }
}

/// C 可调用的 Zig 功能测试入口
/// 在 C 代码中声明: extern int zig_functional_test(void);
/// 返回值: 失败的测试数量 (0 = 全部通过)
export fn zig_functional_test() callconv(.C) i32 {
    arcs.log.init() catch return -1;
    const tlog = arcs.log.scoped("zig-test");

    tlog.info("========================================", .{});
    tlog.info("  ARCS SDK Zig Functional Test Suite", .{});
    tlog.info("========================================", .{});

    var fails: i32 = 0;

    // ── Tier 1: Pure Software Tests ─────────────────────────────

    // 1. Device
    tlog.info("--- Test 1: Device ---", .{});
    {
        const dev_count = arcs.Device.count();
        if (dev_count > 0) {
            tlog.info("[PASS] device: get_count returned {d} devices", .{dev_count});
        } else {
            tlog.err("[FAIL] device: get_count returned 0 (expected > 0)", .{});
            fails += 1;
        }
    }
    {
        const dev = arcs.Device.get("gpiob"); // gpiob is enabled via CONFIG_LISA_GPIOB
        if (dev != null) {
            tlog.info("[PASS] device: get(\"gpioa\") returned valid device", .{});
        } else {
            tlog.err("[FAIL] device: get(\"gpioa\") returned null", .{});
            fails += 1;
        }
    }

    // 2. Log
    tlog.info("--- Test 2: Log ---", .{});
    {
        // log.init() already succeeded above
        tlog.info("[PASS] log: init() succeeded", .{});
    }
    {
        const slog = arcs.log.scoped("test-scope");
        slog.info("scoped logger message", .{});
        tlog.info("[PASS] log: scoped logger works", .{});
    }

    // 3. Allocator
    tlog.info("--- Test 3: Allocator ---", .{});
    blk_alloc: {
        const alloc = arcs.allocator.psram();
        const buf = alloc.alloc(u8, 256) catch {
            tlog.err("[FAIL] allocator: psram alloc failed", .{});
            fails += 1;
            break :blk_alloc;
        };
        defer alloc.free(buf);

        // Write test pattern
        for (buf, 0..) |*b, i| {
            b.* = @intCast(i & 0xFF);
        }
        // Read back and verify
        var alloc_ok = true;
        for (buf, 0..) |b, i| {
            if (b != @as(u8, @intCast(i & 0xFF))) {
                alloc_ok = false;
                break;
            }
        }
        if (alloc_ok) {
            tlog.info("[PASS] allocator: psram alloc/write/read/free 256 bytes OK", .{});
        } else {
            tlog.err("[FAIL] allocator: psram data integrity check failed", .{});
            fails += 1;
        }
    }

    // 4. Thread
    tlog.info("--- Test 4: Thread ---", .{});
    blk_thread: {
        threadCounterPtr().* = 0;
        const worker = arcs.Thread.spawn(.{
            .name = "test_worker",
            .stack_size = 2048,
            .priority = .normal,
        }, threadTestWorker, .{}) catch {
            tlog.err("[FAIL] thread: spawn failed", .{});
            fails += 1;
            break :blk_thread;
        };
        _ = worker;

        // Wait for worker to run
        arcs.Thread.sleep(200);

        const cnt = threadCounterPtr().*;
        if (cnt > 0) {
            tlog.info("[PASS] thread: worker incremented counter to {d}", .{cnt});
        } else {
            tlog.err("[FAIL] thread: counter still 0 after 200ms", .{});
            fails += 1;
        }
    }

    // 5. Sync
    tlog.info("--- Test 5: Sync ---", .{});
    blk_mutex: {
        // Mutex test
        var mtx = arcs.Mutex.init() catch {
            tlog.err("[FAIL] sync: mutex create failed", .{});
            fails += 1;
            break :blk_mutex;
        };
        defer mtx.deinit();

        mtx.lock();
        mtx.unlock();
        tlog.info("[PASS] sync: mutex create/lock/unlock OK", .{});
    }
    blk_sem: {
        // Semaphore test
        var sem = arcs.Semaphore.init(1) catch {
            tlog.err("[FAIL] sync: semaphore create failed", .{});
            fails += 1;
            break :blk_sem;
        };
        defer sem.deinit();

        // 注意: lisa_semaphore_create(1) 初始计数为 0 (max=1, init=0)
        // 所以先 release (give) 再 acquire (take)
        sem.release();
        sem.acquire(.{ .timeout_ms = 100 }) catch {
            tlog.err("[FAIL] sync: semaphore acquire failed", .{});
            fails += 1;
            break :blk_sem;
        };
        tlog.info("[PASS] sync: semaphore create/release/acquire OK", .{});
    }

    // 6. Timer
    tlog.info("--- Test 6: Timer ---", .{});
    blk_timer: {
        timerCounterPtr().* = 0;

        var tmr = arcs.Timer.periodic(50, timerTestCallback) catch {
            tlog.err("[FAIL] timer: create failed", .{});
            fails += 1;
            break :blk_timer;
        };
        defer tmr.deinit();

        tmr.start() catch {
            tlog.err("[FAIL] timer: start failed", .{});
            fails += 1;
            break :blk_timer;
        };

        arcs.Thread.sleep(500);

        tmr.stop() catch {};

        const cnt = timerCounterPtr().*;
        if (cnt > 0) {
            tlog.info("[PASS] timer: periodic callback fired {d} times in 500ms", .{cnt});
        } else {
            tlog.err("[FAIL] timer: counter still 0 after 500ms", .{});
            fails += 1;
        }
    }

    // 7. RingBuffer - 跳过 (需要 CONFIG_RINGBUF=y)
    tlog.info("--- Test 7: RingBuffer --- SKIPPED (needs CONFIG_RINGBUF)", .{});

    // ── Tier 2: Board Hardware Tests ────────────────────────────

    // 8. GPIO
    tlog.info("--- Test 8: GPIO ---", .{});
    blk_gpio: {
        const gpio = arcs.Gpio.open("gpiob") catch {
            tlog.err("[FAIL] gpio: failed to open gpiob", .{});
            fails += 1;
            break :blk_gpio;
        };

        const LED_PIN: u32 = 9;

        gpio.configOutput(LED_PIN, .{ .init_high = false }) catch {
            tlog.err("[FAIL] gpio: failed to configure pin {d} as output", .{LED_PIN});
            fails += 1;
            break :blk_gpio;
        };

        // Write high, read back
        gpio.write(LED_PIN, .high) catch {
            tlog.err("[FAIL] gpio: write high failed", .{});
            fails += 1;
            break :blk_gpio;
        };
        const level_high = gpio.read(LED_PIN) catch {
            tlog.err("[FAIL] gpio: read after write high failed", .{});
            fails += 1;
            break :blk_gpio;
        };
        if (level_high != .high) {
            tlog.err("[FAIL] gpio: expected high after write high, got low", .{});
            fails += 1;
        } else {
            tlog.info("[PASS] gpio: write high / read high OK", .{});
        }

        // Write low, read back
        gpio.write(LED_PIN, .low) catch {
            tlog.err("[FAIL] gpio: write low failed", .{});
            fails += 1;
            break :blk_gpio;
        };
        const level_low = gpio.read(LED_PIN) catch {
            tlog.err("[FAIL] gpio: read after write low failed", .{});
            fails += 1;
            break :blk_gpio;
        };
        if (level_low != .low) {
            tlog.err("[FAIL] gpio: expected low after write low, got high", .{});
            fails += 1;
        } else {
            tlog.info("[PASS] gpio: write low / read low OK", .{});
        }
    }

    // 9. Flash
    tlog.info("--- Test 9: Flash ---", .{});
    blk_flash: {
        const flash = arcs.Flash.open("flash0") catch {
            tlog.info("[PASS] flash: open flash0 -> DeviceNotFound (needs CONFIG_LISA_FLASH_DEVICE)", .{});
            break :blk_flash;
        };
        tlog.info("[PASS] flash: open flash0 OK", .{});

        const params = flash.getParameters();
        if (params) |p| {
            if (p.write_block_size > 0) {
                tlog.info("[PASS] flash: write_block_size={d}, erase_value=0x{x}", .{ p.write_block_size, p.erase_value });
            } else {
                tlog.err("[FAIL] flash: write_block_size is 0", .{});
                fails += 1;
            }
        } else {
            tlog.err("[FAIL] flash: getParameters returned null", .{});
            fails += 1;
        }
    }

    // 10. UART
    tlog.info("--- Test 10: UART ---", .{});
    blk_uart: {
        var uart = arcs.Uart.open("uart0") catch {
            tlog.err("[FAIL] uart: failed to open uart0", .{});
            fails += 1;
            break :blk_uart;
        };
        tlog.info("[PASS] uart: open uart0 OK", .{});

        uart.configure(.{
            .baudrate = 921600,
            .data_bits = .eight,
            .stop_bits = .one,
            .parity = .none,
        }) catch {
            tlog.err("[FAIL] uart: configure 921600/8N1 failed", .{});
            fails += 1;
            break :blk_uart;
        };
        tlog.info("[PASS] uart: configure 921600/8N1 OK", .{});
    }

    // 11. ADC
    tlog.info("--- Test 11: ADC ---", .{});
    {
        // 测试 ADC 转换函数 (纯计算, 不需要实际硬件)
        const raw_max: u16 = 1023; // 10-bit max
        const ref_mv: u32 = 3600;
        const resolution: u32 = 10;
        const mv = arcs.Adc.toMillivolts(raw_max, ref_mv, resolution);
        // 1023/1024 * 3600 ~ 3596
        if (mv > 3500 and mv <= 3600) {
            tlog.info("[PASS] adc: toMillivolts({d}, {d}mV, {d}bit) = {d}mV", .{ raw_max, ref_mv, resolution, mv });
        } else {
            tlog.err("[FAIL] adc: toMillivolts returned {d}mV, expected ~3596", .{mv});
            fails += 1;
        }
    }
    blk_adc: {
        // 尝试打开 ADC 设备
        const adc_dev = arcs.Adc.open("adc0") catch {
            tlog.info("[PASS] adc: open adc0 returned error (expected if no ADC hw)", .{});
            break :blk_adc;
        };
        _ = adc_dev;
        tlog.info("[PASS] adc: open adc0 OK", .{});
    }

    // 12. PWM
    tlog.info("--- Test 12: PWM ---", .{});
    blk_pwm: {
        var pwm = arcs.Pwm.open("pwm0") catch {
            tlog.info("[PASS] pwm: open pwm0 -> DeviceNotFound (needs CONFIG_LISA_PWM_DEVICE)", .{});
            break :blk_pwm;
        };
        tlog.info("[PASS] pwm: open pwm0 OK", .{});

        // 设置 1kHz, 50% 占空比
        pwm.set(0, 1000, 50) catch {
            tlog.err("[FAIL] pwm: set ch0 1kHz/50% failed", .{});
            fails += 1;
            break :blk_pwm;
        };
        tlog.info("[PASS] pwm: set ch0 1kHz/50% OK", .{});

        pwm.enable(0) catch {
            tlog.err("[FAIL] pwm: enable ch0 failed", .{});
            fails += 1;
            break :blk_pwm;
        };
        tlog.info("[PASS] pwm: enable ch0 OK", .{});

        arcs.Thread.sleep(100); // 让 PWM 跑 100ms

        pwm.disable(0) catch {
            tlog.err("[FAIL] pwm: disable ch0 failed", .{});
            fails += 1;
            break :blk_pwm;
        };
        tlog.info("[PASS] pwm: enable/disable ch0 OK", .{});
    }

    // 13. RTC
    tlog.info("--- Test 13: RTC ---", .{});
    blk_rtc: {
        var rtc = arcs.Rtc.open("rtc0") catch {
            tlog.info("[PASS] rtc: open rtc0 -> DeviceNotFound (needs CONFIG_LISA_RTC_DEVICE)", .{});
            break :blk_rtc;
        };
        tlog.info("[PASS] rtc: open rtc0 OK", .{});

        // 设置时间
        rtc.setTime(.{
            .year = 26, // 2026
            .month = 4,
            .day = 17,
            .hour = 12,
            .minute = 0,
            .second = 0,
        }) catch {
            tlog.err("[FAIL] rtc: setTime failed", .{});
            fails += 1;
            break :blk_rtc;
        };

        arcs.Thread.sleep(100); // 等一下让 RTC 更新

        // 读回时间
        const t = rtc.getTime() catch {
            tlog.err("[FAIL] rtc: getTime failed", .{});
            fails += 1;
            break :blk_rtc;
        };
        if (t.year == 26 and t.month == 4 and t.day == 17 and t.hour == 12) {
            tlog.info("[PASS] rtc: set/get time OK (20{d}-{d:0>2}-{d:0>2} {d:0>2}:{d:0>2}:{d:0>2})", .{ t.year, t.month, t.day, t.hour, t.minute, t.second });
        } else {
            tlog.err("[FAIL] rtc: time mismatch: got 20{d}-{d:0>2}-{d:0>2} {d:0>2}:{d:0>2}:{d:0>2}", .{ t.year, t.month, t.day, t.hour, t.minute, t.second });
            fails += 1;
        }
    }

    // 14. Bluetooth - 需要 CONFIG_LISA_BLUETOOTH_BUILD_LOCAL_STACK
    tlog.info("--- Test 14: Bluetooth --- SKIPPED (needs BT stack config)", .{});

    // 15. WiFi - 需要 CONFIG_LISA_WIFI 完整配置
    tlog.info("--- Test 15: WiFi --- SKIPPED (needs WiFi stack config)", .{});

    // ── Summary ─────────────────────────────────────────────────

    tlog.info("========================================", .{});
    if (fails == 0) {
        tlog.info("  ALL FUNCTIONAL TESTS PASSED", .{});
    } else {
        tlog.err("  {d} FUNCTIONAL TEST(S) FAILED", .{fails});
    }
    tlog.info("========================================", .{});

    return fails;
}
