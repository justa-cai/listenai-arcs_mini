# XTTS 算法示例

## 功能说明

本示例演示 ARCS 双核架构下 XTTS 的完整调用链路。CP 侧 sample 通过 `acomp_xtts` 向 AP 侧算法服务发送文本合成请求，AP 侧完成资源加载与语音合成后，将 24kHz/16bit/单声道 PCM 数据通过流通道回传给 CP 侧，最终由 CP 侧本地 `audio0` 设备播放。

本示例同时覆盖以下关键能力：

- CP 侧 XTTS 控制命令下发
- AP/CP 间 IPC 与 PCM stream 回传
- XTTS Flash/eMMC 资源布局与烧录
- AP 侧运行时资源占用评估

## 硬件连接

- 无需额外连接 AP/CP，两核均运行在同一颗芯片内部
- `audio0`：用于播放 AP 回传的 PCM 数据，请按开发板默认音频输出路径连接耳机、喇叭或功放
- `sdmmc0`：用于访问 eMMC/SD 存储上的 `algo_emmc.bin` 资源镜像
- 调试串口：默认查看 AP、CP 两侧日志

## 示例内容

1. CP 侧初始化 `ic_message`
2. CP 侧获取 `sdmmc0` 并执行 `lisa_sdmmc_probe()`，确保后续 `disk_mem` 可正常访问 eMMC 资源
3. CP 侧启动 `disk_mem` 服务、`acomp` 框架和本地音频播放设备
4. CP 侧调用 `acomp_xtts_do_prepare()`，将 XTTS 资源描述发送到 AP 侧
5. AP 侧 XTTS 服务根据资源描述从 Flash/eMMC 加载前端词典、后端音色和 NHV 数据
6. CP 侧设置速度、音量、发音人，并调用 `acomp_xtts_synth_text()` 发起文本合成
7. AP 侧完成合成后持续回传 PCM 数据，CP 侧实时取流并播放

### AP/CP 分工

| 核心 | 主要职责 |
|------|----------|
| AP 侧 | 运行 XTTS 算法服务、加载 Flash/eMMC 模型资源、执行文本合成、回传 PCM stream |
| CP 侧 | 初始化 `sdmmc0` / `disk_mem`、发送 XTTS 控制命令、接收 PCM stream、本地播放音频 |

### AP 侧配套说明

- 配套 AP 固件镜像为 `res/ap.bin`
- AP 侧 XTTS 服务负责模块为 `xtts` / `xtts_algo`
- 如需联调 AP 侧源码，请参考配套 `remote-ap` 工程中的 `acomp/xtts/xtts.c` 与 `acomp/xtts/xtts_algo.c`

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

本示例需要同时准备 Boot、AP 固件、Flash 算法资源、CP 固件以及 eMMC 资源镜像。请按下表地址烧录：

| 镜像 | 介质 | 烧录地址 | 说明 |
|------|------|----------|------|
| `boot.bin` | Flash | `0x000000` | Boot 固件 |
| `ap.bin` | Flash | `0x200000` | AP 固件 |
| `algo.bin` | Flash | `0x420000` | Flash 常驻算法资源 |
| `arcs.bin` | Flash | `0x8D0000` | 本 sample 生成的 CP 固件 |
| `algo_emmc.bin` | eMMC | `0x0` | eMMC 资源镜像，地址为基于 `0x00000000` 的偏移 |

> 地址信息与 [res_info.md](./res/res_info.md) 保持一致。

### 烧录顺序建议

1. 烧录 `boot.bin`
2. 烧录 `ap.bin`
3. 烧录 `algo.bin`
4. 烧录 `arcs.bin`
5. 烧录 `algo_emmc.bin`

## 预期输出

### CP 侧日志

```text
I/main            CP=======! Hard ID: 1
I/main            ic_message_init done!
I/main            sdmmc probe success
I/main            disk_mem_urpc_init done!
I/xtts_res        prepare ret:0
I/xtts_res        start ret:0
I/app_xtts        xtts pcm begin
I/app_xtts        xtts pcm end
I/app_xtts        xtts sample finished: 0
```

### AP 侧日志

```text
AP Hard ID: 0
boot cp from address: 0x308d0000
I/xtts            xtts_prepare enter
I/xtts_algo       xtts_lib_initialize: role=0
I/xtts_algo       loading REAR from eMMC ...
I/xtts_algo       loading REAR_NHV from eMMC ...
I/xtts_algo       synthesis done, pcm_count=...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get("sdmmc0")` | 获取 SDMMC 设备实例 |
| `lisa_sdmmc_probe()` | 初始化 `sdmmc0`，保证 `disk_mem` 可正常访问 eMMC |
| `disk_mem_urpc_init()` | 启动 CP 侧 `disk_mem` 服务 |
| `acomp_xtts_do_prepare()` | 初始化 XTTS、建立 PCM RX 通道并同步资源描述 |
| `acomp_xtts_set_speed()` | 设置合成语速 |
| `acomp_xtts_set_volume()` | 设置合成音量 |
| `acomp_xtts_set_role()` | 设置发音人 |
| `acomp_xtts_synth_text()` | 下发待合成文本 |
| `acomp_xtts_stream_rx_buffer_get()` | 获取 AP 回传的 PCM 数据 |
| `acomp_xtts_stream_rx_buffer_release()` | 释放已消费的 PCM 缓冲 |

## 功能详解

### XTTS 资源布局

本示例使用两类 XTTS 资源：

1. Flash 常驻资源：由 `algo.bin` 提供，随系统镜像烧录到 Flash
2. eMMC 大资源：由 `algo_emmc.bin` 提供，按固定偏移从 eMMC 按需读取

