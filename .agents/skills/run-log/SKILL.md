---
name: run-log
description: 获取设备日志并智能分析。使用 /run-log 或当用户提到"日志"、"log"、"运行"、"查看输出"时触发。
---

# ARCS 设备日志获取与分析

## 约定

- 默认在仓库根目录执行。
- 同一时刻设备只能被一个进程占用（ADB shell / cskburn / picocom）。
- **cskburn 烧录后等 3 秒再抓日志**，否则串口未释放会抓到空文件。
- 串口烧录任务检查日志时，保持 `res/arcs-mini/boot-dev-autostart.bin` 在设备 `0x0`；日志无异常后再按 `flash` skill 恢复原 Boot，然后通知用户验证功能。

## 日志方式选择

| 场景 | 方式 | 说明 |
| --- | --- | --- |
| **默认（推荐）** | ADB shell | 设备出厂不带串口，ADB 是主流日志方式 |
| **重启后尽早抓日志** | `adb wait-for-device` + shell | 设备重启后第一时间连接，捕获启动日志 |
| **开发板有串口** | picocom 串口 | 开发调试场景，可捕获复位前的日志 |
| **Windows 原生** | PowerShell `System.IO.Ports.SerialPort` | Git Bash 下没有 picocom 时的兜底，无需额外依赖 |

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
| ADB shell 日志 | ✅ | ✅ | ✅ |
| `adb wait-for-device` | ✅ | ✅ | ✅ |
| picocom 串口日志 | ✅ | ✅ | ❌ (仅 WSL) |
| `script` 伪终端 | ✅ | ✅ | ❌ |
| PowerShell `SerialPort` 串口日志 | ❌ | ❌ | ✅ |

> **Windows 用户**: Git Bash 下没有 `script` 命令和 picocom。串口日志抓取需要通过 WSL，或优先使用 ADB 方式（推荐）。

## 流程

### 方式 A：ADB shell 日志（推荐）

#### A1. 重启后尽早抓日志（捕获启动阶段日志）

烧录后设备重启，用 `adb wait-for-device` 阻塞等待设备上线，第一时间连接 shell：

```bash
mkdir -p ./tmp && adb shell reboot && adb wait-for-device && timeout 10 adb shell 2>&1 > ./tmp/run-log.txt
```

> **说明**: 当前固件 shell 的 `reboot` 执行软重启，不要默认追加未实现的 `hard` 参数。`adb wait-for-device` 会在设备 USB 枚举完成、ADB daemon 启动后立即返回，确保尽可能早地获取启动日志。

> **macOS 注意**: macOS 自带 `timeout` 功能可能不可用，可用 `gtimeout`（`brew install coreutils`）或 Perl 替代：
> ```bash
> # macOS 备选方案
> adb shell 2>&1 | head -n 500 > ./tmp/run-log.txt
> # 或安装 coreutils: brew install coreutils && gtimeout 10 adb shell ...
> ```

**完整烧录+抓日志流程**：

```bash
# 1. 烧录
bash adb_download.sh -S res/arcs-mini

# 2. 等待设备重启上线并抓启动日志
adb wait-for-device && timeout 10 adb shell 2>&1 > ./tmp/run-log.txt
```

#### A2. 发送 shell 命令并抓响应

```bash
# 单条命令
adb shell "device get_device_id"

# 多条命令
adb shell "device get_device_id; wifi scan"

# 发送命令并保存输出
adb shell "help" 2>&1 > ./tmp/run-log.txt
```

### 方式 B：picocom 串口日志（开发板/有串口时使用）

#### TTY 依赖说明

- picocom 的 `--lower-dtr --lower-rts` **依赖 TTY** 才能控制串口 DTR/RTS 信号。
- 非 TTY 环境（如 CI、脚本后台执行）必须用 `script` 包裹，否则 DTR/RTS 控制失效，设备无法复位启动，只能抓到串口残留乱码。
- `script -q -c "<picocom命令>" /dev/null` 提供伪终端。
- **⚠️ Windows Git Bash 不支持 `script` 和 picocom**，请使用 WSL 或 ADB 方式。

#### B1. 抓日志

```bash
mkdir -p ./tmp && script -q -c "timeout 10 picocom -b 921600 --lower-dtr --lower-rts <tty>" /dev/null > ./tmp/run-log.txt
```

- `timeout` 采集时长，单位秒（默认 10）。
- `> ./tmp/run-log.txt` 日志保存路径，确保 `./tmp/` 目录存在。
- 刚烧录完：`sleep 3 && script -q -c "timeout 10 picocom ..." /dev/null > ./tmp/run-log.txt`。

#### B2. 发送 shell 命令并抓响应

