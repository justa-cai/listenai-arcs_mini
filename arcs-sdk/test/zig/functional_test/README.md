# Zig Functional Test 示例

## 功能说明

该示例是一个面向真实板级环境的 Zig 功能测试集合，用于验证 ARCS SDK Zig HAL 的核心模块在板端是否可用。测试覆盖设备发现、日志、内存分配、线程、同步原语、定时器，以及 GPIO、Flash、UART、ADC 等能力。

相较于 `binding_test` 只校验 ABI，该示例更侧重运行时功能行为是否符合预期。

## 硬件连接

该示例默认面向 `arcs_evb`：

- **GPIO**: 使用 `gpiob` 的 LED 引脚进行输出测试
- **UART**: 打开并配置 `uart0`
- **Flash**: 尝试打开 `flash0`
- 其他纯软件测试（线程、同步、定时器、内存）无需外接硬件

## 示例内容

1. 初始化 Zig 日志系统并打印测试套件标题
2. 执行纯软件能力测试，包括设备发现、日志、内存分配、线程、同步原语和定时器
3. 执行板级硬件能力测试，包括 GPIO、Flash、UART 以及 ADC 辅助计算
4. 为每项测试打印 PASS/FAIL 结果
5. 汇总失败数量并给出最终测试结论

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

建议将示例路径替换为：

```bash
./build.sh -C -S test/zig/functional_test -DBOARD=arcs_evb
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

运行成功时，会看到大量分项测试日志，例如：

```text
========================================
  ARCS SDK Zig Functional Test Suite
========================================
--- Test 1: Device ---
[PASS] device: get_count returned ... devices
--- Test 3: Allocator ---
[PASS] allocator: psram alloc/write/read/free 256 bytes OK
--- Test 6: Timer ---
[PASS] timer: periodic callback fired ... times in 500ms
--- Test 8: GPIO ---
[PASS] gpio: write high / read high OK
[PASS] gpio: write low / read low OK
--- Test 10: UART ---
[PASS] uart: configure 921600/8N1 OK
```

如果某项依赖未满足或行为异常，会输出对应 `[FAIL]` 日志。

## 核心 API

| API | 说明 |
|-----|------|
| `arcs.Device.count()` | 获取已注册设备数量 |
| `arcs.allocator.psram()` | 获取 PSRAM 分配器 |
| `arcs.Thread.spawn()` | 创建线程 |
| `arcs.Mutex.init()` | 创建互斥锁 |
| `arcs.Semaphore.init()` | 创建信号量 |
| `arcs.Timer.periodic()` | 创建周期定时器 |
| `arcs.Gpio.open()` | 打开 GPIO 设备 |
| `arcs.Uart.open()` | 打开 UART 设备 |

## 关键代码

```zig
const worker = arcs.Thread.spawn(.{
    .name = "test_worker",
    .stack_size = 2048,
    .priority = .normal,
}, threadTestWorker, .{}) catch {
    fails += 1;
    break :blk_thread;
};
```

```zig
var sem = arcs.Semaphore.init(1) catch {
    fails += 1;
    break :blk_sem;
};
sem.release();
try sem.acquire(.{ .timeout_ms = 100 });
```

## 注意事项

- **测试范围**: 该示例混合了纯软件测试与板级外设测试，运行环境不同可能影响部分结果
- **结果依赖**: 某些设备项是否 PASS 与当前 `prj.conf`、板级设备注册和目标板型有关
- **日志文案**: 当前日志中个别提示文本可能沿用旧名字（如 `gpioa` / `gpiob` 文案不一致），以实际代码逻辑为准
- **CI 使用**: 若要将其用于 CI，需要先明确哪些项目在不同板型上应视为 PASS、SKIP 或可接受失败
