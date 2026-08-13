# lisa_pm dual_core GPIO 唤醒示例

## 功能说明

本示例在 [basic 子工程](../basic/README.md) 的 PM 双核运行模式基础上，把 **GPIOB_7 / PB7** 注册为 PMU 唤醒源，演示：

- **AP 核 (`remote/`)**：作为 PM 双核对端，冷启动时引导 CP，随后初始化 `lisa_pm` 并保持 AP 上下文保活。
- **CP 核 (`src/`)**：运行 WiFi、LWIP、`lisa_pm` 与 GPIO wakeup 业务；WiFi 获得 IP 后进入 `AUTO_LIGHT_SLEEP`。
- **GPIOB_7 唤醒**：PB7 配置为输入上拉，PMU wakeup 触发条件为 `LISA_GPIO_WAKEUP_LEVEL_LOW`。
- **上下文校验**：AP/CP 都每 5 秒更新 magic/counter/checksum，用于观察 GPIO/WiFi/Timer 唤醒后双核上下文是否保持一致。

仅 `GPIOB_00..GPIOB_09` 可作 ARCS PMU GPIO 唤醒源。

## 硬件连接

- **GPIOB_7 / PB7**：默认按板载 KEY2 使用，pull-up，按下拉低。
- 若使用外部按键，请确保未按下时为高电平、按下时拉低；进入 sleep 前 PB7 不能保持低电平，否则会立即唤醒。

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
| `CONFIG_LISA_PM_DUAL_CORE=y` | CP 侧声明 AP/CP runtime 双核 PM 模式 |
| `CONFIG_ARCS_HAL_IPC=y` | AP/CP 都启用 IPC，使 PM 底层进入双核路径；IPC master/slave 角色由 Kconfig choice 自动选择，本示例不再显式配置 |
| `CONFIG_LISA_DEVICE=y` | 启用 lisa_device 框架 |
| `CONFIG_LISA_GPIO_DEVICE=y` | 启用 lisa_gpio 设备驱动 |
| `CONFIG_LISA_GPIOB=y` | 启用 GPIOB 设备与 GPIOB wakeup_ops |
| `CONFIG_ARCS_HAL_PM_CLOSE_AP=n` | 双核模式 AP 是运行中对端核心，不能睡眠前关闭 AP 子系统 |
| `CONFIG_ARCS_HAL_WCND_WIFI_PS_LDOCORE_TUNE=n` | WiFi PS 初始化阶段不覆盖 LDOCORE tune，避免套用 VDDCORE BUCK 场景参数 |
| `CONFIG_ARCS_HAL_WCND_WIFI_PS_LDOVMEM_TUNE=y` | WiFi PS 初始化阶段覆盖 LDOVMEM tune，匹配当前硬件供电配置 |

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

也可以在 SDK 根目录直接执行：

```bash
./build.sh -C -S samples/subsys/lisa_pm/dual_core/gpio_wakeup -DBOARD=arcs_evb
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
=== lisa_pm dual_core gpio_wakeup AP ===
boot cp from flash: 0x30100000
AP lisa_pm init ok
AP ctx alive: counter=1 checksum=0x...
```

CP 侧关键日志：

```text
=== lisa_pm dual_core gpio_wakeup CP ===
GPIOB_7 configured as LEVEL_LOW wakeup source
lisa_wifi init done
WiFi connecting to YOUR_WIFI_SSID
WiFi connected
DHCP success on VIF-0: 192.168.x.x
WiFi connected and IP obtained
AUTO_LIGHT_SLEEP enabled (WiFi LISTEN interval=10, wake GPIOB_7)
CP ctx alive: counter=1 wifi=1 ip=1 lp=1 wake=WIFI checksum=0x...
GPIOB_7 IRQ (runtime press)
CP ctx alive: counter=2 wifi=1 ip=1 lp=1 wake=GPIO checksum=0x...
```

失败判据：

```text
AP CTX CHECK FAILED
CP CTX CHECK FAILED
GPIOB_7 is already low
WiFi/IP wait timeout
assert
backtrace
exception
fault
```

其中 `GPIOB_7 is already low` 说明进入睡眠前按键已处于触发电平，应先释放 PB7/KEY2 再观察 GPIO 唤醒。

## 核心 API

```c
lisa_gpio_configure(gpiob, 7, LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
lisa_gpio_configure_wakeup(gpiob, 7, LISA_GPIO_WAKEUP_LEVEL_LOW);
lisa_device_wakeup_enable(gpiob, true);
```

运行态按键中断仅用于观察按键动作；sleep 期间真正唤醒 SoC 的路径是 AON PMU wakeup，不依赖 GPIO controller IRQ。

## 注意事项

1. `GPIOB_7` 运行态由 `lisa_gpiob_pinmux()` 配置为普通 GPIO；进入 sleep 前 PM 底层会根据 wakeup mask 切到 AON wake mux，唤醒后 GPIO system PM 会重新恢复普通 GPIO mux。
2. pull-up 按键 + `LEVEL_LOW` 要求进入 sleep 前 PB7 为高电平；如果 PB7 已经为低，PMU 会在入睡瞬间满足唤醒条件。
3. `AUTO_LIGHT_SLEEP` 仍需 WiFi 连接成功后开启；GPIO 唤醒是在 WiFi light sleep 场景上叠加的唤醒源。
4. 两份固件必须一起烧录；只烧 CP 或只烧 AP 都不能完整验证 PM 双核协同。
