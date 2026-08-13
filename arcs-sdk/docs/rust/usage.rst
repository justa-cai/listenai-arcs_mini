Rust 模块本体与关键文件
----------------------

Rust 支持的核心文件如下：

- ``modules/rust/CMakeLists.txt``：负责发现 ``cargo``、缓存 ``ARCS_RUST_DIR``、声明 ``arcs-rust-glue`` C OBJECT target 的源码与编译配置
- ``modules/rust/rust_app.cmake``：通用 Rust staticlib 构建/链接 helper，可被 SDK sample 与 SDK 外部 app 复用
- ``modules/rust/rust_sample.cmake``：SDK sample 的薄封装，提供 ``listenai_maybe_enable_rust_target()`` 默认参数
- ``modules/rust/rust-toolchain.toml``：锁定 Rust 1.83.0 与 ``riscv32imac-unknown-none-elf`` 目标
- ``modules/rust/Cargo.toml``：SDK Rust 工作区根，仅包含 ``arcs`` / ``arcs-macros`` / ``arcs-embassy`` 等 SDK Rust crate
- ``modules/rust/arcs/Cargo.toml``：``arcs`` crate 包定义（``crate-type = ["rlib"]``）
- ``modules/rust/arcs/src/lib.rs``：``arcs`` crate 入口，re-export 公共能力
- ``modules/rust/arcs/glue/``：C 侧 shim，桥接日志、device API 等无法直接用 Rust FFI 表达的接口
- ``modules/rust/README.md``：开发者快速上手与架构说明

Linux 安装示例
~~~~~~~~~~~~~~

推荐使用官方 rustup 安装：

.. code-block:: shell

   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   source "$HOME/.cargo/env"

无需手动执行 ``rustup target add`` —— ``modules/rust/rust-toolchain.toml``
已经把 Rust 1.83.0 与 ``riscv32imac-unknown-none-elf`` 目标锁定，进入
``modules/rust`` 目录后首次调用 ``cargo`` 时 rustup 会自动安装所需 toolchain
与 target。

macOS（Homebrew Intel）下 ``rustup`` 是 keg-only 包，``cargo`` 默认不在
``PATH`` 上；需要在调用 ``build.sh`` 前导出：

.. code-block:: shell

   export PATH="/usr/local/opt/rustup/bin:$HOME/.cargo/bin:$PATH"

Apple Silicon 的 keg 路径为 ``/opt/homebrew/opt/rustup/bin``。

未安装 Rust 时的典型表现
~~~~~~~~~~~~~~~~~~~~~~~~

``modules/rust/CMakeLists.txt`` 与 ``modules/rust/rust_sample.cmake``
都会通过 ``find_program(CARGO_BIN cargo ...)`` 查找 ``cargo`` 可执行文件。
如果开发机未安装 rustup，或者 ``cargo`` 不在 ``PATH`` 与已知 HINTS 路径中，
CMake 配置阶段会直接报错并提示如下内容：

.. code-block:: text

   cargo not found.
   Install rustup via https://rustup.rs (or `brew install rustup` on macOS), then:
     cd modules/rust && rustup show
   (rustup auto-installs the toolchain locked in rust-toolchain.toml)

按提示执行 ``rustup show`` 一次后再重新构建即可。

架构分层
--------

``modules/rust/arcs/src/lib.rs`` 把能力分成两层：

1. **sys 层**

   位于 ``arcs/src/sys/<module>.rs``，提供与底层 C API 严格一致的
   ``extern "C"`` 声明：

   - 保持与 C 数据结构 / ABI 严格一致
   - 仅做必要的命名映射，不引入运行时开销
   - CI 中由 ``tools/rust/bindgen-drift.sh`` 与 ``bindgen`` 生成的参考比对

2. **hal 层**

   位于 ``arcs/src/hal/<module>.rs``，提供更符合 Rust 风格的封装：

   - RAII：资源在 ``Drop`` 中回收（``Thread`` 除外，见下文“注意事项”）
   - 错误统一映射到 ``arcs::Result<T> = Result<T, arcs::Error>``
   - 在 sample 中可直接以对象/方法方式使用设备

另外 ``arcs/glue/`` 提供少量 C shim（如 ``arcs_rust_log.c`` 转发到
``easylogger``、``arcs_rust_dev.c`` 暴露 device 内部字段），由
``arcs-rust-glue`` OBJECT target 统一声明源码与编译配置；sample helper
复用该 target 的源码和编译使用需求注入到当前目标。

