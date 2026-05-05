# WiFi A2DP iperf Coex Sample 设计

## 背景

当前仓库中已经有两个与目标高度相关的 sample：

- `samples/network/iperf_like/`：提供 WiFi 连接、`iperf` shell 控制、吞吐统计和循环压测能力。
- `samples/bluetooth/classic/a2dp_source/`：提供经典蓝牙扫描、按名称或索引连接、A2DP Source 音频发送和对应 shell 命令。

用户希望新增一个专门用于 WiFi/BT 共存性能压测的 sample，并明确要求：

- 目标板型优先支持 `arcs_evb`
- 控制方式为全手动
- WiFi 侧沿用 `iperf_like` 现有命令和模式
- 需要方便测试蓝牙扫描、连接、推流对 WiFi 吞吐的影响，以及 WiFi 压测对蓝牙音频链路的影响

## 目标

新增 sample `samples/network/coex/wifi_a2dp_iperf_coex/`，实现以下能力：

1. 在同一个 sample 中同时初始化 WiFi 和经典蓝牙 A2DP Source 能力
2. 默认仅初始化系统，不自动连接 WiFi、不自动启动 iperf、不自动发起蓝牙扫描和音频推流
3. 保留 `iperf_like` 的 `iperf start|stop|status|mode` 控制方式
4. 新增手动 WiFi 控制命令，支持显式 connect/disconnect/status
5. 保留 `a2dp_source` 的蓝牙扫描、连接、A2DP 推流和音量控制命令
6. 新增统一状态查询命令，便于观察共存场景下 WiFi/BT 当前状态和最近一次吞吐结果

## 非目标

- 不做自动化共存测试编排
- 不在本次实现中引入 BLE provisioning 或双核协处理逻辑
- 不支持新的 BT profile，仅聚焦 classic A2DP Source
- 不扩展 `iperf_like` 到 UDP、多并发流或新协议
- 不把 sample 做成 CI 可自动闭环的串口/蓝牙/网络联动测试

## 总体方案

采用“以 WiFi 为主骨架，按模块并入 BT”的方式实现：

- 以 `samples/network/iperf_like/` 为主骨架，保留其 WiFi 初始化、网络状态判断、吞吐统计、`iperf` shell 入口和测试模式处理逻辑
- 从 `samples/bluetooth/classic/a2dp_source/` 中提取经典蓝牙初始化、发现回调、连接命令、虚拟音频接口和音频发送任务
- 统一到新的 sample 目录中，并按职责拆分为 WiFi、iperf、BT、BT audio、shell 和 main 六个模块

推荐原因：

- WiFi 压测逻辑比 BT 侧更复杂，更适合作为主控制面
- `iperf_like` 已经具备完整的吞吐汇总和 shell 状态接口，复用价值更高
- BT 侧命令天然适合以独立模块方式并入，不需要改动 WiFi 主流程

## 目录与文件结构

新 sample 目录：`samples/network/coex/wifi_a2dp_iperf_coex/`

建议文件结构如下：

```text
samples/network/coex/wifi_a2dp_iperf_coex/
├── CMakeLists.txt
├── Kconfig
├── prj.conf
├── sample.yaml
├── README.md
└── src/
    ├── CMakeLists.txt
    ├── main.c
    ├── coex_app.h
    ├── coex_result.c
    ├── coex_result.h
    ├── coex_wifi.c
    ├── coex_wifi.h
    ├── coex_iperf.c
    ├── coex_iperf.h
    ├── coex_bt.c
    ├── coex_bt.h
    ├── coex_bt_audio.c
    ├── coex_bt_audio.h
    ├── coex_shell.c
    ├── coex_shell.h
    ├── audio_pcm.h
    ├── audio_sbc.h
    └── fs/
        ├── user_fs.c
        └── user_fs.h
```

其中：

- `coex_result.*`：复用 `nettest_result.*` 逻辑，保存最近一轮吞吐结果
- `coex_wifi.*`：复用 `nettest_wifi.c`，改为手动连接模型
- `coex_iperf.*`：复用 `nettest_iperf3.*` 和 `nettest_app.h` 中的状态模型
- `coex_bt.*`：管理 BT 初始化、发现回调、连接状态和对外命令 API
- `coex_bt_audio.*`：管理 A2DP Source 虚拟音频接口、发送任务和流控状态
- `coex_shell.*`：统一导出 `wifi`、`iperf`、`bt_*`、`coex status` 命令
- `main.c`：仅做系统初始化和空闲循环，不承载复杂业务逻辑

## 命令与交互模型

### 1. 启动默认行为

上电后 sample 仅执行初始化：

- 初始化 shell
- 初始化文件系统与 KV
- 初始化 WiFi 子系统（注册回调，不主动连接）
- 初始化经典蓝牙和 BT audio virtual interface

