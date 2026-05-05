# USB Host EC801E ECM 示例

## 功能说明

演示如何在 ARCS EVB 上使用 CherryUSB Host 接入移远 EC801E，并通过 USB AT 接口自动修复 USB 网卡配置后，再通过 USB ECM 模式把模块当作网卡使用。

本示例复用了 CherryUSB 已有的：

- `class/cdc/usbh_cdc_ecm.c`
- `platform/lwip/usbh_lwip.c`

示例本身负责：

1. 初始化 ARCS USB Host PHY
2. 在 USB 初始化前调用 `tcpip_init(NULL, NULL)`
3. 通过 USB 接口 3（AT 接口）发送 AT 命令
4. 自动检查并修复 `usbnet=1`、`nat=1`
5. 必要时通过 `AT+CFUN=1,1` 触发一次重启并等待重新枚举
6. 通过 `AT+QNETDEVCTL=3,1,1` 拉起 USB 网卡连接
7. 等待 ECM 网卡枚举完成、DHCP 获取 IPv4 地址
8. 优先使用 DHCP 下发的 DNS 做 UDP DNS 探测，失败后回退到阿里公共 DNS `223.5.5.5`
9. 对 `ntp1.aliyun.com:123` 做 UDP NTP 探测
10. 对 `www.baidu.com:80` 做 TCP connect 和 HTTP GET 探测

本示例不接入 `components/lisa_modem`，目标只是先验证 EC801E 的 USB ECM 数据面是否可用。

## 硬件连接

### 开发板

- ARCS EVB 开发板（Host 端）x 1

### USB Host 硬件修改

开发板 `USB_ARCS` Type-C 接口默认工作在 Device 模式，作为 Host 使用必须完成以下修改：

- VBUS 直接供电 5V
- CC1 接 22K 上拉到 VBUS
- CC2 接 22K 上拉到 VBUS

### 模块连接

- EC801E USB 口接到 ARCS EVB 的 `USB_ARCS`
- 线材必须支持数据通信

## EC801E 前置条件

参考 `Quectel_LTE_StandardE系列_AT命令手册_V1.3.pdf`：

```text
AT+CGDCONT=1,"IP","<your_apn>"
```

含义：

- `CGDCONT=1,...`：为 `cid=1` 定义 PDP 上下文和 APN

注意：

- 本示例假设 `cid=1` 的 `CGDCONT/APN` 已经有效，不会自动改写 APN
- 本示例会通过 USB AT 接口自动修复 `usbnet=1`、`nat=1` 并执行 `AT+QNETDEVCTL=3,1,1`
- `AT+QCFG="usbnet"` 和 `AT+QCFG="nat"` 为重启生效，所以 sample 可能主动触发一次模块重启
- ECM 枚举和 DHCP 成功只说明 USB 网卡链路正常，不等于公网数据一定可用
- 示例默认使用中国区可达目标：AliDNS、阿里云 NTP、百度 HTTP

## USB 描述符要点

参考 `Quectel_ECx00ZEC801EEG800Z系列_USB描述符介绍_V1.1.pdf`：

- VID:PID = `0x2c7c:0x0903`
- Interface 0: ECM control
- Interface 1: ECM data
- Interface 2: vendor bulk interface
- Interface 3: AT
- Interface 4: Modem

本 sample 使用：

- Interface 3 作为 USB AT 通道
- Interface 0 和 1 作为 ECM 网卡通道

## 编译

```bash
./build.sh -S samples/subsys/usb/host/cherryusb_ec801e_ecm -C -DBOARD=arcs_evb
```

## 预期输出

```text
========================================
  CherryUSB Host EC801E ECM Example
========================================
[INFO] Initializing lwIP core...
[INFO] Initializing USB Host...
[INFO] Preparing EC801E over USB AT interface...
[AT] Waiting for EC801E USB AT interface (intf=3)...
[AT] Using USB AT interface 3 via /dev/ttyUSBx
[AT] usbnet=1
[AT] nat=1
[AT] QNETDEVCTL type=3 cid=1 urc=1 state=1
[AT] EC801E USB AT preparation complete
[NET] Waiting for ECM netif and DHCP lease...
[I/usbh_core] New device found,idVendor:2c7c,idProduct:0903,...
[I/usbh_core] Loading cdc_ecm class driver on interface 0
[NET] ECM netif is ready
[NET] IPv4 Address : x.x.x.x
[NET] IPv4 Netmask : x.x.x.x
[NET] IPv4 Gateway : x.x.x.x
[NET] DNS[0]       : x.x.x.x
[NET] UDP_DNS=PASS
[NET] UDP_NTP=PASS
[NET] TCP_CONNECT=PASS
[NET] HTTP_GET=PASS
[NET] ECM internet probe passed
```