详见 ``modules/rust/README.md`` 的 “Architecture” 与 “Adding a sys binding for a new C header” 章节。

``arcs`` crate 提供的主要能力
-----------------------------

通过 ``arcs::xxx`` 在 sample 中直接使用，无需额外依赖 ``log`` 之外的 crate：

系统与基础设施：

- ``log``（re-export 自 ``log`` crate，使用 ``arcs::log::info!(...)`` 等）
- ``Thread``
- ``Mutex<T>`` / ``MutexGuard<'_, T>``
- ``Semaphore``
- ``Channel<T, N>``
- ``Error`` / ``Result``
- ``heap::ArcsAllocator``（``arcs::entry!`` 会自动注册为 ``#[global_allocator]``）
- ``entry!`` 宏（``#[panic_handler]`` + ``#[global_allocator]`` + log init + ``#[no_mangle] extern "C"`` 入口）
- ``#[arcs::main]`` 属性宏（来自 host 编译的 ``arcs-macros`` crate，注入与 ``entry!`` 相同的运行时机制，但采用属性语法并固定导出符号 ``rust_main``；与 ``entry!`` 二选一）

设备与外设：

- ``Gpio`` / ``Level`` / ``OutputOpts`` / ``InputOpts`` / ``IrqMode`` / ``IrqRegistration``
- ``GpioPin``（由 ``Gpio::pin(n)`` 取得，再用 ``.into_output`` / ``.into_input`` builder 配置方向，实现 embedded-hal 数字引脚 trait ``OutputPin`` / ``InputPin`` / ``StatefulOutputPin``）
- ``Uart`` / ``UartConfig`` / ``Parity``（``Uart`` 同时实现 ``embedded_io::{Read, Write}``；原 ``Uart::read`` 已改名为 ``read_bytes``）
- ``Delay``（``Delay::new()``，实现 ``embedded_hal::delay::DelayNs``）
- ``I2c`` / ``i2c::SPEED_STANDARD`` 等（``I2c::open(c"i2c0")``，主机模式 ``configure(speed)``；``write`` / ``read`` / ``write_read`` / ``probe``；实现 ``embedded_hal::i2c::I2c``，7-bit 地址）
- ``Spi`` / ``spi::Config``（``Spi::open(c"spi0")``，``configure(&Config)``；``transfer`` / ``write`` / ``read``。lisa SPI 为异步完成，封装内部注册完成回调并以信号量阻塞，对外是同步阻塞 API；实现 ``embedded_hal::spi::SpiBus<u8>``）
- ``Pwm``（``Pwm::open(c"pwm0", channel)``，``set_frequency`` / ``configure`` / ``set_duty_percent`` / ``enable`` / ``disable``；实现 ``embedded_hal::pwm::SetDutyCycle``，``max_duty_cycle() = 100``）
- ``Rtc`` / ``DateTime``（``Rtc::open(c"rtc0")``，``set(&DateTime)`` / ``now()``；``DateTime.year`` 为 2000 起的偏移量，0..=127。embedded-hal 1.0 无 RTC trait，仅原生 API）
- ``Adc``（``Adc::open(c"adc0")``，``configure_channel(ch, ref, res)`` / ``read(ch) -> u16``，外加 ``adc::raw_to_mv`` 辅助函数；通道为参数，6=VBAT、7=TEMP 为内部通道。embedded-hal 1.0 无阻塞 ADC trait，仅原生 API）
- ``Flash`` / ``flash::{ERASE_SIZE, WRITE_SIZE}``（``Flash::open(c"flash0")``，``read`` / ``write`` / ``erase`` / ``capacity``；写前需先擦除。实现 ``embedded_storage::nor_flash::{ReadNorFlash, NorFlash}``）
- ``reboot()``（``arcs::reboot() -> !``，整片软复位；为 *full reset*，不保留 SRAM）
- ``Display`` / ``display::FrameBuffer``（``Display::open(c"display")``，总线/面板需先在 C 侧 attach；``width`` / ``height`` / ``blanking_off`` / ``set_brightness`` / ``write``。``FrameBuffer::new(&display)`` 在 PSRAM 分配整屏 RGB565 帧缓冲，实现 ``embedded_graphics_core::DrawTarget``；用 embedded-graphics 绘制后 ``flush()`` 推送到面板）
- 双区分配器：``arcs::Zone::{Psram, Sram}`` + ``heap::alloc_in`` / ``dealloc_in`` + ``arcs::RawBox<T>``（区分物理池的 Box，稳定版、无需 allocator_api；PSRAM 走 ``lisa_mem_*``，内部 SRAM 走 ``inram_*``）
- ``Audio`` / ``audio::PlayConfig``（``Audio::open(c"audio0")``，``configure_play(&PlayConfig)`` 设定采样率/通道/位宽/增益与 DMA 缓冲池；``play_start`` / ``write`` / ``write_all`` / ``flush`` / ``play_stop``。``write`` 接收 ``&[i16]`` PCM，写入驱动 DMA 缓冲池，池满时阻塞直至 DMA 排空。embedded-hal 无音频 trait，仅原生 API）
- ``Wifi`` / ``ScanInfo``（``Wifi::init()`` 调用 ``wifi_mgr_init``；``sta_enable`` / ``scan(&mut [ScanInfo]) -> 个数`` / ``connect(ssid, pwd)`` / ``disconnect`` / ``status``。``ScanInfo`` 提供 ``ssid()`` / ``bssid()`` / ``rssi`` / ``channel`` / ``encryption_mode``，配合 ``wifi::encryption_name``。``wifi_manager`` 是扁平 C 函数 API（非 ``lisa_device`` vtable）；WiFi 核心拉起（mac_manager + 异步 ``lisa_wifi_init``）作为平台 glue 放在 sample 的 C ``main.c`` 中，Rust 侧只驱动 manager。无 embedded-hal WiFi trait，仅原生 API）
- ``Bluetooth``（``Bluetooth::init()`` 拉起 ``lisa_bluetooth`` 协议栈并阻塞等待 enable-complete 回调；``start_advertising`` / ``stop_advertising``，及带 ``adv_id`` / ``adv_type`` 的 ``*_set`` 变体。广播载荷通过在 app 中重写 SDK 的 weak 符号 ``lisa_bt_get_adv_data`` / ``lisa_bt_get_scan_rsp_data`` 提供，配合 ``bt::ADV_*`` / ``bt::AD_TYPE_*`` / ``bt::FLAG_*`` 常量构造 AD 字节数组。BLE 协议栈跑在 CP core（``CONFIG_ARCS_CP_CORE=y``）。无 embedded-hal BLE trait，仅原生 API）

