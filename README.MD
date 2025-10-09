# 快速上手

## 环境搭建

**系统要求：** Ubuntu 18.04 以上

### 自动搭建

1. 运行脚本 `prepare_listenai_tools.sh` 下载开发工具包：

   ```bash
   ./arcs-sdk/prepare_listenai_tools.sh
   ```

2. 运行脚本 `prepare_toolchain.sh` 下载工具链：

   ```bash
   ./arcs-sdk/prepare_toolchain.sh
   ```

### 手动搭建

1. 下载对应平台的工具链后进行解压，如果已经存在工具链，此步骤可跳过：
   - [Linux 下载地址](https://iflyos-external.oss-cn-shanghai.aliyuncs.com/chip_arcs/listenai-linux-amd64-tools.tar.gz)

2. 下载对应平台的 ListenAI 开发工具包：
   - [Linux 下载地址](http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/dev-tools/linux-amd64/v0.0.1/listenai-tools.tar.gz)

3. 设置环境变量：
   - 将 `NUCLEI_TOOLCHAIN_PATH` 指向解压后的**工具链**（步骤1下载的工具）的路径
   - 将 `LISTENAI_TOOLS_PATH` 指向解压后的**ListenAI工具包**（步骤2下载的工具）路径

> **注意：** 目前仅支持 `Linux` 平台，推荐使用 `Ubuntu 18.04` 以上版本

## 编译工程

```bash
./build.sh
```

# 目录说明

```
.
├── arcs-sdk        # ARCS SDK 工程
├── modules         # 存放第三方模块
├── res             # 存放资源文件
├── src             # 存放语音交互代码
├── tools           # 存放工具或者脚本
└── build.sh        # 工程构建脚本
```

# 使用说明

## FLASH 分区说明

| 分区名称 | 分区起始地址 | 备注 |
|----------|-------------|------|
| `boot` | `0x0` | Boot 程序，用于引导/烧录 |
| `ap` | `0x40000` | AP 固件 |
| `app-config.json` | `0xF0000` | App 配置文件 |
| `tone` | `0x100000` | 提示音资源 |
| `wrap.json` | `0x200000` | 算法资源 |
| `esr.bin` | `0x210000` | 算法资源 |
| `aiui.bin` | `0x600000` | CP 固件 |

## 固件及资源烧录

### 进入 ROM 升级模式

在该模式下，可以通过指定工具通过串口烧录固件。

- **进入方式：** 将 `boot` 引脚接地后，复位开发板，此时开发板会进入 ROM 升级模式
- **烧录方式：** 先将 `boot` 引脚悬空（该引脚和烧录串口引脚复用），然后使用 `cskburn` 工具通过串口烧录固件

### 空板烧录

无 `boot` 程序表示空板，空板烧录需要使用 ROM 升级模式进行烧录，将串口板连接开发板的 TX0 RX0，烧录 boot 程序。

```bash
cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 ./res/boot.bin
```

### 非空板烧录

如果已烧录 boot 程序，则可以使用 ADB 命令进行烧录。

在应用开发过程中，可输入命令进入到恢复模式中，在该模式下可进行固件烧录：

```bash
adb shell recovery
```

进入恢复模式后，可通过 `adb push` 命令烧录固件和资源：

```bash
adb push res/ap.bin /RAW/NAND/40000
adb push res/app-config.json /RAW/NAND/F0000
adb push res/tone.bin /RAW/NAND/100000
adb push res/wrap.json /RAW/NAND/200000
adb push res/esr.bin /RAW/NAND/210000
adb push build/aiui.bin /RAW/NAND/600000
```

烧录成功后，运行如下命令重启开发板：

```bash
adb shell reboot hard
```
