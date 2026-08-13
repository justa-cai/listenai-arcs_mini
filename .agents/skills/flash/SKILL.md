---
name: flash
description: 烧录 ARCS 固件到开发板时使用。用户提到 /flash、烧录、刷机、flash、下载固件到设备时触发。
---

# ARCS 固件烧录

## 约定

- 同时适用于 Claude Code 和 Codex；`/xxx` 是触发示例，客户端不支持 slash 命令时用自然语言即可。
- 默认在仓库根目录执行；不要硬编码个人机器路径。
- 烧录是写入操作，执行前先汇总将写入的文件、分区/地址和目标设备。
- 多设备连接时必须明确指定目标设备（ADB serial 或串口），不要猜目标设备。
- 失败时停止后续步骤，保留错误输出再排查。

## 使用方式

`/flash [app名称] [选项]`

## 烧录方式选择

| 场景 | 方式 | 说明 |
| --- | --- | --- |
| **默认（推荐）** | ADB 烧录 | 设备出厂不带串口，ADB 是主流方式；Linux 使用 `adb_download.sh`，Windows 使用 `adb_download.ps1` |
| **仅改 CP / 应用代码** | ADB app 模式 | 使用 `app` 参数只烧录 `build/arcs-mini.bin` |
| **设备死机/进不去 ADB** | cskburn 串口烧录 | 救砖恢复型，需手动进入 ROM Boot 模式；同样执行串口强制闭环 |
| **开发板有串口** | cskburn + 开发专用 Boot | 任何串口烧录都必须临时将 `boot-dev-autostart.bin` 写入 `0x0` |

### 判定"AP 代码是否改动"

按以下规则检查本次会话改动 / `git status` / 待提交 diff（任一命中即视为 AP 改动）：

- 触及 `apps/remote-ap/` 下的源码或配置。
- 触及 `arcs-sdk/` 中 AP 会编入的代码（HAL、驱动、RTOS 等）。
- 改动了 `apps/<app>/build_remote.cmake` 或 AP overlay / 配置。
- 不确定时走整包烧录更稳。

## 跨平台说明

### 串口设备命名

| 平台 | 典型串口路径 | 查找命令 |
| --- | --- | --- |
| **Linux** | `/dev/ttyACM0`, `/dev/ttyUSB0` | `ls /dev/tty*` |
| **macOS** | `/dev/cu.usbmodem*`, `/dev/cu.SLAB_USBtoUART*` | `ls /dev/cu.*` |
| **Windows** | `COM3`, `COM4`, ... | `mode` 或设备管理器 |

### 平台可用性

| 功能 | Linux | macOS | Windows |
| --- | --- | --- | --- |
| ADB 烧录 (`adb_download.sh` / `adb_download.ps1`) | ✅ | ✅ | ✅ (PowerShell) |
| cskburn 串口烧录 | ✅ | ✅ | ✅ |
| picocom (TTY 控制) | ✅ | ✅ | ❌ (仅 WSL 可用) |
| `script` 命令 (伪终端) | ✅ | ✅ | ❌ |

> **Windows 用户**: Git Bash 下没有 `script` 命令，串口日志抓取（run-log）需要通过 WSL 或使用 ADB 方式。Windows 原生 ADB 烧录优先使用 `adb_download.ps1`。

## 流程

### 1. 确定目标 App 与设备

- App 推断规则与 `build` skill 一致（用户指定 → 上下文推断 → 多选询问）。
- 目标设备：`adb devices` 唯一在线时直接用；多设备时让用户选择。
- 串口设备：按上表查找，多设备时让用户选择。

### 2a. 默认：ADB 整包烧录（推荐）

使用仓库根目录的 ADB 烧录脚本，`tools/adb` 只存放内置 ADB 工具。脚本支持单设备和多设备烧录：

