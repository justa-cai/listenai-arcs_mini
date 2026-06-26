---
name: device
description: 处理本仓库设备侧适配与配置任务。用户提到硬件、引脚、PAxx/GPIOx、pinmux、外设复用、PWM、GPIO、I2C、SPI、UART、LCD、LCM、display、panel、屏幕、摄像头、音频、传感器、驱动接口、硬件选型、adb shell、device set_pid、device set_sid、reboot、绑定云端应用到设备或把 PID/SID 写入设备时触发。
---

# 设备适配

## 兼容约定

- 本技能同时适用于 Claude Code 和 Codex；不要依赖单一客户端专属工具名。
- 下文 `/xxx` 是触发示例；客户端不支持 slash 命令时，用对应自然语言请求即可。
- 命令默认从仓库根目录执行；不要硬编码个人机器上的绝对路径。
- 默认目标为 `apps/arcs-mini` 应用和 `arcs_mini` 板型；如果用户指定其它 app、board 或 `boards/*/overlay.conf`，以用户指定为准。
- 需要查询云端应用 PID/SID 时，先检查是否安装 `ling`(https://github.com/LISTENAI/ling), 然后使用 `ling app list --json` / `ling app inspect <project_id> --json`查询；本技能只负责编排写入设备。

## 使用方式

`/device <设备需求描述>`

示例：
- `/device 查询 PAxx 能否作为 PWM 输出`
- `/device 新增一个 GPIO 控制外设`
- `/device 更换 LCD panel 并调整分辨率`
- `/device 查看某个外设应该使用哪个 LISA 驱动接口`
- `/device 绑定云端某某应用到设备`

## 常用路径

| 路径 | 用途 |
| --- | --- |
| `arcs-sdk/boards/arcs_mini/README.md` | 板级引脚说明 |
| `arcs-sdk/boards/arcs_mini/pinmux.h` | 引脚别名与板级 pin 定义 |
| `arcs-sdk/boards/arcs_mini/pinmux.c` | 板级外设复用配置 |
| `arcs-sdk/boards/arcs_mini/projects_data.json` | pinmux 工具生成数据，可反查 pin 与外设映射 |
| `boards/` | 应用级板型覆盖配置 |
| `apps/arcs-mini/prj.conf` | ARCS-MINI 应用 Kconfig 配置 |
| `arcs-sdk/drivers/` | LISA 驱动接口、Kconfig、README 和实现 |
| `arcs-sdk/samples/drivers/` | 驱动示例 |
| `src/shell/cmd/id_cmd.c` | `device set_pid` / `device set_sid` / `device get_*` / `device get_device_id` / `device reset` 命令实现 |
| `src/middleware/kv/kv_user.h` | 运行时 `user.pid` / `user.sid` KV key 定义 |
| `src/category/comm/app_datas.c` | 云端 PID/SID 加载链路：ROMFS `config.ini` -> KV 覆盖 -> Kconfig 默认值兜底；最终优先级为 KV > ROMFS > Kconfig |
| `build/.config` | 构建后最终生效的配置 |

## 查询边界

- 单个硬件问题先查本技能列出的路径，不做全仓库长时间泛搜。
- 引脚到外设的复用关系最多做三类检查：板级 `README.md`/`pinmux.*`、SDK 或应用级 `projects_data.json`、目标 LISA 驱动目录。
- 三类检查后仍找不到映射时，停止搜索并回答“当前仓库未确认该复用”；不要继续查 `git blame`、网页、PDF、整仓 `arcs-sdk/soc`，除非用户明确要求深入查芯片手册或 pinmux 工具。
- 只回答选型类问题时，只列出现有支持项和约束，不进入源码实现搜索。
- 云端应用绑定问题不要泛搜源码；先用 `ling` CLI 获取 PID/SID，再用设备 shell 写入并验证。
- 不要在最终回复里明文展示 SID；需要说明时只展示掩码或后 4 位。

## 执行流程

### 1. 明确设备需求

先提取以下信息：

- 目标 app / board / overlay。
- 引脚或外设名称，例如 `PA08`、`GPIOA_08`、`LCD_PWM`、`SPI0`。
- 外设类型，例如 PWM、GPIO、I2C、SPI、UART、display panel。
- 设备侧操作，例如 ADB shell、写 KV、绑定云端应用、重启设备。
- 期望行为，例如输出波形、读取输入、控制屏幕、切换接口、新增设备或写入 PID/SID。
- 是否需要修改代码、只回答选型、只操作设备，还是需要构建验证。

### 2. 查询引脚和复用

按顺序检查：

```bash
rg -n "<PAxx|GPIOx|外设名>" \
  arcs-sdk/boards/arcs_mini/README.md \
  arcs-sdk/boards/arcs_mini/pinmux.h \
  arcs-sdk/boards/arcs_mini/pinmux.c \
  boards apps/arcs-mini/prj.conf -S

python3 - <<'PY'
import json
# 默认查 SDK 板级数据；若用户指定 boards/<board>/projects_data.json，优先改用应用级数据。
p='arcs-sdk/boards/arcs_mini/projects_data.json'
with open(p) as f:
    data=json.load(f)
print('pinConfig:', data.get('pinConfig', {}).get('<PAxx>'))
print('peripheralConfig:', data.get('peripheralConfig', {}).get('<外设名>'))
PY
```

重点看：

1. `arcs-sdk/boards/arcs_mini/README.md` 是否列出该 pin 的板级用途。
2. `pinmux.h` 是否有别名定义。
3. `pinmux.c` 是否已在对应 `lisa_<peripheral>_pinmux()` 中配置。
4. `projects_data.json` 中该 pin 是否已映射到目标外设，或目标外设当前使用哪些 pin；用户指定应用级 board 时优先查 `boards/<board>/projects_data.json`。
5. `boards/*/pinmux.c` 或 overlay 是否覆盖了 SDK 板级默认配置。

如果仓库里找不到目标 pin 到目标外设的映射，不要直接假定可用；说明当前仓库未确认该复用，并给出替代方案：使用已有映射引脚、用 pinmux 工具/芯片手册确认，或新增板级覆盖。

### 3. 选择驱动接口

优先使用 LISA 设备驱动，不直接从业务代码调用 HAL。

| 外设/需求 | 优先查找 |
| --- | --- |
| PWM、舵机、背光、蜂鸣器、简单电机 | `arcs-sdk/drivers/lisa_pwm/` |
| GPIO 输入输出 | `arcs-sdk/drivers/lisa_gpio/` |
| 显示屏、panel、LCD/LCM | `arcs-sdk/drivers/lisa_display/` |
| 摄像头 | `arcs-sdk/drivers/lisa_camera/` |
| 其它外设 | `arcs-sdk/drivers/` 下对应 `lisa_*` 目录 |

接口确认方式：

```bash
find arcs-sdk/drivers -maxdepth 2 -type f | sort
rg -n "lisa_.*(set|get|init|enable|disable|configure)|LISA_DEVICE_REGISTER" arcs-sdk/drivers arcs-sdk/samples -S
```

写代码时检查：

- 是否包含正确头文件。
- 对应驱动 Kconfig 和 `build/.config` 中的实际符号是否启用；不要按模板猜配置名，例如 PWM 是 `CONFIG_LISA_PWM`，GPIO 是 `CONFIG_LISA_GPIO_DEVICE` + `CONFIG_LISA_GPIOA/B`，Display 是 `CONFIG_LISA_DISPLAY_DEVICE`，Camera 是 `CONFIG_LISA_CAMERA_DEVICE`。
- 是否通过 `lisa_device_get()` 获取设备并检查 `lisa_device_ready()`。
- 是否处理错误返回值。
- 引脚复用是否在设备初始化前完成。

### 4. 设备 Shell 与 ADB 操作

需要直接操作设备时，优先使用当前机器可用的 `adb shell`：

```bash
adb devices
adb shell "device help"
adb shell "device get_pid"
adb shell "device get_sid"
adb shell "device get_device_id"
```

如果当前 ADB 实现不支持 one-shot 命令，就先执行 `adb shell` 进入交互 shell，再输入同样的 `device ...` 命令。ADB 不可用但有串口 shell 时，用 `picocom`、`screen` 或已有串口工具进入 shell，波特率通常参考 `AGENTS.md` 日志说明为 `921600`。

设备命令是否存在，以 `src/shell/cmd/id_cmd.c` 和实际 `device help` 为准；不要臆造未实现的命令。`device reset` 会删除 KV 中的 PID/SID 并恢复默认值，只有用户明确要求重置绑定时才执行。

### 5. 绑定云端应用到设备

用户说“绑定云端某某应用”“把某应用设置到设备里”时，按设备侧 workflow 执行：

1. 先用 `ling`(https://github.com/LISTENAI/ling) CLI 查询应用列表并定位唯一项目：

```bash
ling app list --json
```

如果 `ling` 提示未登录、401 或缺少 API Key，先执行 `ling login` 完成登录后再继续；不要在设备流程里保存或明文输出 API Key。

2. 如果匹配到多个应用，让用户指定更精确的 Name、Project ID 或 App ID；不要猜。
3. 查询项目详情并提取凭据：

```bash
ling app inspect <project_id> --json
```

4. 拿到 `product_id` 作为 PID，拿到 `product.secret` 作为 SID。
5. 写入并验证设备 KV：

```bash
adb devices
adb shell "device set_pid <pid>"
adb shell "device set_sid <sid>"
adb shell "device get_pid"
adb shell "device get_sid"
adb shell "reboot"
```

如果 one-shot `adb shell "..."` 不可用，就进入交互 shell 后依次执行：

```text
device set_pid <pid>
device set_sid <sid>
device get_pid
device get_sid
reboot
```

当前固件的 shell `reboot` 命令执行软重启，不要默认追加未实现的 `hard` 参数；如果 `reboot` 不可用，改用当前固件支持的重启方式（例如 `adb reboot` 或手动断电重启），并说明实际采用的方式。最终回复只说明应用名、Project ID、PID 掩码、写入/验证状态和重启状态；不要明文展示 SID。

### 6. 修改配置和代码

- 应用逻辑优先放在 `apps/arcs-mini/` 或已有服务模块中。
- app 通用能力再考虑 `src/`。
- 非必要不要改 `arcs-sdk/`；如果必须改 SDK 驱动或 HAL，先说明原因、影响范围和替代方案。
- 涉及 Kconfig、`prj.conf`、CMake 时，同时检查配置名、默认值、依赖关系和最终 `build/.config`。
- 设备 shell 无法写 KV 时，才把 `apps/arcs-mini/prj.conf` 或对应 `boards/*/overlay.conf` 的 `CONFIG_CLOUD_PRODUCT_ID_DEFAULT` / `CONFIG_CLOUD_SECRET_ID_DEFAULT` 当作退路；先说明这不是运行时写 KV。

### 7. 构建验证

```bash
./build.sh -S ./apps/arcs-mini -C -DBOARD=arcs_mini
```

如果使用了应用级 overlay 或用户指定其它构建参数，保留用户参数。

## PWM 处理规则

- PWM 相关需求先查 `arcs-sdk/drivers/lisa_pwm/README.md`、`lisa_pwm.h` 和 sample。
- 业务代码优先使用 `lisa_pwm_configure()`、`lisa_pwm_set()`、`lisa_pwm_enable()`、`lisa_pwm_disable()`。
- `pwm0` 的 channel 必须来自实际 pinmux 映射，不要凭引脚号推导 channel。
- 对需要脉宽控制的外设，先把目标脉宽换算为占空比；当前 `lisa_pwm_set()` 的占空比参数是整数百分比，需要更细精度时先确认底层是否已有更合适接口。
- 舵机属于 PWM 脉宽控制需求；先确认舵机频率和脉宽范围，常见舵机用 50Hz，90°通常约 1.5ms 脉宽，再按实际规格换算占空比。
- 需要判断某个 pin 能否作为 PWM 时，从当前分支的 `projects_data.json` 读取 `peripheralConfig.PWM`；如果目标 pin 不在 PWM 映射里，且 overlay 没有新增映射，直接按“当前仓库未确认该 pin 可作 PWM”处理。

## Display / Panel 处理规则

`arcs-sdk/drivers/lisa_display/panels/` 存放 LCD/LCM 面板控制器驱动：

- `Kconfig` 声明当前可选的 `CONFIG_LISA_DISPLAY_PANEL_<CHIP>`。
- `Kconfig.<chip>` 定义该 panel 的尺寸、offset 等配置项。
- `panel_<chip>.c` 实现初始化序列和显示参数；部分相近型号可能共用同一个实现文件。
- `CMakeLists.txt` 决定启用某个 `CONFIG_LISA_DISPLAY_PANEL_*` 后编译哪些实现文件。

以下场景需要查 `panels/`：

- 判断当前仓库支持哪些屏幕控制器型号。
- 为换屏或选型确认可直接复用的 panel 驱动。
- 修改 `CONFIG_LISA_DISPLAY_PANEL_*`、`CONFIG_PANEL_*_WIDTH/HEIGHT/OFFSET`。
- 新增未支持的屏幕控制器驱动。

需要列出支持型号时，从当前分支读取 `panels/Kconfig`，按 `CONFIG_LISA_DISPLAY_PANEL_<CHIP>` 提取 `<CHIP>`，必要时再对照实现文件：

```bash
rg -n "^config LISA_DISPLAY_PANEL_" arcs-sdk/drivers/lisa_display/panels/Kconfig
find arcs-sdk/drivers/lisa_display/panels -maxdepth 1 -type f | sort
```

屏幕选型或适配时还要核对：

- 屏幕接口类型，例如 SPI/QSPI/RGB。
- 分辨率、x/y offset、旋转和颜色格式。
- TE、RST、背光 PWM 等控制引脚。
- 供电、电平、排线和结构尺寸。

已有 panel 通常只需调整 `CONFIG_LISA_DISPLAY_PANEL_*`、`CONFIG_PANEL_*_WIDTH/HEIGHT/OFFSET` 和 overlay；新增 panel 参考 `arcs-sdk/drivers/lisa_display/panels/README.md`，同时更新 `Kconfig`、`CMakeLists.txt` 和 `panel_<chip>.c`。

## 输出要求

回答设备适配问题时，尽量包含：

1. 已查询到的关键路径和结论。
2. 当前是否已有 pinmux / driver / Kconfig / device shell 支持。
3. 推荐使用的 LISA 接口或设备 shell 命令。
4. 需要修改或操作的文件/设备命令列表。
5. 构建、ADB、串口或设备验证命令。
