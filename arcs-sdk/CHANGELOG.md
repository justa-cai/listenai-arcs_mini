# Change Log

## [0.1.8] - 2026-07-08:

- All changes since 0.1.7

### Changed:
  - lisa_pm / lisa_device:
    - 设备生命周期改为 destroy/reinit 模型，支持驱动挂载 deinit 并在睡前销毁、唤醒后重建
    - system PM 接入改为独立 `LISA_DEVICE_PM_ATTACH` registry，降低普通设备注册与 PM 能力耦合
    - 默认 system PM 设备上限提升到 16，并支持默认 SRAM snapshot 与 AP 远端电源锁
    - 唤醒流程支持透传唤醒原因、异步分发应用回调，并补充双核 PM 流程文档
  - drivers:
    - audio、uart、gpio、i2c、spi、i2s、pwm、rtc、hwtimer、wdt、sdmmc、dvp、camera、rgb、qspilcd、display、touch 等驱动接入设备销毁重建或 PM attach 模型
    - VenusA 平台补齐 audio、gpio、flash、adc、rtc、pwm、wdt、hwtimer、spi、uart、camera、dvp 等驱动适配
    - lisa_dvp 新增 CPDMA 支持，camera 增加 SC030IOT 与硬件 reset 适配
  - VenusA / SoC:
    - 引入 VenusA / CSK7002G6U / CSK7004J8U SoC 与 `venusa_rd_evb` 板级支持
    - SoC 公共抽象层统一 `<soc/chip.h>` 入口，并将 SYS_INIT linker fragment 收敛到 system/init
    - VenusA 应用支持 CP/AP 固件合一构建、AP 无 boot 运行、recovery ADB 与 flash/TF OTA 流程
    - 完善 VenusA 内存布局、FreeRTOS SWI 路由和 venusa 仓库地址配置
  - boot / build:
    - uboot 从 arcs-sdk 移出，boot 构建流程改为仓内 boot 子工程与公共 boot 合一编译路径
    - 链接阶段默认将 orphan sections 视为错误，并同步处理 boot linker script 的 metadata 段
    - 新增仓库内 `mkhdr`，更新 Linux cskburn，并新增 Windows 与 macOS arm64 构建环境支持
    - LUNA 共享段统一放入普通 SRAM，补齐 `.sharedmem.*` 子段匹配并调整 AP SRAM 默认范围
  - hal / rpc / ipc:
    - HAL 多轮更新到 20260630 系列，适配 PM、IPC、DMA、flash status register、BLE timer 等变化
    - LSF / uRPC 同步到 20260630 SDK，IPC 自动初始化收敛，支持 direction-independent IPC log forwarding
    - WiFi / dual-core 模式适配新版 HAL IPC API，AP IPC log backend 与 IPC print 策略测试同步更新
  - bluetooth:
    - 适配 20260630 BT HAL 与 stack API，新增 S2M netcfg IPC bridge、通话事件上报和 mSBC 配置
    - 配对数据迁移到 lisa kv，discovery list 移到 PSRAM，并清理旧 kv migration 选项
  - audio / player:
    - 新增 record channel gain API，恢复运行期录音增益配置
    - lisa_player 增加播放器缓冲与超时时间配置能力
    - 修复 I2S / IIS 发送、接收缓冲回收和 TX PiPo restart recovery 问题
  - Zig:
    - Zig target 构建跟随 `CONFIG_FPU` 选择 RISC-V ABI，并新增 FPU build-only 覆盖
    - Zig target 支持 DEPENDS / MODULES 转发与 Kconfig 配置模块生成
    - 自定义 Zig module 继承 SDK include、`arcs` 依赖和 target C headers，避免 `@import("arcs")` / `@cImport` 构建失败
    - 扩展 SDK adapter bindings 与 binding contract 覆盖，新增 LVGL binding 与测试
  - Rust:
    - Rust samples 构建改为更独立的 SDK 集成方式，CI 接入 guardian test stage、lint、build、bindgen-drift
    - cargo sample 构建清理继承环境，修复 rustc stdin 污染、soft-float / hard-float ABI 和 BLE scan-response 问题
    - Rust 文件系统 API 收紧 mount / seek / read / write 语义，避免并发挂载、悬垂挂载点和误格式化风险
  - docs / CI:
    - 文档站补齐示例 README 链接、系统启动流程、lisa_pm 驱动接入与双核 PM 文档
    - CI 新增外部 master 镜像同步、独立 Zig test job、AI wiki ingest / QA 维护流水线与子仓库合并检查
    - 构建缓存、AI review、UTF-8 commit 信息、submodule lock 与 cache 更新流程进一步加固

### Fixed:
  - lisa_pm:
    - 修复附加 PM 未绑定到设备快速路径的问题
    - 修复 wakeup 回调语义、Kconfig 冗余默认值和新版 HAL PM 依赖 IPC role 的构建问题
  - boot / OTA:
    - 修复 uboot OTA 完成后重启可能无法启动的问题
    - 修复 CONFIG_BOOT 应用增量构建时 firmware header 重复叠加的问题
    - 修复 boot linker script orphan section 导致 boot ExternalProject 链接失败的问题
  - display / camera:
    - 修复 lisa_display 分块旋转越界问题
    - 修复 SC030IOT 冷启动不出图、camera 格式切换帧元数据和 DVP ping-pong reload / DMA ISR 竞争问题
    - 修复 lisa_dvp CPDMA BurstThreshold 导致画面异常的问题，并补充单元测试
  - log / RTT / time:
    - CLOG 增加 early UART fallback
    - 修复 SEGGER RTT 后端误用 `SEGGER_RTT_printf` 导致日志乱码的问题
    - 修复关闭调度时调用日志接口 crash 与 CP boot log time 问题
  - ACOMP / algorithms:
    - 修复 CAE master deinit 时 remote device 未释放导致 repeated RTSP talk 泄漏的问题
    - ARCS 平台改为等待 remote 连接后再查询设备信息
    - lnn_resnet18 修复 AP GPIO IRQ 与 Kconfig style 问题
  - modem / bluetooth:
    - 修复 modem 串口交互重试异常并优化串口交互
    - 修复蓝牙状态同步、连接返回值和 classic 配置空实现问题
  - filesystem / kv / heap:
    - 修复 LSFS get / dump 防护问题
    - 禁用 malloc PSRAM fallback，避免 heap 分配行为不确定
    - 调整 ic_mutex / crypto heap size，降低内存冲突风险
  - build / tools:
    - 修复 macOS 固件头生成在 UTF-8 locale 与 BSD stat 下失败的问题
    - 修复 VenusA sample / hwtimer sample 构建差异和 uboot watchdog API 测试配置缺失问题
    - 修复 HAL vrtc.c 在非 IPC role 宏配置下编译失败的问题