```bash
# 默认模式：烧录 ap/tone/wake_word/emoji/respak/app（不烧 boot）
bash adb_download.sh -S res/arcs-mini

# app 模式：只烧录 build/arcs-mini.bin
bash adb_download.sh -S res/arcs-mini app

# 完整模式：额外烧录 boot.bin（首次烧录或 AP 改动后）
bash adb_download.sh -S res/arcs-mini boot
```

```powershell
# Windows 默认模式
powershell -ExecutionPolicy Bypass -File adb_download.ps1 -S res/arcs-mini

# Windows app 模式
powershell -ExecutionPolicy Bypass -File adb_download.ps1 -S res/arcs-mini app

# Windows 完整模式
powershell -ExecutionPolicy Bypass -File adb_download.ps1 -S res/arcs-mini boot
```

开发者按需求选择：

- 只改 CP / 应用代码：用 `app` 参数，只烧录 `build/arcs-mini.bin`。
- 改了静态资源（提示音、唤醒词、表情、respak 等）：用默认模式，烧录静态资源和 app。
- 首次烧录、boot 变更或需要恢复 boot：用 `boot` 参数。

**脚本执行流程**：
1. 检测 ADB 工具（自动适配 WSL 环境）
2. 扫描已连接设备，区分普通模式与 recovery (BOOT-*) 设备
3. 向普通设备发送 recovery 命令使其进入烧录模式
4. 等待设备进入 recovery 并识别 BOOT 序列号；多台相同 BOOT 序列号优先用 `transport_id` 区分
5. 推送固件到各分区
6. 重启设备并汇总结果

**环境变量**（可选覆盖默认值）：

| 变量 | 默认值 | 说明 |
| --- | --- | --- |
| `MAX_DEVICES` | `10` | 最多支持的设备数量 |
| `RECOVERY_TIMEOUT` | `120` | 等待进入 recovery 超时秒数 |
| `POLL_INTERVAL` | `2` | 轮询 recovery 状态的间隔秒数 |
| `ADB_CMD` | 自动检测 | 手动指定 ADB 路径 |
| `BUILD_DIR` | `build` | `arcs-mini.bin` 所在构建目录 |
| `RES_DIR` | `res/arcs-mini` | 默认资源目录，可被 `-S` 覆盖 |

`-S` 指向包含 `partition_table.json` 的资源目录；脚本会按该分区表读取镜像地址和文件路径，跳过没有 `file` 的保留分区。`-B` 或 `BUILD_DIR` 可覆盖 `${BUILD_DIR}` 对应的构建输出目录。

**分区映射**（来自 `res/arcs-mini/partition_table.json`）：

| 分区 | 地址 | 文件 |
| --- | --- | --- |
| boot | `0x000000` | `res/arcs-mini/boot.bin` |
| ap | `0x040000` | `res/arcs-mini/ap.bin` |
| tone | `0x100000` | `res/arcs-mini/tone.bin` |
| wake_word | `0x200000` | `res/arcs-mini/wake_word.bin` |
| emoji | `0x380000` | `res/arcs-mini/emoji.bin` |
| respak | `0x440000` | `res/arcs-mini/respak.bin` |
| app (CP) | `0x600000` | `build/arcs-mini.bin` |

### 2b. 仅烧录 CP app 固件

仅在设备正常运行且 ADB 可用、且**仅改动 CP 代码**时使用 `app` 模式；脚本会自动进入 recovery、推送 `build/<project>.bin` 并重启设备，不需要手动执行 `adb shell recovery`。

```bash
bash adb_download.sh -S res/arcs-mini app
```

```powershell
powershell -ExecutionPolicy Bypass -File adb_download.ps1 -S res/arcs-mini app
```

- `<project>.bin` 名称由 `apps/<app>/CMakeLists.txt` 的 `project()` 决定（arcs-mini 为 `arcs-mini.bin`）。
- CP 分区地址以 `-S` 指定资源目录的 `partition_table.json` 中 `app` 分区为准（arcs-mini 当前为 `0x600000`）。

