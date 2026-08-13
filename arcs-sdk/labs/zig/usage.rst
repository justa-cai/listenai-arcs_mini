Zig 模块本体与关键文件
----------------------

Linux 安装示例
~~~~~~~~~~~~~~

未安装 Zig 时的典型表现
~~~~~~~~~~~~~~~~~~~~~~~

`labs/zig/adapter/CMakeLists.txt` 与 `labs/zig/adapter/zig_target.cmake` 都会通过 `find_program()` 查找 `zig`。
如果开发机未安装 Zig，或者 `PATH` 中找不到 `zig`，配置阶段会直接失败。

Zig 支持的核心文件如下：

- `labs/zig/adapter/CMakeLists.txt`：注册共享 `arcs-zig` 目标并导出头文件路径
- `labs/zig/adapter/build.zig`：定义 Zig 侧库构建入口、目标平台与 include 路径
- `labs/zig/adapter/zig_target.cmake`：为 target 编译单独的 `.zig` 源文件并参与链接
- `labs/zig/adapter/src/root.zig`：定义 `@import("arcs")` 暴露的总入口

架构分层
--------

`labs/zig/adapter/src/root.zig` 将能力分成两层：

1. **bindings 层**

   直接映射底层 C API，适合：

   - 保持与 C 数据结构/ABI 严格一致
   - 做绑定验证、结构体布局检查
   - 在 Zig 中直接访问底层接口

2. **HAL 层**

   提供更符合 Zig 风格的封装，适合：

   - 以对象/方法方式使用设备
   - 统一错误处理
   - 在 sample 中快速编写应用逻辑

`@import("arcs")` 提供的主要能力
--------------------------------

系统与基础设施：

- `allocator`
- `log`
- `Thread`
- `Mutex`
- `Semaphore`
- `Channel`
- `Timer`
- `RingBuffer`

设备与外设：

- `Device`
- `Gpio`
- `Uart`
- `I2c`
- `Spi`
- `Adc`
- `Pwm`
- `Flash`
- `Display`
- `Rtc`
- `Audio`

连接相关：

- `Bluetooth`
- `WiFi`

构建集成方式
------------

典型的 target `CMakeLists.txt` 需要完成两件事：

1. 正常创建 ARCS SDK 可执行目标
2. 通过 `LISTENAI_ZIG_APP` 声明 target 自己的 `.zig` 文件

参考写法：

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.19)
   set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

   find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})
   project(zig_helloworld)

   set(LISTENAI_ZIG_APP ${CMAKE_CURRENT_SOURCE_DIR}/src/zig_app.zig)

   listenai_add_executable(${PROJECT_NAME})
   listenai_maybe_enable_zig_target(${PROJECT_NAME})
   target_sources(${PROJECT_NAME} PRIVATE src/main.c)

`listenai_maybe_enable_zig_target()` 会在检测到 `LISTENAI_ZIG_APP` 后自动完成以下工作：

- 链接 `arcs-zig`
- 编译并链接 target 自己的 `.zig` 文件

如果 target 的 Zig root 还会导入其它本地 Zig 文件，或需要额外注入 Zig module，可以把这些信息直接传给 helper：

.. code-block:: cmake

   set(LISTENAI_ZIG_APP ${CMAKE_CURRENT_SOURCE_DIR}/src/zig_app.zig)
   set(APP_ZIG_IMPORTS
       ${CMAKE_CURRENT_SOURCE_DIR}/src/zig/core/state.zig
       ${CMAKE_CURRENT_SOURCE_DIR}/src/zig/audio/audio.zig
   )

   zig_write_config_module(${CMAKE_CURRENT_BINARY_DIR}/zig_app_config.zig
       BOOL
           feature_enabled=CONFIG_APP_FEATURE_ENABLE
       UINT
           log_level=${APP_LOG_LEVEL}
       STRING
           product_name=CONFIG_APP_PRODUCT_NAME
   )

   listenai_maybe_enable_zig_target(${PROJECT_NAME}
       DEPENDS ${APP_ZIG_IMPORTS}
       MODULES app_config=${CMAKE_CURRENT_BINARY_DIR}/zig_app_config.zig
   )

其中：

- `DEPENDS` 用于声明 `@import()` 依赖文件，避免增量构建复用过期的 Zig object
- `MODULES` 使用 `name=path` 格式，会为 root module 注入 `--dep name` 与 `-Mname=path`；额外 module 也会自动依赖 `arcs`
- `C_INCLUDE_DIRS` 用于为 target 额外注入 `@cImport` 需要的 C 头文件搜索路径
- `zig_write_config_module()` 用于把 Kconfig/CMake 值生成成 `pub const ...` 形式的 Zig config module
- `BOOL` 会把 CMake/Kconfig 真值映射为 Zig `true`/`false`，`INT`/`UINT` 写入整数，`STRING` 写入字符串，`VALUE` 写入调用方提供的 Zig 字面量
- `STRING` 引用未定义的 `CONFIG_*` 符号时会在 CMake 配置阶段报错，避免把拼写错误的配置名写入运行时字符串；需要空字符串时应定义对应 Kconfig 字符串为空值