### Added:
  - 语言绑定:
    - rust: 新增 Rust no_std 语言支持与 `arcs` crate
      - 覆盖 log、thread、sync、gpio、uart、i2c、spi、pwm、rtc、adc、flash、display、audio、wifi、bt、filesystem 等模块
      - 提供 `arcs::entry!`、`#[arcs::main]`、全局 allocator、panic handler、easylogger / RTT 日志接入
      - 支持 embedded-hal / embedded-io / embedded-storage / embedded-graphics trait 适配
      - 新增 PSRAM/SRAM 双区分配器、`RawBox<T>`、`FrameBuffer` 与 display embedded-graphics 绘制能力
      - 新增 reboot、panic-reboot、hardware-FPU、WiFi station、BLE advertising、async/embassy、audio playback 等示例能力
      - 新增 helloworld、blinky、eh_blink、i2c_scan、spi_loopback、adc_log、panic_reboot、rtt_log、display_gfx、audio_play、wifi_scan、ble_adv、fpu_demo、async_tasks 等示例
    - zig: 增强实验性 Zig 语言支持
      - 扩展 SDK adapter bindings、target helper、配置模块和 LVGL binding
      - 增加 FPU ABI、custom module、SDK include、binding contract 与 display 上板测试覆盖
  - lisa_pm / device_wakeup:
    - 新增双核 basic、双核 GPIO 唤醒、WiFi 保活、audio device wakeup、USB UVC wakeup 与 remote device wakeup 测试/示例
    - USB UVC 唤醒支持睡前 software disconnect、GPIOB9 唤醒后重新枚举
  - samples / demos:
    - 新增 VenusA face_detect、palm 掌静脉算法、lnn resnet18、CP 摄像头显示与 AP 推理示例
    - 新增 FreeRTOS、lisa kv、CherryUSB UAC device、TinyUSB UAC device、LISA flash halt-by-remote 配置示例
    - demo 目录按 arcs / venusa 平台重组
  - ACOMP:
    - 新增 palm 算法组件，提供 CP 侧接口、IPC 协议和图像流通道封装
    - 新增 wakeup fan state IPC API，并适配 VenusA 平台 FD 组件
  - system / shell / wakeup:
    - lisa_shell 支持 UART poll_in 接收模式
    - ic-message 支持 cidu 通知方式
    - wakeup 支持命令词超时时间配置，并更新唤醒门限等级有效范围
  - network / storage / boot:
    - lisa_modem 新增 status 与 identity API，UART sync 默认优先 921600 baud
    - VenusA uboot 子仓支持多 SoC、image header BOOT_HARTID 与 watchdog 实现更新
    - coreHTTP / coreMQTT、CherryUSB、FreeRTOS、cAT 等子模块同步更新

### Deprecated:

## [Unreleased]

