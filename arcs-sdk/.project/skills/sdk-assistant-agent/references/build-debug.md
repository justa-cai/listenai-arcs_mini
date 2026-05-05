<!-- type: worker -->
<!-- 开发规则: README.md#开发规则 -->

# ARCS 编译烧录调试助手

覆盖 ARCS SDK 开发的三大核心操作：**编译构建**、**固件烧录**、**串口日志查看**，以及对应的故障诊断。

本 worker 只提供可独立复用的编译、烧录、串口日志能力，由大模型根据上下文决定是只做某项还是按需组合。

## 触发条件

**操作类**（帮用户做事）:
- 编译/构建项目（build、编译、make）
- 烧录固件到设备（烧录、flash、burn）
- 查看串口日志/调试输出（串口、日志、log、monitor）
- 运行 menuconfig 配置 Kconfig

**诊断类**（帮用户排查问题）:
- 编译错误或警告（编译期 / 链接期 / CMake 配置期）
- Kconfig 选项不生效或依赖问题
- 内存溢出（ROM/RAM overflow）
- 烧录失败（串口不通、超时、BOOT 模式、eMMC 问题）
- 串口无日志输出、乱码、shell 不响应
- GDB 连接失败、断点无法命中
- 环境配置问题（工具链、CMake、Ninja）

---

## 执行流程

### Step 1: 识别用户意图

根据用户描述分为两大类：

| 意图类型 | 典型表述 | 跳转 |
|----------|----------|------|
| **A: 操作执行** | "帮我编译"、"怎么烧录"、"我要看日志"、"build 一下" | → **Step 2** |
| **B: 故障诊断** | "编译报错"、"烧录失败"、"串口没输出"、"undefined reference" | → **Step 3** |

**细分关键词**:

| 关键词 | 场景 |
|--------|------|
| build、编译、构建、make、menuconfig | 编译构建 (2A / 3A) |
| 烧录、cskburn、flash、burn、BOOT | 烧录 (2B / 3B) |
| 日志、log、串口、UART、monitor、picocom | 串口查看 (2C / 3C) |
| shell、GDB、JTAG、debug、RTT | 调试 (3C) |

**在线文档参考:**
- 快速入门（编译/烧录命令）: `get_started.html`
- GDB 调试指南: `gdb.html`

---

### Step 2: 操作执行

#### 2A: 编译构建

**信息收集**（依次确认，已知则跳过）:

1. **项目路径** — 用户要编译哪个项目？
   - 若用户说"编译 helloworld" → 推断 `samples/helloworld`
   - 若用户说"编译当前项目" → 询问具体路径或从工作区推断
   - 常见路径模式: `samples/<name>`, `demos/<name>`, 或用户自定义路径
2. **板型** — `arcs_evb`（EVB 评估板）还是 `arcs_mini`（Mini 开发板）？
   - 若用户未指定，询问板型
   - 若项目 `prj.conf` 或上下文中有板型信息，直接使用
3. **是否清理重建** — 是否需要 `-C` 清理？
   - 遇到奇怪问题时建议清理重建
   - 首次编译或修改了 Kconfig/CMakeLists.txt 时建议清理

**执行前自检**:
- 当前目录是否为 SDK 根目录，且存在 `./build.sh`
- 目标项目路径是否存在
- 板型是否明确
- 若用户在排查缓存问题，是否需要使用 `-C`

**执行流程**:
1. 确认项目路径和板型
2. 构造编译命令:
   ./build.sh `[-C]` `-S <项目路径>` `-DBOARD=<板型>` [其他参数]
3. 用 Bash 工具执行命令
4. 分析输出:
   ├─ 成功 → 报告产物位置（bin/elf/map 文件）
   └─ 失败 → 跳转 Step 3A 诊断

**命令构造示例**:
```bash
# 基本编译
./build.sh -S samples/helloworld -DBOARD=arcs_evb

# 清理重建
./build.sh -C -S samples/helloworld -DBOARD=arcs_evb

# menuconfig 配置
./build.sh -S samples/helloworld -t menuconfig -DBOARD=arcs_evb
```

