<!-- type: worker -->

# Zig 绑定与 HAL 封装开发

新增或修改 ARCS SDK 的 Zig 绑定（bindings 层）和惯用封装（HAL 层）。Zig adapter 源码已直接纳入主仓维护，不再依赖 git 子模块同步。

---

## Step 1: 需求确认

向用户确认以下信息：

| 信息 | 说明 |
|------|------|
| **目标模块** | 要绑定哪个 C 驱动/组件？（如 lisa_wdt、lisa_sdmmc） |
| **层级** | 只需 bindings（FFI 映射）？还是同时需要 HAL 封装？ |
| **优先 API** | 有哪些关键 API 需要覆盖？（默认全覆盖） |

---

## Step 2: 读取 C 头文件

**必须先读取** C 端的公开头文件，获取完整的类型和函数签名：

```
Read drivers/lisa_{module}/lisa_{module}.h
```

提取以下信息：
- 枚举定义（`enum`）
- 结构体定义（`typedef struct`），注意字段顺序和类型
- API 虚表（`xxx_api_t` 结构体中的函数指针）
- 内联辅助函数（`static inline` 分发函数）
- 错误码（通常复用 `LISA_DEVICE_ERR_*`）
- 回调函数类型（`typedef void (*xxx_callback_t)(...)`)

如涉及依赖类型，额外读取：
```
Read drivers/lisa_device/lisa_device.h   # Device 基础结构
Read system/os/inc/lisa_typedef.h        # 基础类型
```

---

## Step 3: 创建 Bindings 文件

路径：`labs/zig/adapter/src/bindings/{module}.zig`

### 3.1 文件模板

```zig
///! ARCS SDK {Module} 驱动 FFI 绑定
///! 映射 lisa_{module}.h
const device = @import("device.zig");

// ═══════════════ 枚举 ═══════════════
// 1:1 映射 C enum，使用 enum(u32) 或 enum(c_int)

// ═══════════════ 结构体 ═══════════════
// 使用 extern struct，字段顺序必须与 C 完全一致
// 可选字段使用默认值

// ═══════════════ API 虚表 ═══════════════
// extern struct 映射 C 的 xxx_api_t
// 每个函数指针为 ?*const fn(...) callconv(.C) c_int

// ═══════════════ 内联辅助 ═══════════════
// 模拟 C 中的 static inline 分发函数
// 统一 getApi() 辅助

// 注意：不可声明 const c_int = i32; (Zig 0.13 内置原语)
```

### 3.2 关键规则

| 规则 | 说明 |
|------|------|
| 枚举底层类型 | C `enum` → Zig `enum(u32)` 或 `enum(c_int)` |
| 结构体 | C `struct` → Zig `extern struct`（保证内存布局一致） |
| 函数指针 | `?*const fn(...) callconv(.C) ReturnType` |
| 回调 | `*const fn(...) callconv(.C) void` |
| 可选指针 | C `void *` → Zig `?*anyopaque` |
| 字符串 | C `const char *` → Zig `[*c]const u8` 或 `[*:0]const u8` |
| 布尔 | C `bool` → Zig `bool` |
| 不透明句柄 | C `void *handle` → Zig `?*anyopaque` |
| c_int 使用 | 直接使用内置 `c_int`，**不要** `const c_int = i32;` |

### 3.3 vtable 辅助函数模式

所有驱动统一使用 getApi + 可选函数检查模式：

```zig
pub fn someOperation(dev: *device.Device, arg: u32) c_int {
    const api = getApi(dev) orelse return -1;  // INVALID
    const func = api.some_op orelse return -6;  // NOT_SUPPORT
    return func(dev, arg);
}

fn getApi(dev: *device.Device) ?*const XxxApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
```

---

## Step 4: 创建 HAL 封装文件

路径：`labs/zig/adapter/src/hal/{module}.zig`

### 4.1 设计原则

| 原则 | 说明 |
|------|------|
| 类型安全 | 使用 Zig enum 替代 C 的整数标志位 |
| Error Union | C 错误码 → Zig error set，支持 `try`/`catch` |
| 零成本抽象 | inline 分发函数在编译期展开 |
| 惯用接口 | 实现 `std.io.Writer`、`std.mem.Allocator` 等标准接口 |
| 文档注释 | 每个 pub 函数带 `///!` 用法示例 |

### 4.2 标准结构

```zig
///! ARCS SDK Zig {Module} 驱动封装
///!
///! 用法:
///!   var dev = try arcs.{Module}.open("{device_name}");
///!   ...
const c_{module} = @import("../bindings/{module}.zig");
const c_device = @import("../bindings/device.zig");

pub const {Module} = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    // ... 惯用 API 方法
};

// 错误映射
fn mapError(code: i32) {Module}Error { ... }
```

---

## Step 5: 注册到 root.zig

在 `labs/zig/adapter/src/root.zig` 中添加：

1. **bindings 层**：在 `pub const c = struct { ... }` 内添加 `pub const {module} = @import("bindings/{module}.zig");`
2. **HAL 层**：添加 `pub const {Module} = @import("hal/{module}.zig").{Module};`
3. **test 块**：添加 `_ = @import("bindings/{module}.zig");` 和 `_ = @import("hal/{module}.zig");`

---

## Step 6: 编译验证

```bash
cd labs/zig/adapter && zig build --summary all
```

必须 3/3 steps succeeded。如有编译错误，自动修复直到通过。

---

## Step 7: 添加单元测试

在 `labs/zig/adapter/tests/` 下添加测试文件，验证：
- 枚举值与 C 定义一致
- `extern struct` 的 `@sizeOf` 合理
- 默认值正确
- 配置组合有效

---

## 经验捕获

任务完成后评估是否值得记录（如发现 C struct 有意外的 padding、头文件路径变更等）。