### Added:
  - 语言绑定:
    - rust: 新增 Rust no_std 语言支持（MVP）
      - 目标三元组 `riscv32imac-unknown-none-elf`，验证板型 `arcs_evb`
      - 提供 `arcs` crate，覆盖模块 log、thread、sync（Mutex/Semaphore/Channel）、gpio、uart
      - `arcs::entry!` 宏统一注入 panic handler、global allocator 与 log 初始化
      - 新增示例 `samples/libraries/rust/helloworld` 与 `samples/libraries/rust/blinky`
      - 新增 `test/rust/binding_test`（已在 arcs_evb 上验证 8/8 通过）
      - CI 新增 `rust:lint` / `rust:build` / `rust:bindgen-drift` / `rust:onboard-binding-test` 四个 job（最后一个为 manual / scheduled）
    - rust: R1 新增 embedded-hal 1.0 + embedded-io 0.6 trait 适配层
      - 新增 host 编译的 `arcs-macros` crate，提供 `#[arcs::main]` 属性宏（固定导出符号 `rust_main`）；`arcs::entry!` 保留（两者二选一）
      - `GpioPin`（由 `Gpio::pin(n)` 取得）实现 `OutputPin` / `InputPin` / `StatefulOutputPin`；`Uart` 实现 `embedded_io::{Read, Write}`；`Delay` 实现 `DelayNs`
      - `embedded-hal` 为默认开启的 feature（可用 `--no-default-features` 关闭以编译掉 trait 实现）
      - 破坏性变更（pre-1.0）：`Uart::read` 改名为 `read_bytes`，避免与 `embedded_io::Read::read` 冲突
      - 新增示例 `samples/libraries/rust/eh_blink`；`binding_test` 升级为 10/10（已在 arcs_evb 实板验证）
    - rust: R2 新增 I2C 与 SPI 外设绑定
      - `sys::i2c` + `hal::i2c::I2c`（主机模式 configure/write/read/write_read/probe），实现 `embedded_hal::i2c::I2c`
      - `sys::spi` + `hal::spi::Spi`（configure/transfer/write/read；lisa SPI 为异步完成，封装内部注册完成回调并以信号量阻塞，对外呈现同步 API），实现 `embedded_hal::spi::SpiBus<u8>`
      - `arcs::Error` 增加 `embedded_hal::i2c::Error` 与 `embedded_hal::spi::Error` 实现
      - 新增示例 `samples/libraries/rust/i2c_scan`（I2C 总线扫描）与 `samples/libraries/rust/spi_loopback`（SPI 全双工/回环），均已在 arcs_evb 实板验证
      - 修复 `sys::spi::SpiConfig` 枚举字段宽度的 ABI 错误（C enum 为 4 字节，应为 u32 而非 u8；曾导致 master_mode 错位、传输完成中断不触发而超时）
      - `rust:bindgen-drift` CI 规则纳入 `drivers/lisa_i2c` / `drivers/lisa_spi` 头文件变更
    - rust: R3 新增 PWM / RTC / ADC / Flash 外设绑定
      - `sys::pwm` + `hal::pwm::Pwm`（configure/set_duty_percent/enable/disable），实现 `embedded_hal::pwm::SetDutyCycle`
      - `sys::rtc` + `hal::rtc::Rtc` + `DateTime`（set/now；year 为 2000 起偏移；embedded-hal 1.0 无 RTC trait，仅原生 API）
      - `sys::adc` + `hal::adc::Adc`（configure_channel/read + `raw_to_mv`；通道 6/7 为内部 VBAT/TEMP；embedded-hal 1.0 无阻塞 ADC trait，仅原生 API）
      - `sys::flash` + `hal::flash::Flash`（read/write/erase/capacity），实现 `embedded_storage::nor_flash::{ReadNorFlash, NorFlash}`；新增 `embedded-storage = 0.3` 依赖（并入 embedded-hal feature）
      - `arcs::Error` 增加 `embedded_hal::pwm::Error` 与 `embedded_storage::nor_flash::NorFlashError` 实现
      - `binding_test` 扩展至 14/14（新增 pwm_configure / rtc_set_get / adc_read / flash_scratch 子测试），新增示例 `samples/libraries/rust/adc_log`；均已在 arcs_evb 实板验证
      - `rust:bindgen-drift` 纳入 `drivers/lisa_{adc,flash,pwm,rtc}` 头文件变更
    - rust: R4 新增系统复位与 panic 恢复（调试基础设施）
      - 新增 `arcs::reboot()`（绑定 `sys_platform_sw_full_reset`，整片软复位，不保留 SRAM）
      - 新增 opt-in cargo feature `panic-reboot`：Rust panic 先记录日志再触发软复位（恢复），替代默认的 ebreak→SDK 故障处理器挂死（默认关闭，保留可调试的挂起行为）
      - 新增示例 `samples/libraries/rust/panic_reboot`，已在 arcs_evb 实板验证（panic 记录 `panicked at <file:line>` → 软复位 → 重启循环）
      - RTT 日志已接通：新增示例 `samples/libraries/rust/rtt_log`（`CONFIG_LOG_BACKEND_SEGGER_RTT=y`），Rust `log` 经 easylogger 输出钩子改道 `SEGGER_RTT_printf`，无需 Rust 侧改动。固件侧已在 arcs_evb 验证（开启 RTT 后端后 UART 日志在 easylogger 初始化后停止＝输出已改道 RTT，固件继续在 CP 核运行）
      - 主机侧 RTT 读取已验证（J-Link PLUS V11 + J-Link 软件 V9.46，cJTAG 接 CP 核 core1）：经 J-Link 读出 `rtt_log` 的 Rust 日志（`rtt_log: tick 0/1/2...`）。注意旧版 J-Link V7.52a 无法解析 ARCS RISC-V cJTAG（回退 ARM7），需 V9.46+；RTT 控制块在 J-Link 默认搜索范围之下，需按 `_SEGGER_RTT` 符号地址读取
      - 修复 SDK RTT 后端 bug：`system/log/lisa_log_backend_segger_rtt.c` 误用 `SEGGER_RTT_printf` 把非 NUL 结尾的日志缓冲当格式串，导致首行之后全是乱码；改为 `SEGGER_RTT_Write(0, log, len)`
      - 暂缓：真正的 defmt 框架接入；「跨复位持久化 panic 记录」需保留式 RAM/复位策略（arcs 故障为挂起、软复位为整片复位不保留 SRAM）
    - rust: R5 新增 PSRAM/SRAM 双区分配器与 Display 支持
      - `sys::mem` + `heap::{Zone, alloc_in, dealloc_in, zone_of}` + `RawBox<T>`（区分物理池的 Box，稳定版无需 allocator_api）。PSRAM 走 `lisa_mem_align_alloc`，内部 SRAM 走 `inram_malloc`（注意 `lisa_mem_sram_alloc` 实为 PSRAM 别名，非真 SRAM）。实板验证：SRAM=0x2001_xxxx、PSRAM=0x2880_xxxx
      - `sys::display` + `hal::display::Display`（get_capabilities/write/blanking/set_brightness）+ `FrameBuffer`（PSRAM 整屏 RGB565 帧缓冲，实现 `embedded_graphics_core::DrawTarget`）；新增 `embedded-graphics-core = 0.4` 依赖（并入 embedded-hal feature）
      - `binding_test` 扩展至 15/15（新增 dual_alloc 子测试）；新增示例 `samples/libraries/rust/display_gfx`（embedded-graphics 在 ST7789P3 上绘制，已实板验证面板初始化+绘制+刷新）

## [0.1.7] - 2026-05-22:

- All changes since 0.1.6

