# AGENTS.md

## 项目概述

ARCS-MINI 语音助手固件，基于 LISTENAI ARCS SoC（RISC-V 双核 AP/CP 架构）。

## Skill 优先级

当任务与 PDK / ARCS 开发相关时，必须优先加载并遵循 `.agents/skills/` 下对应 skill 的 `SKILL.md`。
`.claude/skills/<skill>` 仅作为兼容入口，通过目录软链接指向 `.agents/skills/<skill>`，不要维护两份不同内容。

优先级规则：

1. 用户明确使用 `/xxx` 命令或直接点名某个 skill 时，加载该 skill。
2. 用户未点名但任务语义匹配时，自动加载最相关的 skill。
3. 同一任务涉及多个阶段时，按工作流顺序加载多个 skill，例如：协议设计 -> 编码 -> 编译 -> 测试 -> 提交。
4. skill 中的具体流程、命令、路径、注意事项优先于本文件的通用说明。
5. 如果 skill 缺失或与当前仓库实际情况冲突，先说明原因，再按本文件和本地代码上下文继续处理。

## 可用 Skills
| Skill | 触发场景 | 说明 |
| --- | --- | --- |
| `device` | 引脚、pinmux、PWM/GPIO/I2C/SPI/UART、屏幕、panel、硬件选型、adb shell、设备配置、绑定云端应用 | 处理设备侧硬件适配、运行时配置和云端应用写入 |
| `mcp` | MCP 工具、tools/list、tools/call、工具描述、参数 schema、工具调用处理 | 新增工具强制使用无前缀普通名称；维护既有工具时保持原工具名不变 |

## 关键路径

- `apps/remote-ap`: AP 固件，负责算法
- `apps/arcs-mini`: CP 固件，主要业务逻辑
- `arcs-sdk/`: SDK 子模块，包含 HAL、驱动、组件和第三方库
- `arcs-sdk/boards/arcs_mini`: ARCS-MINI 默认板型配置
- `arcs-sdk/boards/arcs_mini_doll_v2`: ARCS-MINI doll_v2 板型配置
- `res/arcs-mini`: ARCS-MINI 相关静态资源（如提示音、唤醒词等），以及串口开发烧录专用的 `boot-dev-autostart.bin`

如无特别说明，默认构建 `apps/arcs-mini` 应用，使用 `arcs_mini` 板型。doll_v2 版型需明确指定 `-DBOARD=arcs_mini_doll_v2`。

**主分支**: `arcs-mini/main`（非 `main`）。PR、审查、diff 基准均以此分支为准。

## 配置

该项目使用 Kconfig 组织配置。应用级配置位于 `apps/arcs-mini/prj.conf`。构建后可通过 `build/.config` 确定最终实际生效的配置。

## 环境搭建

按平台选择下面任一方式即可。脚本检测到工具链就绪后会设置 `ARCS_BASE` / `NUCLEI_TOOLCHAIN_PATH` / `LISTENAI_TOOLS_PATH` / `PATH`，并顺手检查子模块和串口权限。

### Linux（含 WSL2）

在仓库根目录 `source` 一次：

```bash
source ./arcs-sdk/env.sh           # 检测 + 按需安装 + 设置环境
source ./arcs-sdk/env.sh check     # 单独跑一次完整环境体检
source ./arcs-sdk/env.sh setup     # 强制重装工具链
source ./arcs-sdk/env.sh info      # 打印当前 SDK / 工具链版本和路径
```

> **必须用 `source`**，直接 `./arcs-sdk/env.sh` 执行会立即退出。脚本会自动判断 `~/.listenai` 或仓库内 `listenai-dev-tools/` 是否已有工具链，避免重复下载。

### Windows（PowerShell）

```powershell
.\arcs-sdk\env.ps1               # 检测 + 按需安装 + 设置环境
.\arcs-sdk\env.ps1 check         # 单独跑一次完整环境体检
.\arcs-sdk\env.ps1 setup         # 强制重装工具链
```

### macOS

`env.sh` 不会自动为 macOS 编译 GCC 工具链，所以分两步：

```bash
# 用 Homebrew 装 GCC 工具链
brew install listenai/tap/arcs-toolchain
```

### 其它平台（跨平台 CI、FreeBSD 等）

直接用 Docker 镜像，环境已预装好：

```bash
docker pull ghcr.io/listenai/arcs-builder:latest --platform linux/amd64
docker run --rm --init -w $(pwd) -v $(pwd):$(pwd) --platform linux/amd64 \
    ghcr.io/listenai/arcs-builder:latest ./build.sh -S ./apps/arcs-mini -DBOARD=arcs_mini
```