### 2c. 救砖：cskburn 串口烧录

仅当设备无法进入 ADB 时使用。优先使用系统全局的 `cskburn`，如果没有，则使用仓库内 `tools/cskburn/cskburn`。

> **注意**：Linux/macOS 下首次使用需 `chmod +x tools/cskburn/cskburn` 确保可执行权限。

> **默认波特率**：cskburn 统一使用 `-b 1500000`。只有在已验证串口适配器和连线稳定时，才可尝试 `3000000` 或 `6000000` 提速。

**ARCS-MINI 串口烧录强制使用开发 Boot**：

`res/arcs-mini/boot-dev-autostart.bin` 是串口开发专用 Boot 镜像。**只要使用 cskburn 串口烧录，无论本次目标是 CP、AP、资源分区还是整包，都必须确保该文件在所有会覆盖 `0x0` 的操作之后写入 `0x0`。**

```bash
# 串口烧录其它分区时，可在同一命令中写入开发 Boot
cskburn -C arcs -b 1500000 -s <tty> --verify-all \
  0x0 res/arcs-mini/boot-dev-autostart.bin \
  0x600000 build/<project>.bin
```

- 该文件只替换 Boot 分区，不包含 AP、CP 和静态资源；首次整机恢复时还需烧录其它必要分区。
- 现有 `res/*/boot.bin` 等常规 `0x0` Boot 镜像用 cskburn 烧录并复位后不会直接进入业务，需要手动长按开机键。
- 不要用专用 Boot 覆盖 `res/arcs-mini/boot.bin`，也不要修改 `partition_table.json` 的默认引用；这样可避免改变现有 ADB/发布烧录流程。
- 开发专用 Boot 仅供串口开发期间临时使用；完成功能检查与日志分析后，必须按下文恢复原 Boot。

**CP 固件烧录**（仅改 CP）：

```bash
cskburn -C arcs -b 1500000 -s <tty> --verify-all \
  0x0 res/arcs-mini/boot-dev-autostart.bin \
  0x600000 build/<project>.bin
```

**AP 改动或恢复：整包烧录**：

```bash
cskburn -C arcs -s <tty> -b 1500000 --verify-all 0x0 build/<project>-all.bin

# 整包会覆盖 0x0，烧录完后必须再写入开发 Boot
cskburn -C arcs -s <tty> -b 1500000 --verify-all \
  0x0 res/arcs-mini/boot-dev-autostart.bin
```

`build/<project>-all.bin` 及常规 `res/*/boot.bin` 在 `0x0` 的启动行为与开发专用 Boot 不同；整包烧录后必须再将 `res/arcs-mini/boot-dev-autostart.bin` 单独写入 `0x0`。

- `<project>.bin` 名称由 `apps/<app>/CMakeLists.txt` 的 `project()` 决定（arcs-mini 为 `arcs-mini.bin`）。
- `<project>-all.bin` 由 `-DPACK_IMAGES=y` 生成；若不存在，提示先执行 `build` skill 整包编译。
- `--verify-all` 烧录后校验，确保写入正确。
- 设备无法响应时，需手动按 Boot 键上电或复位进入 ROM Boot 模式。
- **⚠️ 烧录完成后**：cskburn 会复位设备，串口可能需几秒才能完全释放。如需紧接着抓日志，先 `sleep 3`。

### 3. 串口烧录后：运行业务并检查日志（必须）

保持开发专用 Boot，让设备复位后直接进入业务。按 `run-log` skill 抓取日志；cskburn 烧录后先等待 3 秒释放串口，再使用 ADB shell、picocom 或 Windows `SerialPort` 采集输出。

- 在开发 Boot 下执行本次目标功能，确认业务流程已完整跑通。
- 检查日志中的 `error`、`assert`、`fault`、`panic`、超时、内存或栈异常，以及与本次功能相关的失败。
- 日志或功能还有问题时继续修复和验证，不得提前恢复原 Boot。
- 只有目标功能已跑通且日志无异常时，才进入下一步。