### Changed:
  - boot/uboot:
    - 多轮子模块升级，引入 store-based recovery、SHA-256 HSU OTA 校验和 CRC32 hardening
    - 支持 mixed nor+sdraw OTA target、SDMMC RAW-only 与 BOOT_OTA_SKIP_UNCHANGED 开关
    - libuboot_api.a 拆库、arcs_evb 接入 display OTA UI
    - 优化 boot 占用 SRAM，引入 boot_info_store 抽象层与 flash 默认后端
  - runtime_ops 解耦:
    - lisa_sdmmc 和 components/adb 改用 runtime_ops，消除对 boot_watchdog/uboot 的反向依赖
    - 删除过时 boot_watchdog 并下沉 uboot 资源
    - lisa_flash 移除对 ARCS_HAL_IPC 的强制依赖
  - hal:
    - wifi_bt 库多版本滚动更新到 20260430
    - 修复 IPC print bounds 与 sdmmc erase 问题
  - modem/4G:
    - 优化 ml307 4G 传输速率与默认传输编码模式
    - 规整 modem 速率测试示例命名与文档
  - drivers:
    - lisa_pwm 支持模式极性配置
    - cherryusb/adb 接入 boot recovery handshake 校验
  - soc/dual_core: 双核 WiFi 内存布局抽到 Kconfig，跨核地址访问加固
  - soc/arcs/hal: urpc 线程 stack 支持 Kconfig 配置
  - ci:
    - AI review 从 Anthropic Messages API 迁移到 OpenAI Responses API
    - 大 MR 自动切换概要审查模式并跳过 modules/ 第三方库
    - submodule 缓存清理 stale lock，AI review 切换至 docker_codex_cli runner
  - modules/mbedtls: 优化 socket 适配
  - docs:
    - 前端文档站接入 AI 助手
    - 补充 CV/XTTS/Translation/Tuner/WSP 算法组件文档
    - cAT 示例提取为独立索引页，规整示例目录与标题

### Fixed:
  - adb:
    - 修复 task 自删导致的静态 TCB use-after-free
    - NAND/SDMMC raw 慢 IO 路径补 yield 喂狗
    - 忽略 USB 零长度包避免虚假 malloc error
    - shell_task stack 提升至 8 KB
    - ADB sync ext disk policy 改为显式 ops 注册
    - 改用公共 boot_flash.h 头文件
  - sdmmc: 修复 arcs_evb 无 TF 卡时误触发 boot recovery
  - soc: soc_pre_init 阶段清除 ROM code 遗留的 GPIO 中断使能
  - lwip: 适配 hal 2.2.1 socket 接口
  - 4G/ml307: 修复文本模式下数据解析异常
  - bt_audio: 改进 lifecycle 处理
  - acomp/xtts: do_cleanup 释放 IPC stream channel
  - tinyusb: 修复导出错误的包含目录导致 letter_shell 误用 uboot 配置头
  - drivers/lisa_camera: 修正 tc6036 VGA 窗口配置与 UVC 格式设置时序
  - samples:
    - mqtt sample 与 wifi_pm demo 移除硬编码 WiFi 凭据
    - recovery_basic 默认 app 地址对齐到 0x40000
    - flash OTA 包大小默认更新为 0xFE1C
  - ci/ai-review: 流式请求 UTF-8、429 重试、错误日志增强，移除推理痕迹泄漏

### Added:
  - 语言绑定:
    - lua: 新增 Lua 解释器接入
      - 提供 shell 交互式 REPL
      - 支持 USB MSC 导入并运行 Lua 脚本
      - 自动生成 LISA driver FFI bindings，启用 GPIOA/GPIOB/I2C
      - 新增 LVGL display/touch binding 工程
      - 提供 blink.lua 示例与 lisa.msleep helper
    - zig: 新增实验性 Zig 语言支持
      - 完整 Zig 工具链与 sample adapter 接入
      - adapter 作为 modules/zig 子模块独立维护
      - CI 切换到 Zig-enabled Ubuntu runner，兼容 Zig 0.13
      - 配套实验性 Zig 文档与 sample 验证规范
  - lisa_pm: 新增低功耗管理框架
    - 应用级 sleep callback 与 API 收敛
    - ILM/DLM snapshot 与 PSRAM NOLOAD 段管理
    - 设备侧 wakeup-source API
    - 各 lisa 驱动接入 system PM 三回调
    - console flush 链路与 vrtc_init 初始化内化
    - 新增 wifi_ps 示例（basic/mqtt/gpio_wakeup 三子工程）与设计文档
  - samples:
    - demo: 新增 NES 游戏 demo，支持 LVGL 显示、LISA_AUDIO、SD 卡 ROM 加载和 USB Host 键盘输入
    - network: 新增 4G 速率测试示例
    - drivers: 新增 lisa_pwm 对齐模式示例
    - bluetooth: 新增 AVRCP key 日志，a2dp_source NVS 初始化，wifi+a2dp coex threads 命令
    - subsys/cae: 新增 CAE UAC 验证示例
  - drivers:
    - 新增 GC9307 显示驱动和 CHSC6540 触摸驱动
    - lisa_camera 支持 dvp+i2c 双摄切换
    - lite_adc 新增 MIC1 增益配置
  - acomp: 新增 CAE 组件
  - system:
    - 新增 sys_chip_id_get 跨芯片 API
    - 新增 sys_reset_reason 跨芯片 API
    - PMU 接管 SYSRST_STATUS snapshot，应用层可读复位原因
  - bluetooth:
    - 新增 BLE netcfg 自定义操作处理与数据发送
    - 新增 classic shell connect 命令
    - 新增 playback drain API
  - coex: 支持运行时 wifi 与 iperf 参数配置

### Deprecated:

## [0.1.6] - 2026-04-22:

- All changes since 0.1.5

### Changed:
  - env/build:
    - 增加 macOS 开发环境和 cskburn 支持
    - 完善 boot 构建与 app-only watchdog 接入
  - system/boot:
    - boot 控制存储改为寄存器+Flash 混合方案
    - OTA 触发和 config.json 流程同步更新
  - samples:
    - 优化 USB 与网络示例的内存配置
    - 调整双屏相关示例的目录组织和运行方式
    - 更新 uboot 相关示例的重启与触发流程
  - soc: 更新 dual-core scanpen 默认 SRAM 内存布局
  - modules: 更新 micro-rtsp-c，完善 RTSP 相关资源释放与传输时序