## 构建

`build.sh`（Linux / macOS / WSL）和 `build.ps1`（Windows 原生 PowerShell）参数一一对应，按平台选一个即可。命令都在**仓库根目录**执行。

**arcs-mini（默认版型）**

```bash
./build.sh -S ./apps/arcs-mini -DBOARD=arcs_mini
```

```powershell
.\build.ps1 -S .\apps\arcs-mini -DBOARD=arcs_mini
```

**arcs-mini doll_v2 版型**

```bash
./build.sh -S ./apps/arcs-mini -B build-doll_v2 -DBOARD=arcs_mini_doll_v2
```

```powershell
.\build.ps1 -S .\apps\arcs-mini -B build-doll_v2 -DBOARD=arcs_mini_doll_v2
```

**AP 核构建命令**

```bash
./build.sh -S ./apps/remote-ap -B build-remote-ap -DCONFIG_FILES=arcs_mini.conf -DBOARD=arcs_mini
```

```powershell
.\build.ps1 -S .\apps\remote-ap -B build-remote-ap -DCONFIG_FILES=arcs_mini.conf -DBOARD=arcs_mini
```

**其它平台**（跨平台 CI、FreeBSD 等）用 Docker 镜像直接跑：

```bash
docker run --rm --init -w $(pwd) -v $(pwd):$(pwd) --platform linux/amd64 ghcr.io/listenai/arcs-builder:latest \
  ./build.sh -S ./apps/arcs-mini -DBOARD=arcs_mini
```

## 烧录

### 首选：ADB 烧录

使用仓库根目录的 `adb_download.sh` / `adb_download.ps1`，按改动范围选模式（脚本会按 `res/arcs-mini/partition_table.json` 自动派发到各分区）：

```bash
# 推荐：只烧 CP app 固件（agent 开发通常只改 CP 代码，默认走这条）
bash adb_download.sh -S res/arcs-mini app
# 完整：烧 ap / tone / wake_word / emoji / respak / app（不烧 boot，CP + 资源改动时用）
bash adb_download.sh -S res/arcs-mini
# 整包：额外烧 boot.bin（更新 boot 时用）
bash adb_download.sh -S res/arcs-mini boot
```

```powershell
.\adb_download.ps1 -S res\arcs-mini app      # 推荐：只烧 CP app
.\adb_download.ps1 -S res\arcs-mini          # 完整：CP + 资源
.\adb_download.ps1 -S res\arcs-mini boot     # 整包：含 boot
```

完整工作流（多设备选择、recovery 等待、超时变量、常见错误恢复）见 `.agents/skills/flash/SKILL.md`。

### 降级：cskburn 串口烧录

仅在设备无法进入 ADB（救砖、boot 损坏、首次上电）时使用 `cskburn`。优先使用系统全局的 `cskburn`，没有就用仓库内 `tools/cskburn/`。详细命令、波特率、多分区烧录等说明保持与原版一致。

> **串口烧录强制闭环**：只要使用 `cskburn` 串口烧录，无论只烧 CP、单个资源分区还是整包，开发阶段都必须确保 `res/arcs-mini/boot-dev-autostart.bin` 最后被写入 `0x0`，使设备复位后直接进入业务。如果整包烧录会覆盖 `0x0`，须在整包烧录后再单独烧录开发 Boot。完成开发和功能检查后，先在开发 Boot 下抓取并分析日志；确认日志无异常后，必须将 `res/arcs-mini/boot.bin` 烧回 `0x0`，最后通知用户长按开机并验证目标功能。

```bash
# 串口更新 CP app 时，必须同时写入开发 Boot
cskburn -C arcs -b 1500000 -s /dev/ttyACM0 --verify-all \
  0x0 res/arcs-mini/boot-dev-autostart.bin \
  0x600000 build/arcs-mini.bin

# 完成开发并确认日志无异常后：必须恢复原 Boot
cskburn -C arcs -b 1500000 -s /dev/ttyACM0 --verify-all 0x0 res/arcs-mini/boot.bin
```

* `-s` 指定了串口设备，Linux 下通常是 `/dev/ttyACM0` 或 `/dev/ttyUSB0`，Windows 下可能是 `COM3` 等，Mac 下通常是一个 `/dev/cu.` 开头的路径
* `-b` 指定烧录波特率，本项目默认使用 `1500000`；只有在已验证串口适配器和连线稳定时，才尝试 `3000000` 或 `6000000` 提速
* 支持多个地址同时烧录，如 `0x40000 ./res/arcs-mini/ap.bin 0x100000 ./res/arcs-mini/tone.bin`，各个分区的地址可参考 `res/arcs-mini/partition_table.json` 中的定义

