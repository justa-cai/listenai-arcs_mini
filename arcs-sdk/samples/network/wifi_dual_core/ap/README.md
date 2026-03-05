# WiFi 双核运行示例 (AP 核)

## 功能说明

此示例演示如何在双核架构下的 AP (Application Processor) 核上运行 WiFi 协议栈。

在双核 WiFi 架构中：
- **AP 核**：运行 WiFi 协议栈底层，负责 RF 校准和硬件驱动
- **CP 核**：运行 LWIP 网络栈和应用层

两个核心通过 IPC 进行通信，实现 WiFi 功能。

## 硬件连接

本示例使用芯片内部 WiFi 外设，无需额外接线。

**串口输出：**
- 串口 TX: PA21
- 波特率：921600

## 编译运行

### 编译

```{eval-rst}
.. include:: /sample_build.rst
```

### 烧录

```bash
cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 build/ap.bin
```

> **注意**：双核运行时需要配合 CP 核固件一起使用，请参考 [CP 核示例](../cp/README.md)。

## 预期输出

AP 核启动后，将输出 WiFi 初始化日志：

```
MAX STA NUM SUPPORT: 4 
MAX VIF NUM: 1 
MAX BANDWIDTH: BW 20Mhz 
RX BUF SIZE: 1523 
RX BUF NUM: 11 
RX BA NUM: 8 
TX BA NUM: 2 
Set Country : CN success
```

## 核心代码

AP 核的主要工作非常简单：

```c
// 1. 启动 CP 核
IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;

// 2. 初始化 WiFi
lisa_wifi_init();
```

## 相关文档

- [LISA WiFi 组件文档](../../../../components/lisa_wifi/README.md) - 了解 WiFi 初始化的详细说明
- [WiFi 双核 CP 核示例](../cp/README.md) - CP 核（LWIP 侧）的示例说明

## 注意事项

1. **启动顺序**：必须先启动 AP 核，再启动 CP 核
2. **固件地址**：AP 核固件烧录到 0x0，CP 核固件烧录到 0x800000
3. **射频配置**：需要在 `prj.conf` 中配置 `CONFIG_ARCS_HAL_WCND_RF_BOARD_VER=1`