可复用 module 注册
~~~~~~~~~~~~~~~~~~~

多个 app 或项目共享的 Zig package 不建议通过 `MODULES name=path` 在每个 app 中重复声明。模块所有者可以先注册 package，再由 app/test 通过名称引用：

.. code-block:: cmake

   listenai_zig_register_module(
       NAME ebus
       ROOT ${CMAKE_CURRENT_SOURCE_DIR}/zig/root.zig
       DEPENDS
           ${CMAKE_CURRENT_SOURCE_DIR}/zig/bindings.zig
       C_INCLUDE_DIRS
           ${CMAKE_CURRENT_SOURCE_DIR}/include
       MODULE_DEPS
           another_module
   )

   listenai_maybe_enable_zig_target(${PROJECT_NAME}
       USE_REGISTERED_MODULES ebus
   )

其中：

- `NAME` 是 Zig 侧 `@import("<name>")` 使用的 module 名，不能使用保留名 `root` 或 `arcs`
- `ROOT` 是该 package 的 Zig root 文件
- `DEPENDS` 声明 package 内部被 `@import()` 的 Zig 文件，保证增量构建正确
- `C_INCLUDE_DIRS` 声明 package 内部 `@cImport` 需要的 C 头文件搜索路径
- `MODULE_DEPS` 声明该 package 依赖的其它已注册 Zig package；helper 会递归注入
- `USE_REGISTERED_MODULES` 会把已注册 package 注入当前 target，并把其 `DEPENDS` 与 `C_INCLUDE_DIRS` 一并纳入构建

如果模块的 `register.cmake` 可能在 `find_package(listenai-cmake)` 之前被 include，可以先写入 `ARCS_ZIG_PENDING_MODULE_<name>_*` 全局属性；target 后续使用 `USE_REGISTERED_MODULES` 时会按需完成注册。

`zig_target.cmake` 内部会执行以下工作：

- 调用共享 Zig adapter 的按需初始化逻辑
- 编译 target 中的 `.zig` 文件并挂接到当前目标

更具体地说，helper 内部会继续完成这些底层步骤：
- 查找本机 Zig 编译器
- 将 target 中的 `.zig` 文件编译为目标文件
- 为 root module 注入 `arcs` 依赖
- 按需注入调用方通过 `MODULES` 或 `USE_REGISTERED_MODULES` 声明的额外 Zig module
- 追加 ARCS SDK、注册 package 与 target 额外声明的头文件搜索路径，以满足 `@cImport` 所需的 C 头文件访问

目标平台与工具链约束
--------------------

根据 `labs/zig/adapter/build.zig` 与 `labs/zig/adapter/zig_target.cmake` 的定义，当前 Zig 构建链路面向：

- `CONFIG_FPU=y` 时使用 `riscv32-freestanding-eabihf`
- 未启用 `CONFIG_FPU` 时使用 `riscv32-freestanding-none`
- 与现有 ARCS SoC / RISC-V 工具链兼容的 CPU 特性配置，并跟随 `CONFIG_FPU` 开关启用或禁用 `f`
- 与 SDK 现有 C 代码一致的 include 体系

这意味着 Zig 代码并不是脱离 ARCS SDK 独立构建，而是作为当前 SDK 工程的一部分参与交叉编译和链接。

最小工作流
----------

1. 在 target 中保留一个 C 入口文件，例如 `src/main.c`
2. 在 Zig 侧导出可由 C 调用的函数，例如：

.. code-block:: zig

   const arcs = @import("arcs");

   export fn zig_hello_main() callconv(.C) i32 {
       arcs.log.init() catch return -1;
       arcs.log.info("Hello from Zig on ARCS SDK!", .{});
       return 0;
   }

注意事项
--------

- Zig target 仍依赖 C 入口和现有 ARCS SDK 工程骨架，不是纯 Zig 裸工程
- 工程必须能找到 Zig 编译器，否则 `labs/zig/adapter/CMakeLists.txt` 与 `zig_target.cmake` 会直接报错
- Zig bindings 与 HAL 暂时以当前已封装模块为准，后续能力范围可能继续扩展
- 某些 target 依赖特定板级外设、panel、WiFi 或音频设备，阅读 README 时需先确认硬件条件