XTTS 当前使用的 eMMC 资源如下：

| 资源 | 偏移地址 | 大小（字节） |
|------|----------|-------------:|
| `FRONT_USER` | `0x5A00000` | 3082688 |
| `CNCN_DICT` | `0x5F00000` | 3166858 |
| `ENUS_DICT` | `0x6400000` | 2470275 |
| `CRF` | `0x6900000` | 7791842 |
| `REAR` | `0x7D00000` | 4273824 |
| `REAR_NHV` | `0x8200000` | 284992 |

XTTS 当前使用的 eMMC 资源总量为 `21070479` 字节，约 `20.10 MiB`。

### AP 侧算法资源占用

AP 侧 XTTS 算法在运行时主要占用 `PSRAM`。配套 AP 工程中的关键配置和计算如下：

| 项目 | 大小（字节） | 说明 |
|------|-------------:|------|
| `CONFIG_PSRAM_HEAP_SIZE` | 8349696 | AP 侧总 PSRAM heap |
| `CONFIG_ALGO_RESOURCE_PSRAM_BUFFER` | 7618560 | AP 侧算法资源缓冲池上限 |
| `ivTTS_HEAP_SIZE` | 1843200 | XTTS 内部 heap |
| `REAR` 对齐后 | 4274176 | 后端音色资源运行时拷贝 |
| `REAR_NHV` 对齐后 | 285184 | NHV 资源运行时拷贝 |
| 保护余量 | 1024 | 固定保留空间 |
| XTTS 工作缓冲峰值 | 6403584 | `heap + rear + rear_nhv + 1024` |
| eMMC 读取缓存 | 204800 | `TTS_NAND_CACHE_SIZE`，按需分配 |
| XTTS 峰值占用 | 6608384 | 工作缓冲峰值 + eMMC 读取缓存 |

按当前配置，AP 侧 XTTS 峰值占用低于 `CONFIG_ALGO_RESOURCE_PSRAM_BUFFER`，仍保留约 `1010176` 字节缓冲余量。

### 镜像文件占用

`res/` 目录下当前配套镜像大小如下：

| 文件 | 大小（字节） | 说明 |
|------|-------------:|------|
| `boot.bin` | 136780 | Boot 固件 |
| `ap.bin` | 1216864 | AP 固件 |
| `algo.bin` | 4860443 | Flash 算法资源 |
| `algo_emmc.bin` | 214958080 | eMMC 资源镜像 |

## 关键代码

### CP 侧启动顺序

```c
ic_message_init();

sdmmc = lisa_device_get("sdmmc0");
ret = lisa_sdmmc_probe(sdmmc);

disk_mem_urpc_init();
acomp_init();
app_xtts_init();
```

### XTTS 合成流程

```c
xtts_audio_out_init();
acomp_xtts_do_prepare(xtts_event_handler, NULL);

acomp_xtts_set_speed(50);
acomp_xtts_set_volume(50);
acomp_xtts_set_role(0);
acomp_xtts_synth_text(text, strlen(text));
```

### PCM 拉流播放

```c
buffer = acomp_xtts_stream_rx_buffer_get(0, &len, &desc_idx);
if (buffer != NULL && len > 0) {
    xtts_audio_out_write(buffer, len);
    acomp_xtts_stream_rx_buffer_release(0, desc_idx, len, buffer);
}
```

## 配置说明

本示例的关键配置位于 `prj.conf`：

| 配置项 | 说明 |
|--------|------|
| `CONFIG_LISA_SDMMC_DEVICE=y` | 编入 `sdmmc0` 设备驱动 |
| `CONFIG_DISK_MEM=y` | 开启 `disk_mem` 组件 |
| `CONFIG_DISK_MEM_SERVER=y` | 在 CP 侧启用 `disk_mem` 服务 |
| `CONFIG_DISK_DRIVER=y` / `CONFIG_DISK_DRIVER_SDMMC=y` | 开启磁盘驱动与 SDMMC 后端 |
| `CONFIG_ACOMP_XTTS=y` | 开启 XTTS `acomp` 模块 |
| `CONFIG_ACOMP_XTTS_RES_*` | 固定 XTTS eMMC 资源地址和大小 |
| `CONFIG_BOOT_HART=n` / `CONFIG_BOOT=n` | 不生成 AP 启动 CP 的 boot 合并镜像 |

## 注意事项

1. `algo_emmc.bin` 必须正确烧录到 eMMC；若未烧录或内容不匹配，XTTS 会在 AP 侧资源加载阶段卡住或失败
2. `disk_mem` 访问 eMMC 之前，CP 侧必须先完成 `lisa_sdmmc_probe()`；否则会出现 `Disk device not initialized`
3. 本 sample 当前只编译 CP 固件，AP 侧默认使用 `res/ap.bin`
4. AP 和 CP 固件、`algo.bin`、`algo_emmc.bin` 需要来自同一套资源版本，避免地址、大小或模型内容不匹配
5. 默认文本为英文句子，默认发音人为 `role=0`，默认语速和音量均为 `50`
6. CP 侧等待 XTTS 完成的超时时间为 `120000 ms`，便于区分“资源未准备好”和“算法正在执行”

## 相关文档

- [res_info.md](./res/res_info.md)：镜像烧录地址说明
- [app_xtts.c](./src/app_xtts/app_xtts.c)：CP 侧 XTTS 控制逻辑
- [main.c](./src/main.c)：CP 侧启动与 `sdmmc0` / `disk_mem` 初始化入口