### Fixed:
  - https: 修复 https 交互异常
  - sdmmc: 恢复 app 侧稳定初始化时序
  - udp: 增加 UDP tx copy 配置，修复发送数据异常
  - fs: 修复 lsfs fat mkfs 栈溢出
  - audio/bluetooth:
    - 修复 bt_audio_session 和 hfp_source 相关稳定性问题
    - 对齐蓝牙接口与 lisa_audio echo 行为
  - runtime/log:
    - 修复 rtc 语义不一致问题
    - 修复 printk 栈溢出和 easylogger 输出交织
    - 修复 lisa_websocket 分片处理问题
  - camera/display:
    - 修复 camera/dvp 运行时重配置问题
    - 修复双屏 SPI 挂载与总线参数问题
    - 补齐 NV3030B 默认面板支持
  - samples:
    - 修复 wake_up 示例不可运行问题
    - 修复 boot-only 宏和 lisa_net 绑定同步问题

### Added:
  - adb/boot-adb:
    - 新增 CherryUSB recovery 后端和 raw flash 传输能力
    - 新增 sync metadata、app class 和 device 示例
    - 完善重连、死锁、FATFS push 与校验链路稳定性
  - display:
    - 新增多显示实例支持
    - 新增双屏独立旋转能力
    - 新增全 panel custom init_params 支持
    - 补齐 dual display 示例
  - modem:
    - 重构 modem 示例
    - 新增 USB AT 后端
    - 完善默认 transport 配置
  - acomp:
    - 新增 tuner 组件和示例
    - 支持 wakeup/fd 算法从 SD 卡加载资源
    - 完善相关释放与停止流程
  - samples:
    - network:
      - 新增 wifi_pm 示例
      - 新增 wifi A2DP iperf 共存示例
    - adb:
      - 新增 adb push benchmark 示例
      - 新增 recovery boot standalone 示例
    - ota:
      - 新增 app-only flash/tf trigger 示例
      - 新增 flash real-flow OTA 示例
    - display:
      - 新增 lvgl8 dual widgets 示例
      - 新增 lvgl8 dual benchmark 和 multiple displays 示例
  - system: 新增 sys_reboot，支持 soft 和 hard 重启
  - coex: 新增 coex_slot_time shell 命令

### Deprecated:

## [0.1.5] - 2026-03-31:

- All changes since 0.1.4

### Changed:
  - boot: 使用 uboot 替代 system/boot
  - acomp: 使用 lisa_mem 替代 plat_os 内存函数
  - acomp: 提取共享资源管理器，xtts/translation 延迟 prepare 和资源加载
  - hal: 更新 wifi 到 20260326 版本，修复扫描时偶现崩溃；更新至 20260316 版本
  - hal: 更新 bt 库到 20260324 版本；更新 bt/ble/cli/atcmd 到 20260318 版本
  - build: 集中根目录构建流程和 CI 辅助脚本
  - modules/cAT: 更新 cAT 模块，修复 process task 在非 IDLE 状态下的空转问题
  - sntp: 使用 closesocket 替代 close 接口，兼容 4G 模块
  - chryusb: 更新子模块以支持 USB Host

### Fixed:
  - bt_audio:
    - 设置 SCO disconnect reason 用于 HFP stop 回调
    - 上行 PCM underrun 时填充静音而非跳过，防止蓝牙断连
  - lis_algo: OCR 模型加载策略强制设置 boot type 为 1
  - lvgl8: 更新子模块，修复触摸坐标反转字段名错误
  - i2c: 用独立信号量替代 task notification 避免假超时
  - i2s: 移除 TX_FIFO_EMPTY 的误判处理，仅响应真正的 underrun
  - ipc: 修复 ipc print auto init
  - boot: 修复启动时钟初始化不完整的问题
  - hal: 更新 arcs-hal 包含 DMA2D 修复；修复编译失败问题
  - uart: 修复串口接收大数据异常问题
  - coex: 修复 wifi_ble_net_cfg 崩溃
  - wifi: 修复 IPC wifi 连接失败
  - wifi_manager: 连接后立即断开视为失败
  - bt_sink: 支持 NULL audio interface 并修复 bt_audio_types.h 包含路径
  - lisa_modem: MDNSGIP 响应优先使用 IPv4 地址，避免 IPv6 解析失败
  - bluetooth: 启用 HCI transport 修复 a2dp_source 静音播放
  - acomp/cv: 添加 cv_handle 和 message 的 NULL 检查
  - adb:
    - 修复 shell task 被强杀导致 adb_msg_send_lock 死锁
    - 修复 local_id 溢出后可能冲突的问题
    - 修复 shell 重复打开时旧 session 未正确清理
    - 修正 __builtin_expect 的期望值方向
    - 移除未使用的 TX 队列和任务，释放资源
    - 修复 USB 断开重连时未清理活跃服务导致卡死
  - vaddr_remap: 支持不依赖 FreeRTOS 的最小 loader
  - env: 修复新环境工具链安装路径和查找逻辑；移除错误的 python3 fallback
  - translation: 稳定重复运行和方向资源
  - xtts: 稳定重复播放生命周期
  - samples: 统一多个示例的 CI 入口和 guardian pattern

### Added:
  - acomp_xtts: 支持多角色
  - display: 适配 NV3030B 面板驱动到新 lisa_display 接口
  - bluetooth:
    - 添加 A2DP/HFP profile 连接回调机制
    - 添加 BT Classic 连接/断连/AVRCP 回调机制
  - app_player: 支持运行时 PCM 输出回调（app_player_init）
  - drivers/lisa_audio: 添加软件回采(soft echo)支持
  - adb:
    - 添加 early boot 日志缓存
    - 解析 host 端 feature 协商信息
    - 添加 reboot 服务支持 adb reboot/recovery
  - ble: 解耦 AT_CMD 依赖，更新 lisa_modem
  - vaddr_remap: 新增 HAL 驱动和验证示例/测试
  - samples:
    - 新增 iperf3 吞吐量测试和上下行测试
    - 新增 spv/translation/xtts 算法示例
    - 新增 ec801e USB Host ECM 示例
    - 新增 cherryusb video/serial/audio host 示例
    - 新增 dual-core IPC log 示例
    - 更新 face_detect 版本和 classic audio 示例
  - docs:
    - 新增系统启动流程文档和系统架构章节
    - 新增硬件支持章节，整合 SoC 规格与板型文档
    - 修复 rst inline literal 语法错误
  - ci:
    - AI review 添加重试机制和备用 TOKEN 支持
    - 添加 MR pipeline 状态检查
    - 新增 GitLab MR review follow-up workflow

