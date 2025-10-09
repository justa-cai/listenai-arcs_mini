# SDK 快速开始指南

## 概述
本文档为AI编程工具提供ToyCloud-CP SDK的快速开始指南，包含环境准备、项目构建、烧录部署、调试和故障排除的完整流程。

**项目结构详情请参考**: [项目概览文档](./project-overview.md)

## 环境准备

### 必要工具链
| 工具 | 环境变量 | 说明 |
|------|----------|------|
| GCC工具链 | `NUCLEI_TOOLCHAIN_PATH` | RISC-V交叉编译工具链 |
| 聆思开发工具 | `LISTENAI_TOOLS_PATH` | CMake、Ninja等构建工具 |
| Git LFS | - | 大文件支持 |
| 烧录工具 | - | cskburn (位于tools/burn/) |

### 环境检查命令
```bash
# 检查环境变量
echo $NUCLEI_TOOLCHAIN_PATH
echo $LISTENAI_TOOLS_PATH

# 检查git-lfs
git lfs version

# 检查串口设备
ls /dev/ttyUSB*
```

### 自动环境查找
项目支持自动查找开发工具，工具链目录结构应为：
```
listenai-dev-tools/
├── gcc/                    # NUCLEI_TOOLCHAIN_PATH
└── listenai-tools/         # LISTENAI_TOOLS_PATH
    ├── cmake/
    └── ninja/
```

## 项目构建

### 基础构建命令
```bash
# 默认构建（当前目录）
./build.sh

# 构建指定项目
./build.sh -S samples/helloworld

# 构建指定示例项目
./build.sh -S samples/drivers/gpio
./build.sh -S samples/modules/lvgl
```

### 构建选项
| 参数 | 说明 | 示例 |
|------|------|------|
| `-S, --Source` | 指定源码路径 | `-S samples/helloworld` |
| `-t, --target` | 指定构建目标 | `-t menuconfig` |
| `-C, --Clean` | 清理构建目录 | `-C` |
| `-B, --build` | 指定输出目录 | `-B custom_build` |
| `-r, --release` | Release模式 | `-r` |
| `-w, --warnings-as-errors` | 警告视为错误 | `-w` |

### 常用构建流程
```bash
# 1. 配置项目
./build.sh -S samples/helloworld -t menuconfig

# 2. 清理构建
./build.sh -S samples/helloworld -C

# 3. 正式构建
./build.sh -S samples/helloworld

# 4. Release构建
./build.sh -S samples/helloworld -r
```

## 烧录与部署

### 烧录前必要准备
⚠️ **关键步骤**: 确保设备进入Boot模式
1. **断开设备电源**
2. **根据硬件手册进入boot模式**（通常需要按住特定按键或跳线）
3. **重新连接设备电源**
4. **确认设备状态指示**（观察LED或其他状态指示）

### 设备连接检查
```bash
# 检查USB设备
lsusb

# 检查串口设备
ls -la /dev/ttyUSB*

# 添加用户到dialout组（解决权限问题）
sudo usermod -a -G dialout $USER
sudo usermod -a -G plugdev $USER
# 注销重新登录生效

# 临时权限解决
sudo chmod 666 /dev/ttyUSB0
```

### Flash烧录
```bash
# 基础Flash烧录
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0 build/your_project.bin --verify-all

# 参数说明：
# -s: 串口设备
# -b: 波特率 (3000000 用于烧录)
# 0: 起始地址
# your_project.bin: 固件文件（根据CMakeLists.txt中的project名称）
# --verify-all: 校验烧录结果

# 示例：烧录helloworld项目
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0 build/helloworld.bin --verify-all
```

### eMMC烧录
```bash
# eMMC烧录并校验
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 --emmc 0 emmc.bin --verify-all

# eMMC擦除
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 --emmc --erase 0:0x10000

# eMMC数据读取
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 --emmc --read 0:0x10000:read.bin

# eMMC校验
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 --emmc --verify 0:0x10000
```

## 调试与日志

### 串口调试
```bash
# 使用picocom（推荐，包含完整专业参数）
picocom -b 921600 /dev/ttyUSB0 --lower-dtr --lower-rts --imap=lfcrlf

# 使用minicom
sudo minicom -D /dev/ttyUSB0 -b 921600

# 注意：日志监控波特率通常为921600，与烧录波特率3000000不同
```

### 串口连接故障排除
```bash
# 检查串口占用情况
sudo lsof /dev/ttyUSB0

# 终止占用进程
sudo kill <进程PID>

# 如果串口工具未安装
sudo apt update && sudo apt install picocom minicom
```

### 日志系统
参考 [日志系统文档](./log-system.md) 了解详细配置：
- 日志级别设置
- 输出格式配置
- 性能优化选项

### 调试工具
```bash
# GDB调试（如果支持）
$NUCLEI_TOOLCHAIN_PATH/bin/riscv64-unknown-elf-gdb

# 内存分析
$NUCLEI_TOOLCHAIN_PATH/bin/riscv64-unknown-elf-objdump -h build/app.elf
$NUCLEI_TOOLCHAIN_PATH/bin/riscv64-unknown-elf-size build/app.elf
```

## 快速开发路径

### 开发入口目录
- `samples/`: 示例代码，AI开发的最佳起点
- `components/`: 可复用的系统组件
- `modules/`: 第三方模块库

### 常用示例项目
```bash
# Hello World入门
./build.sh -S samples/helloworld

# 硬件驱动示例
./build.sh -S samples/drivers/gpio
./build.sh -S samples/drivers/uart

# 功能模块示例
./build.sh -S samples/modules/lvgl
./build.sh -S samples/modules/cjson
```

