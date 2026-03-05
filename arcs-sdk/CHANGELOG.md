# Change Log

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

