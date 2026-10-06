# ARCS-MINI 端侧 NN 推理验证（Yolo/master）

基于 LISTENAI ARCS SoC（RISC-V 双核 AP/CP）的端侧神经网络推理验证分支，
从产品主线 `master` 衍生（原产品说明见 [README-original.MD](README-original.MD)）。

目标：评估 **YOLO 级检测模型在设备端（AP 核 Luna NPU + Thinker 运行时）** 的移植可行性，
基于 SDK 自带的 `thinker_resnet18_real` 双核实时识别 demo 做适配与压测。

## 当前状态（2026-10-06）

| 项 | 状态 |
| --- | --- |
| 双核实时识别 demo（arcs_mini 板级适配） | ✅ 摄像头 19.4fps / LVGL 预览流畅 / 自动识别循环 |
| ADB 全自动烧录闭环 | ✅ demo 固件自带 `adb shell` + `adb reboot recovery` |
| ResNet18 官方模型推理 | ✅ 真机 135ms/帧（32×32 输入） |
| MaxPool 内存约束（40002 排查） | ✅ 已定位并修复（threshold4 策略） |
| YOLOv5n 完整模型推理 | ❌ 被 iqCat 阻断（详见下文） |

## 已定位的两个关键根因

### 1. MaxPool 要求共享内存（已修复）

Thinker 运行时 `maxpool_luna` 的三个实现分支全部要求输入/输出张量位于
共享内存（SM/APRAM），否则直接返回 `40002 INVALID_DATATYPE`。
tpacker 打包时 `--threshold4` 低于 MaxPool 输出张量尺寸即产出必挂模型。

```
修复: tpacker ... --threshold4 65536   # ≥ MaxPool 输出尺寸（192 输入 SPPF 需 73KB）
验证: MaxPool 探针真机跑通，35ms/帧
```

### 2. iqCat 在板上 Luna 硬件路径不可用（当前阻断点）

最小复现：`conv → iqCat(2 输入) → conv`（3.8KB 模型）在真机 `tForward`
无限挂起，而同一模型在 x86 ARCS 仿真器上正常通过。

- 板端 libthinker 3.0.10 的 iqCat 走 Luna 硬件路径时挂死（CPU 路径正常）
- 官方 ResNet18 不含任何 concat 算子 → iqCat 从未被可用模型在此设备验证
- YOLO 的 FPN/PAN 结构必需 concat → 被此阻断

**出路**：① 携最小复现找 LISTENAI FAE（板端库版本锚点：libthinker 3.0.10 /
nlang `63b1073a` 2025-04-11 构建）② 改用无 concat 的单尺度检测头架构 ③ 升级板端运行时库

## 工具链版本对齐（重要）

| 组件 | 版本 | 说明 |
| --- | --- | --- |
| Linger（量化训练） | **3.0.10** | commit `89e04b2`，与板端运行时配套；CPU-only 构建需 GPU 符号打桩 |
| tpacker（模型打包） | **3.0.10** | thinker 仓库 tag `v3.0.10`，`pip install pythinker==3.0.10` |
| 板端 libthinker | 3.0.10 | `arcs-sdk/labs/lnn/common/arcs/libraries/thinker/`（闭源预编译） |

版本不一致时（如 Linger 3.1.0 导出）产物行为不可预期——务必三方对齐。

## YOLO 模型适配清单（模型侧改造）

| 改造 | 原因 |
| --- | --- |
| Stem 6×6 卷积 → 5×5 | 板端运行时 conv kernel 上限 5 |
| SiLU 激活 → ReLU | ARCS 无 QSwish，Linger 无 PReLU 量化 |
| nn.Upsample → Reshape+Cat 交织复制 | ARCS 无 Resize 算子 |
| Detect 头取 raw 三尺度输出 | 解码（含 3 输入 cat）移到 CP 侧 C 实现 |
| BN 融合进 Conv（`model.fuse()`） | 独立 QBatchNorm2d 浪费 SM；融合后参数需重建叶子 |

真机性能实测（探针）：摄像头链路 19.4fps（GC0328@640×480 上限），
推理内存 SM ≤ 355KB（硬件上限 393KB）、PSRAM ≤ 2.35MB。

## demo 板级适配（arcs_mini）

- 显示：spi0 + gpioa CS/DC/TE + PWM 通道 1 背光 + gpiob RST（对齐产品配置）
- 触摸：关闭（板型无触摸硬件），识别由 2s 自动定时触发
- USB：移植 cherryusb_adb 最小实现（shell + reboot recovery）
- 摄像头/预览 90° 旋转（预览与 NN 输入共用同一映射）
- 帧率计：`fps-stat` 每 5s 打印 capture/scale/ui-refresh/lv_handler

## 分区映射（产品分区表，0x0 boot 永不触碰）

```
AP   → 0x40000    （demo AP 固件）
模型 → 0x200000   （借 wake_word 分区，≤0x220000）
CP   → 0x600000   （demo CP 固件）
```

## 烧录工作流

```bash
./auto.sh build       # 构建 + 烧录 AP/CP/模型（全量）
./auto.sh app         # 只烧 AP/CP（快速迭代）
./auto.sh             # 不构建，只烧录
./auto.sh log         # 抓 15 秒串口日志
./burn_serial.sh      # 串口救援（设备无 ADB 时）
```

`auto.sh` 自动侦测设备模式（`BOOT-` 前缀 = recovery 直烧；
普通模式自动 `adb reboot recovery` 切换）。

## 模型转换流水线（PC 侧）

```bash
# 1. 量化导出（Linger 3.0.10，需 torch 环境）
python3 tmp/quant_yolo.py

# 2. x86 ARCS 仿真验证（无需刷板）
cd tmp/thinker && sh scripts/x86_linux.sh   # THINKER_TARGET_PLATFORM=ARCS
./bin/test_thinker <pkg> <input.int8> /dev/null

# 3. 打包（threshold4 ≥ MaxPool 输出尺寸）
tpacker -g model_linger.onnx -o model.pkg -p arcs \
    --threshold1 16384 --threshold2 16384 --threshold4 65536
```

## 关键路径

- demo 工程：`arcs-sdk/labs/lnn/thinker_resnet18_real/`（CP）+ `remote/`（AP）
- Thinker/Luna 闭源库：`arcs-sdk/labs/lnn/common/arcs/libraries/`
- 量化配置：`tmp/linger_arcs.yaml`（`platform: arcs` 是顶层键）
- 探针模型源码：`tmp/probe_yolo.py`（iqCat 最小复现）
