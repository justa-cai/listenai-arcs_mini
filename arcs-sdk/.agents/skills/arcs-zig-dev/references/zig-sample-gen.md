<!-- type: worker -->

# Zig 样例代码生成

生成符合 ARCS SDK Zig adapter 规范的完整 Zig 示例项目。Zig adapter 源码已直接纳入主仓维护，不再依赖 git 子模块同步。

---

## Step 1: 需求确认

| 信息 | 默认值 | 说明 |
|------|--------|------|
| **功能目标** | — | 演示什么功能（GPIO 闪烁、UART 回显、传感器读取等） |
| **文件名** | — | `labs/zig/adapter/examples/{name}.zig`（snake_case） |
| **涉及模块** | — | 用到哪些 arcs API（Gpio、Uart、Thread 等） |

---

## Step 2: 读取参考

### 2.1 读取现有 Zig 示例

```
Read labs/zig/adapter/examples/helloworld.zig   # 最简骨架
Read labs/zig/adapter/examples/blinky.zig       # GPIO + 中断
Read labs/zig/adapter/examples/uart_echo.zig    # UART + std.io.Writer
```

### 2.2 读取目标模块的 HAL 文件

确认 API 签名和用法：

```
Read labs/zig/adapter/src/hal/{module}.zig      # 惯用 Zig API
```

### 2.3 若涉及 C SDK 交互，参考 C 示例

```
Glob "samples/**/*{module}*/"     # 找 C 端参考
Read samples/{path}/src/main.c     # 了解初始化模式
```

---

## Step 3: 生成示例文件

路径：`labs/zig/adapter/examples/{name}.zig`

### 3.1 标准模板

```zig
///! ARCS SDK Zig 示例: {Title}
///! {一句话功能描述}
///!
///! 演示:
///!   - {功能点 1}
///!   - {功能点 2}
const std = @import("std");
const arcs = @import("arcs");

pub fn main() !void {
    // ── 初始化日志 ──────────────────────────────────────
    try arcs.log.init();
    const log = arcs.log.scoped("{tag}");

    log.info("=== {Title} ===", .{});

    // ── 设备初始化 ──────────────────────────────────────
    // 使用 arcs.{Module}.open("{device}") 获取设备句柄

    // ── 主逻辑 ──────────────────────────────────────────

    // ── 主循环 ──────────────────────────────────────────
    while (true) {
        // ...
        arcs.sleep(1000);
    }
}
```

### 3.2 关键约定

| 约定 | 规则 |
|------|------|
| 导入方式 | `const arcs = @import("arcs");` |
| 日志初始化 | `try arcs.log.init();` 必须在最前面 |
| 带 Tag 日志 | `const log = arcs.log.scoped("tag");` |
| 设备获取 | `try arcs.{Module}.open("{name}")` |
| 错误处理 | 使用 `try` 传播，顶层 `!void` 返回 |
| 延时 | `arcs.sleep(ms)` 或 `arcs.Thread.sleep(ms)` |
| 内存分配 | `const alloc = arcs.allocator.psram();` |
| 线程创建 | `try arcs.Thread.spawn(.{...}, fn, .{args})` |
| 中断回调 | `callconv(.C)` 签名，不使用阻塞 API |

### 3.3 多线程示例模式

```zig
// 工作线程
fn workerTask() void {
    const wlog = arcs.log.scoped("worker");
    while (true) {
        arcs.sleep(1000);
        wlog.info("tick", .{});
    }
}

// 在 main 中创建
_ = try arcs.Thread.spawn(.{
    .name = "worker",
    .stack_size = 4096,
    .priority = .normal,
}, workerTask, .{});
```

---

## Step 4: 注册到 build.zig

在 `labs/zig/adapter/build.zig` 的 `example_names` 数组中添加新示例名：

```zig
const example_names = [_][]const u8{
    "helloworld",
    "blinky",
    "uart_echo",
    "{new_name}",  // 新增
};
```

---

## Step 5: 编译验证

```bash
cd labs/zig/adapter && zig build example-{name} --summary all
```

---

## Step 6: 输出说明

告知用户：
1. 生成的文件路径
2. 构建命令
3. 涉及的硬件引脚（如有 TODO 标注）
4. 依赖的 C SDK 配置（prj.conf 中需启用的 CONFIG）
