# 4G Modem 模块示例

## 功能说明

本示例演示如何使用 `lisa_modem` 的 socket 风格接口进行蜂窝网络通信，包括：

- modem 初始化
- DNS 域名解析
- TCP socket 建立、发送、接收、关闭
- UDP `sendto` / `recvfrom`
- 打印 UDP 响应源地址

示例内部不再依赖默认全局 modem，而是：

- 通过 `sample_modem_open()` 按 backend 创建 `lisa_modem_t *`
- 通过 `lisa_modem_*_on()` 实例接口执行 DNS / socket 操作

## Backend 模式

本 sample 现在支持两种 AT 传输后端，默认使用 UART。

### 1. UART backend（默认）

- 使用 `src/modem_backend_uart.c`
- 通过 `lisa_modem_create_uart()` 打开 modem
- 默认 AT 设备为 `uart2`
- sample 内部会覆盖 `lisa_uart2_pinmux()`，默认把 `uart2` 配到 `PA15/PA16`

### 2. USB AT backend（overlay）

- 使用 `src/modem_backend_usb_at.c`
- 通过 CherryUSB Host 枚举 `/dev/ttyUSBx`
- 当前仅匹配 `EC801E USB ECM`
- 当前仅使用 AT 接口 `intf=3`
- 通过 `at_transport_usbh_serial_create()` 包装 USB 串口
- 再通过 `lisa_modem_create_with_transport()` 创建 modem

注意：

- “找 AT 口”的逻辑保留在 sample 内部，不放到 `lisa_modem` 或 CherryUSB 侧
- 这个 USB backend 只替换 AT 传输层，不会在 sample 中自动执行 `AT+QCFG="usbnet"`、`AT+QCFG="nat"`、`AT+QNETDEVCTL`
- 如果你的 USB 方案依赖 ECM 数据面，请先用独立流程把模块准备好；本示例仍然走 `lisa_modem` 的 AT socket 能力

## 硬件连接

### UART 模式

- 4G 模块通过 UART 与主控连接
- 默认使用 `uart2`
- 需要根据实际硬件连接修改 `src/main.c` 中的 `MODEM_UART_DEVICE`

### USB AT 模式

- 目前只支持 ARCS EVB 作为 USB Host 接入 EC801E
- 需要把 EC801E 的 USB 口接到开发板 `USB_ARCS`
- 线材必须支持数据通信
- 如果开发板默认是 Device 角色，需要先完成 Host 侧硬件改造

## 示例流程

1. 初始化文件系统和 KV 存储
2. 根据 backend 打开 modem
3. 等待网络注册
4. 解析 `httpbin.org`
5. 建立 TCP 连接并发送 HTTP GET
6. 接收服务器响应
7. 解析 `ntp.aliyun.com`
8. 发送 UDP NTP 请求并通过 `recvfrom()` 读取响应

## 关键配置

使用前请检查 `src/main.c` 中的配置：

```c
#define MODEM_UART_DEVICE   "uart2"
#define TEST_SERVER_HOST    "httpbin.org"
#define TEST_SERVER_PORT    80
#define UDP_TEST_HOST       "ntp.aliyun.com"
#define UDP_TEST_PORT       123
```

## 编译

### UART backend

```bash
./build.sh -C -S samples/network/modem -DBOARD=arcs_evb
```

### USB AT backend

```bash
./build.sh -C -S samples/network/modem -DBOARD=arcs_evb -DCONFIG_FILES=prj_usb_at.conf
```

`prj_usb_at.conf` 会额外启用：

- `CONFIG_SAMPLE_MODEM_BACKEND_USB_AT=y`
- `CONFIG_CHERRYUSB=y`
- `CONFIG_CHERRYUSB_HOST=y`
- `CONFIG_CHERRYUSB_HOST_MUSB_LISA=y`
- `CONFIG_CHERRYUSB_HOST_CDC_ECM=y`
- `CONFIG_CHERRYUSB_HOST_GSM=y`
- `CONFIG_CHERRYUSB_HOST_CDC_ECM_NETIF=n`

同时会切换到适合 CherryUSB Host 的单核内存布局，并保留 EC801E 的 ECM 类驱动加载能力，但不在 lwIP 中注册 USB 网卡，不影响 USB AT 口创建。

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 关键 API

| API | 说明 |
|-----|------|
| `sample_modem_open()` | 按 sample backend 创建 modem |
| `sample_modem_close()` | 销毁 sample 创建的 modem |
| `lisa_modem_dns_resolve_on()` | DNS 解析 |
| `lisa_modem_socket_open_on()` | 创建 TCP/UDP socket |
| `lisa_modem_socket_connect_on()` | 建立 TCP 连接 |
| `lisa_modem_socket_send_on()` | 发送 TCP 数据 |
| `lisa_modem_socket_recv_on()` | 接收 TCP 数据 |
| `lisa_modem_socket_sendto_on()` | 发送 UDP 数据 |
| `lisa_modem_socket_recvfrom_on()` | 接收 UDP 数据并带回源地址 |
| `lisa_modem_socket_close_on()` | 关闭 socket |

## 验证要点

- UART 默认配置可编译
- USB AT overlay 可编译
- DNS 解析
- TCP connect / send / recv / close
- UDP `sendto` / `recvfrom`
- `recvfrom src=` 源地址打印

## 注意事项

1. 使用 USB AT 模式时，当前只支持 `VID:PID=0x2c7c:0x0903` 的 EC801E USB ECM 设备。
2. 使用 USB AT 模式时，当前只扫描并匹配 AT 接口 `intf=3`。
3. 发送和接收都带超时参数，网络较差时请适当调大。
4. UDP 响应依赖运营商网络和目标服务器，可优先关注是否打印 `recvfrom src=...`。

## 相关文档

- [LISA Modem 组件文档](../../../components/lisa_modem/README.md)
- [CherryUSB Host EC801E ECM 示例](../../subsys/usb/host/cherryusb_ec801e_ecm/README.md)
