const std = @import("std");

/// ARCS SDK Zig Adapter Build Configuration
/// Targets: RISC-V 32-bit (Nuclei cores, ARCS SoC, rv32imac)
pub fn build(b: *std.Build) void {
    // ── Target Configuration ──────────────────────────────────────────
    // Default: cross-compile to RISC-V 32 for ARCS SoC
    // CMake passes -Dfpu=true when CONFIG_FPU selects GCC's ilp32f ABI.
    const fpu = b.option(bool, "fpu", "Use single-precision hard-float ABI (ilp32f)") orelse false;
    const abi: std.Target.Abi = if (fpu) .eabihf else .none;
    const enabled_features = if (fpu)
        std.Target.riscv.featureSet(&.{ .i, .m, .a, .c, .f })
    else
        std.Target.riscv.featureSet(&.{ .i, .m, .a, .c });
    const disabled_features = if (fpu)
        std.Target.riscv.featureSet(&.{.d})
    else
        std.Target.riscv.featureSet(&.{ .d, .f });

    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .riscv32,
            .os_tag = .freestanding,
            .abi = abi,
            .cpu_features_add = enabled_features,
            .cpu_features_sub = disabled_features,
        },
    });

    const optimize = b.standardOptimizeOption(.{});

    // ── SDK Path Configuration ────────────────────────────────────────
    const sdk_root = b.option([]const u8, "sdk-root", "Path to arcs-sdk root") orelse "../../..";

    // ── Core Library ──────────────────────────────────────────────────
    const arcs_mod = b.addModule("arcs", .{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Add C include paths for @cImport
    const sdk_root_path: std.Build.LazyPath = if (std.fs.path.isAbsolute(sdk_root))
        .{ .cwd_relative = sdk_root }
    else
        b.path(sdk_root);
    const c_include_dirs = [_][]const u8{
        "labs/zig/adapter/include",
        "modules/freertos/include",
        "system/os/inc",
        "system/log",
        "system/init",
        "system/console",
        "system/heap",
        "system/ringbuf",
        "drivers/lisa_device",
        "drivers/lisa_gpio",
        "drivers/lisa_uart",
        "drivers/lisa_i2c",
        "drivers/lisa_spi",
        "drivers/lisa_adc",
        "drivers/lisa_pwm",
        "drivers/lisa_flash",
        "drivers/lisa_display",
        "drivers/lisa_camera",
        "drivers/lisa_audio",
        "drivers/lisa_i2s",
        "drivers/lisa_rtc",
        "drivers/lisa_sdmmc",
        "drivers/lisa_touch",
        "soc/arcs",
        "soc/common",
        "soc/arcs/hal/chip/arcs/include",
        "modules/wifi_manager/include",
        "modules/mac_manager/src",
        "modules/httpclient/include",
        "modules/httpclient/include/API",
        "modules/http_ssl/include",
        "modules/lvgl8",
        "modules/lvgl8/src",
        "modules/mbedtls/mbedtls/include",
        "modules/mbedtls/configs",
        "modules/mbedtls/port/mem",
        "modules/mbedtls/port/platform",
        "components/app_player",
        "components/lisa_bluetooth",
        "components/lisa_kv",
        "components/lisa_wifi",
        "soc/arcs/hal/chip/arcs/bt_hal",
        "soc/arcs/hal/chip/arcs/btos",
        "soc/arcs/hal/chip/arcs/btos/task/api",
        "soc/arcs/hal/chip/arcs/wcnd/include/bt_inc",
        "soc/arcs/hal/modules/profiles/netcfg_ble/netcfg_bles/api",
        "soc/arcs/hal/chip/arcs/lwip/lwip-2.2.1/src/include",
        "soc/arcs/hal/chip/arcs/lwip/lwip-2.2.1/contrib/ports/rtos/include",
        "soc/arcs/hal/chip/arcs/lwip/port/include",
        "soc/arcs/hal/chip/arcs/rtos/rtos_al",
        "soc/arcs/hal/chip/arcs/include/net",
        "soc/arcs/hal/chip/arcs/include/wifi",
    };
    const generated_include = b.option(
        []const u8,
        "generated-include",
        "Optional CMake generated/include directory for autoconf.h when building adapter bindings inside an SDK target",
    );
    const generated_include_path: ?std.Build.LazyPath = if (generated_include) |dir|
        if (std.fs.path.isAbsolute(dir)) .{ .cwd_relative = dir } else b.path(dir)
    else
        null;

    for (c_include_dirs) |dir| {
        arcs_mod.addIncludePath(sdk_root_path.path(b, dir));
    }
    if (generated_include_path) |path| {
        arcs_mod.addIncludePath(path);
    }

    // ── Static Library (for linking into ARCS SDK CMake build) ────────
    const lib = b.addStaticLibrary(.{
        .name = "arcs-zig",
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });

    for (c_include_dirs) |dir| {
        lib.addIncludePath(sdk_root_path.path(b, dir));
    }
    if (generated_include_path) |path| {
        lib.addIncludePath(path);
    }

    b.installArtifact(lib);

    // ── Host Tests ───────────────────────────────────────────────────
    const host_target = b.resolveTargetQuery(.{});
    const lvgl_test_mod = b.addModule("lvgl-test-bindings", .{
        .root_source_file = b.path("src/bindings/lvgl.zig"),
        .target = host_target,
        .optimize = optimize,
    });
    for (c_include_dirs) |dir| {
        lvgl_test_mod.addIncludePath(sdk_root_path.path(b, dir));
    }
    if (generated_include_path) |path| {
        lvgl_test_mod.addIncludePath(path);
    }

    const lvgl_tests = b.addTest(.{
        .root_source_file = b.path("tests/lvgl_bindings_test.zig"),
        .target = host_target,
        .optimize = optimize,
    });
    lvgl_tests.root_module.addImport("lvgl", lvgl_test_mod);

    const run_lvgl_tests = b.addRunArtifact(lvgl_tests);
    const test_step = b.step("test", "Run Zig adapter host tests");
    test_step.dependOn(&run_lvgl_tests.step);
}