默认不执行以下动作：

- 不自动连 WiFi
- 不自动开始 iperf 轮询
- 不自动蓝牙扫描
- 不自动连接远端设备
- 不自动启动 A2DP 音频发送

### 2. WiFi 命令

保留 `iperf_like` 的 `iperf` 子命令：

- `iperf start`
- `iperf stop`
- `iperf status`
- `iperf mode uplink|downlink|bidirectional`

新增 WiFi 控制命令：

- `wifi connect`
- `wifi disconnect`
- `wifi status`

行为约束：

- `iperf start` 不再隐式触发 WiFi 连接
- 当 WiFi 尚未 ready 时，`iperf start` 直接提示用户先执行 `wifi connect`
- `wifi disconnect` 只影响 WiFi，不自动停止 BT
- `iperf stop` 只停止当前测试循环，不自动断开 WiFi

### 3. 蓝牙命令

保留 `a2dp_source` 的经典蓝牙和音频命令：

- `bt_inquiry`
- `bt_connect <device_name>`
- `bt_connect_index <device_index>`
- `bt_audio_start`
- `bt_audio_stop`
- `bt_audio_volume <0-100>`

行为约束：

- 扫描、连接、推流由用户手动触发
- `bt_audio_start` 依赖已建立的 A2DP 连接；未连接时应输出明确错误提示
- `bt_audio_stop` 只停止音频推流，不影响 WiFi

### 4. 共存状态命令

新增：

- `coex status`

输出内容建议包括：

- WiFi：initialized / connecting / connected / DHCP ready / IP
- iperf：enabled、当前状态、当前模式、待生效模式、最近一轮 tx/rx 吞吐
- BT：discovery callback 是否已注册、是否已连接、是否正在推流

该命令作为共存实验的主观察入口，避免用户在多个命令之间来回切换。

## 初始化顺序设计

`main.c` 中按如下顺序初始化：

1. `lisa_shell_init()`
2. `user_fs_init()`
3. `lisa_kv_init()`
4. `coex_wifi_init()`
5. `coex_bt_init()`
6. `coex_bt_audio_init()`
7. `coex_shell_register()`（如果 shell 导出需要集中入口；若使用静态导出则只需确保相关模块被编入）

注意点：

- WiFi 的 `lisa_wifi_init()` 仍保持异步回调模式，但 `init_done` 中只完成环境准备，不启动 auto-connect
- BT 初始化后需要注册 discovery callback，并初始化 `bt_vintf`
- WiFi 和 BT 共享文件系统 / KV 能力时，避免重复初始化

## 模块详细设计

### WiFi 模块

基于 `samples/network/iperf_like/src/nettest_wifi.c` 改造：

- 保留 MAC manager、DHCP callback、WiFi manager callback
- 去掉 `init_done` 中自动保存 AP 和自动启动 `wifi_mgr_auto_connect_start()` 的行为
- 改为在 `wifi connect` 时使用 `CONFIG_IPERF_WIFI_SSID/PWD` 主动保存 AP 并触发连接
- 新增可查询的 WiFi 状态接口，供 `wifi status` 和 `coex status` 复用
- 新增主动断开接口，供 `wifi disconnect` 使用

### iperf 模块

基于 `samples/network/iperf_like/src/main.c`、`nettest_iperf3.c`、`nettest_shell.c` 改造：

- 保留原有 `nettest_mode_t`、运行状态、最近一轮统计结构
- 取消主循环中的自动重连策略，转为“仅当 WiFi ready 且 `iperf enabled` 时才执行一轮测试”
- `iperf start` 仅启用循环，不再主动触发 WiFi 连接
- 保留 `iperf mode` 的“下一轮生效”语义
- 状态和吞吐统计继续沿用 `iperf_like` 的展示模型

### BT 模块

基于 `samples/bluetooth/classic/a2dp_source/src/main.c` 改造：

- 提取 `lisa_bluetooth_init(NULL)`
- 提取 `lisa_bluetooth_register_discovery_callback(...)`
- 保留按名称和按索引连接逻辑
- 增加简单的连接状态缓存，供 `coex status` 打印
- 所有日志统一改用新的 sample tag 前缀，便于串口抓取共存日志

### BT Audio 模块

基于 `samples/bluetooth/classic/a2dp_source/src/main.c` 的音频任务和 `bt_vintf` 使用方式：

- 默认保持 PCM encode mode，与现有 `a2dp_source` 一致
- 保留循环写入 `audio_pcm` 的任务模型
- 将播放任务、启动、停止、音量设置抽到独立模块
- 保留必要的运行态变量：task handle、是否已启动、是否已打开虚拟 profile
- 保证启动失败或任务创建失败时能清理 profile 状态

## 配置设计

