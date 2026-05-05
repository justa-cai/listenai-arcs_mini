# iperf3 TCP 吞吐示例

## 功能说明

演示如何在 ARCS SDK 中使用`iperf3`协议进行 TCP 吞吐测试，设备上电后会自动连接指定 WiFi，待网络就绪后循环执行吞吐测试。

示例支持`uplink`、`downlink`和`bidirectional`三种模式，可单独验证上行或下行，也可在双向同时运行时观察上下行吞吐变化。

### 新特性

- **自动测试循环**: 上电后自动联网并开始吞吐测试，无需手动输入 shell 命令
- **断网自动恢复**: WiFi 断开后暂停测试，重连并重新获取 IP 后自动继续
- **多模式支持**: 支持`uplink`、`downlink`和`bidirectional`三种测试模式
- **Shell 可控**: 支持通过`iperf start`、`iperf stop`、`iperf status`、`iperf mode ...`控制运行状态和测试模式

## 硬件连接

无需外部连接，WiFi 为芯片内部资源。

**串口输出：**
- **日志串口**: 使用当前开发板映射出的串口设备
- **波特率**: `921600`

## 测试环境准备

运行本示例前，需要准备可连接的 WiFi 热点和一台用于启动`iperf3`服务端的主机。

### 主机工具下载

主机侧使用标准`iperf3`服务端，仓库内不再附带 Windows 压缩包，请从官方渠道获取：

- 官方发布页：<https://github.com/esnet/iperf/releases>
- 官方文档：<https://software.es.net/iperf/invoking.html>

### 配置 WiFi 和服务端地址

运行前请根据实际测试环境修改以下配置：

```ini
CONFIG_IPERF_WIFI_SSID="ssid"
CONFIG_IPERF_WIFI_PWD="password"
CONFIG_IPERF_SERVER_IP="192.168.1.100"
CONFIG_IPERF_SERVER_PORT=5201
CONFIG_IPERF_MODE_UPLINK=y
```

### Windows 主机使用方式

```powershell
cd C:\path\to\iperf-3.x-win64
.\iperf3.exe -s
```

### Ubuntu 主机下载方式

```bash
sudo apt update
sudo apt install -y iperf3
```

### Ubuntu 主机使用方式

```bash
iperf3 -s
```

## 示例步骤

1. 初始化 shell、WiFi 和吞吐测试运行环境
2. 连接配置好的 WiFi 热点并等待 DHCP 完成
3. 按当前模式连接主机侧 `iperf3` 服务端并执行一轮吞吐测试
4. 输出区间吞吐信息和每轮汇总结果
5. 根据轮次间隔继续执行下一轮测试
6. 网络断开时暂停测试，网络恢复后自动继续执行下一轮

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**板端日志：**

```text
I/iperf           ... ARCS SDK iperf-like sample starting
I/iperf           ... mode: iperf3 protocol
I/iperf           ... default test mode: uplink
I/iperf           ... target server: 192.168.x.x:5201
I/iperf.wifi      ... wifi connected to AP
I/iperf.wifi      ... DHCP success on VIF-0: IP=192.168.x.x
I/iperf           ... round 1 start, mode=uplink
I/iperf3          ... connect to iperf3 server 192.168.x.x:5201
I/iperf3          ... [ ID] Interval           Transfer     Bitrate
I/iperf3          ... [  8]   0.00-1.00   sec  1.16 MBytes  9.75 Mbits/sec
...
I/iperf3          ... [  8]   0.00-10.00  sec  11.5 MBytes  9.63 Mbits/sec  sender
I/iperf           ... round 5 summary: mode=uplink duration=10000 ms tx_bytes=12036240 tx_throughput=9628992 bps rx_bytes=0 rx_throughput=0 bps errors=0 reason=complete
```

**主机侧 `iperf3` 输出：**

```text
-----------------------------------------------------------
Server listening on 5201 (test #1)
-----------------------------------------------------------
Accepted connection from 192.168.x.x, port 55815
[  5] local 192.168.x.x port 5201 connected to 192.168.x.x port 55816
[ ID] Interval           Transfer     Bitrate
[  5]   0.00-1.00   sec  1.10 MBytes  9.20 Mbits/sec
...
[  5]   0.00-10.00  sec  11.6 MBytes  9.67 Mbits/sec                  receiver
-----------------------------------------------------------
```

**模式切换后板端汇总：**

```text
I/iperf           ... round 7 summary: mode=downlink duration=10001 ms tx_bytes=0 tx_throughput=0 bps rx_bytes=11766140 rx_throughput=9411970 bps errors=0 reason=complete
I/iperf           ... round 2 summary: mode=bidirectional duration=10002 ms tx_bytes=6206460 tx_throughput=4964175 bps rx_bytes=6206460 rx_throughput=4964175 bps errors=0 reason=complete
```

## 核心 API

| API | 说明 |
|-----|------|
| `nettest_wifi_init()` | 初始化 WiFi 相关资源和事件处理流程 |
| `nettest_wifi_request_connect()` | 触发 WiFi 连接并等待网络恢复 |
| `nettest_runner_iperf3_run()` | 按当前测试模式执行一轮 `iperf3` 吞吐测试 |
| `nettest_app_set_enabled()` | 通过 shell 启停自动测试循环 |
| `nettest_app_set_mode()` | 设置下一轮测试模式 |
| `nettest_app_get_status()` | 获取当前测试状态、测试模式和最近一次测试结果 |

## 关键代码