异步 / embassy（R9，独立 crate ``arcs-embassy``）：

- ``arcs_embassy::run(|spawner| { ... })`` 在当前 FreeRTOS 任务上跑起 embassy executor（不返回，当前任务即 executor）；空闲时阻塞在 FreeRTOS 任务通知（而非 ``wfi``），与 FreeRTOS 协作调度。
- ``embassy_time::{Timer, Duration, Instant}`` 通过本 crate 提供的 time driver 工作（``now()`` 取自 SoC 64 位 1 MHz 单调定时器）；``critical-section`` 用 FreeRTOS 中断屏蔽实现。
- ``arcs_embassy::Delay`` 实现 ``embedded_hal_async::delay::DelayNs``。
- 全部基于 **stable** Rust 1.83.0（固定 edition-2021 末版：embassy-executor 0.9.1 / embassy-time 0.4.0，使用 generic timer queue）。app 需编译随附的 C glue ``arcs-embassy/glue/arcs_embassy_glue.c``；用 ``#[embassy_executor::task]`` 的 crate 需直接依赖 ``embassy-executor`` / ``embassy-time``。参见 ``samples/libraries/rust/async_tasks``。

embedded-hal / embedded-io / embedded-storage / embedded-graphics 适配层：

- ``embedded-hal`` 是 ``arcs`` crate 默认开启的 feature（``default = ["embedded-hal"]``，作为“社区 trait”总开关，启用 ``embedded-hal = 1.0``、``embedded-io = 0.6`` 与 ``embedded-storage = 0.3``）；可用 ``--no-default-features`` 关闭，此时仅保留原生 inherent HAL，所有 trait 实现被编译掉
- ``arcs::Error`` 实现了 ``embedded_hal::digital::Error`` / ``embedded_io::Error`` / ``embedded_hal::i2c::Error`` / ``embedded_hal::spi::Error`` / ``embedded_hal::pwm::Error`` 与 ``embedded_storage::nor_flash::NorFlashError``，作为上述 trait 实现的关联 ``Error`` 类型
- 因此面向这些 trait 编写的社区 driver crate 可直接复用 ARCS 外设

