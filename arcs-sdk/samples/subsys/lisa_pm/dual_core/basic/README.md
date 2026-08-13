# lisa_pm dual_core basic 示例

## 功能说明

本示例是 `samples/drivers/hal/wifi_pm_dual_core` 的 `lisa_pm` 框架版本，先提供双核模式下的 basic 场景：

- **AP 核 (`remote/`)**：作为运行中的 PM 对端核心，冷启动时把 CP 从 flash 拉起，随后通过 `lisa_pm_init()` 初始化 PM 框架，并持续打印 AP 上下文保活日志。
- **CP 核 (`src/`)**：运行 `lisa_pm`、WiFi、LWIP 和应用逻辑；WiFi 获得 IP 后进入 `AUTO_LIGHT_SLEEP`。
- **上下文校验**：AP/CP 都每 5 秒更新一次 magic/counter/checksum，用于观察 light sleep 唤醒后双核上下文是否保持一致。

本示例不使用 `wifi_ps/basic` 那种单核 PM boot-shim 流程，`remote/` 中没有 `arcs_early_startup_hook()` / `ap_startup_dispatch()`。

本示例不包含 GPIO 唤醒和 MQTT；如需验证 GPIO PMU 唤醒，请参考同级的 `gpio_wakeup/` 子工程。

## 使用前修改

`src/main.c` 顶部：

```c
#define TARGET_WIFI_SSID    "YOUR_WIFI_SSID"
#define TARGET_WIFI_PWD     "YOUR_WIFI_PASSWORD"
```

## 关键配置

| 配置 | 说明 |
|------|------|
| `CONFIG_LISA_PM=y` | AP/CP 两侧都启用 lisa_pm 框架，并自动选择 HAL PM/VRTC 模块 |
| `CONFIG_LISA_PM_CORE_PRIMARY=y` | CP 侧作为 PM primary，lisa_pm CMake 自动导出 `PM_CORE_PRIMARY=1` 与 `CFG_VRTC=1` |
| `CONFIG_LISA_PM_CORE_PRIMARY=n` | AP 侧作为 PM peer/proxy，lisa_pm CMake 自动导出 `PM_CORE_PRIMARY=0` 与 `CFG_VRTC_PROXY=1` |
| `CONFIG_LISA_PM_DUAL_CORE=y` | CP 侧声明 AP/CP runtime 双核 PM 模式，避免走 `PM_CLOSE_AP` 单核路径 |
| `CONFIG_ARCS_HAL_IPC=y` | AP/CP 都启用 IPC，使 PM 底层进入 `CONFIG_CORE_NUM=2` 路径；IPC master/slave 角色由 Kconfig choice 自动选择，本示例不再显式配置 |
| `CONFIG_ARCS_HAL_PM_CLOSE_AP=n` | 双核模式 AP 是运行中对端核心，不能睡眠前关闭 AP 子系统 |
| `CONFIG_ARCS_HAL_WCND_WIFI_PS_LDOCORE_TUNE=n` | WiFi PS 初始化阶段不覆盖 LDOCORE tune，避免套用 VDDCORE BUCK 场景参数 |
| `CONFIG_ARCS_HAL_WCND_WIFI_PS_LDOVMEM_TUNE=y` | WiFi PS 初始化阶段覆盖 LDOVMEM tune，匹配当前硬件供电配置 |

## 内存布局

Flash:

| 固件 | Flash base | 烧录 offset |
|------|------------|-------------|
| AP | `0x30000000` | `0x0` |
| CP | `0x30100000` | `0x100000` |

PSRAM:

| 核 | 范围 | 大小 |
|----|------|------|
| AP | `0x28000000` - `0x28400000` | 4MB |
| CP | `0x28400000` - `0x28800000` | 4MB |

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

也可以在 SDK 根目录直接执行：

```bash
./build.sh -C -S samples/subsys/lisa_pm/dual_core/basic -DBOARD=arcs_evb
```

## 烧录

本工程产出两份固件，必须同时烧到 flash：

| 固件 | 路径 | 烧录地址 |
|------|------|----------|
| AP 固件 | `build/remote/ap.bin` | `0x0` |
| CP 固件 | `build/arcs.bin` | `0x100000` |

```bash
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 \
    0x0       build/remote/ap.bin \
    0x100000  build/arcs.bin \
    -C arcs
```

## 预期输出

AP 侧关键日志：

```text
=== lisa_pm dual_core basic AP ===
boot cp from flash: 0x30100000
AP lisa_pm init ok
AP ctx alive: counter=1 checksum=0x...
```

CP 侧关键日志：

```text
=== lisa_pm dual_core basic CP ===
lisa_wifi init done
WiFi connecting to YOUR_WIFI_SSID
WiFi connected
DHCP success on VIF-0: 192.168.x.x
WiFi connected and IP obtained
AUTO_LIGHT_SLEEP enabled (WiFi LISTEN interval=10)
CP ctx alive: counter=1 wifi=1 ip=1 lp=1 wake=WIFI checksum=0x...
CP ctx alive: counter=2 wifi=1 ip=1 lp=1 wake=TIMER checksum=0x...
```

失败判据：

```text
AP CTX CHECK FAILED
CP CTX CHECK FAILED
WiFi/IP wait timeout
assert
backtrace
exception
fault
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_pm_init()` | AP/CP 两侧初始化 LISA PM 框架，内部完成 HAL PM 和 VRTC 初始化 |
| `lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_LISTEN, &cfg)` | 设置 WiFi LISTEN 省电，必须在 `wifi_sta_connect()` 前调用 |
| `net_enable_keep_alive()` | 进入 WiFi light sleep 前打开网络 keep-alive |
| `lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP)` | 允许系统空闲时自动进入 light sleep |
| `lisa_pm_get_wakeup_cause()` | 读取最近一次归一化唤醒原因 |


## 注意事项

1. 本示例演示的是 **PM 双核模式**，不是 WiFi 协议栈/LWIP 异核拆分；CP 侧仍使用 `CONFIG_WIFI_LWIP_SAME_CORE=y`。
2. 双核 PM 依赖 AP/CP 都启用 IPC；若 CP 侧构建结果出现 `CONFIG_PM_CLOSE_AP=1`，说明误走了单核 PM 路径。
3. `LISA_PM_WIFI_PS_LISTEN` 必须在连接 AP 前配置，否则 listen interval 不会生效。
4. 两份固件必须一起烧录；只烧 CP 或只烧 AP 都不能完整验证 PM 双核协同。
5. 本示例依赖外部 WiFi AP，`sample.yaml` 只做 `build_only`。