### Deprecated:


## [0.1.4] - 2026-03-13

- All changes since 0.1.3

### Added:
  - bluetooth:
    - 增加 BLE 协议栈初始化完成回调机制
    - 增加 BLE connected/disconnected 回调
    - 增加 BLE pairing callback 验证示例
    - 增加 bond indication 和 key request 回调
    - 支持自定义 BLE 广播数据
    - 支持 IPC 双核 BLE 配网
    - 支持 BT Source（A2DP/HFP）
    - 增加 classic/a2dp_sink 和 classic/hfp_source 示例
  - components/lisa_bt_audio_framework: 新增蓝牙音频框架模块及示例
  - components/bt_source: 增加音量设置接口
  - drivers/lisa_camera: 新增 TC6036 sensor 支持
  - drivers/lisa_flash: 支持双 FLASH
  - drivers/lisa_audio: 支持 CONFIG 配置选择 MIC0/MIC1 引脚初始化
  - build: 更新 cmake 子仓库，添加 SLOT-based linker injection API
  - ci:
    - 添加 AI 代码审查到 MR 流水线
    - 添加 Kconfig 格式规范 CI 检查
    - 添加分支新鲜度检查，确保 MR 分支与目标分支保持同步
  - env: 新增 env.sh 一键开发环境配置脚本，支持工具链检测安装、子模块同步、环境变量设置
  - linker:
    - MEMORY 区域地址重叠构建时检测
    - 支持板级 linker 片段目录
  - multi-soc: 完成多 SoC 平台改造（Board→SoC Kconfig 绑定、驱动平台守卫、CHIP 参数化）
  - usb: CherryUSB 添加 Kconfig 支持
  - samples:
    - 新增 CherryUSB Host Video (UVC Bulk) 示例，支持 MJPEG 解码 LCD 显示
    - 新增 CherryUSB Host Serial 示例
    - 新增 WiFi + BLE 单核/双核配网示例
    - 新增 WiFi + HTTPDNS 示例
    - 新增链接脚本特性示例（code_relocate/section_attribute/custom_section/app_registry）

### Changed:
  - build:
    - 恢复 ARCS_BASE 自动查找，支持 SDK 作为子目录使用
    - 同步 build.sh 到所有项目目录
    - Kconfig 警告检查改为 cmake 变量控制
  - linker:
    - system.ld 标准化重构，段定义外迁至各组件片段（WiFi/BLE/IPC/Shell/LUNA/C++ runtime/LVGL 等）
    - system.ld 提升至 startup/common/ 作为通用模板
    - HEAP_SIZE 从硬编码改为 Kconfig 参数化
    - PSRAM/ILM/DLM/ITCM/DTCM 段添加条件编译守卫
    - 移除 flash 驱动 EXCLUDE_FILE 和 mapi.o 硬编码
  - soc:
    - 统一内存 Kconfig 定义，拆分 LUNA 共享/专属内存配置
    - 对齐 Kconfig 内存默认值与 memap.h，支持 AP/CP 条件配置
    - SOC_ARCS 定义移至父仓库，避免变更 HAL 子仓库
    - memory Kconfig 从 startup 迁移到 soc/Kconfig
    - SoC Kconfig 配置下沉至对应 soc 路径
    - SoC linker 逻辑下沉至芯片目录
    - 移除 MEM_WFRAM_ISOLATED 配置，固定 WiFi RAM 隔离模式
    - 将 APRAM/LUNA 内存区域注册到链接脚本
  - device: 为 lisa_device 添加 init_level 分级初始化机制，所有驱动适配新增 init_level 参数
  - log: 日志后端重构，移除 sys_uart 后端，新增 console 后端
  - startup: 将 startup 代码拆分为 soc/arcs/startup 和 system
  - system: 系统基础设施模块从 components 迁移至 system
  - kconfig: 全面规范化 Kconfig 文件格式
  - samples: 重组 samples 子目录结构（modules 拆分为 media/subsys/security/libraries，BLE 示例归入 peripheral）
  - docs:
    - 组件文档重组为系统/服务/网络/媒体/算法五分类
    - 一级目录整理，散落指南归入 build_and_debug/
    - 统一 107 个示例文档结构，符合 Samples_Spec.md 规范
    - 新增构建系统指南和链接脚本指南文档
  - drivers: 移除驱动 Kconfig 中 SOC_HAS_* 能力守卫
  - ci: 用 !reference 消除重复脚本，doc 镜像升级至 0.4.0
  - lisa_audio: 修改 gpdma 为 Kconfig 配置
  - lisa_bluetooth: 解耦 BLE netcfg 与 WiFi 依赖
  - lisa_kv: 调整 kconfig 依赖
  - cst816d: 优化触摸报点
  - lvgl7: 默认初始化关闭屏显和亮度设置
  - 整理根目录，辅助脚本和文档下沉至合适位置