### 4. 开发任务结束：恢复原 Boot（必须）

任何串口烧录任务在目标功能跑通、日志确认无异常后，必须在交还设备前将原始 `boot.bin` 烧回 `0x0`：

```bash
cskburn -C arcs -b 1500000 -s <tty> --verify-all \
  0x0 res/arcs-mini/boot.bin
```

- 恢复命令必须保留 `--verify-all`；烧录或校验失败时，不得将设备视为已完成交付。
- 恢复后设备复位不直接进入业务是常规 Boot 的预期行为；需要运行时手动长按开机键。
- 恢复原 Boot 后不再主动改回开发 Boot；后续功能验收由用户在常规 Boot 下完成。

### 5. 恢复后：通知用户验证功能（必须）

原 Boot 烧录与 `--verify-all` 校验成功后，立即通知用户：

- 开发专用 Boot 已移除，`res/arcs-mini/boot.bin` 已恢复。
- 恢复后不会自动进入业务，请用户长按开机键启动。
- 请用户在常规 Boot 下验证本次目标功能，并反馈结果。
- 最终回复必须说明日志检查结果、原 Boot 的恢复/校验结果，以及用户需执行的验证动作。

**`<tty>` 跨平台替换**：

```
Linux:   /dev/ttyACM0 或 /dev/ttyUSB0
macOS:   /dev/cu.usbmodem* 或 /dev/cu.SLAB_USBtoUART*
Windows: COM3, COM4 等
```

### 6. 执行前后汇总

执行前简短列出：烧录方式、目标 app 与设备、写入的文件与分区/地址、触发整包烧录的依据（如适用）。串口烧录时还要汇总开发 Boot 写入、日志检查、原 Boot 恢复和用户验证通知四个闭环步骤。

## 禁止事项

- 不在未确认设备 ID/串口时盲烧。
- 不硬编码分区地址，始终以 `-S` 指定资源目录的 `partition_table.json` 为准。
- 不在烧录失败后继续后续步骤；先排查再重试。
- 不得跳过串口烧录后的开发 Boot 写入，即使本次只烧 CP 或资源分区。
- 不得在日志检查通过前恢复原 Boot。
- 不得在开发任务结束后将 `boot-dev-autostart.bin` 留在设备 `0x0`；必须恢复 `res/arcs-mini/boot.bin` 并通知用户验证功能。

## 常见问题

### `ERROR: Failed opening device`

设备未进入 ROM Boot 模式（可能上次烧录异常中断导致设备卡在中间状态）。解决方法：

```bash
# Linux/macOS: 用 picocom 的 DTR/RTS 复位设备（退出时自动恢复高电平触发复位）
timeout 1 picocom -b 921600 --quiet --lower-dtr --lower-rts <tty>
sleep 3
# 然后重试烧录
```

> **Windows**: Git Bash 下无 picocom，需使用 WSL 或手动按 Boot 键 + 复位进入 ROM Boot 模式。

如果仍不行，需手动按 Boot 键 + 复位进入 ROM Boot 模式。

### 设备进不去 recovery / ADB 不识别

1. 检查 USB 线缆（部分线缆只充电不传数据）
2. 检查设备是否正常上电
3. `adb devices` 确认设备状态为 `device`
4. 尝试 `adb kill-server && adb start-server` 重启 ADB 服务
5. Windows 下检查 ADB 驱动是否安装

### 烧录后设备无法启动

1. 确认 `boot.bin` 未被意外损坏（默认不烧 boot）
2. 仅更新应用代码时用 `adb_download.sh -S res/arcs-mini app` 或 `adb_download.ps1 -S res/arcs-mini app`
3. 如 boot 确实损坏，需用 `adb_download.sh -S res/arcs-mini boot`、`adb_download.ps1 -S res/arcs-mini boot` 或 cskburn 整包恢复
