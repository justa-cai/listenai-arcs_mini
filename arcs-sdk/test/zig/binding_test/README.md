# Zig Binding Test 示例

## 功能说明

该示例用于验证 Zig bindings 中定义的数据结构布局是否与 ARCS SDK C 侧完全一致。它通过 C 代码获取各结构体的 `sizeof(type)`，再调用 Zig 导出的查询函数获取对应 Zig 类型大小，逐项比对并输出 PASS/FAIL。

该测试适合在新增或调整 Zig bindings 后运行，用于尽早发现 ABI 或结构体布局不一致问题。

## 硬件连接

无需额外外设连接。

该示例主要进行结构体大小校验，属于运行时绑定一致性验证。

## 示例内容

1. Zig 侧初始化测试日志
2. C 侧枚举设备、GPIO、UART、I2C、SPI、ADC、PWM、Flash、Display、Audio、RTC 等结构体
3. 对每项执行 `sizeof(C)` 与 `@sizeOf(Zig)` 比较
4. 打印每项校验结果
5. 汇总输出全部通过或失败数量

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

建议将示例路径替换为：

```bash
./build.sh -C -S test/zig/binding_test -DBOARD=arcs_evb
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

正常情况下，日志中会看到连续的 PASS：

```text
=== Zig Binding Struct Verification ===
[PASS] Device: c=... zig=...
[PASS] DeviceStats: c=... zig=...
[PASS] GpioApi: c=... zig=...
...
=== ALL 24 BINDING TESTS PASSED ===
```

如果某个 Zig binding 与 C 结构不一致，会看到类似：

```text
[FAIL] DisplayApi: c=128 zig=120
=== 1/24 BINDING TESTS FAILED ===
```

## 核心 API

| API | 说明 |
|-----|------|
| `zig_get_sizeof()` | 根据测试 ID 返回 Zig 类型大小 |
| `zig_check_size()` | 记录单项比较结果 |
| `zig_binding_test_start()` | 初始化测试日志 |
| `zig_binding_test_end()` | 汇总输出测试结果 |

## 关键代码

```c
#define CHECK(id, name, type) \
    fails += zig_check_size(name, (uint32_t)sizeof(type), zig_get_sizeof(id))

CHECK(0,  "Device",      lisa_device_t);
CHECK(2,  "GpioApi",     lisa_gpio_api_t);
CHECK(21, "DisplayApi",  lisa_display_api_t);
```

```zig
export fn zig_get_sizeof(id: u32) callconv(.C) u32 {
    return switch (id) {
        0 => @sizeOf(c_device.Device),
        2 => @sizeOf(c_gpio.GpioApi),
        21 => @sizeOf(c_display.DisplayApi),
        else => 0,
    };
}
```

## 注意事项

- **验证范围**: 该示例验证的是结构体大小一致性，不等价于完整功能正确性验证
- **维护要求**: 若新增 bindings 类型，记得同步扩展 C 侧 `CHECK()` 列表和 Zig 侧 `zig_get_sizeof()` 分支
- **排查方向**: 出现 FAIL 时，应优先检查 Zig `extern struct`、字段顺序、对齐方式以及条件编译差异
