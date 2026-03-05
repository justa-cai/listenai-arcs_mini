# ARCS 语音解决方案

基于 ARCS SDK 的语音交互解决方案，支持多项目的语音解决方案定制功能。

## 项目概述

本项目是一个完整的语音助手解决方案，集成了语音唤醒、语音识别、云端交互、本地播放等功能。

### 主要特性

- 语音唤醒和识别
- 云端语音服务对接
- 本地音频播放
- WiFi/BLE 网络连接

## 环境搭建

参考SDK的开发文档：
- [开发文档（在线）](https://docs2.listenai.com/arcs-sdk/v0.1.0/zh/html/get_started.html)

## 工程结构

### 顶层目录

```
voiceassistant/
├── apps/              # 应用方案工程集合
├── arcs-sdk/          # ARCS SDK 核心框架
├── modules/           # 项目级第三方模块
├── res/               # 资源文件（固件、配置、音频、图片等）
├── src/               # 语音交互核心代码
├── tools/             # 开发工具和脚本
├── yaml/              # 配置文件
└── build.sh           # 工程构建脚本
```

### 详细目录说明

#### [apps/](apps/) - 应用程序

包含多个应用方案项目,详细说明请参考对应工程下的README.MD（如有）。

#### [src/](src/) - 语音交互核心代码

包含语音助手的核心业务逻辑、中间件和工具库：

```
src/
├── category/          # 品类相关代码
│   ├── comm/          # 品类通用代码
│   ├── evb/           # EVB 品类实现
│   └── yuba/          # YUBA 品类实现
├── framework/         # 核心框架
│   └── voice_msg.*    # 语音消息处理
├── middleware/        # 中间件层
├── server/            # 语音服务
├── shell/             # 调试命令行
│   └── cmd/           # Shell 命令实现
├── system/            # 系统服务
│   ├── platform.c     # 平台抽象层
│   └── remote_logger.*# 远程日志
└── utils/             # 工具库
```

**核心模块说明：**

- **category**: 不同产品品类的特定实现，封装品类差异
  - **comm**: 品类间通用代码和共享组件
- **framework**: 核心框架，提供语音消息传递机制
- **middleware**: 提供硬件抽象和系统服务，供应用层调用
- **server**: 语音交互核心逻辑，包括云端通信和唤醒处理
- **shell**: 提供调试命令行接口，用于开发调试
- **system**: 系统级服务，包括平台抽象和远程日志
- **utils**: 通用工具库，提供事件、配置、编解码等基础功能


## 快速开始

### 编译工程

基本编译命令,这里以/apps/evb-arcs项目为例,用户根据自己的实际项目路径选择：

```bash
./build.sh -S ./apps/arcs-evb -C
```

编译成功后，固件文件位于 [build/arcs-evb.bin](build/arcs-evb.bin)

### 固件烧录

#### FLASH 分区说明
参考对应app的资源分配置表说明文件

#### 烧录方式

##### 方式一: ROM 升级模式（空板烧录）

适用于首次烧录或 boot 程序损坏的情况。

**进入 ROM 升级模式：**

1. 将开发板的 `boot` 引脚接地
2. 复位开发板
3. 开发板进入 ROM 升级模式

**烧录 boot 程序：**

```bash
# 连接串口 UART0(GPIOA2_RX0/GPIOA3_TX0)
cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 ./res/res_pack_a/boot.bin
```

> **注意：** boot 引脚和烧录串口引脚复用，烧录前需将 boot 引脚悬空

##### 方式二: ADB 烧录（推荐）

适用于已有 boot 程序的开发板，支持快速烧录。

**步骤 1: 进入恢复模式**

```bash
adb shell recovery
```

**步骤 2: 烧录固件和资源**

参考对应app的资源分配置表说明文件,烧录所有分区数据.


**步骤 3: 重启设备**

```bash
adb shell reboot hard
```

## 常见问题

### 编译问题

**Q: 找不到工具链**
```
A: 检查环境变量是否正确设置：
   echo $NUCLEI_TOOLCHAIN_PATH
   echo $LISTENAI_TOOLS_PATH
```

**Q: 编译失败**
```
A: 尝试清理后重新编译：
   ./build.sh -C
```

### 烧录问题

**Q: ADB 连接不上设备**
```
A: 1. 检查 USB 连接
   2. 运行 adb devices 查看设备
   3. 尝试重启 adb: adb kill-server && adb start-server
```

**Q: 烧录后无法启动**
```
A: 1. 检查是否烧录了所有必要分区
   2. 查看串口日志定位问题
   3. 尝试重新烧录 boot.bin
```

## 版本信息

- **当前分支**: master
- **主分支**: master

## 参考文档

- [ARCS SDK 文档](arcs-sdk/README.MD)
- [ebus 模块文档](modules/ebus/readme.md)
- [lschat 模块文档](modules/lschat/README.MD)

## 许可证

本项目遵循相应的开源许可证，详见各模块的 LICENSE 文件。
