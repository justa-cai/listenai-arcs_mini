# EC801E USB AT 4G HTTP 下行测速示例

## 功能说明

本示例用于测试 EC801E 模块通过 USB AT 接口进行 4G HTTP 下载时的下行吞吐能力。

示例保持最小化配置，默认关闭 UI、音频、算法、播放器和云端相关模块，避免这些业务模块影响测速结果。程序会通过 `lisa_modem` 创建 TCP socket，发送 HTTP GET 请求，并持续读取响应体直到文件接收完成。

测试完成后会打印：

- 实际接收的响应体大小
- 响应体接收耗时
- 平均下载速度，单位为 KiB/s 和 Mbps
- 测试前后的信号强度信息

## 硬件连接

- 当前面向 ARCS EVB + EC801E USB ECM 设备
- 将 EC801E 的 USB 口连接到开发板 `USB_ARCS`
- USB 线材需要支持数据通信
- 示例会枚举 `/dev/ttyUSBx`，并匹配 EC801E 的 AT 接口

## 测试地址

```c
#define SPEEDTEST_URL_1M       "http://47.107.81.30:80/test_1M.bin"
#define SPEEDTEST_URL_5M       "http://47.107.81.30:80/test_5M.bin"
#define SPEEDTEST_URL_CUSTOMER "http://test-qiniu-oss1.phone580.net/fzs-box/rk3308ota/music/yiqiannianyihou.mp3"
```

如需更换测试文件，可以在 `src/main.c` 中修改上述 URL。建议使用稳定可访问、响应体大小明确的 HTTP 文件地址。

## 编译

```bash
./build.sh -S arcs-sdk/samples/network/modem_usb_speed -DBOARD=arcs_evb
```

## 日志

测速日志使用以下 tag，调试时可按该关键字过滤：

```text
4g.usb.speed
```

重点关注以下日志内容：

- `signal`：测试前后的 CSQ、RSSI 和 BER
- `target resolved`：域名解析结果
- `app recv`：接收过程中的实时速率、调用次数和等待耗时
- `result`：最终接收大小、耗时和平均速度

## 注意事项

1. 当前示例只匹配 `VID:PID=0x2c7c:0x0903` 的 EC801E USB ECM 设备。
2. 当前示例只使用 EC801E 的 USB AT 接口 `intf=3`。
3. 测速结果受运营商网络、信号强度、服务器带宽和测试文件所在网络环境影响。
4. 如果下载长时间无数据，请先确认模块已注册网络、SIM 卡状态正常，并检查目标 URL 是否可访问。
