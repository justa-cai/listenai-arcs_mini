# 单麦 CAE 算法示例

## 功能说明

演示如何使用 ACOMP CAE 算法组件完成前端语音处理。该示例只使用 CAE 能力，不包含唤醒词识别和 ESR 识别流程。

示例流程包括：

1. 初始化 ACOMP CAE 组件
2. 加载 CAE AES 模型和 CAE 参数 JSON
3. 创建音频输入 M2R 流和算法输出 R2M 流
4. 采集麦克风和硬回采参考通道音频，按单麦 CAE 输入格式送入 CAE 引擎
5. 接收 CAE 处理后的 PCM 输出流，并通过 USB UAC 麦克风送到电脑
6. 录音过程中持续播放固定 1kHz 测试音，用于验证回采和回消效果
7. 打印声源角度和错误事件

## 硬件连接

- **音频输入**: 双通道录音，`mic0` 为麦克风数据，`ref0` 使用另一路硬回采/录音通道数据，采样率 16kHz
- **音频输出**: DAC 单声道播放固定 1kHz 测试音，采样率 16kHz
- **USB 输出**: CherryUSB UAC 麦克风设备，输出 CAE 处理后的 16kHz/16bit/mono PCM
- **调试串口**: CP 核 UART0，PA3 引脚，波特率 921600
- **调试串口**: AP 核 UART2，PA21 引脚，波特率 921600

## 音频数据流

```text
双通道录音
  ├── mic0: 主麦克风输入
  └── ref0: 硬回采参考输入
        │
        ▼
ACOMP CAE / AP 算法处理
        │
        ▼
CAE 输出 PCM
        │
        ▼
USB UAC microphone -> 电脑录音软件
```

`cae_audio_in.c` 会在录音启动后创建 1kHz 播放任务，持续向 DAC 写入测试音；`cae_audio_out.c` 收到 CAE 输出流后调用 UAC 发送接口，把处理后的单通道 PCM 送到电脑。

## UAC 验证方法

1. 按烧录步骤启动 AP 固件、CAE 资源和 CP sample。
2. 使用 USB 线连接开发板和电脑。
3. 电脑侧选择录音设备 `ListenAI CAE UAC`。
4. 使用 Audacity、系统录音机或 `arecord` 采集 USB 麦克风音频。
5. 对比 CAE 输出音频中 1kHz 测试音的残留强度，判断 AEC/回消是否生效。

如果电脑侧没有枚举出 `ListenAI CAE UAC`，优先检查 USB 线、供电和当前固件是否已经占用 USB 控制器。当前 sample 在 CP 核初始化 CherryUSB device，如果平台限制 USB 只能由 AP 核运行，需要将 UAC 输出迁移到 AP 侧。

## 资源配置

CAE 模型资源地址和长度由 `prj.conf` 定义：

```bash
CONFIG_ACOMP_CAE_RES_CAE_AES_ADDRESS=0x30200000
CONFIG_ACOMP_CAE_RES_CAE_AES_LENGTH=200000
CONFIG_ACOMP_CAE_RES_CAE_JSON_ADDRESS=0x304D0000
CONFIG_ACOMP_CAE_RES_CAE_JSON_LENGTH=141
```

如果 CAE AES 模型或参数 JSON 变化，需要同步修改对应的 length 配置，否则 AP 核解析资源时会失败或读到错误数据。

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

也可以在工程根目录执行：


## 烧录

```bash
# 烧录 AP 核固件（Boot Core）
# 烧录地址：0x30000000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x00 ./res/ap.bin

# 烧录 CAE AES 模型资源
# 烧录地址：0x30200000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x200000 ./res/algo/cae_aes.bin

# 烧录 CAE 参数 JSON
# 烧录地址：0x304D0000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x4d0000 ./res/algo/cae_ffw0_100.json

# 烧录 CP 核固件
# 烧录地址：0x30800000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x800000 ./build/arcs.bin
```

> `res/ap.bin` 需要使用与当前 remote-ap CAE 组件匹配的 AP 固件；`res/algo/cae_aes.bin` ，当前长度为 200000 字节。`res/algo/cae_ffw0_100.json` 已放在 sample 中，当前长度为 141 字节。

## 预期输出

CP 日志会出现如下关键信息：

```text
I/acomp_cae      ... acomp cae init enter
I/cae           ... acomp_cae_prepare ret:0
I/cae           ... acomp_cae_start ret:0
I/cae_uac       ... UAC microphone started, 16000Hz/16bit/1ch
I/cae_in        ... 1kHz测试音任务已启动
I/cae_in        ... CAE 音频输入已启动
I/cae           ... cae angle[0]: <angle>
```

AP 日志会出现 `acomp.cae` 注册、资源加载、CAE 启动和音频流处理相关信息。
