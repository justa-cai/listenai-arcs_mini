# WiFi 低功耗 Demo (wifi_pm)

基于 FreeRTOS 的 WiFi 低功耗 + MQTT 通信示例，运行在 ARCS 双核 AMP 平台上。
核心目标：在保持 MQTT 长连接的同时尽可能降低系统功耗。

## 工程结构

```
demos/wifi_pm/
├── boot/                        # Bootloader 阶段
│   ├── src/
│   │   ├── main.c              # Boot 入口，负责 PM 唤醒判断与 CP 核启动
│   │   ├── boot.S              # RISC-V 汇编启动代码，含唤醒检查入口
│   │   ├── linker.ld           # Boot 链接脚本
│   │   └── CMakeLists.txt
│   ├── CMakeLists.txt
│   ├── Kconfig / Kconfig.boot  # Boot 配置选项
│   └── prj.conf                # Boot 项目配置
├── src/                         # 主应用
│   ├── main.c                  # 应用入口 (WiFi + MQTT + PM 全部逻辑)
│   ├── fs/
│   │   ├── user_fs.c           # 文件系统初始化 (当前未启用)
│   │   └── user_fs.h
│   └── CMakeLists.txt
├── docs/                        # 技术文档
│   ├── architecture.md         # 整体架构与双核 AMP 设计
│   ├── power-management.md     # 低功耗技术方案 (睡眠/唤醒/保活)
│   ├── pm-api-reference.md     # PM/WiFi PS 全部接口参考
│   ├── mqtt.md                 # MQTT 交互与消息协议
│   ├── wifi-reconnect.md       # WiFi 重连机制
│   └── build-and-config.md     # 构建系统与配置说明
├── CMakeLists.txt              # 主工程 CMake
├── Kconfig                     # 主 Kconfig
├── prj.conf                    # 主项目配置 (PM/WiFi/MQTT 等开关)
├── memap.h                     # 内存地址映射定义
├── system.ld                   # 主链接脚本 (含 PM 专属段)
├── boot.cmake                  # Boot 集成脚本 (合并 boot.bin + app.bin)
├── build.sh                    # 构建脚本
└── sample.yaml                 # 测试配置
```

## 快速开始

### 使用前配置

修改 `src/main.c` 中的连接参数：

```c
#define TARGET_WIFI_SSID   "你的WiFi名称"
#define TARGET_WIFI_PWD    "你的WiFi密码"
#define MQTT_BROKER_HOST   "你的MQTT Broker地址"
#define MQTT_BROKER_PORT   1883
#define MQTT_CLIENT_ID     "你的客户端ID"
```

### 构建

```bash
./build.sh          # 常规构建
./build.sh -p       # 清理后构建
./build.sh -m       # 打开 menuconfig
```

### 功能开关

```c
#define LOW_POWER_ENABLED   1    // 1=启用低功耗, 0=禁用
#define MQTT_ENABLED        1    // 1=启用MQTT, 0=仅WiFi连接
```

## 文档索引

| 文档 | 内容 |
|------|------|
| [整体架构](docs/architecture.md) | 双核 AMP 架构、内存布局、任务模型 |
| [低功耗技术方案](docs/power-management.md) | 两层功耗控制、睡眠/唤醒流程、Boot CP 复位、时间补偿、连接保活 |
| [PM 接口参考](docs/pm-api-reference.md) | 全部 PM/WiFi PS 接口、枚举、结构体、宏定义、外设回调 |
| [MQTT 交互](docs/mqtt.md) | 连接参数、消息协议、生命周期管理 |
| [WiFi 重连机制](docs/wifi-reconnect.md) | 三阶段重连、事件驱动、同步保护 |
| [构建与配置](docs/build-and-config.md) | 构建系统、prj.conf 配置项、链接脚本 |

## 设计要点

| 要点 | 实现方式 |
|------|---------|
| 触发方式 | 重写 FreeRTOS `vPortSuppressTicksAndSleep()`，空闲时自动进入 |
| 睡眠深度 | PMU MODE2 + WFI = CPU 核心域真正断电，非简单时钟门控 |
| 快速唤醒 | AON 寄存器存储跳转地址，ROM 直接跳到 RAM 入口，绕过完整引导 |
| 透明恢复 | 汇编级寄���器 + C 级外设寄存器保存/恢复 + VRTC 时间补偿，应用层无感 |
| 连接保持 | WiFi DTIM PS + ARP Keep-Alive + MQTT Keep-Alive 三层保活 |
| 状态守护 | 断连 → 退出低功耗 → 三阶段重连 → 成功后重新进入低功耗 |