**成功时的输出分析**:
- 报告 `build/<app_name>.bin` 文件路径和大小
- 若实际产物路径不明确，可执行 `ls -lh build/*.bin build/*.elf build/*.map 2>/dev/null`
- 若启用了 `CONFIG_PRINT_MEMORY_USAGE=y`，解读内存使用输出（各区域 Used/Region Size/%）
- 若未启用，建议添加到 prj.conf 以便跟踪内存
- 提示下一步: "可以烧录了，需要我帮你烧录吗？"

#### 2B: 固件烧录

**前置检查**（Agent 主动执行）:
1. 检查 bin 文件是否存在
   - `ls -l build/*.bin`
   - 不存在 → 提示先编译
2. 检测可用串口（兼容 zsh，避免通配符无匹配时报错）
   - `ls /dev/ttyUSB* 2>/dev/null; ls /dev/ttyACM* 2>/dev/null`
   - 两条命令用 `;` 分开，确保一个无匹配不影响另一个
   - 无串口 → 提示检查 USB 连接
3. 检查串口是否被占用
   - `fuser /dev/ttyACM0`（或 ttyUSB0）
   - 有进程占用（如 picocom/minicom）→ `kill <PID>` 释放端口后再烧录
4. 判断是否需要手动进入 BOOT 模式
   - arcs_evb（ttyACM 设备）→ `cskburn` 通过串口流控（DTR/RTS）自动控制 BOOT/RST，无需手动操作，直接烧录
   - arcs_mini 或 ttyUSB 设备 → 提示用户按住 BOOT 按键，然后复位开发板

**烧录规则解析顺序**:
1. 先读取目标工程 `README.md`
2. 若 README 中存在明确的 `cskburn` 命令，则优先按 README 执行
3. 将 README 中的串口设备替换为当前探测到的设备
4. 将 README 中的相对 `.bin` 路径转换为工程内或 SDK 根目录下的实际路径
5. 若 README 未提供烧录规则，再回退到默认单 bin 烧录：`0x0 <artifact>`

**信息收集**:

1. **bin 文件路径** — 默认 `build/<app_name>.bin`，从最近构建中推断
2. **串口设备** — 从检测结果中选择；arcs_evb 通常是 `/dev/ttyACM0`，arcs_mini 通常是 `/dev/ttyUSB0`
3. **烧录目标** — Flash（默认）还是 eMMC（需 `--emmc`）
4. **波特率** — 默认 3000000，不稳定时建议降低

**执行流程**:

```text
1. 执行前置检查
2. 若非 EVB 板 → 提示用户手动进入 BOOT 模式，等待确认
3. 构造烧录命令:
   ./tools/burn/cskburn -C arcs -s <串口> -b <波特率> 0x0 <bin文件>
4. 用 Bash 工具执行命令
5. 分析输出:
   ├─ 成功 → cskburn 结束时自带 Reset，设备会自动重启运行新固件
   │         若用户需要日志，可直接跳转 2C 抓取（无需再次复位）
   └─ 失败 → 跳转 Step 3B 诊断
```

**命令构造示例**:
```bash
# 基本烧录
./tools/burn/cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 build/app.bin

# 烧录并验证
./tools/burn/cskburn -C arcs -s /dev/ttyACM0 -b 3000000 --verify-all 0x0 build/app.bin

# eMMC 烧录
./tools/burn/cskburn -C arcs -s /dev/ttyACM0 --emmc -b 3000000 0x0 build/app.bin
```

#### 2C: 串口日志查看

**检测串口设备**（每次操作前执行，兼容 zsh）:
```bash
ls /dev/ttyUSB* 2>/dev/null; ls /dev/ttyACM* 2>/dev/null
```

**场景 A：Agent 自动抓取（默认）**

适用于：
- 需要非交互式抓取启动日志
- 需要把日志保存为文件再分析
- 用户不想手动开串口终端

**核心约束**:
- 串口同一时间只能被一个进程占用
- `cskburn` 和串口读取不能同时占用同一个串口
- 自动抓取默认复用 `references/scripts/serial_read.py`

**执行步骤**:
1. 释放串口: `fuser <设备> 2>/dev/null` → 有占用则 `kill <PID>`
2. 判断是否需要触发一次重启
   - 刚烧录完成 → 通常可直接抓取
   - 需要从冷启动重新抓日志 → 先触发一次复位，再抓取
3. 抓取: `python3 .project/skills/sdk-assistant-agent/references/scripts/serial_read.py <设备> -b 921600 -t <秒数> > /tmp/arcs_serial.log 2>&1`
4. 读取 `/tmp/arcs_serial.log`，并给出结论

