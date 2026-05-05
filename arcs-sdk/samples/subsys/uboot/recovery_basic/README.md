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

## 烧录

1. 烧 `samples/subsys/uboot/recovery_basic/build/boot.bin` 到 `0x0`，boot image 本身掌控恢复 OTA 的执行逻辑。
2. 烧 `samples/subsys/uboot_ota/app_only_trigger/build/uboot_ota_app_only_trigger.bin` 到 `0x40000`，APP-only 触发样例用于写入 OTA request。
3. Flash 模式下再将 `samples/subsys/uboot_ota/app_only_trigger/build/ota.txz` 烧到 `0x600000`，recovery boot 会从此地址拉取并执行升级。
4. TF 模式下把同一个 `build/ota.txz` 放到 TF 卡默认路径 `download/update.txz`（boot 会从 TF 上读取），记得插入 TF 卡后再 reboot。

烧录后如果默认 application 地址没有有效固件，设备会停留在 recovery second stage，等待 OTA 请求被触发。
