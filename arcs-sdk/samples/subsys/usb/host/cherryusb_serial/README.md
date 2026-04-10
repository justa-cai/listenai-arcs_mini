# USB Host Serial 示例

## 功能说明

演示如何使用 CherryUSB 在 ARCS 芯片上实现 USB Host Serial 功能，与 USB 串口设备进行通信。

本示例基于 CherryUSB 协议栈实现 USB Host Serial 功能，支持多种 USB 串口设备类型：
- **CDC ACM 设备**：Arduino、部分 USB 转串口设备（已测试）
- **USB 转串口芯片**：CH340、CP2102、FT232、PL2303 等（理论支持，未测试）

示例实现了完整的串口数据回环测试，发送递增测试数据并验证接收的数据正确性。

## 硬件连接

### 开发板
- ARCS EVB 开发板

### USB 连接

- **ARCS EVB 开发板USB硬件修改**（重要）：
  - 开发板的 USB接口是标识为USB_ARCS的type-c硬件接口, 默认用作 Device 模式
  - 作为 Host 使用需要修改硬件：
    - Type-C 接口 VBUS 需要直接供电 5V
    - CC1 和 CC2 线分别需要接上拉 22K 电阻到 VBUS
- **USB 串口设备要求**
  - 因为开发板的硬件USB接口为type-c母座接口, 推荐串口设备的硬件usb接口为type-c公头
  - 对于硬件usb接口为type-c母头的串口设备, 可以用双公头的type-c线(推荐质量好的,可以进行数据通信的线)来连接
  - 对于硬件usb接口为其他类型的串口设备,理论上用usb转接线也可以控制,但没有实际测试过
  - 需要短接串口设备的RX和TX引脚, 方便进行回环测试
- **连接方式**: 将 USB 串口设备插入开发板的标识为USB_ARCS的type-c接口
- **支持设备**: 
  - **已测试**：标准CDC ACM 串口设备
  - **理论支持（未测试）**：CH340、CP2102、FT232、PL2303 等 USB 转串口芯片
  - **理论支持（未测试）**：Arduino 开发板、其他 CDC ACM 设备

## 示例内容

1. 初始化 USB Host 硬件和 PHY
2. 等待 USB 串口设备插入
3. 枚举 USB 设备并识别串口设备类型（CDC ACM 或 USB 转串口芯片）
4. 如果识别为串口设备,就会打开串口设备并配置参数（115200 8N1）
5. 设置控制线状态（DTR、RTS）
6. 执行串口回环测试：
   - 发送 256 字节递增测试数据（0x00-0xFF）
   - 接收并验证数据正确性
   - 显示测试结果
7. 等待设备断开，安全清理资源

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**设备端日志:(标准CDC ACM的串口设备)**

```
********Arcs SDK 0.1.3 @ v0.0.1-1009-g1a8840876ae7********
Running on hart-id: 1
[INIT] Configuring USB PHY for Host mode...
[INIT] USB Host PHY configured:
       - IDDIG = 0 (Host mode)
       - Clock enabled
       - 16-bit data�I/elog            [1034:42:44.197 1 elog_async] EasyLogger V2.2.99 is initia
========================================
  CherryUSB Host CDC ACM Example
==============lize success.
==========================

[INFO] Initializing USB Host...

[USB Event] busid=0, hub_index=0, hub_port=0, intf=255, event=12 (INVALID)
[INFO] USB Host initialized, waiting for device...
[INFO] Please connect USB Serial device (CDC ACM or USB-to-Serial)


[USB Event] busid=0, hub_index=1, hub_port=1, intf=255, event=3 (REMOVED)

[USB Event] busid=0, hub_index=1, hub_port=1, intf=255, event=2 (DISCONNECTED)
[I/usbh_hub] New full-speed device on Bus 0, Hub 1, Port 1 connected
[I/usbh_core] New device found,idVendor:1a86,idProduct:55d3,bcdDevice:0445
[I/usbh_core] The device has 1 bNumConfigurations
[I/usbh_core] The device has 2 interfaces
[W/usbh_core] Do not support Manufacturer string
[I/usbh_core] Product: USB Single Serial
[I/usbh_core] SerialNumber: 5735007032
[I/usbh_core] Enumeration success, start loading class driver

[USB Event] busid=0, hub_index=1, hub_port=1, intf=255, event=5 (INVALID)
[I/usbh_core] Loading cdc_acm class driver on interface 0
[I/usbh_cdc_acm] Ep=83 Attr=03 Mps=16 Interval=01 Mult=00
[I/usbh_serial] Ep=02 Attr=02 Mps=32 Interval=00 Mult=00
[I/usbh_serial] Ep=82 Attr=02 Mps=64 Interval=00 Mult=00
[I/usbh_serial] Register Serial Class: /dev/ttyACM0 (cdc_acm)
[INFO] Creating serial thread...

[USB Event] busid=0, hub_index=1, hub_port=1, intf=0, event=10 (INVALID)
[I/usbh_core] Loading cdc_data class driver on interface 1

[USB Event] busid=0, hub_index=1, hub_port=1, intf=1, event=10 (INVALID)
[INFO] Serial device opened successfully
[INFO] Configuring serial port: 115200 8N1
[INFO] Serial port configured, waiting for device ready...
[1] Still waiting for USB device... (check cable and device)
[INFO] Generating test pattern (incremental sequence 0x00-0xFF)...
[INFO] Start serial loopback test, len: 1024
[TX] Sent 1024 bytes, total: 1024/1024
[INFO] Send over
[INFO] Waiting for loopback data...
[RX] Received 960 bytes, total: 960/1024
[RX] Received 64 bytes, total: 1024/1024
[INFO] Receive over
[INFO] Verifying loopback data...
[SUCCESS] All 1024 bytes verified correctly! (0 errors)

========================================
  Serial Loopback Test: SUCCESS ✓
========================================
[INFO] Closing serial device...
[INFO] Test completed, thread waiting for device disconnect...
[2] Still waiting for USB device... (check cable and device)

```

