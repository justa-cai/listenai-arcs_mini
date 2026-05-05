<!-- type: experience -->
<!-- 开发规则: README.md#开发规则 -->

# build-debug 经验记录

## arcs_evb 编译前先判断是否需要清理重建

- 场景：为 `arcs_evb` 编译指定工程时，直接执行 `./build.sh -S <project> -DBOARD=arcs_evb`，但当前 `build/` 目录残留了其他工程的 CMake cache。
- 日期：2026-03-27
- 现象：构建失败，报 `source ... does not match the source ... used to generate cache`。
- 判断：应归类为 `BUILD_FAILED`，但优先怀疑旧 `build/` 缓存，而不是当前 sample 自身代码错误。
- 处理：优先改用 `./build.sh -C -S <project> -DBOARD=arcs_evb` 清理重建，再继续后续动作。

## arcs_evb 默认串口路径以 ttyACM 为主

- 场景：为 `arcs_evb` 编写烧录和日志抓取指引时，需要区分 EVB 与 Mini 的串口设备。
- 日期：2026-03-27
- 现象：旧说明中同时混用 `/dev/ttyUSB0` 与 `/dev/ttyACM0`，容易让 EVB 闭环路径跑偏。
- 判断：`arcs_evb` 的常规操作应优先围绕单个 `/dev/ttyACM*` 设计，烧录和抓日志都先检查该设备。
- 处理：在 EVB 指引中固定 `ttyACM` 为主路径；只有非 EVB 或用户明确说明其他设备时，再回到通用串口选择逻辑。

## 启动日志乱码优先检查波特率

- 场景：`arcs_evb` 烧录成功后抓取启动日志，输出主要是乱码或替换字符。
- 日期：2026-03-27
- 现象：用 `115200` 抓取启动日志时，日志主体不可读；改为 `921600` 后可读到 `SDK 0.1.4` 和 `Hello, world!`。
- 判断：该类问题应优先怀疑波特率不匹配，而不是先怀疑烧录失败。
- 处理：先确认 `serial_read.py` 或终端工具使用 `921600`，再检查 UART 配置和物理链路。
