# APP-only 双源 OTA Trigger

本样例演示 **APP-only** 的 OTA 触发流程：应用仅通过 uboot public API
写入 OTA request，再调用系统侧 `sys_reboot(SYS_REBOOT_SOFT)`
触发软复位，不负责编 boot，不负责从介质读包，也不负责执行升级。

真正的 OTA 包读取、解析与镜像拷贝仍由 recovery boot 侧执行。

## 特性

- 通过 `CONFIG_BOOT_RECOVERY_API=y` / `CONFIG_BOOT_OTA_API=y` 使用 public API
- 通过 `sys_reboot(SYS_REBOOT_SOFT)` 直接复用系统软复位能力
- 支持两种触发源（宏切换）：
  - Flash：`uboot_ota_start_from_flash(offset, size)`
  - TF：`uboot_ota_start_from_tf(path)`
- 默认 Flash 触发；TF 模式默认路径 `download/update.txz`
- Flash 模式通过 `CONFIG_SAMPLE_BOOT_OTA_FLASH_PACKAGE_SIZE` 提供 OTA 包大小；默认值 `0xFDE4` 与当前 `res/` 生成的 `ota.txz` 对齐
- 样例本身只编译“触发 OTA 请求”的 APP 固件

## 与 recovery_basic 的配合关系

本样例只覆盖 APP 侧触发动作。若要形成完整闭环，需要配套一个已烧录并支持
recovery OTA 的 boot（例如 `samples/subsys/uboot/recovery_basic` 对应产物）。

换句话说：

- `app_only_trigger`：负责“提交升级请求 + 重启”
- `recovery_basic`：负责“读取 OTA 包 + 执行升级”

## 构建

```bash
./build.sh -C -S samples/subsys/uboot_ota/app_only_trigger -DBOARD=arcs_evb
```

构建完成后，`build/` 目录典型产物：

- `uboot_ota_app_only_trigger.bin`

TF 模式构建：

```bash
./build.sh -C -S samples/subsys/uboot_ota/app_only_trigger -DBOARD=arcs_evb \
  -DCONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH=n -DCONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF=y
```

## 手动生成 ota.txz

`ota.tar` / `ota.txz` 不再由 `app_only_trigger` 构建自动产出。当前样例改为直接使用
`res/` 目录中准备好的 OTA 资源：

- `res/config.json`：打包输入配置
- `res/uboot_ota_app_only_upgrade_fw.bin.without.boot`：升级目标固件

基于这些静态资源生成 OTA 包：

```bash
python3 system/uboot/tools/package_ota.py \
  --config samples/subsys/uboot_ota/app_only_trigger/res/config.json \
  --txz build/ota.txz
```

如果不指定输出参数，`package_ota.py` 默认会在当前目录生成 `ota.txz`。

## res 资源来源

`res/` 里的资源不是运行时动态生成的，而是样例随仓库一起提供的静态打包输入。

- `res/uboot_ota_app_only_upgrade_fw.bin.without.boot`
  - 来源：一个单独编译的“升级后目标固件”，不是 `app_only_trigger` 自己
  - 用途：被打进 OTA 包，升级成功后由 recovery boot 写入目标分区
- `res/config.json`
  - 来源：与上面这个 bin 配套的静态打包配置
  - 用途：提供 `package_ota.py` 生成 OTA 包时需要的镜像路径和烧录地址

如果你要重新生成 `res/uboot_ota_app_only_upgrade_fw.bin.without.boot`，需要注意：

- 它必须是“升级目标固件”，不能拿 `app_only_trigger` 这个触发样例自己的 bin 代替
- 它必须是 APP-only 固件，也就是保持：
  - `CONFIG_BOOT=n`
  - `CONFIG_BOOT_HART=n`
- 它的链接地址必须和 `res/config.json` 中的 `flash_addr` 一致；当前固定为 `0x30040000`
- 它应保留 `.bin.without.boot` 这种纯 app payload 形式，再覆盖到
  `samples/subsys/uboot_ota/app_only_trigger/res/uboot_ota_app_only_upgrade_fw.bin.without.boot`
- 如果你替换了这个 bin，并且升级目标地址也变了，需要同步修改
  `samples/subsys/uboot_ota/app_only_trigger/res/config.json`

## 烧录

1. 烧 `samples/subsys/uboot/recovery_basic/build/boot.bin` 到 `0x0`，保证 recovery boot 拥有 OTA 执行能力。
2. 烧 `samples/subsys/uboot_ota/app_only_trigger/build/uboot_ota_app_only_trigger.bin` 到 `0x40000`，APP-only 样例用于写入 OTA request 并触发 reboot。
3. Flash 模式下再把 `samples/subsys/uboot_ota/app_only_trigger/build/ota.txz` 烧到 `0x600000`，recovery boot 会从这个地址拉取镜像并执行升级。
4. TF 模式下将同一个 `build/ota.txz` 放到 TF 卡默认路径 `download/update.txz`，recovery boot 将在 boot 启动时从 TF 卡读取并应用升级。APP 侧只写请求，不参与镜像读取或执行。

`app_only_trigger` 文档只说明 APP 侧职责：提交 OTA request 并重启，所有介质读取与升级逻辑由已经烧录的 recovery boot 负责。

## 预期日志

Flash 模式：

- `APP-only OTA trigger sample running`
- `APP-only OTA source: flash`
- `APP-only OTA request saved, rebooting`

TF 模式：

- `APP-only OTA trigger sample running`
- `APP-only OTA source: tf path=download/update.txz`
- `APP-only OTA request saved, rebooting`

预置升级后固件会打印：

- `APP-only OTA upgraded firmware running`

## 说明

`sample.yaml` 当前使用 `build_only: true`，避免误导为“可直接在 CI 自动完成板测”。
如果要对外宣称“已完成板级验证”，仍需按板型、烧录命令与串口日志补齐实板证据。
