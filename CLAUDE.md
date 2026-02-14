# CLAUDE.md

此文件为 Claude Code (claude.ai/code) 在此代码仓库中工作时提供指导。

## 项目概述

ARCS Mini 语音助手 (AIUI) v1.7.0 - 运行在 ListenAI ARCS 双核 RISC-V 平台上的嵌入式语音助手设备。功能包括触摸屏显示、音乐播放、语音交互、通过 MCP (模型上下文协议) 实现的云服务、摄像头照片识别、BLE 配置和闹钟功能。

## 调试分析

**堆栈地址定位:**

当用户提供堆栈跟踪信息时，优先使用 `addr2line` 工具定位具体代码位置：

```bash
# CP 核 (aiui.bin) 堆栈分析
riscv64-unknown-elf-addr2line -e build/aiui -a <地址1> <地址2> <地址3> ...

# 示例：分析 CP 崩溃堆栈
riscv64-unknown-elf-addr2line -e build/aiui -a 307d6dc6 3061d6aa 3061d71e 3062fe28 307329c4
```

**注意事项:**
- 地址通常从 `PC` (程序计数器) 和 `RA` (返回地址) 寄存器获取
- CP 核使用 `build/aiui`
- `0x3` 前缀的地址通常表示 CP 核 (Flash 地址空间)
- `0x2` 前缀的地址通常表示 AP 核 (SRAM 地址空间)

## 构建系统

**主要构建命令:**
```bash
./build.sh                    # 默认构建
./build.sh -t menuconfig     # 使用 menuconfig 配置
./build.sh -C                # 清理并重新构建
./build.sh -r                # 发布构建 (无调试路径)
```

**必需环境:**
- `NUCLEI_TOOLCHAIN_PATH` - RISC-V 工具链
- `LISTENAI_TOOLS_PATH` - ListenAI 开发工具
- `ARCS_BASE` - 自动检测为 `arcs-sdk/` 目录

**构建输出:** `build/aiui.bin` (CP 固件)

**通过 ADB 刷写:**
```bash
adb shell recovery
adb push build/aiui.bin /RAW/NAND/600000
adb shell reboot hard
```

## 架构

### 双核设计
- **AP 核** (应用处理器) - 主应用程序运行于此
- **CP 核** (通信处理器) - WiFi/BT 协议栈
- **IPC** - 进程间通信通过共享内存实现

### 关键目录

| 目录 | 用途 |
|-----------|---------|
| `src/main.c` | 应用程序入口点，系统初始化 |
| `src/audio/` | 音频播放器、PA 管理器、麦克风增益控制 |
| `src/player/` | 音乐播放逻辑、音频焦点管理器、播放模式 |
| `src/cloud/` | 云通信、MCP 框架 |
| `src/cloud/mcp/tools/` | MCP 工具实现 |
| `src/controller/` | 助手控制器 (事件系统) |
| `src/display/` | 显示驱动和 LVGL 集成 |
| `arcs-sdk/` | ListenAI SDK (供应商，请勿修改) |
| `modules/` | 本地第三方模块 (ebus, lisaui) |
| `res/` | 资源 (boot.bin, 配置文件, 提示音) |

### 音频焦点管理

系统使用基于优先级的音频焦点管理器 (`src/player/listen_audiomgr.c`):

| 通道 | 优先级 | 用途 |
|---------|----------|---------|
| AIP | 30 | AI 语音 |
| TTS | 10 | TTS 输出 |
| ALERT | 40 | 提示音/闹钟 |
| CONTENT | 50 | 音乐/内容 |
| LOCAL | 10 | 本地音频 |
| EXTRA | 50 | 扩展通道 |

焦点状态: `FOREGROUND` (活跃), `BACKGROUND` (被压低), `NONE` (停止)

### MCP 框架

模型上下文协议工具在 `src/cloud/mcp/tools/` 中使用 `MCP_REGISTER_TOOL_STATIC()` 宏注册。每个工具:
- 自动生成 JSON Schema
- 实现同步执行
- 与云 AI 服务集成

