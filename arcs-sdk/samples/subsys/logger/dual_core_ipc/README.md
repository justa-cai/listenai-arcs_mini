# 双核 IPC 日志示例

这个示例专门演示双核环境下的 AP 日志通过 IPC 转发到 CP 打印，不依赖 WiFi。

## 行为

- AP 核启动并引导 CP 核
- AP 核初始化 IPC slave，并周期性打印 `AP heartbeat`
- CP 核初始化 IPC master，并在本地串口输出 AP 转发过来的日志
- CP 侧会为转发日志补上来源前缀，默认显示为 `[AP]`

## 自定义来源标记

如需修改转发日志前缀，可在 CP 侧 `prj.conf` 中覆盖：

```ini
CONFIG_ARCS_HAL_IPC_PRINT_SOURCE_TAG="APP"
```

设置后，CP 串口上的 AP 转发日志将显示为 `[APP] ...`。

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

执行一次构建会同时生成：

- `build/dual_core_ipc_cp.bin`
- `build/remote/dual_core_ipc_ap.bin`

## 烧录

`guardian` 会按照 [sample.yaml](sample.yaml) 中的 `images` 配置依次烧录 AP 和 CP 两个 bin。

如需手动烧录，可执行：

```bash
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/remote/dual_core_ipc_ap.bin -C arcs
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0x80000 build/dual_core_ipc_cp.bin -C arcs
```

`build/merged.bin` 仍会保留，供需要单镜像烧录的场景使用。

## 预期结果

- `ttyACM0`（CP）能看到类似 `[AP] I/ap_main ... AP heartbeat` 的输出
- `ttyACM1`（AP）不再输出 AP 运行期日志