```bash
mkdir -p ./tmp && ( sleep 0.5; printf '<cmd>\r'; sleep 2 ) | \
  script -q -c "timeout 4 picocom -b 921600 --lower-dtr --lower-rts <tty>" /dev/null > ./tmp/run-log.txt
```

- `printf '<cmd>\r'` 末尾 `\r` 提交命令。
- 多条命令按顺序排队 `sleep + printf`。
- `script` 包裹确保有伪终端，DTR/RTS 可正常工作。

### 方式 C：Windows 原生 PowerShell 串口

Git Bash 下没有 picocom 也没有 `script`，直接用 PowerShell 内置的 `System.IO.Ports.SerialPort` 即可。波特率固定 `921600`：

```powershell
$port = New-Object System.IO.Ports.SerialPort COM8, 921600, None, 8, One
$port.Open()
$t = [DateTime]::Now.AddSeconds(10)
while ([DateTime]::Now -lt $t) {
    $d = $port.ReadExisting()
    if ($d) { Write-Host -NoNewline $d }
    Start-Sleep -Milliseconds 50
}
$port.Close()
```

- 把 `COM8` 换成实际的串口号（设备管理器里查，或 `mode` 列出）。
- `921600` 是本项目固定波特率，需要改时再调。
- 想存到文件：把 `Write-Host -NoNewline $d` 换成 `Add-Content -Path ./tmp/run-log.txt -Value $d -NoNewline`，并先 `New-Item -ItemType File -Force ./tmp/run-log.txt | Out-Null`。
- DTR/RTS 控制有限：如需复位后立即抓启动日志，先用烧录脚本触发设备重启，再立刻跑上面这段。
- 想抓取并打印设备 ID 等交互响应，可在 while 循环里用 `$port.WriteLine("<cmd>\r")` 发送命令再 `ReadExisting()`。

### 3. 日志分析

按时间线排列关键事件，匹配下表模式，定位源码给出根因与修复建议。

| 模式 | 级别 | 含义 |
| --- | --- | --- |
| `[E]` / `error` | 错误 | 错误日志 |
| `assert` / `fault` / `panic` | 严重 | 程序崩溃 |
| `HardFault` / `MemManage` | 严重 | 硬件异常 |
| `alloc fail` / `heap` + `fail` | 错误 | 内存分配失败 |
| `stack overflow` | 严重 | 栈溢出 |
| `timeout` | 警告 | 超时 |
| `wifi` + `disconnect` | 警告 | 网络断连 |
| `wakeup` | 信息 | 唤醒事件 |

> 串口输出含 ANSI 颜色码，分析时忽略。ADB shell 输出同样可能包含 ANSI 转义序列。

### 4. 串口烧录任务的日志闭环

如果本次日志检查是 cskburn 串口烧录开发流程的一环：

1. 在开发专用 Boot 下运行本次目标功能并抓取日志。
2. 日志或功能存在异常时，继续修复和复测，不得提前恢复原 Boot。
3. 目标功能跑通且日志无异常后，加载 `flash` skill，使用默认 `1500000` 波特率和 `--verify-all` 将 `res/arcs-mini/boot.bin` 烧回 `0x0`。
4. 恢复成功后，通知用户长按开机键，在常规 Boot 下验证本次目标功能并反馈结果。

## 常用 shell 命令

发送 `help` 获取完整列表；常用：

| 命令 | 用途 |
| --- | --- |
| `device get_device_id` | 获取设备 ID |
| `wifi scan` / `wifi connect <ssid> [pwd]` | WiFi 扫描 / 连接 |
| `kv show` / `kv set <type> <key> <val>` | 查看 / 写入 KV |
| `heap` / `threads` | 堆 / 线程 |
| `reboot` / `recovery` | 重启 / 进 recovery |

## 常见问题

### ADB shell 连接后无输出

1. 确认设备已正常启动（`adb devices` 状态为 `device`）
2. 尝试 `adb kill-server && adb start-server` 重启 ADB
3. 检查设备是否被其他 ADB 进程占用

### `adb wait-for-device` 一直阻塞

1. 设备未重启或重启失败 — 检查 USB 连接和设备状态
2. ADB daemon 启动慢 — 等待时间可能超过预期，默认无超时
3. 可设置超时：`timeout 30 adb wait-for-device`（30 秒超时）

### picocom 抓到空日志

1. 刚烧录完需 `sleep 3` 等串口释放
2. 检查波特率是否为 `921600`
3. 检查 `--lower-dtr --lower-rts` 是否正确设置
4. macOS 上检查是否使用了正确的 `/dev/cu.*` 设备（不是 `/dev/tty.*`）
5. **Windows Git Bash 不支持 picocom**，请使用 ADB 方式或 WSL