`prj.conf` 以 `samples/network/iperf_like/prj.conf` 为主，并合入 `samples/bluetooth/classic/a2dp_source/prj.conf` 的经典蓝牙和 BT 音频框架依赖。

### 必需的 WiFi / 网络配置

- `CONFIG_WIFI=y`
- `CONFIG_LISA_WIFI=y`
- `CONFIG_WIFI_MANAGER=y`
- `CONFIG_LWIP=y`
- `CONFIG_LISA_NETWORK=y`
- `CONFIG_LISA_KV=y`
- `CONFIG_MAC_MANAGER=y`
- `CONFIG_CJSON=y`

### 必需的 Classic BT / A2DP 配置

- `CONFIG_LISA_BLUETOOTH=y`
- `CONFIG_LISA_BLUETOOTH_CLASSIC=y`
- `CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO=y`
- `CONFIG_LISA_BT_AUDIO_FRAMEWORK=y`
- `CONFIG_LISA_BT_AUDIO_INTERFACE_VIRTUAL=y`
- `CONFIG_LISA_BT_AUDIO_ADAPTER=y`
- `CONFIG_BT=y`
- `CONFIG_BT_DUAL=y`
- `CONFIG_BT_CLASSIC_ROLE_SOURCE=y`

### 公共依赖

- `CONFIG_LISA_SHELL=y`
- `CONFIG_LISA_OS=y`
- `CONFIG_LISA_PORTING=y`
- 文件系统、SDMMC、LSFS、LVFS 相关配置

### 资源配置原则

由于 WiFi + classic BT + BT audio framework 会明显抬高内存和 shell 栈需求：

- `CONFIG_HEAP_SIZE` 和 `CONFIG_PSRAM_HEAP_SIZE` 取两侧 sample 中更保守的较大值
- `CONFIG_LISA_SHELL_TASK_STACK_SIZE` 参考 `a2dp_source` 较高配置，避免命令较多时 shell 栈不足
- 若首次构建出现内存不足，再以 map 文件和链接错误为依据迭代调整

## README 与验证设计

README 应明确该 sample 是“手动共存压测工具”，而不是自动 demo。

建议包含以下内容：

1. 功能说明
2. 构建与烧录方式
3. 依赖环境：WiFi AP、主机侧 `iperf3 -s`、蓝牙耳机/音箱
4. 命令列表
5. 推荐实验步骤

推荐实验矩阵：

1. 基线 WiFi：`wifi connect` -> `iperf start`
2. WiFi 压测中触发蓝牙扫描：`bt_inquiry`
3. WiFi 压测中建立 A2DP 连接并启动音频：`bt_connect ...` -> `bt_audio_start`
4. A2DP 推流期间切换 `iperf mode bidirectional` 并开始压测
5. 分别观察：吞吐变化、音频卡顿、连接成功率、状态切换是否正常

## sample.yaml 设计

`sample.yaml` 初版使用 `build_only: true`。

原因：

- 该 sample 同时依赖外部 WiFi 环境、主机侧 `iperf3` 服务端和蓝牙 sink 设备
- 现有 CI 难以自动闭环验证 WiFi + classic BT + 音频推流的联合场景
- 更合适的验证方式是 README 中提供明确的实板手工验证流程

## 风险与对策

### 1. 资源冲突或内存不足

风险：合并 WiFi 和 BT classic audio 后，堆或任务栈不足导致构建或运行异常。

对策：

- 先按两侧 sample 的高配值合并 `prj.conf`
- 构建阶段优先检查链接错误和 map 文件
- 运行阶段观察 shell、WiFi 和 BT audio 任务是否稳定

### 2. 自动连接逻辑残留导致行为不够“全手动”

风险：直接复用 `nettest_wifi.c` 时，可能仍然在初始化或轮询阶段触发自动连接。

对策：

- 明确删除 `init_done` 内自动连接
- 明确删除主循环中的自动 `request_connect` 逻辑
- 将 WiFi 连接动作收敛到 `wifi connect`

### 3. BT 音频状态和实际链路不同步

风险：A2DP 未真正就绪时就启动音频任务，导致 `vintf_profile_open()` 失败或写音频报错。

对策：

- `bt_audio_start` 前检查连接状态
- 失败路径统一清理 profile 和 task handle
- `coex status` 中区分“BT connected”和“audio streaming”两个维度

## 结论

该方案以 `iperf_like` 为主骨架，保留完整 WiFi 压测能力，并将 `a2dp_source` 的经典蓝牙扫描、连接和 A2DP Source 推流能力以独立模块方式合并进新的 sample `samples/network/coex/wifi_a2dp_iperf_coex/`。

最终结果是一个适合 `arcs_evb` 的手动共存压测 sample：用户可以独立触发 WiFi 连接、iperf 压测、蓝牙扫描、蓝牙连接和 A2DP 推流，从而观察两侧业务在真实共存场景下的性能与稳定性影响。
