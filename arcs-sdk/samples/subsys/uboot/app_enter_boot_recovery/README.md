# App Enter Boot Recovery

这个 sample 演示的重点不是“单独构建 boot”，而是“应用如何进入 boot 的
recovery mode”：

1. 应用正常启动
2. 应用调用 `uboot_recovery_request(UBOOT_RECOVERY_MODE_SOFT)`
3. 应用调用系统侧 `sys_reboot(SYS_REBOOT_SOFT)`
4. boot_s 识别 `soft req`
5. boot_s 进入 recovery mode

为了让 recovery 进入结果有一个可直接观察、可重复验证的出口，本样例当前让
boot recovery 同时打开了 ADB shell、ADB sync 和 RAW 访问能力。也就是说，
BOOT_ADB 是这个 sample 的验证手段，不是这个 sample 想表达的主题。

如果你要的是“纯 boot 工程，只编 recovery boot”，请改用
`samples/subsys/uboot/recovery_basic`。本样例是 app sample，用来展示 app
侧如何请求进入 boot recovery mode。

本样例不依赖 OTA，也不会生成或烧录 `partab.bin`、`ota.tar`、`ota.txz` 或
`easyflash_reset.bin`。

## 关键配置

本样例通过 public boot 开关请求 recovery-only 能力：

- `CONFIG_BOOT_ADB=y`
- `CONFIG_BOOT_ADB_SHELL=y`
- `CONFIG_BOOT_ADB_SYNC=y`

`src/shell_cfg_user.h` 是 sample 本地的 letter-shell overlay，供 boot
recovery shell 复用。

## 构建

```bash
./build.sh -C -S samples/subsys/uboot/app_enter_boot_recovery -DBOARD=arcs_evb
```

构建完成后只需要关注：

- `build/arcs.bin`：主固件，烧录到 `0x0`

## 手动烧录

`guardian` 会按照 `sample.yaml` 里的 `images` 顺序烧录。

如果手工使用 `cskburn`，只需要烧主 bin：

```bash
./tools/burn/cskburn -C arcs -s /dev/ttyACM0 -b 3000000 --erase-all
./tools/burn/cskburn -C arcs -s /dev/ttyACM0 -b 3000000 --verify-all \
    0x0 build/arcs.bin
```

## 预期日志

应用阶段会打印：

- `APP enter boot recovery sample running`
- `APP requested boot recovery mode, rebooting`

进入 recovery 后会打印：

- `boot reason: soft req`
- `boot adb: start reason=soft req`

正常应用阶段 `adb devices` 必须保持为空；只有进入 recovery 后才允许枚举 ADB
设备。

## BOOT_ADB recovery bench

仓库内可重复执行的 bench 入口：

```bash
bash samples/subsys/uboot/app_enter_boot_recovery/verify_boot_adb_shell.sh --board arcs_evb
```

如果只想先检查 host 侧合同、build truth 和 marker 文档是否一致，而不触发
build、burn 或板级动作，可执行：

```bash
bash samples/subsys/uboot/app_enter_boot_recovery/verify_boot_adb_shell.sh --preflight-only
```

`--preflight-only` 只做前置合同检查，不触发 build、burn 或板级动作；但仍会把
`preflight.txt`、`recovery_contract.txt`、`adb_devices_baseline.txt` 等诊断证据
持久化到 `.cache/boot_adb_shell_bench/latest/`。

脚本会把最近一次执行的证据持久化到 `.cache/boot_adb_shell_bench/latest/`，重点看
这些文件：

- `context.txt` / `preflight.txt`：worktree 路径、解析出的 `build_arcs_base`、
  boot cache、串口/工具前置条件
- `recovery_contract.txt`：`src/main.c`、`verify_boot_adb_shell.sh` 与 README
  的 app-to-boot recovery 合同检查结果
- `build_truth.txt`：`build/boot/app.config`、`build/boot/.config` 与
  `build/boot/CMakeCache.txt` 的 truth check 结果
- `serial_normal.txt` / `serial_recovery_wait.txt` / `serial_recovery.txt`：
  应用阶段、等待 recovery 枚举期间、以及 recovery 后的串口日志
- `adb_devices_baseline.txt` / `adb_devices_normal.txt` /
  `adb_devices_recovery.txt`：baseline、应用阶段、recovery 枚举结果
- `adb_push_file.txt` / `adb_pull_file.txt` / `adb_push_sync.txt` /
  `adb_push_sync_stale.txt`：相对文件路径 push/pull 与相对路径
  `adb push --sync` 证据（仅覆盖默认根 `/SD:/adb/`）
- `adb_push_raw_sdraw.txt` / `adb_pull_raw_sdraw.txt`：
  `/RAW/SDRAW/0/1000` 的 push/pull 闭环证据
- `adb_unlock.txt` / `adb_status_after_unlock.txt` /
  `adb_push_raw_flash_fastpath.txt` / `adb_push_raw_nand_fastpath.txt`：
  `/RAW/FLASH/40000/10000` 与 `/RAW/NAND/40000/10000` 的对齐 fast-path push
  证据
- `result.txt`：完整跑通时生成的通过项摘要

### 实板执行流程

1. 运行 bench；脚本会先做 preflight，再调用：

```bash
bash ./build.sh -C -S samples/subsys/uboot/app_enter_boot_recovery -DBOARD=arcs_evb
```

2. build 完成后，脚本会复制并校验 `build/boot/app.config`、`build/boot/.config`
   与 `build/boot/CMakeCache.txt`，确认 boot image 真正打开了：
   - `CONFIG_BOOT_ADB_SHELL=y`
   - `CONFIG_BOOT_ADB_SYNC=y`
   - `CONFIG_ADB_SHELL=y`
   - `CONFIG_ADB_SYNC=y`
   - `CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT="/SD:/adb/"`
3. 脚本随后使用仓库自带 `./tools/burn/cskburn -C arcs` 只烧录 `arcs.bin`。
4. 应用阶段必须先看到：
   - `APP enter boot recovery sample running`
   - `APP requested boot recovery mode, rebooting`

   bench 会在 `serial_normal.txt` 中等待这组 marker，并在这个阶段持续确认
   `adb devices` 仍然为空。
5. recovery 阶段验收内容：
   - `adb devices` 恰好出现 1 个设备
   - quoted one-shot `adb shell "root;listenai;upgrade status"`，也就是 shell 里的
     `upgrade status`，在 unlock 前保持
     `exit`，错误口令后仍保持 `exit`；执行 `upgrade enter` 后切换为 `enter`
   - 文件路径 flow：相对路径 `adb push` / `adb pull` / `adb push --sync` 通过默认根
     `/SD:/adb/` 落到设备端；绝对路径和 `/RAW/...` 不属于 stale-sync freshness
     合同
   - `SDRAW` flow：`adb push` / `adb pull` 必须与 host 侧 4KB payload checksum
     一致
   - `FLASH/NAND` flow：bench 使用 64KB 对齐 payload，分别 `adb push` 到
     `/RAW/FLASH/40000/10000` 与 `/RAW/NAND/40000/10000`；这两条路径用于验证
     boot raw flash fast-path push，可和 `upgrade enter` transcript 一起作为真板
     证据
