<!-- type: worker -->

# Zig 构建与调试助手

覆盖 Zig adapter 的构建系统配置、交叉编译、CMake 集成和常见编译错误诊断。Zig adapter 源码随主仓分发，不再依赖 git 子模块初始化。

---

## 触发条件

- `zig build` 报错
- RISC-V 交叉编译问题
- 链接阶段找不到 C 符号
- CMake 集成配置
- build.zig 修改

---

## 构建系统概述

### Zig 独立构建

```bash
cd labs/zig/adapter

# 默认交叉编译 (RISC-V 64, freestanding)
zig build --summary all

# 生成静态库供 CMake 集成
zig build cmake-lib --summary all

# 构建特定示例
zig build example-helloworld --summary all

# 运行主机测试
zig build test --summary all
```

### CMake 集成

在样例的 `CMakeLists.txt` 中添加：

```cmake
add_subdirectory(${ARCS_BASE}/zig zig_adapter)
target_link_libraries(app PRIVATE arcs-zig)
```

### 目标平台配置

| 参数 | 值 | 说明 |
|------|---|------|
| CPU 架构 | riscv64 | 64 位 RISC-V |
| OS | freestanding | 裸机 (FreeRTOS) |
| ABI | none | 无标准 ABI |
| ISA 扩展 | I + M + A + C | 整数 + 乘法 + 原子 + 压缩 |
| 内存模型 | medany | 中等距离寻址 |

---

## 常见编译错误诊断

### 错误分类

```text
编译错误 → 提取错误信息
├─ "name shadows primitive" → Zig 0.13 内置类型冲突
├─ "function parameter shadows declaration" → 参数名与方法名冲突
├─ "pointless discard" → 无用参数丢弃
├─ "extern struct" / "packed struct" 问题 → 内存布局
├─ "undefined symbol" → 链接时缺少 C 符号
├─ "@ptrCast" / "@alignCast" 失败 → 类型转换错误
├─ "error: expected" → 语法错误
└─ 构建系统错误 → build.zig 配置
```

### 常见错误及解决方案

| 错误模式 | 原因 | 解决方案 |
|----------|------|----------|
| `name shadows primitive 'c_int'` | 声明了 `const c_int = i32` | 删除该声明，直接使用内置 `c_int` |
| `function parameter shadows declaration` | 参数名与同作用域声明冲突 | 重命名参数（如 `name` → `dev_name`） |
| `pointless discard of function parameter` | `_ = param` 但 param 已被使用 | 移除无用的 `_ = ...` |
| `extern struct` 大小不匹配 | 字段类型或顺序与 C 不一致 | Read C 头文件核对字段 |
| `undefined symbol: lisa_xxx` | C 库未链接 | 检查 CMakeLists.txt 或 build.zig 的库链接 |
| `callconv(.C)` 缺失 | FFI 函数缺少调用约定 | 所有 `extern "c" fn` 必须标注 `callconv(.C)` |
| `error: expected type` 在 `@intCast` | Zig 0.13 需要推断目标类型 | 使用 `@as(TargetType, @intCast(value))` |

### Zig 0.13 特定注意事项

1. **内置原语**：`c_int`, `c_uint`, `c_long` 等是内置类型，不可重新声明
2. **`@intCast` 语法**：不再接受类型参数，由赋值目标推断
3. **`callconv`**：所有 C 交互函数必须显式标注
4. **`extern struct`**：保证与 C 兼容的内存布局

---

## 交叉编译调试

### 验证编译产物

```bash
# 检查静态库
file labs/zig/adapter/zig-out/lib/libarcs-zig.a

# 预期输出: current ar archive
# 内含 RISC-V 目标文件
```

### 添加 C 头文件搜索路径

若新增了 SDK 组件支持，需在 `build.zig` 的 `c_include_dirs` 中添加路径：

```zig
const c_include_dirs = [_][]const u8{
    // ... 现有路径
    "drivers/lisa_{new_module}",  // 新增
    "components/lisa_{new_component}",
};
```

---

## 测试运行

```bash
# 主机测试（验证类型映射、常量值、纯逻辑）
cd labs/zig/adapter && zig build test --summary all

# 预期: 所有测试通过
# 注意: 涉及 extern "c" fn 的代码在主机测试时被跳过（链接不到 C 库）
```
