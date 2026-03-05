# LISA Flash 双核环境示例

## 📖 概述

本示例演示在双核环境下使用 LISA Flash 驱动，通过启用 `CONFIG_LISA_FLASH_ARCS_HALT_REMOTE_CORE` 配置，
在 Flash 操作期间自动停止远端核心，确保 Flash 访问的安全性和数据完整性。

**核心角色分配：**
- **AP 核（Boot Core）**：启动核心，负责引导 CP 核启动，作为远端核心，在 CP 执行 Flash 操作时被自动停止
- **CP 核（Master）**：运行 Flash 驱动操作，执行擦除/写入/读取，通过 IPC 机制控制 AP 核的停止和恢复

## 🎯 功能特点

- ✅ **双核协同**：AP 核启动并引导 CP 核，双核协同工作
- ✅ **SYS_INIT 框架**：使用 ARCS SYS_INIT 框架管理初始化顺序
- ✅ **自动停止**：Flash 操作时通过 IPC 自动停止 AP 核
- ✅ **自动恢复**：Flash 操作完成后自动恢复 AP 核运行
- ✅ **安全保护**：避免双核同时访问 Flash 导致的冲突
- ✅ **回调机制**：AP 核提供 halt/resume 回调函数监控状态
- ✅ **一键构建**：主工程 CMake 自动编译两个核心的固件

## 🏗 项目结构

```
halt_remote_core/              # 主工程目录
├── src/
│   └── main.c                # CP 核主程序（运行 Flash 操作）
├── remote/                   # AP 核远端工程（Boot Core & Remote Core）
│   ├── src/
│   │   └── main.c           # AP 核主程序（启动 CP 并被停止）
│   ├── CMakeLists.txt       # AP 核 CMake 配置
│   ├── Kconfig
│   ├── prj.conf             # AP 核配置
│   ├── build.sh
│   ├── sample.yaml
│   └── README.md
├── CMakeLists.txt            # 主 CMake 配置（编译两个核心）
├── Kconfig
├── prj.conf                  # CP 核配置
├── build.sh                  # 一键构建脚本
├── sample.yaml
└── README.md                 # 本文件
```

## 📊 预期输出

### CP 核输出（Master - 运行 Flash 操作）

```
========================================
=== LISA Flash Dual-Core Example ===
===      (CP Core - Master)        ===
========================================

This example demonstrates Flash operations in dual-core environment.
CONFIG_LISA_FLASH_ARCS_HALT_REMOTE_CORE is enabled, so the remote
core (AP) will be automatically halted during Flash operations.

1. Getting flash device 'flash0'...
   Flash device 'flash0' is ready and auto-initialized at 0x...

=== Flash Device Information ===
Write block size: 1 bytes
Erase value: 0xFF
...

=== Flash Read/Write Test (Iteration 1) ===
Using page size: 4096 bytes

2. Erasing flash at offset 0x100000, size 4096 bytes...
   [CP Core] Halting remote core (AP) during erase...
   Erase successful
   [CP Core] Remote core (AP) resumed

3. Writing 256 bytes to flash at offset 0x100000...
   [CP Core] Halting remote core (AP) during write...
   Write successful
   [CP Core] Remote core (AP) resumed

4. Reading 256 bytes from flash at offset 0x100000...
   Read successful

5. Verifying data...
   Verification successful! All 256 bytes match.

=== Test Iteration 1 Completed Successfully ===
...
```

### AP 核输出（Remote - 被停止）

```
========================================
=== LISA Flash Dual-Core Example ===
===      (AP Core - Remote)        ===
========================================
Observe the log messages indicating halt and resume events.

[remote_main] CP core booted.
[remote_main] IPC remote initialized.

[remote_main] ipc_utils_before_halt_by_peer_core  # CP 核开始 Flash 操作
[remote_main] ipc_utils_after_resume_by_peer_core # CP 核完成 Flash 操作

[remote_main] ipc_utils_before_halt_by_peer_core
[remote_main] ipc_utils_after_resume_by_peer_core
...
```

## 🔍 工作原理

### 双核协同机制

```
┌─────────────────────────────────────────────────────────────┐
│                        启动流程                               │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  1. AP Core 启动 (Boot Core)                                  │
│     └─> SYS_INIT: boot_cp()      [启动 CP 核]                │
│     └─> SYS_INIT: ipc_init()     [初始化 IPC Slave]          │
│                                                               │
│  2. CP Core 启动                                              │
│     └─> SYS_INIT: ipc_init()     [初始化 IPC Master]         │
│     └─> 执行 main() 进行 Flash 操作                           │
│                                                               │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    Flash 操作时间线                           │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  CP Core (Master):                                            │
│    │─────────────────────────────────────────────            │
│    │  运行      Flash操作     运行                            │
│    │           (擦除/写入)                                    │
│    │               │                                          │
│    │         IPC Halt                                         │
│    │         IPC Resume                                       │
│    ▼               ▼                                          │
│                                                               │
│  AP Core (Remote):                                            │
│    │─────────────────────────────────────────────            │
│    │  运行    [暂停]     运行                                 │
│    │           ▲   ▲                                          │
│    │      before_halt                                         │
│    │      after_resume                                        │
│    ▼                                                          │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```


## 🚀 构建和运行

### 方法 1：使用构建脚本

```bash
cd arcs_sdk/samples/drivers/devices/lisa_flash/halt_remote_core
./build.sh -C -DBOARD=arcs_evb
```

### 烧录固件

```bash
# 烧录 AP 核固件（Boot Core）
# 烧录地址：0x30000000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x00 ./build/remote/remote.bin

# 烧录 CP 核固件
# 烧录地址：0x30080000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x80000 ./build/arcs.bin
```

### 运行

1. 复位设备
2. AP 核从 `0x30000000` 启动，自动引导 CP 核从 `0x30080000` 启动
3. 观察串口输出，查看双核协同工作和 Flash 操作日志


## 🤝 贡献

如有问题或建议，请提交 Issue 或 Pull Request。

## 📄 许可证

Copyright (c) 2025, LISTENAI
SPDX-License-Identifier: Apache-2.0
