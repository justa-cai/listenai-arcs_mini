# LISA Flash 双核 Flash 操作保护示例

## 功能说明

本示例用于验证双核系统下 Flash 擦写时对另一个核心的保护流程：CP 核执行 `lisa_flash_erase()`、`lisa_flash_write()` 和 `lisa_flash_read()`，AP 核在 CP 擦写 Flash 期间进入停止或等待状态，CP 完成后 AP 恢复运行。

示例默认保持 ARCS 平台原有 halt/resume 机制；VenusA 平台通过独立配置文件启用 `ic_message` + `lisa_flash_*_hook` 流程，避免影响 ARCS 的运行方式。

## 硬件连接

无需外部连接，Flash、AP 核和 CP 核均为芯片内部资源。需要使用支持对应双核启动和 Flash 擦写验证的 ARCS 或 VenusA 板卡。

## 示例内容

1. AP 镜像从 Flash 基地址启动，并在 `SYS_INIT` 阶段拉起 CP 镜像 `0x30080000`
2. CP 获取 `flash0` 设备，打印 Flash 参数与 layout 信息
3. CP 循环擦除、写入并读取 `TEST_OFFSET` 对应的 Flash 区域
4. AP 在 CP 擦写/读取 Flash 期间进入平台对应的保护流程
5. CP 校验读回数据，全部迭代通过后打印 `CP flash passed`

### ARCS 默认流程

默认 `prj.conf` 和 `remote/prj.conf` 保持 ARCS 原有配置：

- CP 侧启用 `CONFIG_LISA_FLASH_ARCS_HALT_REMOTE_CORE`
- AP 侧启用 `CONFIG_LISA_FLASH_ARCS_HALT_BY_REMOTE_CORE`
- AP 侧通过 `ipc_utils_before_halt_by_peer_core()` 和 `ipc_utils_after_resume_by_peer_core()` 观察 halt/resume 事件

### VenusA 适配流程

VenusA 使用 `prj_venusa.conf` 和 `remote/prj_venusa.conf`，不会改写 ARCS 默认配置：

1. AP/CP 两侧只需启用 `CONFIG_LISA_FLASH_HALT_REMOTE_CORE`，该配置会根据 SoC 弱使能对应底层依赖；VenusA 下会弱使能 `CONFIG_IPC_CIDU` 与 `CONFIG_IC_MESSAGE_AUTO_INIT`
2. LISA Flash VenusA 驱动初始化时调用 `lisa_flash_hook_init()` 注册 `IC_MESSAGE_ID_FLASH_MSG`
3. CP 在擦除/写入/读取前发送一个 `uint32_t` payload，payload 内容为 SRAM/RAM mailbox 地址
4. AP 收到消息后执行位于 SRAM 的等待函数，关闭全局中断并轮询 mailbox
5. AP 将 mailbox 从 `WAITING` 更新为 `HALTED`，CP 开始擦写 Flash
6. CP 的 write/erase done hook 将 mailbox 更新为 `IDLE`，AP 退出轮询并恢复中断
7. AP 的强符号 hook 通过 `IC_MESSAGE_ID_COMMON` 向 CP 回报等待计数

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

ARCS 默认构建命令：

```bash
./build.sh -C -S samples/drivers/devices/lisa_flash/halt_remote_core -B build/lisa_flash_halt_remote_core_arcs -DBOARD=arcs_evb -j4
```

VenusA 构建需要显式选择 VenusA 配置文件：

```bash
./build.sh -C -S samples/drivers/devices/lisa_flash/halt_remote_core -B build/lisa_flash_halt_remote_core_venusa -c prj_venusa.conf -DBOARD=venusa_rd_evb -j4
```

示例构建会同时生成 AP/CP 两个镜像，并合并为 `merged.bin`。VenusA 构建时，根工程使用 `prj_venusa.conf`，AP 外部工程自动使用 `remote/prj_venusa.conf`；ARCS 构建时仍使用原有 `prj.conf` 和 `remote/prj.conf`。

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

`sample.yaml` 中的镜像布局为：

- AP: `remote/lisa_flash_halt_remote_core_ap.bin` -> `0x0`
- CP: `lisa_flash_halt_remote_core_cp.bin` -> `0x80000`

## 核心 API

