# USB Device ADB Push Benchmark Sample

## 功能说明

该 sample 专门用于测量主机通过 `adb push` 向设备端两类存储介质写入时的吞吐速度：

- 片上 flash：`/RAW/NAND/<addr>/<size>` 原始写入
- TF 卡：`/SD:/adb_bench/`

它复用现有 USB ADB sample 的 ADB/TinyUSB 初始化方式，但把设备侧职责收敛为：准备 benchmark 目录、打印 ready 日志、等待主机侧脚本发起重复 `adb push` 压测。

其中 flash benchmark 走 `adb` 的 RAW disk 通道，直接写入 `NAND` 磁盘地址区间；TF benchmark 继续走文件系统路径 `/SD:/adb_bench/`。这样可以避免当前 ADB 文件路径 push 到 `/NAND:/...` 时的 host copy-response 异常。

## 设备侧预期日志

启动后，串口日志应至少出现以下 marker：

- `adb benchmark: flash ready at /NAND:/adb_bench/`
- `adb benchmark: sd ready at /SD:/adb_bench/`
- 或 `adb benchmark: sd not ready`

如果未插 TF 卡，允许只测 flash。

## 编译

```bash
bash ./build.sh -C -S samples/subsys/usb/device/adb_benchmark -DBOARD=arcs_evb
```

## 烧录

```bash
./tools/burn/cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 build/arcs.bin
```

实际产物名以 sample 的 `build/` 输出为准。

## 串口日志

```bash
python3 /home/openclaw/.agents/skills/arcs-dev-tools/serial_read.py /dev/ttyACM0 -b 921600 -t 10
```

## 主机侧 benchmark

sample 自带 `bench_adb_push.sh`，会：

1. 生成指定大小的测试文件
2. 分别向 raw flash / TF 目标执行多轮 `adb push`
3. 解析 `adb push` 输出中的传输字节数和耗时
4. 汇总 avg/min/max MB/s
5. 把原始日志和 summary 保存到 `.cache/`

示例：

```bash
cd samples/subsys/usb/device/adb_benchmark
./bench_adb_push.sh --serial <adb-serial> --size-mb 32 --rounds 5 --targets flash,sd
```

只测 flash：

```bash
./bench_adb_push.sh --serial <adb-serial> --size-mb 32 --rounds 5 --targets flash
```

默认 flash 目标形如：

```text
/RAW/NAND/500000/<size_hex>
```

如需改 raw flash 起始地址，可额外传：

```bash
./bench_adb_push.sh --serial <adb-serial> --size-mb 8 --rounds 3 --targets flash --flash-addr-hex 500000
```

## 结果产物

每次运行会在 sample 目录下生成 `.cache/adb_push_bench/<timestamp>/`，包含：

- 每轮 `adb push` 原始输出
- 远端路径检查输出
- 结果汇总 `summary.txt`
- 本地测试文件 `test.bin`

## 说明

- 本 sample 只关注 `adb push` 到 flash / TF 的吞吐，不混入 `pull` 或 recovery 流程
- 设备默认 ADB 相对路径根设置为 `/NAND:/adb_bench/`，但 benchmark 脚本统一使用绝对路径，避免歧义
- flash benchmark 当前使用 `/RAW/NAND/...`，TF benchmark 使用 `/SD:/adb_bench/...`
- TF 路径依赖 `lisa_sdmmc` 的公开用法，不修改 `drivers/lisa_sdmmc/*`
- 不修改 `modules/tinyusb/*`
