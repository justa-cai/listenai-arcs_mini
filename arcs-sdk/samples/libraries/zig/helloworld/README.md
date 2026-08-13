# Zig Hello World 示例

## 功能说明

该示例用于验证 ARCS SDK 当前的 Zig 支持已经可以正常工作。工程通过 C 入口调用 Zig 导出的函数，在 Zig 侧完成日志初始化并输出多条日志，用于确认 Zig 代码能够被成功编译、链接并在板端运行。

## 硬件连接

无需额外外设连接。

**串口输出：**
- 使用默认日志串口
- 波特率请以当前板级 syslog 配置为准

## 示例步骤

1. C 入口打印 Zig 测试启动日志
2. 调用 Zig 导出的 `zig_hello_main()`
3. Zig 侧执行 `arcs.log.init()`
4. 通过 `arcs.log.info()` 和 scoped logger 输出日志
5. 返回执行结果给 C 入口并打印 PASS/FAIL

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

建议将示例路径替换为：

```bash
./build.sh -C -S samples/libraries/zig/helloworld -DBOARD=arcs_evb
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

成功运行后，可看到类似输出：

```text
=== Zig Language Support Test ===
Calling zig_hello_main()...
Hello from Zig on ARCS SDK!
Zig language support is working!
zig-hello: Zig version running on RISC-V ARCS SoC
zig-hello: Log system initialized successfully
Zig function returned successfully (ret=0)
=== Zig Test PASSED ===
```

## 核心 API

| API | 说明 |
|-----|------|
| `zig_hello_main()` | Zig 侧导出的 C 可调用入口函数 |
| `arcs.log.init()` | 初始化 Zig 日志系统 |
| `arcs.log.info()` | 输出普通日志 |
| `arcs.log.scoped()` | 创建带标签的 scoped logger |

## 关键代码

### 1. Zig 入口函数

```zig
const arcs = @import("arcs");

export fn zig_hello_main() callconv(.C) i32 {
    arcs.log.init() catch return -1;
    arcs.log.info("Hello from Zig on ARCS SDK!", .{});
    return 0;
}
```

### 2. C 调用 Zig 导出函数

```c
extern int zig_hello_main(void);

int main(int argc, char **argv)
{
    return zig_hello_main();
}
```

## 注意事项

- **模板用途**: 该示例是最小接入示例，适合作为后续 Zig sample 的模板
- **工具链依赖**: 工程依赖本机已安装 Zig 编译器，否则配置阶段会直接失败
- **能力来源**: `@import("arcs")` 的能力由 `labs/zig/adapter/src/root.zig` 统一导出
- **CI 判定**: 真机验证建议以最终串口口令 `=== Zig Test PASSED ===` 作为成功条件，而不是只匹配中间日志