## AI工具操作模板

### 创建新项目
```bash
# 1. 复制示例项目
cp -r samples/helloworld my_project/

# 2. 修改CMakeLists.txt中的项目名
# 3. 构建测试
./build.sh -S my_project/
```

### 添加新组件
```bash
# 1. 在components/目录创建组件
mkdir components/my_component/
# 2. 创建CMakeLists.txt和源码文件
# 3. 在项目中引用组件
```

### 配置系统选项
```bash
# 运行配置界面
./build.sh -S your_project -t menuconfig

# 或直接编辑配置文件
# 修改 sdkconfig 或 *.conf 文件
```

### 测试验证流程
```bash
# 1. 编译检查
./build.sh -S your_project -w

# 2. 烧录测试
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0 build/your_project.bin --verify-all

# 3. 串口监控
picocom -b 921600 /dev/ttyUSB0 --lower-dtr --lower-rts --imap=lfcrlf
```

## 故障排除

### 编译错误
| 错误类型 | 可能原因 | 解决方案 |
|----------|----------|----------|
| 工具链未找到 | 环境变量未设置 | 设置 NUCLEI_TOOLCHAIN_PATH |
| CMake错误 | 聆思工具未找到 | 设置 LISTENAI_TOOLS_PATH |
| CMake缓存冲突 | 构建目录存在冲突缓存 | 使用 `-C` 选项清理构建 |
| 头文件未找到 | 包含路径错误 | 检查CMakeLists.txt配置 |
| 链接错误 | 库文件缺失 | 检查依赖组件是否正确引入 |

### 烧录问题
| 错误类型 | 可能原因 | 解决方案 |
|----------|----------|----------|
| 烧录等待超时 | 设备未进入boot模式 | 按硬件手册操作进入boot模式 |
| 权限问题 | 用户无串口权限 | `sudo usermod -a -G dialout $USER` |
| 设备不存在 | USB连接问题 | 检查USB连接和驱动 |
| 烧录失败 | 波特率或硬件问题 | 降低波特率到115200重试 |

```bash
# 权限问题解决
sudo chmod 666 /dev/ttyUSB0
# 或永久解决
sudo usermod -a -G dialout $USER

# 设备检查
ls /dev/ttyUSB*
lsusb

# 烧录重试（降低波特率）
./tools/burn/cskburn -s /dev/ttyUSB0 -b 115200 0 build/your_project.bin --verify-all
```

### 串口调试问题
| 错误类型 | 可能原因 | 解决方案 |
|----------|----------|----------|
| 串口占用 | 其他进程使用串口 | 使用lsof查找并终止占用进程 |
| 无法连接 | 权限或设备问题 | 检查权限和设备状态 |
| 无日志输出 | 波特率不匹配 | 确认使用921600波特率 |
| 乱码输出 | 参数配置错误 | 使用推荐的picocom参数 |

```bash
# 串口占用解决
sudo lsof /dev/ttyUSB0
sudo kill <进程PID>

# 重新连接
picocom -b 921600 /dev/ttyUSB0 --lower-dtr --lower-rts --imap=lfcrlf
```

### 运行时问题
```bash
# 系统无响应
# 1. 检查电源供应
# 2. 复位设备
# 3. 重新烧录固件

# 串口无输出
# 1. 检查波特率配置（应为921600）
# 2. 确认日志级别设置
# 3. 检查串口连接和参数
```

### 环境问题
```bash
# Git子模块更新失败
git submodule update --init --recursive --force

# Git LFS文件损坏
git lfs pull --force

# 工具链版本不兼容
# 更新到推荐版本或降级到稳定版本
```

## 性能优化建议

### 编译优化
```bash
# Release模式构建
./build.sh -S your_project -r

# 并行编译（默认-j4）
# 可在build.sh中调整并行度
```

### 固件优化
- 启用编译器优化选项
- 移除调试信息（Release模式）
- 使用合适的内存配置

### 开发效率
- 使用增量编译（避免频繁使用-C选项）
- 合理配置日志级别
- 利用menuconfig快速配置

## 版本兼容性

### 支持的平台
- Linux (推荐Ubuntu 20.04+)
- WSL2 (Windows子系统)
- macOS (部分支持)

### 工具链要求
- GCC版本: 推荐10.x以上
- CMake版本: 3.15+
- Python版本: 3.6+ (部分工具需要)

## 验证案例

### HelloWorld示例完整流程
以下是一个经过验证的完整开发流程：

```bash
# 1. 环境检查
echo $NUCLEI_TOOLCHAIN_PATH $LISTENAI_TOOLS_PATH
git lfs version
ls /dev/ttyUSB*

# 2. 编译项目
./build.sh -S samples/helloworld -C  # 清理构建
./build.sh -S samples/helloworld     # 正式构建

# 3. 准备烧录（确保设备进入boot模式）
# 断电 → 进入boot模式 → 上电

# 4. 烧录固件
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0 build/helloworld.bin --verify-all

# 5. 观察运行日志
picocom -b 921600 /dev/ttyUSB0 --lower-dtr --lower-rts --imap=lfcrlf
```

**预期输出**:
```
********Arcs SDK@V0.0.14-18-gb2293f33-@v0.0.14********
Running on hart-id: 1
Power Lock!
Hello, world!
```

---
**注意**: 本文档基于实际验证结果编写，为AI编程工具提供标准化操作流程。在实际使用中请结合具体硬件和项目需求进行调整。 