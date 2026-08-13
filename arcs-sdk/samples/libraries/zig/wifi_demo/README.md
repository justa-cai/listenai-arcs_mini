# Zig WiFi Scan 示例

## 功能说明

该示例演示如何在 Zig 中初始化底层 WiFi、等待硬件就绪、启用 STA 模式并扫描周围可见的 AP。示例会将扫描结果以表格形式打印到日志中，用于验证 Zig 对 WiFi 相关 C API 的调用、异步初始化流程以及扫描结果读取逻辑。

## 硬件连接

本示例使用芯片内部 WiFi 外设，无需额外接线。

建议准备：

- 可正常工作的 `arcs_evb` 开发板
- 周围存在可扫描到的 WiFi 热点环境
- 串口日志接收工具

## 示例步骤

1. 初始化 Zig 日志系统
2. 初始化 MAC Manager
3. 通过 `lisa_wifi_init()` 启动 WiFi 硬件初始化
4. 等待 Zig 回调标记 WiFi ready
5. 启用 STA 模式
6. 发起全频道扫描
7. 等待扫描完成事件
8. 打印扫描到的 AP 列表

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

建议将示例路径替换为：

```bash
./build.sh -C -S samples/libraries/zig/wifi_demo -DBOARD=arcs_evb
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

串口中可看到类似日志：

```text
=== Zig WiFi Scan Demo ===
MAC manager OK
WiFi init started...
WiFi hardware ready!
MAC: xx:xx:xx:xx:xx:xx
STA mode enabled
Starting WiFi scan...
Scan done!
Found 5 AP(s)
╔════╦══════════════════════════════════╦═══════╦══════╦════════╗
║ #  ║ SSID                             ║ RSSI  ║  CH  ║  AUTH  ║
...
=== WiFi Scan Complete: 5 AP(s) displayed ===
=== WiFi Scan PASSED ===
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_wifi_init()` | 初始化底层 WiFi 模块 |
| `wifi_sta_mode_enable()` | 启用 STA 模式 |
| `wifi_scan_start()` | 发起 WiFi 扫描 |
| `wifi_event_wait()` | 等待扫描完成事件 |
| `wifi_sta_scanlist_dump()` | 读取扫描结果 |
| `arcs.sleep()` | 在等待硬件 ready 时轮询休眠 |

## 关键代码

```zig
while (!wifi_init_ready) {
    arcs.sleep(100);
}

if (wifi_sta_mode_enable() != 0) {
    return -1;
}

var scan_params = WifiScanParams{};
if (wifi_scan_start(&scan_params) != 0) {
    return -2;
}

if (wifi_event_wait(EVENT_WIFI_SCAN_DONE, 10000) != 0) {
    return -3;
}
```

## 注意事项

- **接口层级**: 该示例使用的是底层 WiFi 接口，而不是更上层的 WiFi Manager
- **配置依赖**: 需要开启 `CONFIG_LISA_WIFI`、`CONFIG_MAC_MANAGER` 等相关配置，当前 sample 的 `prj.conf` 已包含必要选项
- **环境差异**: 运行结果与现场无线环境相关，扫描到的 AP 数量、RSSI 和认证类型会有所不同，因此当前 sample 继续保持 build-only
- **真机调试**: 现在会在 WiFi 初始化等待超过 10 秒时明确报超时失败，便于定位板级 WiFi 未 ready 的问题
- **后续 CI 方向**: 若未来要接入真机 CI，建议改为受控 AP 环境下的固定 SSID / DHCP 成功判定，而不是直接使用扫描结果数量
- **故障排查**: 若 WiFi 初始化失败，可先检查板级 WiFi 配置、日志输出和相关驱动是否正常