**可用工具:** `music_random`, `play_control`, `photo_recognition`, `alarm_control`, `volume_control`, `brightness_control`, `led_control`, `show_qrcode`, `show_image`, `kuwo_music`, `exit_skill`

### 内存映射

**SRAM:**
- `0x20000000`: CP WiFi RAM (64KB)
- `0x20040000`: AP SRAM (100KB)
- `0x20050000`: AP SRAM 延续部分
- `0x20070000`: 算法 RAM (256KB)

**Flash:**
- `0x30000000`: 总计 16MB
- `0x30040000`: AP flash (1MB)
- `0x30060000`: CP flash (10MB)

**PSRAM:**
- `0x28000000`: AP PSRAM (8MB)
- `0x28800000`: CP PSRAM (8MB)

### 配置系统

- **配置地址:** `0x300f0000` - 应用配置存储在固定的 flash 位置
- **NVS 地址:** `0xFF8000` (flash 最后 32KB)
- **项目配置:** `prj.conf` - 基于 Kconfig 的构建配置

## 按键事件

| 操作 | 功能 |
|--------|----------|
| 单击 (非主页) | 返回主页 |
| 单击 (主页 + 空闲) | 唤醒语音助手 |
| 单击 (主页 + 监听中) | 进入空闲模式 |
| 双击 | 拍照识别 |
| 三击 | 切换信息页面 |
| 5+ 次点击 | 网络重置 + BLE 配置 |
| 8+ 次点击 | 恢复出厂设置 |
| 长按 (无 USB) | 关机 |

## 编码规范

1. **文件命名:** 小写加下划线 (`audio_player.c`, `app_cloud.c`)
2. **头文件保护:** `__FILENAME_H__` 模式
3. **日志:** 每个文件使用 `#define TAG "filename"` 配合 `LISA_LOGI/T/E/W/D` 进行日志记录
4. **错误处理:** 常见模式使用 `goto cleanup` 进行资源清理
5. **播放器类型:** `PLAYER_T_CLOUD` 用于音乐, `PLAYER_T_TONE` 用于系统提示音
6. **播放模式:** `MODE_ORDER`, `MODE_CYCLE`, `MODE_SINGLE`, `MODE_RANDOM`

## 主要初始化顺序 (main.c → app_task)

1. 硬件初始化 (GPIO, 电源, 看门狗)
2. 外部存储器使能 (PSRAM)
3. 从 `0x300f0000` 解析配置
4. 创建 app_task
5. IPC、WiFi、BLE、音频、显示初始化
6. WiFi 配置检查 → 若无已保存配置则进入 BLE 配置

## 添加新功能

**新增 MCP 工具:** 添加到 `src/cloud/mcp/tools/`，使用 `MCP_REGISTER_TOOL_STATIC()` 注册

**新增控制器事件:** 添加到 `src/controller/assistant_controller.h` 枚举，使用 `assist_controller_trigger_event()` 触发

**新增播放器:** 遵循 `src/player/` 中的模式，集成音频焦点

**UI 更改:** 修改 `src/controller/view/` (LVGL 组件)

## Flash 分区表

| 分区 | 地址 | 描述 |
|-----------|----------|-------------|
| `boot.bin` | 0x0 | 引导加载程序 |
| `ap.bin` | 0x40000 | AP 固件 (预编译固件在 res/ 目录下) |
| `app-config.json` | 0xF0000 | 应用配置 |
| `tone.bin` | 0x100000 | 提示音资源 |
| `wake_word.bin` | 0x200000 | 唤醒词算法 |
| `respak.bin` | 0x400000 | 图片/字体 |
| `aiui.bin` | 0x600000 | CP 固件 |

## 重要常量

```c
#define ARCS_DAC_USE_LITE_DAC    1  // DAC 类型选择
#define CONFIG_PSRAM_HEAP_SIZE   0x400000  // 4MB PSRAM 堆
#define BLE_ADV_START_MAX_RETRIES 5
#define NVDS_FLASH_ADDRESS_OFFSET 0xFF8000  // NVS flash 偏移
```