## 配置说明

### USB 串口设备类型与配置

本示例支持多种 USB 串口设备，不同设备类型需要启用对应的驱动配置：

| 设备类型 | 设备节点 | 需要的配置 | 说明 |
|---------|---------|-----------|------|
| **标准 CDC ACM** | `/dev/ttyACM0` | `CONFIG_CHERRYUSB_HOST_CDC_ACM=y` | Arduino、部分 USB 转串口设备 |
| **CH340/CH341** | `/dev/ttyUSB0` | `CONFIG_CHERRYUSB_HOST_CH34X=y` | 常见的 USB 转串口芯片 |
| **FTDI FT232** | `/dev/ttyUSB0` | `CONFIG_CHERRYUSB_HOST_FTDI=y` | FTDI 系列芯片 |
| **CP210x (CP2102)** | `/dev/ttyUSB0` | `CONFIG_CHERRYUSB_HOST_CP210X=y` | Silicon Labs 芯片 |

### prj.conf 配置示例

```
# USB Serial 设备支持
CONFIG_CHERRYUSB_HOST_CDC_ACM=y      # 标准 CDC ACM 设备
CONFIG_CHERRYUSB_HOST_CH34X=y        # CH340/CH341 芯片
CONFIG_CHERRYUSB_HOST_FTDI=y         # FTDI FT232 芯片
CONFIG_CHERRYUSB_HOST_CP210X=y       # CP210x 芯片
```

**注意**：
- 示例代码会依次尝试打开 `/dev/ttyACM0` 和 `/dev/ttyUSB0`
- 如果只需要支持特定芯片，可以只启用对应的配置以减小固件大小
- 所有这些驱动都会自动选择 `CONFIG_USBHOST_SERIAL`，提供统一的串口抽象层接口

## 核心 API

### USB Host 初始化 API

| API | 说明 |
|-----|------|
| `usbh_initialize()` | 初始化 USB Host 协议栈 |
| `usb_host_init()` | 初始化 USB Host 硬件和 PHY（通过 SYS_INIT 自动调用） |

### Serial 抽象层 API

| API | 说明 |
|-----|------|
| `usbh_serial_open()` | 打开串口设备（/dev/ttyACM0 或 /dev/ttyUSB0） |
| `usbh_serial_close()` | 关闭串口设备 |
| `usbh_serial_control()` | 控制串口设备（设置参数、获取状态等） |
| `usbh_serial_write()` | 发送数据（非阻塞） |
| `usbh_serial_read()` | 接收数据（非阻塞） |

### Serial 回调函数（用户实现）

| 回调函数 | 说明 |
|---------|------|
| `usbh_serial_run()` | 设备连接时被调用，用于启动串口通信线程 |
| `usbh_serial_stop()` | 设备断开时被调用，用于停止串口通信线程 |

## 关键代码

### USB Host 初始化

