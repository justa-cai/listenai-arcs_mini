# Game NES 示例

## 功能说明

本示例将 NES 模拟器核心适配到 ARCS SDK，在 `arcs_evb` 上从 SD 卡加载 `.nes` ROM，通过 LVGL 显示游戏画面和触摸按键，并通过 `audio0` 播放 NES APU 音频。

平台适配主要位于 `src/nes/port/nes_port.c` 和 `src/nes/port/nes_conf.h`。

## 硬件连接

- **arcs_evb**: 使用板载 ST7789P3 LCD、CST328 触摸和音频播放通道。
- **SD 卡**: 通过 SDMMC0 接入，使用 FAT 文件系统。
- **USB 键盘**: 可选，启用 `CONFIG_GAME_NES_USB_HOST_KEYBOARD` 后通过 USB Hub 接入。

## 示例步骤

1. 准备 FAT 格式 SD 卡，并创建 `/roms/` 目录。
2. 将 `.nes` ROM 文件拷贝到 SD 卡 `/roms/` 目录，例如 `/roms/Super_mario_brothers.nes`。
3. 编译、烧录并上电运行示例。
4. 若未插入 SD 卡，界面会提示插入 SD 卡并点击 `Retry`。
5. 在 LVGL ROM 列表中触摸选择 ROM；若未扫描到 `.nes` 文件，则使用 `CONFIG_GAME_NES_ROM_PATH` 指定的回退路径。
6. 使用触摸按键或可选 USB 键盘操作 NES 1P 手柄，点击 `ROM` 可返回 ROM 列表。

## 编译

```bash
./build.sh -C -S demos/arcs/game/nes -DBOARD=arcs_evb
```

## 烧录

根据实际串口设备替换 `/dev/ttyACM0`：

```bash
./tools/burn/cskburn -s /dev/ttyACM0 -b 3000000 0x0 build/arcs.bin -C arcs
```

## 配置说明

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `CONFIG_GAME_NES_ROM_PATH` | `/SD:/roms/mario.nes` | ROM 扫描或选择失败时使用的回退路径 |
| `CONFIG_GAME_NES_AUDIO_ENABLE` | `y` | 启用 NES 音频播放 |
| `CONFIG_GAME_NES_FRAME_SKIP` | `1` | 逻辑帧约 60 FPS，绘制帧约 30 FPS |
| `CONFIG_GAME_NES_PERF_LOG_ENABLE` | `y` | 每 120 个逻辑帧输出一次性能日志 |
| `CONFIG_GAME_NES_USB_HOST_KEYBOARD` | `n` | 可选 USB HID 键盘输入 |

## 输入方式

| 输入 | NES 操作 |
|------|----------|
| 触摸按键 | `UP`、`DN`、`L`、`R`、`A`、`B`、`SE`、`ST` |
| ROM | 返回 ROM 列表 |
| 方向键 / WASD | 方向键 |
| X / K / Alt | A |
| Z / J / Ctrl | B |
| Enter / 小键盘 Enter | Start |
| Space / Tab | Select |

USB 键盘输入需在 `prj.conf` 中启用：

```kconfig
CONFIG_GAME_NES_USB_HOST_KEYBOARD=y
```

## 预期输出

启动后应看到 SD 卡挂载、LCD/触摸初始化、音频启动、ROM 加载和周期性性能日志：

```text
I/game_nes        ... Game NES sample start
I/game_nes        ... Mounted /SD:
I/game_nes_port   ... display: 240x320, draw region: 240x240@(0,40)
I/lisa_audio_play ... Play started
I/game_nes_port   ... Audio playback started (48000Hz)
I/game_nes        ... Selected ROM: /SD:/roms/Super_mario_brothers.nes
I/game_nes        ... ROM loaded: /SD:/roms/Super_mario_brothers.nes
I/game_nes_port   ... perf[120f]: logic_fps=60.2 draw_fps=30.1 draw_avg=0ms lv_avg=4ms audio_avg=3ms audio_calls=120 drop=0 empty=0
```

稳定运行时，`logic_fps` 应接近 60，`draw_fps` 应接近 30，且 `drop=0`。启动或切换阶段可能出现短暂的 `empty` 非 0。

## 注意事项

1. **SD 卡格式化**: 若挂载失败，示例会尝试 `lsfs_mkfs()` 后重新挂载，SD 卡已有数据可能被清除。
2. **ROM 路径**: 自动扫描目录固定为 `/SD:/roms/`，ROM 文件后缀需为 `.nes`。
3. **性能日志**: 默认开启低开销性能日志；`CONFIG_GAME_NES_CORE_PROFILE_ENABLE` 和 `CONFIG_GAME_NES_CPU_OPCODE_PROFILE_ENABLE` 仅用于专项分析，不建议日常打开。