**复位建议**:
- `arcs_evb`：可优先尝试 `./tools/burn/cskburn -C arcs -s <设备> --chip-id` 触发一次自动复位
- `arcs_mini`：通常需要提示用户手动按复位键

**场景 B：用户手动交互式查看**

当用户想自己持续监控串口输出时，推荐以下工具（在用户终端运行）：

| 工具 | 命令 | 退出方式 |
|------|------|----------|
| picocom | `picocom -b 921600 /dev/ttyACM0` | `Ctrl-A Ctrl-X` |
| minicom | `minicom -D /dev/ttyACM0 -b 921600` | `Ctrl-A X` |
| screen | `screen /dev/ttyACM0 921600` | `Ctrl-A K` 然后按 `Y` |
| miniterm | `python3 -m serial.tools.miniterm /dev/ttyACM0 921600` | `Ctrl-]` |

- 波特率 **921600**，8N1；`arcs_evb` 串口设备通常是 `/dev/ttyACM0`，多设备时需确认
- 串口监视工具会独占终端，建议用户在另一个终端运行
- 日志保存: `picocom -b 921600 /dev/ttyACM0 --logfile output.log`
- 权限不足时: `sudo usermod -a -G dialout $USER`（需重新登录）

---

### Step 3: 故障诊断

**通用诊断流程**:
1. 让用户提供**完整的错误输出**（或从 Step 2 的执行结果中直接获取）
2. 读取 `references/experience/` 下相关模块的经验文件查找已知问题
3. 确认用户的**板型**和**项目路径**
4. 如需要，检查用户的 `prj.conf` 和 `CMakeLists.txt`
5. 匹配已知模式 → 给出解决方案；未知模式 → 引导式排查

#### 3A: 编译构建诊断

**自动错误分析**（从编译输出中提取关键信息）:

```text
错误输出 → 提取错误行
  ├─ 包含 "CMake Error" / "FATAL_ERROR" → CMake 配置期错误
  │   ├─ "LISTENAI_TOOLS_PATH is not defined" → 工具链路径问题
  │   ├─ "ARCS_BASE is not defined" / "ARCS_BASE not found" → SDK 路径问题
  │   ├─ "Tool xxx does not exist" → 工具文件缺失
  │   ├─ "Kconfig parse failed" → prj.conf 语法错误
  │   ├─ "CMake 3.19 or higher is required" → CMake 版本不足
  │   └─ "Board xxx not found" → 板型拼写/路径错误
  ├─ 包含 "error:" (带文件名前缀) → 编译期错误
  ├─ 包含 "undefined reference" / "multiple definition" → 链接期错误
  ├─ 包含 "will not fit in region" / "overflow" → 内存溢出
  ├─ 包含 "HEAP size is under the size limit" → 堆空间不足 ASSERT
  ├─ 包含 "error: wifi ram" → WIFI_RAM 区域约束违反
  ├─ 包含 "ABI mismatch" / "has float ABI" → FPU 配置不一致
  └─ 包含 "relocation truncated" → 代码跳转距离超限
```

**CMake 配置期错误**:

| 错误模式 | 原因 | 解决方案 |
|----------|------|----------|
| `ARCS_BASE is not set` | 未设置 SDK 根目录 | build.sh 会自动设置，确认从 SDK 根目录运行 |
| `Board xxx not found` | 板型名拼写错误或自定义板型路径未设置 | 检查 `-DBOARD=` 参数；自定义板型用 `-DBOARD_SEARCH_PATH=` |
| `find_package(xxx) failed` | 缺少依赖包 | 检查 Kconfig 是否启用对应模块 |
| `CMake version too old` | CMake 版本 < 3.19 | 升级 CMake 或检查 `LISTENAI_TOOLS_PATH` |
| `target_link_libraries` 找不到目标 | 模块未被 Kconfig 启用 | 检查 prj.conf 和 Kconfig 依赖 |

**编译期错误**:

| 错误模式 | 原因 | 解决方案 |
|----------|------|----------|
| `xxx.h: No such file or directory` | 头文件路径未加入 include | 检查 CMakeLists.txt 的 `listenai_library_include_directories` |
| `implicit declaration of function` | 缺少头文件包含或函数未声明 | 添加对应的 `#include` |
| `incompatible pointer type` | 类型不匹配 | 检查函数签名和参数类型 |
| `xxx undeclared` / `use of undeclared identifier` | 宏未定义（Kconfig 未开启） | 检查 prj.conf 中对应 `CONFIG_xxx=y` 是否启用 |
| `redefinition of xxx` | 同名符号多次定义 | 检查头文件保护宏，确认函数是否应为 static |

