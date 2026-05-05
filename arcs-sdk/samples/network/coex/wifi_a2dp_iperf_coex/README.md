# WiFi A2DP iperf Coexistence Sample

## 功能说明

本示例用于在同一个 `arcs_evb` 工程中，手动组合 WiFi 吞吐压测与经典蓝牙 A2DP Source 推流，观察两者的共存影响。

示例默认只完成系统初始化，不会自动执行以下动作：

- 不自动连接 WiFi
- 不自动启动 `iperf` 压测循环
- 不自动扫描或连接蓝牙设备
- 不自动启动 A2DP 音频推流

当前 sample 提供以下能力：

- 初始化 WiFi、经典蓝牙与 BT Audio Framework
- 通过 `wifi connect|disconnect|status` 手动控制联网
- 通过 `iperf start|stop|status|mode` 手动控制吞吐测试
- 通过 `bt_inquiry`、`bt_connect`、`bt_connect_index` 手动控制蓝牙扫描与连接
- 通过 `bt_audio_start`、`bt_audio_stop`、`bt_audio_volume` 手动控制 A2DP Source 推流
- 通过 `coex status` 统一查看 WiFi / iperf / BT / audio 当前状态

## 硬件与测试环境

### 硬件

- `arcs_evb`
- 一台支持 A2DP Sink 的蓝牙耳机或蓝牙音箱
- 一台可作为 `iperf3` 服务端的主机

### 串口

- 日志串口：开发板映射出的串口设备
- 波特率：`921600`

### 运行前配置

运行前请根据实际环境修改 `samples/network/coex/wifi_a2dp_iperf_coex/prj.conf` 中的配置：

```ini
CONFIG_IPERF_WIFI_SSID="your_wifi_ssid"
CONFIG_IPERF_WIFI_PWD="your_wifi_password"
CONFIG_IPERF_SERVER_IP="192.168.1.100"
CONFIG_IPERF_SERVER_PORT=5201
CONFIG_IPERF_MODE_UPLINK=y
```

主机侧启动 `iperf3` 服务端示例：

```bash
iperf3 -s
```

## 示例流程

1. 上电后初始化 shell、文件系统、KV、WiFi、BT 和 BT audio virtual interface
2. 用户手动执行 `wifi connect` 建立 WiFi 连接并等待 DHCP 完成
3. 用户手动执行 `iperf start` 开启吞吐压测循环
4. 用户手动执行 `bt_inquiry` 扫描设备，再通过 `bt_connect` 或 `bt_connect_index` 连接远端 A2DP Sink
5. 用户手动执行 `bt_audio_start` 启动音频推流
6. 通过 `coex status` 和串口日志观察 WiFi / BT 共存状态变化

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## Shell 命令

### WiFi

- `wifi connect`：按 `CONFIG_IPERF_WIFI_SSID/PWD` 发起连接
- `wifi disconnect`：主动断开 WiFi
- `wifi status`：查看 WiFi 初始化、连接、DHCP 与 IP 状态

### iperf

- `iperf start`：启用压测循环；若 WiFi 未 ready，会提示先执行 `wifi connect`
- `iperf stop`：停止当前压测循环
- `iperf status`：查看当前状态、模式和最近一轮测试结果
- `iperf mode uplink|downlink|bidirectional`：设置下一轮测试模式

### 蓝牙与音频

- `bt_inquiry`：扫描周边经典蓝牙设备
- `bt_connect <device_name>`：按设备名连接
- `bt_connect_index <device_index>`：按扫描结果索引连接
- `bt_audio_start`：在 A2DP 已连接后启动 PCM encode 模式推流
- `bt_audio_stop`：停止推流并关闭虚拟播放 profile
- `bt_audio_volume <0-100>`：设置推流音量

### 共存状态

- `coex status`：统一输出 WiFi、iperf、BT、audio 当前状态

## 推荐共存实验

建议按以下顺序验证：

1. `wifi connect` -> `iperf start`
2. `iperf` 运行中执行 `bt_inquiry`
3. `iperf` 运行中执行 `bt_connect <device_name>` -> `bt_audio_start`
4. A2DP 推流运行中执行 `iperf mode bidirectional` -> `iperf start`
5. 在每个阶段执行 `coex status`

## 预期现象

### 启动日志

```text
I/coex            ... WiFi+A2DP iperf coex sample booting
I/coex.wifi       ... wifi init requested
I/coex.iperf      ... iperf init: default_mode=uplink server=192.168.x.x:5201 ...
I/coex.bt         ... bt classic init done
I/coex.bt.audio   ... bt audio virtual interface initialized
```

### WiFi 联网后

```text
I/coex.wifi       ... wifi connected to AP
I/coex.wifi       ... DHCP success on VIF-0: IP=192.168.x.x
```

### iperf 开始后

```text
I/coex.iperf      ... round 1 start, mode=uplink
I/coex.iperf3     ... connect to iperf3 server 192.168.x.x:5201
I/coex.iperf3     ... [ ID] Interval           Transfer     Bitrate
I/coex.iperf      ... round 1 summary: mode=uplink ... reason=complete
```

### 蓝牙扫描 / 连接 / 推流后

```text
I/coex.bt         ... discovered device: XX:XX:XX:XX:XX:XX RSSI=-xx Name=...
I/coex.bt         ... bt classic connected: XX:XX:XX:XX:XX:XX
I/coex.bt         ... a2dp profile connected
I/coex.bt.audio   ... bt audio open requested
I/coex.bt.audio   ... bt audio stream started
```

### `coex status` 输出示例

```text
[wifi]
initialized: yes
stack_ready: yes
connecting: no
connected: yes
dhcp_ready: yes
ip: 192.168.x.x
[iperf]
enabled: yes
state: running
current_test_mode: uplink
pending_test_mode: uplink
...
[bt]
acl_connected: yes
a2dp_connected: yes
audio_streaming: yes
```

## 当前验证状态

- 已完成本地构建验证：`./build.sh -C -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb`
- 当前 README 反映的是已实现命令与构建结果
- 尚未在当前会话中完成 `arcs_evb` 实板烧录、串口日志抓取与完整共存矩阵验证

若需要补齐实板验证，可继续执行：

```bash
./build.sh -F -S samples/network/coex/wifi_a2dp_iperf_coex -DBOARD=arcs_evb
```