```c
/* USB Host 硬件初始化（通过 SYS_INIT 自动调用）*/
static int usb_host_init(void)
{
    /* 配置 USB PHY 为 Host 模式 */
    // ... PHY 配置代码 ...
    return 0;
}
SYS_INIT(usb_host_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);

/* 初始化 USB Host 协议栈 */
usbh_initialize(0, 0x41000000UL, usbh_event_handler);
```

### 串口设备打开和配置

```c
/* 打开串口设备（尝试 ttyACM0 或 ttyUSB0）*/
struct usbh_serial *serial;
serial = usbh_serial_open("/dev/ttyACM0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
if (serial == NULL) {
    serial = usbh_serial_open("/dev/ttyUSB0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
}

/* 配置串口参数为 115200 8N1 */
struct usbh_serial_termios termios;
memset(&termios, 0, sizeof(termios));
termios.baudrate = 115200;
termios.stopbits = 0;  // 1 停止位
termios.parity = 0;    // 无校验
termios.databits = 8;
termios.rtscts = false;

ret = usbh_serial_control(serial, USBH_SERIAL_CMD_SET_ATTR, &termios);
```

### 数据收发

```c
/* 发送数据（非阻塞）*/
int ret = usbh_serial_write(serial, send_buffer, send_len);
if (ret > 0) {
    printf("Sent %d bytes\n", ret);
}

/* 接收数据（非阻塞）*/
int ret = usbh_serial_read(serial, recv_buffer, recv_len);
if (ret > 0) {
    printf("Received %d bytes\n", ret);
}
```

### 设备连接和断开回调

```c
/* 设备连接回调 - 启动串口通信线程 */
void usbh_serial_run(struct usbh_serial *serial)
{
    if (serial_is_opened) {
        printf("Serial thread already running\n");
        return;
    }
    serial_is_opened = true;
    serial_device_disconnected = false;  // 重置断开标志
    
    /* 创建串口通信线程 */
    xTaskCreate(usbh_serial_thread, "usbh_serial", 2048, serial, 
                CONFIG_USBHOST_PSC_PRIO + 1, NULL);
}

/* 设备断开回调 - 停止串口通信线程 */
void usbh_serial_stop(struct usbh_serial *serial)
{
    printf("Device disconnected, stopping serial thread\n");
    serial_device_disconnected = true;  // 设置断开标志
    serial_is_opened = false;
    
    /* 给线程一些时间安全退出 */
    vTaskDelay(pdMS_TO_TICKS(200));
}
```

### 设备断开检测和关闭

```c
/* 在发送/接收循环中检测设备断开 */
while (1) {
    if (serial_device_disconnected) {
        printf("Device disconnected\n");
        break;
    }
    
    ret = usbh_serial_write(serial, buffer, len);
    // ... 处理发送 ...
}

/* 关闭串口设备 */
if (serial != NULL) {
    usbh_serial_close(serial);
    serial = NULL;
}
```

## 注意事项

1. **硬件修改要求**（必须）：
   - 开发板 USB 口默认为 Device 模式，作为 Host 使用需要硬件修改
   - Type-C 接口 VBUS 需要直接供电 5V（不能依赖 USB 协商）
   - CC1 和 CC2 线分别需要接 22K 上拉电阻到 VBUS
   - 修改后的硬件才能正确识别和供电给 USB Host 设备

2. **设备兼容性**: 
   - **参考资料**: [Cherryusb Serial Host](https://cherryusb.readthedocs.io/zh-cn/latest/demo/usbh_serial.html)
   - **已测试设备**：CDC ACM 串口设备
   - **理论支持（未测试）**：CH340、CP2102、FT232、PL2303 等 USB 转串口芯片
   - **理论支持（未测试）**：Arduino、其他 CDC ACM 设备

3. **串口参数**: 示例默认配置为 115200 8N1，可根据实际需求修改

4. **回环测试**: 需要将 USB 转串口设备的 TX 和 RX 短接才能进行回环测试，或在 PC 端运行串口回环程序

5. **设备断开**: 示例实现了安全的设备断开处理，拔出设备不会导致系统崩溃

6. **线程管理**: 串口通信线程会等待设备断开后才退出，确保资源正确释放

7. **非阻塞模式**: 发送和接收操作都是非阻塞的，需要在循环中处理部分读写情况

8. **错误处理**: 示例包含完整的错误处理逻辑，所有 API 调用都检查返回值

9. **测试数据**: 使用递增数据模式（0x00-0xFF）便于验证数据完整性和正确性