| API | 说明 |
| --- | --- |
| `lisa_device_get()` | 获取 `flash0` 设备句柄 |
| `lisa_device_ready()` | 检查 Flash 设备是否完成初始化 |
| `lisa_flash_page_layout()` | 获取 Flash 擦除页 layout |
| `lisa_flash_erase()` | 擦除测试区域，触发平台保护流程 |
| `lisa_flash_write()` | 写入测试数据，触发平台保护流程 |
| `lisa_flash_read()` | 读回测试区域并用于校验 |
| `ic_message_register_by_id()` | VenusA AP/CP 注册 hook 验证消息 |
| `venusa_start_cp()` | VenusA AP 启动 CP 镜像 |

## 配置说明

ARCS CP 侧关键配置：

```kconfig
CONFIG_ARCS_CP_CORE=y
CONFIG_BOOT=n
CONFIG_LISA_DEVICE=y
CONFIG_LISA_FLASH=y
CONFIG_LISA_FLASH_ARCS_HALT_REMOTE_CORE=y
CONFIG_MEM_FLASH_BASE=0x30080000
```

ARCS AP 侧关键配置：

```kconfig
CONFIG_ARCS_AP_CORE=y
CONFIG_RTOS_AL=y
CONFIG_LISA_FLASH_ARCS_HALT_BY_REMOTE_CORE=y
CONFIG_LOG_BACKEND_IPC_WRITER=y
```

VenusA CP 侧关键配置：

```kconfig
CONFIG_HARTID=1
CONFIG_BOOT=n
CONFIG_LISA_DEVICE=y
CONFIG_LISA_FLASH=y
CONFIG_LISA_FLASH_HALT_REMOTE_CORE=y
CONFIG_MEM_FLASH_BASE=0x30080000
CONFIG_MEM_PSRAM_SIZE=0x00400000
```

VenusA AP 侧关键配置：

```kconfig
CONFIG_HARTID=0
CONFIG_LISA_DEVICE=y
CONFIG_LISA_FLASH=y
CONFIG_LISA_FLASH_HALT_REMOTE_CORE=y
CONFIG_MEM_FLASH_BASE=0x30000000
CONFIG_MEM_PSRAM_SIZE=0x00400000
```

## 预期输出

ARCS CP 侧应看到：

```text
=== ARCS Flash Halt Sample (CP) ========
CP erase offset 0x00100000 size 4096; AP should enter flash wait/halt flow
CP write offset 0x00100000 size 256; AP should enter flash wait/halt flow
CP iteration 1 verified
...
CP flash passed
```

ARCS AP 侧应看到：

```text
=== ARCS Flash Halt Sample (AP) ===
ipc_utils_before_halt_by_peer_core
ipc_utils_after_resume_by_peer_core
```

VenusA CP 侧应看到：

```text
=== VenusA Flash Hook Sample (CP) ======
CP flash hook peer connected
CP erase offset 0x00100000 size 4096; AP should enter flash wait/halt flow
CP write offset 0x00100000 size 256; AP should enter flash wait/halt flow
AP flash wait report: disable=1 poll=1 enable=1 done=1
CP iteration 1 verified
...
CP flash passed
```

VenusA AP 侧应看到：

```text
=== VenusA Flash Hook Sample (AP) ======
AP booted CP from flash 0x30080000
AP flash wait before disable: mailbox=0x...
AP flash wait after enable: mailbox=0x... disable=1 poll=1 enable=1 done=1
```

VenusA 的 `AP flash wait before disable` / `AP flash wait after enable` 成对出现，且 CP 收到 `AP flash wait report`，表示 AP 已进入 SRAM 等待代码并在 CP Flash 操作完成后恢复。

## 注意事项

1. **平台兼容**: ARCS 使用默认 `prj.conf`；VenusA 必须通过 `-c prj_venusa.conf` 显式启用，避免影响 ARCS 默认行为。
2. **实板验证**: 本示例需要双核板卡实测，主机侧构建通过不能替代串口日志验证。
3. **测试区域**: Flash 测试偏移为 `0x100000`，VenusA CP 物理地址为 `0x30180000`。
4. **VenusA 等待代码**: AP 等待函数位于 SRAM，mailbox 当前放在 .data 对应的 SRAM/RAM 区域，等待期间避免执行会访问 Flash 的代码。
5. **日志限制**: VenusA 的 `before_poll` 和 `before_enable_irq` hook 只更新计数，避免在关中断期间执行阻塞输出。