```c
/* 打印测试模式和目标服务端信息 */
LOGI("ARCS SDK iperf-like sample starting");
LOGI("mode: iperf3 protocol");
LOGI("default test mode: %s", nettest_mode_str(NETTEST_DEFAULT_MODE));
LOGI("target server: %s:%d", NETTEST_SERVER_IP, NETTEST_SERVER_PORT);

while (1) {
    /* Shell 停止测试后进入 stopped 状态，等待再次启动 */
    if (!g_nettest_enabled) {
        nettest_app_set_state_locked(NETTEST_APP_STATE_STOPPED);
        vTaskDelay(pdMS_TO_TICKS(200));
        continue;
    }

    /* 网络未就绪时触发 WiFi 重连并等待 DHCP 完成 */
    if (!nettest_wifi_is_ready()) {
        nettest_app_set_state_locked(NETTEST_APP_STATE_WAIT_NETWORK);
        (void)nettest_wifi_request_connect();
        vTaskDelay(pdMS_TO_TICKS(500));
        continue;
    }

    /* 网络就绪后应用待生效模式，并执行一轮 iperf3 测试 */
    nettest_app_set_state_locked(NETTEST_APP_STATE_RUNNING);
    round_result.mode = g_nettest_mode;
    ret = nettest_runner_iperf3_run(g_nettest_mode, &g_nettest_enabled, &round_result);
}
```

## 配置说明

### 连接配置

以下配置项会直接影响主机连接和吞吐结果：

- **`CONFIG_IPERF_WIFI_SSID`**: 目标 WiFi 热点名称
- **`CONFIG_IPERF_WIFI_PWD`**: 目标 WiFi 热点密码
- **`CONFIG_IPERF_SERVER_IP`**: 运行`iperf3`服务端的主机 IP 地址
- **`CONFIG_IPERF_SERVER_PORT`**: `iperf3`服务端监听端口，默认`5201`
- **`CONFIG_IPERF_MODE_UPLINK`**: 默认模式为上行测试
- **`CONFIG_IPERF_MODE_DOWNLINK`**: 默认模式为下行测试
- **`CONFIG_IPERF_MODE_BIDIRECTIONAL`**: 默认模式为双向同时测试

### 测试参数配置

常用吞吐测试参数如下：

- **`CONFIG_IPERF_ROUND_SECONDS`**: 单轮吞吐测试持续时间，单位为秒
- **`CONFIG_IPERF_SEND_BLOCK_SIZE`**: 每次`send()`的发送块大小，推荐使用 **1460**
- **`CONFIG_IPERF_ROUND_INTERVAL_MS`**: 每轮测试结束后的等待时间，单位为毫秒
- **`CONFIG_IPERF_CONNECT_TIMEOUT_MS`**: TCP 连接服务端的超时时间，单位为毫秒

### Shell 控制命令

- **`iperf start`**: 启动自动测试循环
- **`iperf stop`**: 停止当前测试并暂停自动循环
- **`iperf status`**: 查看当前状态、当前模式、待生效模式和最近一轮测试结果
- **`iperf mode uplink|downlink|bidirectional`**: 设置下一轮测试模式

## 验证方法

以下结果来自 **2026-03-31** 在`arcs_evb`实板上的验证，可按以下流程复现：

1. 在主机侧启动`iperf3 -s`
2. 在 Kconfig 中配置 WiFi 热点信息和主机 IP 地址
3. 根据需要选择默认模式，或在运行时通过`iperf mode ...`设置下一轮模式
4. 编译并烧录`samples/network/iperf_like`
5. 观察板端串口日志，确认 WiFi 连接成功并自动开始测试
6. 观察主机侧输出，确认能够按所选模式打印发送端或接收端统计结果
7. 在 shell 中执行`iperf stop`、`iperf start`、`iperf status`、`iperf mode uplink|downlink|bidirectional`验证控制逻辑

### 验证环境

- **板型**: `arcs_evb`
- **测试日期**: `2026-03-31`
- **主机侧命令**: `iperf3 -s`
- **测试模式**: `uplink`、`downlink`、`bidirectional`

### 吞吐量结论

基于本次实板日志，当前示例在 **单连接 TCP、10 秒测试窗口** 下得到如下结果：

1. **上行模式（uplink）**: 板端 `tx_throughput=9628992 bps`，约 **9.63 Mbit/s**
2. **下行模式（downlink）**: 板端 `rx_throughput=9411970 bps`，约 **9.41 Mbit/s**
3. **双向模式（bidirectional）**: 板端 `tx_throughput=4964175 bps`、`rx_throughput=4964175 bps`，上下行同时约 **4.96 Mbit/s**

### 数据口径说明

- **最终口径**: 优先采用板端 `round summary` 中的 `tx_throughput` 和 `rx_throughput`
- **交叉校验**: `sender` / `receiver` 汇总行可用于与 `round summary` 结果交叉校验
- **区间日志**: 单秒区间吞吐仅用于观察波动范围，不作为最终吞吐结论
- **适用范围**: 该结果仅对应当前板型、测试日期、网络环境、主机版本和默认测试参数

## 注意事项

1. **协议模式**: 本文档仅覆盖`iperf3`测试路径，主机侧需要运行标准`iperf3`服务端
2. **参数匹配**: `CONFIG_IPERF_SERVER_IP` 和 `CONFIG_IPERF_SERVER_PORT` 必须与主机侧实际监听地址一致
3. **热点环境**: 使用移动热点或局域网测试时，应先确认主机网卡的实际 IP 地址，再写入 `CONFIG_IPERF_SERVER_IP`
4. **模式切换**: `iperf mode ...`修改的是下一轮测试模式，不会中断当前正在运行的一轮测试
5. **能力边界**: 当前版本支持 TCP Client 上行、下行和双向测试，不支持 UDP 和多并发流
