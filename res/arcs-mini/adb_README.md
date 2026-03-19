# ADB 烧录脚本使用说明

本文档说明 `adb_download.sh` 的常见用法，以及默认烧录行为带来的影响。

## 1. 脚本位置

脚本路径：

```bash
./res/arcs-mini/adb_download.sh
```

建议在仓库根目录执行。

## 2. 基本用法

默认烧录：

```bash
./res/arcs-mini/adb_download.sh
```

说明：

- 默认不会烧录 `boot.bin`
- 会烧录脚本内配置的其余资源和固件：
  - `ap.bin`
  - `tone.bin`
  - `wake_word.bin`
  - `emoji.bin`
  - `respak.bin`
  - `build/arcs-mini.bin`

如果需要连 `boot.bin` 一起烧录：

```bash
./res/arcs-mini/adb_download.sh boot
```

说明：

- 该模式会额外烧录 `boot.bin`


## 3. 默认行为说明

默认情况下，这个脚本会烧录除 `boot.bin` 之外的所有资源文件和主固件。

这意味着如果你直接执行：

```bash
./res/arcs-mini/adb_download.sh
```

那么像 `tone.bin`、`wake_word.bin`、`emoji.bin`、`respak.bin` 这些资源也会被重新写入设备。  
在某些场景下，这会导致设备烧录完成后又去做云端资源更新，看起来像是每次烧录后都会重新更新 `tone.bin` 等资源文件。

如果你的目的只是验证某一个固件改动，这种“全量烧录”通常不是最省事的方式。

## 4. 只烧录某一个固件

如果你只想烧录某一个固件，比如：

```bash
build/arcs-mini.bin
```

建议直接修改脚本里的烧录表。你可以把它理解成“脚本内手写的分区表配置”，对应的是 `adb_download.sh` 里的：

- `ALL_LOCAL_FILES`
- `ALL_REMOTE_PATHS`

例如，如果你只想烧录 `arcs-mini.bin`，可以临时改成：

```bash
ALL_LOCAL_FILES=(
    "build/arcs-mini.bin"
)

ALL_REMOTE_PATHS=(
    "/RAW/NAND/600000"
)
```

这样执行：

```bash
./res/arcs-mini/adb_download.sh
```

就只会烧录 `build/arcs-mini.bin`。

## 5. 常见建议

- 如果你只是修改了应用代码，通常优先只烧录 `build/arcs-mini.bin`
- 如果你修改了提示音、唤醒词、表情资源等，再把对应资源文件加回烧录表
- 只有在确实需要更新 boot 时，才使用 `./res/arcs-mini/adb_download.sh boot`

## 6. 前置条件

- 已安装可用的 `adb`
- 设备已连接，并且 `adb devices -l` 能看到设备状态为 `device`
- 如果是 Windows + WSL 环境，脚本会优先尝试复用 Windows 侧的 `adb`

## 7. 出错时先检查什么

- 本地待烧录文件是否存在
- USB 连接是否稳定
- 设备是否已经授权 ADB
- 设备是否正常进入 recovery
- 当前脚本里的烧录表是否还是你想要的那一组文件