**链接期错误**:

| 错误模式 | 原因 | 解决方案 |
|----------|------|----------|
| `undefined reference to xxx` | 源文件未编译或 Kconfig 未开启 | 检查 `listenai_library_sources_ifdef` 条件；确认 prj.conf |
| `multiple definition of xxx` | 非 static 函数定义在头文件中 | 改为 `static inline` 或移到 .c 文件 |
| `section .xxx not found` | 自定义段 .ld.in 未注册 | 在 CMakeLists.txt 中添加 `listenai_add_custom_section()` |
| `will not fit in region` / `overflow` | ROM/RAM 空间不足 | 检查 .map 文件，优化大数组/启用 XIP/调整栈大小 |

**内存溢出分析指引**:

```text
1. 启用内存统计: 在 prj.conf 添加 CONFIG_PRINT_MEMORY_USAGE=y，重新编译
2. 查看各段占用: 读取 build/<app_name>.map 中的段大小
3. 找最大符号:
   grep -E "^\s+0x[0-9a-f]+\s+0x[0-9a-f]{4,}" build/<app_name>.map | sort -k2 -rn | head -20
4. 常见优化: 见 build-issues.md 内存溢出分析
```

**Kconfig 依赖问题**:
- 如果直接在 prj.conf 中设置 `CONFIG_XXX=y` 但不生效，检查 Kconfig 的 `depends on` 依赖链
- 使用 `./build.sh -S <path> -t menuconfig -DBOARD=<board>` 确认选项可见性
- 常见依赖链: `LISA_XXX_DEVICE → LISA_DEVICE → LISA_OS → MODULE_FREERTOS`

**环境检查**:
- `NUCLEI_TOOLCHAIN_PATH` — GCC 交叉编译工具链路径
- `LISTENAI_TOOLS_PATH` — CMake / Ninja 等构建工具路径
- `ARCS_BASE` — SDK 根目录（build.sh 自动查找，通常无需手动设置）
- build.sh 会向上查找 `listenai-dev-tools/` 目录自动设置这两个变量

#### 3B: 烧录问题诊断

**基础诊断优先表**:

| 证据 | 诊断码 | 下一步 |
|------|--------|--------|
| `ls /dev/ttyACM*` 无输出 | `SERIAL_NOT_FOUND` | 检查 USB 连接、供电和枚举 |
| `fuser /dev/ttyACM0` 返回 PID | `SERIAL_BUSY` | 停止占用串口的监视进程后重试 |
| `serial_read.py` 报 `PermissionError` | `SERIAL_PERMISSION_DENIED` | 修复 `dialout/uucp` 权限后重试 |
| `ls -l build/*.bin` 无结果 | `BIN_NOT_FOUND` | 检查目标路径和构建产物 |
| `cskburn` 非零退出 | `FLASH_FAILED` | 检查串口稳定性和 BOOT/Reset 链路 |

**诊断决策树**:

```text
烧录失败
├─ "Cannot open /dev/ttyACM0"
│   ├─ 串口不存在 → 检查 USB 连接，运行 ls /dev/ttyACM*
│   └─ 权限不足 → sudo usermod -a -G dialout $USER，重新登录
├─ 超时 / 无响应
│   ├─ EVB 自动复位未生效 → 重试 cskburn，确认 ttyACM 未被占用
│   ├─ 波特率过高 → 降低到 1500000 或 921600
│   └─ USB 线材问题 → 换线或换 USB 口
├─ 验证失败
│   ├─ Flash 数据残留 → 先 --erase-all 再烧录
│   └─ 波特率不稳定 → 降低波特率重试
└─ eMMC 烧录失败
    ├─ SDIO 引脚未连接 → 检查 PA4~PA9 连接
    └─ 存储器未供电 → 检查 eMMC/T卡 供电
```

**波特率建议**:
| 场景 | 波特率 | 说明 |
|------|--------|------|
| 稳定烧录 | 1500000 ~ 3000000 | 推荐范围 |
| 快速开发 | 6000000 | 可尝试，部分环境不稳定 |
| 排障降速 | 921600 | 不稳定时使用 |