R2 新增 I2C / SPI，R3 新增 PWM / RTC / ADC / Flash，R5 新增 双区分配器 + Display，
R6 新增 Audio 播放，R8a 新增 WiFi station 扫描/连接，R8b 新增 BLE 广播，
R9 新增 async/embassy（``arcs-embassy`` crate），R7 新增 arcs AP 核硬件 FPU 上的
``f32`` 运算，提供两个示例：``fpu_softfloat``（stable，``riscv32imac`` +
``target-feature=+f`` 软浮点 ABI，无需 nightly）与 ``fpu_hardfloat``（整镜像硬浮点
``ilp32f``，需 nightly ``-Zbuild-std``），均在 AP 核 hartid 0 上实板验证
（``misa.F=1``）。roadmap 主线（R1–R9 + R7）至此完成。"整镜像硬浮点 ABI（``ilp32f``，``CONFIG_FPU=y`` +
``CONFIG_RISCV_FPU=y`` + Rust ``riscv32imafc`` 经 nightly ``-Zbuild-std``）"在
AP 核上同样已实板验证可用（C 与 Rust 都能用 FP），只是需要 nightly，故 sample
默认走免 nightly 的 ``+f`` 方案。仍暂缓的只有 R4 真正的 defmt 接入与跨复位
panic 持久化。

复位与 panic 行为（R4）
-----------------------

默认情况下，Rust panic 会先记录消息+位置（tag ``rust-panic``，ERROR 级），再执行
``ebreak`` 陷入 SDK 的 RISC-V 故障处理器——后者打印寄存器后 **挂死**（``while(1)``），
便于调试停住的内核。

opt-in cargo feature ``panic-reboot`` 改为：记录后调用 ``arcs::reboot()`` 触发整片软复位，
使设备 **恢复重启**（而非挂死）。逐 sample 启用：
``arcs = { ..., features = ["panic-reboot"] }``。参见
``samples/libraries/rust/panic_reboot``（已在 arcs_evb 实板验证：panic 记录后复位、复位原因
变为 ``WATCHDOG SOFTWARE`` 并循环重启）。建议配合同步日志
（``CONFIG_EASYLOGGER_LOG_MODE_ASYNC=n``），否则 panic 消息会在复位前被异步缓冲丢弃。

**暂缓**：defmt 框架接入与「跨复位持久化 panic 记录」需要调试探针（J-Link，读取 RTT）与
保留式 RAM/复位策略。当前 arcs 故障为挂起（非复位）、软复位为整片复位（不保留 SRAM），
且无 J-Link 连接，故二者暂缓；Rust ``log`` 可经 SDK 既有 ``lisa_log_backend_segger_rtt``
后端走 RTT（Kconfig 开启，读取需探针）。

构建集成方式
------------

典型的 Rust sample ``CMakeLists.txt`` 需要完成三件事：

1. 正常创建 ARCS SDK 可执行目标
2. 通过 ``LISTENAI_RUST_APP`` 声明 sample 的 Cargo 工程目录
3. ``include`` 共享 helper 并调用 ``listenai_maybe_enable_rust_target()``

参考写法：

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.19)
   set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

   find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})
   project(rust_helloworld)

   set(LISTENAI_RUST_APP ${CMAKE_CURRENT_SOURCE_DIR})

   listenai_add_executable(${PROJECT_NAME})
   include($ENV{ARCS_BASE}/modules/rust/rust_sample.cmake)
   listenai_maybe_enable_rust_target(${PROJECT_NAME})
   target_sources(${PROJECT_NAME} PRIVATE src/main.c)

``listenai_maybe_enable_rust_target()`` 在检测到 ``LISTENAI_RUST_APP``
后自动完成以下工作：

- 引入 ``modules/rust`` 子目录（once-guard），构建出 ``arcs-rust-glue``
  OBJECT 库
