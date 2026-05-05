# Change Log

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
