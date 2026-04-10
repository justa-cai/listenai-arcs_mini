# AGENTS.md

## 项目概述

ARCS-MINI 语音助手固件，基于 LISTENAI ARCS SoC（RISC-V 双核 AP/CP 架构）。

## 关键路径

- `apps/remote-ap`: AP 固件，负责算法
- `apps/arcs-mini`: CP 固件，主要业务逻辑
- `arcs-sdk/`: SDK 子模块，包含 HAL、驱动、组件和第三方库
- `arcs-sdk/boards/arcs_mini`: ARCS-MINI 板型配置
- `boards`: 其它应用级板型配置
- `res/arcs-mini`: ARCS-MINI 相关静态资源（如提示音、唤醒词等）

如无特别说明，默认构建 `apps/arcs-mini` 应用，使用 `arcs_mini` 板型。

## 配置

该项目使用 Kconfig 组织配置。应用级配置位于 `apps/arcs-mini/prj.conf`。构建后可通过 `build/.config` 确定最终实际生效的配置。

## 环境搭建

在 Linux、Windows (WSL2) 下，使用这些脚本搭建环境：

```bash
./arcs-sdk/tools/scripts/prepare_toolchain.sh
./arcs-sdk/tools/scripts/prepare_listenai_tools.sh
```

之后需要指定 `NUCLEI_TOOLCHAIN_PATH` 和 `LISTENAI_TOOLS_PATH` 环境变量。

其它平台可以直接使用 Docker 镜像：

```bash
docker pull ghcr.io/listenai/arcs-builder:latest --platform linux/amd64
```

## 构建

在 Linux、Windows (WSL2) 下，可以直接使用 `./build.sh` 脚本构建：

```bash
./build.sh -S ./apps/arcs-mini -DBOARD=arcs_mini
```

其它平台需要使用 Docker 构建：

```bash
docker run --rm --init -w $(pwd) -v $(pwd):$(pwd) --platform linux/amd64 ghcr.io/listenai/arcs-builder:latest \
  ./build.sh -S ./apps/arcs-mini -DBOARD=arcs_mini
```

## 烧录

优先使用系统全局的 `cskburn`，如果没有，则使用仓库内 `tools/cskburn` 目录下的二进制。

```bash
cskburn -C arcs -b 3000000 -s /dev/ttyACM0 --verify-all 0x600000 build/arcs-mini.bin
```

* `-s` 指定了串口设备，Linux 下通常是 `/dev/ttyACM0` 或 `/dev/ttyUSB0`，Windows 下可能是 `COM3` 等，Mac 下通常是一个 `/dev/cu.` 开头的路径
* `-b` 指定了烧录波特率，某些串口适配器可以尝试 `6000000` 以达到更快的烧录速度，通常可尝试 `3000000` 或 `1500000`
* 支持多个地址同时烧录，如 `0x40000 ./res/arcs-mini/ap.bin 0x100000 ./res/arcs-mini/tone.bin`，各个分区的地址可参考 `res/arcs-mini/partition_table.json` 中的定义

## 日志

烧录串口同样用于日志输出，波特率为 `921600`。串口适配器的 DTR 连接了设备的烧录模式选择引脚，RTS 连接了设备的复位引脚。因此在打开串口时应当确保 DTR 为高电平，并且可以拉低再拉高 RTS 来复位设备得到最开始的日志输出。

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