### Fixed:
  - build:
    - 修复 Makefile 生成器下传递 Ninja 特有参数的问题
    - 修复 auto-sync-build.sh 路径错误和健壮性问题
    - 统一 cmake_minimum_required 版本为 3.19
    - 修复 rebase 后构建失败的三个问题
  - soc:
    - 修正 BTRAM size 及 WiFi RAM 内存布局
    - WFRAM_SIZE=0 时跳过 WiFi RAM 链接脚本片段
    - 将 PSRAM DCache invalidate 移出 CONFIG_PSRAM_INIT 宏，修复堆分配失败
  - kconfig:
    - 修复所有 Kconfig 警告以兼容 kconfig 工具 -W 参数
    - 还原 lisa_audio 被误删的 ECHO DMA 通道配置
    - 还原 lisa_bluetooth 被误删的配置项
  - startup: 串口初始化移至 soc_init 之后，修复 CONFIG_CLOCK_INIT 下波特率异常
  - console: 将 console_mutex 改为递归锁，修复日志 flush 死锁
  - log: 对接 logDbg 到 easylogger 输出
  - lisa_device: 将 lisa_device_init 初始化级别调整为 PRE_KERNEL
  - bluetooth: 修复 classic a2dp 初始化崩溃，优化 BT Source 连接顺序与播放状态同步
  - samples:
    - 适配 Kconfig 内存布局变更，修复 10 个示例构建失败
    - cherryusb_video 修复从 Hub 拔出摄像头后的 URB 死循环
  - lisa_wdt: 修复中断回调错误和 sample 重复输出无用日志
  - samples/lvgl: 修复开机显示花屏问题
  - ci: branch-freshness 修复 shallow clone 下误报
  - docs: 修复文档 toctree 告警、修正 build_and_debug 文档与 SDK 实现不一致之处

### Deprecated:


## [0.1.3] - 2026-02-09

- All changes since 0.1.2

### Added:
  - components/app_player: 新增焦点状态查询接口、同步恢复接口
  - components/lisa_modem & lisa_net: 添加调制解调器和网络抽象层组件
  - drivers/lisa_audio: audio_ioctl支持LISA_AUDIO_IOCTL_PLAY_GET_STATUS
  - drivers/lisa_display: 支持面板初始化参数配置
  - drivers/lisa_i2s: 新增I2S设备驱动
  - drivers/lisa_touch: 新增read_chip_id功能
  - feat: lwip支持httpdns解析
  - feat: 支持segger rtt为日志后端
  - feat: websocket线程优先级支持通过kconfig配置
  - feat: 支持litedac音量实时调节
  - modules: 新增cherry usb模块和micro-rtsp-c模块
  - samples: 新增lisa_i2s驱动示例、micro-rtsp-c视频流示例及相关组件示例

### Changed:
  - components/acomp/wakeup: 添加模式切换的算法服务通知事件
  - components/app_player:
    - 统一接口为同步方式，移除_sync后缀
    - 移除IGNORE焦点丢失策略
    - 移除tone组件
    - 调整日志等级
    - 优化PA控制逻辑和暂停缓存URL
  - drivers/lisa_audio: 使用lisa gpio代替标准gpio api
  - drivers/lisa_flash: 优化读取效率，使用memcpy代替flash_read接口
  - feat: 优化crash时重启的逻辑
  - feat: 修改mic偏置电压为1v9
  - wifi: 升级wifi库至20260128版本
  - wifi_manager: 更新WiFi Manager，修复断连未报告reason code、连接未禁用自动连接等问题

### Fixed:
  - components/acomp: 修复发送同步消息时的线程安全问题
  - components/app_player:
    - 修复异常超时卡住的问题
    - 修复切换URL时停止播放器不释放焦点
    - 修复焦点管理导致多播放器同时播放问题
    - 修复重复播放导致的崩溃问题
    - 修复音频焦点标志残留导致的状态异常
    - 修复无效状态下reset导致卡死问题
    - 修复流式播放的prepare和play时序问题
    - 修复焦点并发测试配置污染问题
  - components/lisa_websocket: 修复double free内存的问题
  - drivers/lisa_audio:
    - 防止播放启动后重新配置
    - 修复8K采样率的OSR配置
    - 修复回采音频数据丢弃错误的问题
  - drivers/lisa_i2s: 修复回调函数声明错误的bug
  - drivers/lisa_pwm: 修复输出频率和设置不一致的问题
  - drivers/lisa_thread: 修复lisa_thread_delete未释放task句柄导致的内存泄漏问题
  - fix(build): 修复httpdns编译问题、sample编译异常、优化CMakelists
  - samples/algorithms/face_detect: 修复示例文档错误、优化demo、提高虚警阈值、修复卡死bug
  - docs: 修复文档warning、添加cskburn工具位置说明、减轻视频轮播黑屏

### Deprecated:


## [0.1.2] - 2026-01-15

- All changes since 0.1.1

### Added:
  - algorithms/face_detect: 新增人脸识别算法组件和示例
  - boards: 新增rgb pinmux适配
  - components/app_player:
    - 支持流式播放
    - 支持焦点管理
    - 添加单实例线程安全
    - 支持文件系统音频播放
  - components/cAT: 新增AT指令解析器模块及示例（basic/demo/unsolicited）
  - components/coreMQTT: 新增MQTT客户端库及多种示例（TCP/SSL/WebSocket/WSS/Agent）
  - components/quirc: 适配QRCode识别库
  - components/libjpeg-turbo: 适配libjpeg-turbo库
  - docs/tools: 新增LISA Pinmux Tool使用文档、cskburn烧录工具和Tone音频打包工具文档
  - docs/get_started: 新增环境变量必须使用绝对路径的警告说明
  - docs: 为示例文档自动添加源码位置链接、新增问题反馈入口
  - drivers/lisa_camera: 支持set_reg和get_reg接口
  - samples/algorithms/face_detect: 新增人脸识别算法组件示例
  - samples/demo/face_detect: 新增人脸识别演示demo
  - samples/network: 新增MQTT相关示例（TCP/SSL/WebSocket/Agent）
  - samples/wifi_ble_coex: 新增WiFi蓝牙共存单核示例
  - wifi: 支持WiFi快连功能