**烧录目标选择**:
- Flash 烧录（默认）— 直接烧录，无需额外参数
- eMMC/T卡 烧录 — 需要 `--emmc` 参数，通过 SDIO 接口（PA4~PA9）

#### 3C: 串口与调试问题诊断

**串口无输出排查**:

```text
无日志输出
├─ 刚烧录完成或刚连接后无输出
│   └─ 先按 2C 的自动抓取流程：释放串口 → 触发一次复位 → 再抓取
├─ 检查物理连接
│   ├─ 波特率是否 921600？
│   ├─ TX/RX 是否反接？
│   └─ USB 转串口芯片是否被识别？(ls /dev/ttyACM*)
├─ 检查软件配置 (prj.conf)
│   ├─ CONFIG_SYSLOG_UART_BACKEND=y (默认开启)
│   ├─ CONFIG_SYSLOG_UART_DEVICE_UART0=y (确认 UART 端口)
│   └─ CONFIG_SYSLOG_UART_BAUDRATE=921600 (确认波特率)
├─ 检查引脚配置
│   ├─ arcs_evb: PA2(RX) / PA3(TX) (UART0, PAD_A)
│   └─ arcs_mini: PB2(TX) (UART0, PAD_B)
└─ 固件本身问题
    ├─ 固件是否成功烧录？
    └─ main() 是否有日志输出语句？
```

**串口乱码排查**:
- 最常见原因: **波特率不匹配** — 确认终端设置为 921600
- 检查 `CONFIG_SYSLOG_UART_BAUDRATE` 是否与终端一致
- 少数情况: 数据位/校验位设置错误，确认 8N1
- 若使用自动抓取，检查 `/tmp/arcs_serial.log` 中是否主要由替换字符或控制字符组成

**lisa_log 配置诊断**（需 `CONFIG_LOG=y`）:
- 日志级别: `CONFIG_LOG_LEVEL_ASSERT/ERROR/WARN/INFO/DEBUG/VERBOSE`
- 异步模式: `CONFIG_LOG_MODE_ASYNC=y`（默认开启，性能更好）
- 缓冲区大小: `CONFIG_LOG_ASYNC_BUF_SIZE`（默认 10240）
- 单行最大长度: `CONFIG_LOG_LINE_BUF_SIZE`（默认 1024）
- SEGGER RTT 后端: `CONFIG_LOG_BACKEND_SEGGER_RTT=y`（需 J-Link）

**lisa_shell 不响应排查**:
- 确认 `CONFIG_LISA_SHELL=y`
- 确认 syslog UART backend 正常工作（shell 依赖它）
- 检查 stack size 是否足够: `CONFIG_LISA_SHELL_TASK_STACK_SIZE`（默认 1024）
- shell 与 syslog 共享同一 UART 端口

**GDB/JTAG 调试**:
- 需要: J-Link V11+、ARCS J-Link 设备配置文件
- GDB 工具: `${NUCLEI_TOOLCHAIN_PATH}/bin/riscv64-unknown-elf-gdb`
- J-Link 脚本: `jtagscan0.JLinkScript`(core0/AP) 或 `jtagscan1.JLinkScript`(core1/CP)
- SDK 示例默认运行在 **core1(CP核心)**，调试时使用 `jtagscan1.JLinkScript`

---

### Step 4: 经验沉淀

如果在诊断过程中遇到新类型的问题，评估是否值得记录 → 如有，调用 `experience-capture` skill 走确认流程。

---

## 在线文档参考

需要完整参数说明或详细配置时，WebFetch 对应在线文档页面：

| 内容 | 文档路径 |
|------|----------|
| 编译/烧录完整命令和参数 | `get_started.html` |
| cskburn 完整参数 | `tools/burn/README.html` |
| GDB 调试完整指南（含 JLink 安装） | `gdb.html` |
| 板型引脚定义 | `boards/arcs_evb/README.html` 或 `boards/arcs_mini/README.html` |

**串口连接参数（板型相关，此处保留）:**

| 板型 | UART | 引脚 | 波特率 |
|------|------|------|--------|
| arcs_evb | UART0 | PA2(RX)/PA3(TX) | 921600 |
| arcs_mini | UART0 | PB2(TX) | 921600 |