- 解析 sample 的 ``Cargo.toml`` 里 ``[package].name``
- sample 是独立 Cargo package，``Cargo.toml`` 不声明指向
  ``modules/rust`` 的相对 ``package.workspace``；helper 在构建时生成临时
  Cargo patch config 文件，并通过 ``--config <file>`` 注入 SDK 本地 crate
  路径
- 默认使用 ``modules/rust`` 的 stable toolchain 与 ``.cargo/config.toml``；
  像 ``fpu_hardfloat`` 这种带 sample-local ``rust-toolchain.toml`` 或
  ``.cargo/config.toml`` 的样例则使用自身目录，以保留 nightly/build-std
  配置
- 调用 ``cargo build --manifest-path <sample>/Cargo.toml --profile <release|dev> -p <pkg>`` 生成
  ``lib<pkg>.a``
- 创建 IMPORTED 目标并以 ``-Wl,--whole-archive`` 形式链接到 ``${TARGET}``
- 复用 ``arcs-rust-glue`` target 的源码、include、宏、编译选项与 linked usage
  requirements，把 C shim 直接加入 ``${TARGET}``，避免部分 SDK 链接路径把
  OBJECT target 转成 ``-l`` 标志

``arcs`` crate 自身是 ``crate-type = ["rlib"]``，并不产出独立的
``libarcs.a``。Sample 的 staticlib 通过普通版本依赖（由 helper patch 到 SDK
本地 path）把 ``arcs`` 的 rlib 代码一起打包进 ``lib<pkg>.a``，``arcs::entry!`` 注入的
``#[panic_handler]`` / ``#[global_allocator]`` 也在 sample 这一侧。
``--whole-archive`` 是为了避免 ``#[no_mangle] extern "C"`` 入口符号被
linker DCE 消除。

目标平台与工具链约束
--------------------

根据 ``modules/rust/rust-toolchain.toml`` 与
``modules/rust/rust_sample.cmake`` 的定义，当前 Rust 构建链路面向：

- Rust toolchain ``1.83.0``，components ``rustfmt`` + ``clippy``
- 目标三元组 ``riscv32imac-unknown-none-elf``：32 位 RISC-V，含 IMAC 扩展
- 软浮点 ``ilp32`` ABI。需要硬件 FPU 时，可在 arcs **AP 核**（hartid 0，带 FPU）
  上用 ``riscv32imac`` + ``-C target-feature=+f`` 发射 FPU 指令（仍保持 ``ilp32``
  ABI，与软浮点 SDK 链接、无需 nightly），参见 ``samples/libraries/rust/fpu_softfloat``。
  "整镜像硬浮点 ABI（``CONFIG_FPU=y`` + ``CONFIG_RISCV_FPU=y`` / ``ilp32f``）"也已在 AP 核实板验证可用（C 与 Rust 都能用 FP，FreeRTOS 按任务保存 F 寄存器），只是 Rust 侧需 nightly ``-Zbuild-std``，参见 ``samples/libraries/rust/fpu_hardfloat``
- ``no_std`` + ``alloc``：通过 ``arcs::heap::ArcsAllocator`` 接入 SDK heap
- 与 SDK 现有 C 代码一致的 include 体系（``arcs-rust-glue`` 这一侧）

也就是说 Rust 代码并不是脱离 ARCS SDK 独立构建，而是作为当前 SDK 工程的
一部分参与交叉编译和链接，并由 SDK 的 linker 脚本统一布局。

最小工作流
----------

1. 在 sample 目录下创建独立 ``Cargo.toml``（``crate-type = ["staticlib"]``；
   空 ``[workspace]`` 避免 copy-out 后被父级 workspace 捕获；SDK crate 路径
   由 ``rust_sample.cmake`` 在构建时注入）：

   .. code-block:: toml

      [package]
      name    = "rust_my_sample"
      version = "0.1.0"
      edition = "2021"
      rust-version = "1.83"
      license = "Apache-2.0"

      [workspace]

      [lib]
      crate-type = ["staticlib"]
      path       = "src/lib.rs"

      [dependencies]
      arcs = "0.1.0"
      log  = { version = "0.4", default-features = false }

   常规 Rust sample 不再纳入 ``modules/rust/Cargo.toml`` 的 workspace
   members；新增 sample 不需要修改 SDK Rust workspace。需要 nightly/build-std
   等特殊工具链的样例可保留 sample-local ``rust-toolchain.toml`` /
   ``.cargo/config.toml``，例如 ``fpu_hardfloat``。