### Changed:
  - algorithms: 算法组件prepare接口支持传入资源地址
  - app_player: 调整app_player_play_opt_t配置，移除throw_low_energy字段
  - drivers/gc0328: RGB565格式默认为小尾端
  - lisa_wifi: 在lisa_wifi任务中分发done回调
  - modules/lvgl8: 更新lvgl8子模块
  - startup/arcs/backtrace: 优先输出backtrace信息，避免二次异常
  - wifi_manager: 更新WiFi Manager，修复断连时未报告reason code、连接时未禁用自动连接等问题
  - wifi:
    - 更新WiFi库至20251229版本
    - 重定向wifi内部ls_read_temp_voltage函数实现

### Fixed:
  - components/lisa_audio: 修复音频覆盖及回采帧同步问题
  - components/lisa_log: 使用正确的API操作递归互斥锁
  - drivers/lisa_camera: 停止和初始化时重置帧缓冲队列
  - drivers/lisa_flash: 修复边界检测
  - drivers/lisa_gpio: 修复中断被重复触发的问题
  - drivers/lisa_pwm: 修复输出频率和设置不一致的问题
  - drivers/lisa_uart: 防止传输过程中重新配置
  - gc0328: 寄存器设置后添加延时
  - system: 修复系统使用异步日志时崩溃信息无法输出的问题
  - test: 修复DMA测试用例和efuse测试错误

### Deprecated:


## [0.1.1] - 2025-12-25:

- All changes since 0.1.0

### Changed:
  - drivers/lisa_spi: 重构SPI驱动API,简化DMA配置流程
    - 移除 `LISA_SPI_AUTO_TRANSFER` 传输模式
    - 重命名 `LISA_SPI_PIO_TRANSFER` 为 `LISA_SPI_INTERRUPT_TRANSFER`
    - 移除 `lisa_spi_configure_dma()` 接口,DMA配置合并到 `lisa_spi_configure()` 中
    - 移除 `lisa_spi_unreserve_dma_channel()` 接口,DMA通道由驱动自动管理
    - 移除配置结构体中的 `tx_dma_priority` 和 `rx_dma_priority` 字段
    - DMA通道限制0~3
  - drivers/lisa_rtc: 移除 `lisa_rtc_alarm_t` 中 `enabled` 字段,由 `lisa_rtc_enable_alarm` 统一管理

### Fixed:
  - drivers/lisa_audio:
    - 修复未使能CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE情况下编译错误
    - 修复录音增益调节声道配置错误
  - drivers/lisa_camera: 修复缓存不可用时丢帧问题
  - drivers/lisa_i2c: 修复时钟非法参数问题
  - drivers/lisa_pwm: 修复输出极性设置失败和多次配置问题
  - drivers/lisa_adc: 修复偶现读取数据残留问题
  - drivers/lisa_uart:
    - 修复缓存模式下poll_in接口支持问题
    - 修复DMA非法通道配置未报错
    - 修复多级缓存模式下接收丢失数据问题
  - drivers/lisa_gpio:
    - 修复输入模式下写入操作未返回错误码
    - 修复无效配置检查
    - 修复无法读取debounce状态
  - drivers/flash: 更新flash驱动初始化参数
  - components/fs:
    - 修复fatfs_statvfs中获取文件系统bsize错误
    - 修复sqlite3_open_v2支持create并优化sample
  - components/wifi: 修复IPC模式下wifi崩溃问题
  - samples:部分示例文档整理

### Added:
  - drivers/lisa_spi: 新增DMA通道合法性检查
  - drivers/lisa_rtc: 增加参数合法性检查
  - drivers/lisa_display:
    - 新增ST7701S面板驱动
    - 支持RGB并行总线和软件SPI命令总线
    - 新增背光极性配置
  - drivers/lisa_rgb: 新增lisa_rgb设备驱动
  - drivers/lisa_camera: 支持可配置DVP频率和帧格式
  - drivers/lisa_audio: 支持mic偏置电压配置选择
  - components/app_player: 新增app_player组件,支持本地和网络音频播放、本地提示音播放
  - components/work_queue: 新增work queue组件
  - components/wifi: 更新WiFi库至20251211版本
  - components/bluetooth: 20251211 BT更新
  - samples/app_player: 新增本地音频和网络音频播放示例及单元测试
  - samples/algorithms: 新增单麦唤醒算法示例
  - samples/usb_camera: 新增USB摄像头示例及文档
  - samples/rgb_bounce_buffer: 新增RGBBounce Buffer示例
  - samples/modules: 新增cjson/collections-c/flexlayout/freetype/giflib/jpeg/mbedtls/mbedtls示例文档说明
  - tools: 提供tone打包工具

### Deprecated:


## [0.1.0] - 2025-12-09:

- All changes since 0.0.22

### Changed:
  - 调整SDK目录结构，移除arcs-base
  - cmake：调整构建脚本build.sh，构建命令需指定板型
  - components: 移除display/touch/camera/flash/lisa_evs组件

### Fixed:
  - components/lisa_websocket: 重构websocket组件，解决内部依赖问题
  - samples/network: 修复网络相关示例

### Added:
  - drivers: 新增设备驱动（UART/SPI/I2C/GPADC/PWM/GPIO/RTC/FLASH/SDMMC/WDT/HWTIMER/DISPLAY/TOUCH/CAMERA/QSPI_LCD/AUDIO/DVP）
  - samples/drivers: 新增设备驱动示例
  - samples/bluetooth: 新增蓝牙广播和GATT服务示例
  - samples/algorithms: 新增唤醒算法示例
  - components/acomp: 新增唤醒算法组件
  - components/lisa_evt_pub: 新增事件发布组件
  - components/lisa_shell: 新增shell组件
  - components/lisa_wifi: 新增wifi组件
  - components/lisa_bluetooth: 新增蓝牙组件
  - components/lisa_sntp: 新增sntp组件
  - boards: 新增板型支持，内置evb/mini板型
  - docs: 首次部署在线文档并完善部分组件和示例文档

### Deprecated:
  - samples/drivers: hal驱动示例不做维护，建议使用新的设备驱动

