# U-Boot Recovery Basic

这是一个 standalone boot sample（独立 boot 样例）：

- 只构建 boot
- 不包含 app
- 不执行 boot/app merge
- 最终产物仍为 `build/boot.bin`

boot 侧能力：

- recovery second stage
- ADB device
- ADB shell
- ADB push / pull / sync
- `/SD:/adb/` 默认文件根
- `/RAW/FLASH/...`、`/RAW/NAND/...`、`/RAW/SDRAW/...` RAW 访问
- 支持 Flash OTA request execution（boot 侧执行 Flash OTA 请求）
- 支持 TF OTA request execution（boot 侧执行 TF OTA 请求）

## OTA 执行职责

`recovery_basic` 作为 recovery boot 负责读取 Flash/TF 上的 `ota.txz`、解析镜像与实际执行升级，
应用端（如 `samples/subsys/uboot_ota/app_only_trigger`）只需写入 request 并重启。
本说明聚焦 boot 侧的介质准备与升级行为。

## 构建

```bash
./build.sh -C -S samples/subsys/uboot/recovery_basic -DBOARD=arcs_evb
```

ARCS Mini 3：

```bash
./build.sh -C -S ./arcs-sdk/samples/subsys/uboot/recovery_basic -B build-boot-mini3 -DBOARD=arcs_mini3
```

`arcs_mini3.conf` 会覆盖 Mini 3 的 AP 入口、USB_DET、power guard 和 boot LCD recovery 指示等 boot 侧配置。Mini 3 的 boot 功能与 `arcs_mini` 对齐：打开 recovery ADB/sync 与 boot display，关闭 boot TF OTA 和 ADB 的 SDMMC RAW/FS 路径。

Mini 3 boot display 的 ST7789P3 MADCTL 使用 `0xC0`，方向以实机 LCD 安装为准；该方向下 240x240 可见窗口需要 `Y_OFFSET=80`。业务固件的 Mini 3 屏幕旋转走 LVGL flush 层，boot 则直接写 ST7789P3 初始化序列和像素数据，两者不是同一条显示路径。charging UI 的 POWER_KEY 提示箭头位置/指向按九宫格数字配置，Mini 3 取右上角、指向右上。

## 烧录

1. 烧 `samples/subsys/uboot/recovery_basic/build/boot.bin` 到 `0x0`，boot image 本身掌控恢复 OTA 的执行逻辑。
2. 烧 `samples/subsys/uboot_ota/app_only_trigger/build/uboot_ota_app_only_trigger.bin` 到 `0x40000`，APP-only 触发样例用于写入 OTA request。
3. Flash 模式下再将 `samples/subsys/uboot_ota/app_only_trigger/build/ota.txz` 烧到 `0x600000`，recovery boot 会从此地址拉取并执行升级。
4. TF 模式下把同一个 `build/ota.txz` 放到 TF 卡默认路径 `download/update.txz`（boot 会从 TF 上读取），记得插入 TF 卡后再 reboot。

烧录后如果默认 application 地址没有有效固件，设备会停留在 recovery second stage，等待 OTA 请求被触发。