2. 创建必备的 ``Kconfig``（``osource "$ARCS_BASE/Kconfig"``）与 ``prj.conf``
   （至少打开 ``CONFIG_LOG_FRONTEND_EASYLOGGER=y``；用到 GPIO/UART 时打开
   ``CONFIG_LISA_GPIO_DEVICE`` / ``CONFIG_LISA_GPIOA`` 等）
3. 在 sample 中保留一个 C 入口文件 ``src/main.c``，例如：

   .. code-block:: c

      #define LOG_TAG "rust_app"
      #include <lisa_log.h>

      extern int rust_app_main(void);

      int main(int argc, char **argv)
      {
          (void)argc; (void)argv;
          return rust_app_main();
      }

4. 在 ``src/lib.rs`` 中通过 ``arcs::entry!`` 暴露可被 C 调用的入口：

   .. code-block:: rust

      #![no_std]
      extern crate alloc;

      arcs::entry!(rust_app_main, {
          arcs::log::info!("Hello from Rust on ARCS SDK!");
          Ok(())
      });

   或改用 ``#[arcs::main]`` 属性宏（注入相同的 panic handler / global
   allocator / log init，但固定导出符号 ``rust_main``，此时 C 侧改为声明
   ``extern int rust_main(void)`` 并调用它）：

   .. code-block:: rust

      #![no_std]
      extern crate alloc;

      #[arcs::main]
      fn main() -> arcs::Result<()> {
          arcs::log::info!("Hello from Rust on ARCS SDK!");
          Ok(())
      }

5. 构建并烧录：

   .. code-block:: shell

      bash build.sh -C -S samples/libraries/rust/<name> -DBOARD=arcs_evb

注意事项
--------

- Rust target 仍依赖 C 入口和现有 ARCS SDK 工程骨架，``main()`` 必须留在
  C 侧，不能写纯 Rust 裸工程
- ``arcs::entry!`` 与 ``#[arcs::main]`` 二选一，且在一个 crate 内只能使用
  一次（两者都定义 crate-root 的 ``#[panic_handler]`` / ``#[global_allocator]``
  单例）；需要多个 Rust 入口时手写额外的 ``#[no_mangle] pub extern "C" fn``
- 默认目标三元组为软浮点 ``riscv32imac-unknown-none-elf``；需要硬件 FPU 时在
  arcs AP 核上加 ``-C target-feature=+f``（见 ``fpu_softfloat``），或走
  ``CONFIG_FPU=y`` 的 ``ilp32f`` 整镜像硬浮点（见 ``fpu_hardfloat``，需 nightly）
- ``Cargo.toml`` 使用 ``panic = "abort"``（``arcs::entry!`` 的 panic
  handler 会调用 SDK 日志后 trap）
- ``Thread::Drop`` 是空实现：SDK 在任务返回时通过 ``vTaskDelete(NULL)``
  + ``vPortCleanUpTCB`` 自动回收 TCB，从 Rust 调用
  ``lisa_thread_delete`` 会触发 use-after-free；如需在闭包返回前强制
  终止，调用 ``unsafe { thread.abort() }``（消费 ``self``）
- ``Semaphore::new(N)`` 的 ``N`` 表示**最大计数**，初始计数恒为 0；
  用作 binary signal 时使用 ``Semaphore::new(1)``。``N = 0`` 会触发
  FreeRTOS 硬断言 ``configASSERT(uxMaxCount != 0)`` 并使 SoC panic，
  没有 graceful error 路径
- ``Channel<T, N>`` 要求 ``T: Copy`` —— 底层 FreeRTOS queue 直接按字节
  拷贝，带 ``Drop`` 的类型会泄漏或 double-free
- GPIO/UART 驱动是 Kconfig-gated，``prj.conf`` 需要按 sample 实际使用情况
  打开对应的 ``CONFIG_LISA_GPIO*`` / ``CONFIG_LISA_UART*``
- 烧录 / 串口抓取的工作流详见 ``modules/rust/README.md`` 的
  “Flash + verify recipe” 章节
- 某些 sample 依赖板级特定外设（LED 引脚、UART 端口编号等），porting
  到其它板型时需对应修改 ``prj.conf`` 与 Rust 源码中的常量
