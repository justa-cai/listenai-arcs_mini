# EBUS - 轻量级软件总线通讯框架

## 简介

EBUS是一个轻量级的软件总线通讯框架，用于组件间的消息传递。它实现了发布-订阅模式，支持同步消息处理，为嵌入式系统提供了一种高效、灵活的组件间通信机制。

## 特性

- **轻量级设计**：针对资源受限的嵌入式系统优化
- **总线-通道-订阅者模型**：清晰的层次结构
- **同步消息处理**：简化并发控制
- **跨平台支持**：支持FreeRTOS和POSIX环境
- **简单易用的API**：易于集成到现有项目中

## 架构

EBUS框架基于以下核心概念：

- **总线(Bus)**：顶层通信实体，可以包含多个通道
- **通道(Channel)**：特定类型消息的传输通道
- **订阅者(Subscriber)**：接收并处理通道消息的实体

```
+-------------+
|    总线     |
+-------------+
      |
+-------------+     +-------------+     +-------------+
|    通道1    |     |    通道2    |     |    通道3    |
+-------------+     +-------------+     +-------------+
      |                   |                   |
+-------------+     +-------------+     +-------------+
|  订阅者1-1  |     |  订阅者2-1  |     |  订阅者3-1  |
+-------------+     +-------------+     +-------------+
|  订阅者1-2  |     |  订阅者2-2  |
+-------------+     +-------------+
```

## 使用示例

### 完整示例

请参考 `examples/main.c` 文件中的完整示例程序，该示例展示了如何创建总线、通道，以及在进程间进行消息传递。

## 编译与配置

### 配置选项

在 `Kconfig` 文件中提供了以下配置选项：

```
config EBUS
    bool "ebus support"
    default n
    help
        Enable ebus support

choice
    prompt "Select Operating System"
    default EBUS_ENV_OS_FREERTOS

config EBUS_ENV_OS_POSIX
    bool "POSIX"

config EBUS_ENV_OS_FREERTOS
    bool "FreeRTOS"

endchoice
```

### 编译方法

#### 在扫描笔工程中编译

EBUS已集成到扫描笔工程中，可以通过配置`CONFIG_EBUS=y`来启用。

#### 独立编译（Linux环境）

```bash
mkdir build && cd build
cmake ..
make
```

#### 编译示例程序

```bash
cd examples
mkdir build && cd build
cmake ..
make
./ebus_example
```

## 移植指南

EBUS框架设计为易于移植到不同操作系统。目前支持POSIX和FreeRTOS环境。

### 移植到新平台

1. 在 `port/` 目录下创建新的平台适配文件
2. 实现 `platform.h` 中定义的接口
3. 修改 `CMakeLists.txt` 和 `Kconfig` 添加新平台支持

## 许可证

EBUS框架采用Apache 2.0许可证。

## 贡献

欢迎提交问题报告和改进建议。

## 联系方式

如有问题，请联系项目维护者。