## 日志

### 首选：ADB shell 日志

使用下面这条 ADB 命令抓日志：先重启设备，等待 USB/ADB 重新枚举上线，再抓取 10 秒 shell 输出到 `./tmp/run-log.txt`。命令在仓库根目录执行：

```bash
mkdir -p ./tmp && adb shell reboot && adb wait-for-device && timeout 10 adb shell 2>&1 > ./tmp/run-log.txt
```

命令组成：
* `mkdir -p ./tmp`：确保日志目录存在，不存在就创建。
* `&&`：串联命令，前一步成功后才继续下一步，避免目录或 ADB 操作失败后继续误抓。
* `adb shell reboot`：通过 ADB 让设备重启，便于抓重启后的启动阶段日志。
* `adb wait-for-device`：阻塞等待设备 USB 枚举并重新进入 ADB 在线状态。
* `timeout 10 adb shell`：进入设备 shell 并采集 10 秒输出；要抓更久就调整 `10`。
* `2>&1 > ./tmp/run-log.txt`：按当前顺序，标准输出写入 `./tmp/run-log.txt`，本地 ADB 错误仍可能打印在终端；若要把标准错误也写入文件，改成 `> ./tmp/run-log.txt 2>&1`。

### 降级：串口日志（无 ADB 时）

开发板带串口或设备进不了 ADB 时按平台选一种：

#### Linux / macOS / WSL：picocom

串口适配器的 DTR 连接了设备的**烧录模式选择**引脚（低电平=烧录模式，高电平=正常运行），RTS 连接了设备的**复位**引脚（低电平=复位，高电平=释放）。使用 picocom 的 `--lower-dtr --lower-rts` 确保设备处于已知状态：picocom 退出后 DTR/RTS 自动恢复高电平，设备随即复位启动。

```bash
timeout 1 picocom -b 921600  --lower-dtr --lower-rts  /dev/ttyACM0 > ./run-log.log
```

* `timeout` 指定采集日志的时长，单位秒（默认 10）
* `-b` 指定波特率，只能是 `921600`
* `--lower-dtr --lower-rts`：picocom 运行期间拉低 DTR/RTS，退出后自动恢复高电平触发设备复位启动
* `/dev/ttyACM0` 为串口设备，也可能是 `/dev/ttyUSB0`，需要与烧录时使用的设备一致
* `> ./tmp/run-log.txt` 指定日志保存的路径

> **⚠️ cskburn 烧录后串口粘滞问题**：cskburn 烧录完成并复位设备后，串口设备可能需要几秒才能完全释放并稳定。如果在烧录后**立即**用 picocom 抓日志，可能抓到空文件或乱码。**解决方法**：烧录后等待 3~5 秒再启动 picocom，或用 `sleep 3 && timeout 10 picocom ...` 串行执行。

#### Windows 原生：PowerShell `System.IO.Ports.SerialPort`

Git Bash 下没有 picocom，直接用 PowerShell 内置的 `SerialPort` 类即可。波特率固定 `921600`：

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

* 把 `COM8` 换成实际的串口号（设备管理器里查，或 `mode` 列出）。
* `921600` 是本项目固定波特率，需要改时再调。
* 想存到文件：把 `Write-Host -NoNewline $d` 换成 `Add-Content -Path ./tmp/run-log.txt -Value $d -NoNewline`，并先 `New-Item -ItemType File -Force ./tmp/run-log.txt | Out-Null`。
* DTR/RTS 控制有限：如需复位后立即抓启动日志，先用烧录脚本触发设备重启，再立刻跑上面这段。

完整抓取流程、日志分析模式、shell 命令速查见 `.agents/skills/run-log/SKILL.md`。


## 崩溃分析

```bash
riscv64-unknown-elf-addr2line -e build/arcs-mini -a 0xAAAAAAAA 0xBBBBBBBB
```

同理，非 Linux 平台需要使用 Docker：

```bash
docker run --rm --init -w $(pwd) -v $(pwd):$(pwd) --platform linux/amd64 ghcr.io/listenai/arcs-builder:latest \
  /opt/arcs/gcc/bin/riscv64-unknown-elf-addr2line -e build/arcs-mini -a 0xAAAAAAAA 0xBBBBBBBB
```

## 提交规范

- 以当前仓库的用户身份提交，末尾附加 `Co-Authored-By` 带上实际的模型和版本信息，如 `Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>`
- 提交信息使用 conventional commits 格式，中文描述，如 `fix(player): 修复唤醒提示音随机选择在特定数量下可能死循环的问题`，并附上必要的情况说明
- 保持合理的提交粒度，原子性提交
