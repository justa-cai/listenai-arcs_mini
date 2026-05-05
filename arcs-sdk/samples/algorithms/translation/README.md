# Translation 算法示例

## 功能说明

本示例演示 ARCS 双核架构下 Translation 的完整调用链路。CP 侧 sample 通过 `acomp_translation` 向 AP 侧翻译服务发送文本翻译请求，AP 侧按需从 Flash/eMMC 加载词典与模型资源，完成翻译后再通过 IPC 回传状态和结果。

本示例同时覆盖以下关键能力：

- CP 侧 Translation 控制命令下发
- AP/CP 间 IPC 与翻译结果回传
- Translation Flash/eMMC 资源布局与烧录
- AP 侧运行时资源占用评估

## 硬件连接

- 无需额外连接 AP/CP，两核均运行在同一颗芯片内部
- `sdmmc0`：用于访问 eMMC/SD 存储上的 `algo_emmc.bin` 资源镜像
- 调试串口：默认查看 AP、CP 两侧日志

## 示例内容

1. CP 侧初始化 `ic_message`
2. CP 侧获取 `sdmmc0` 并执行 `lisa_sdmmc_probe()`，确保后续 `disk_mem` 可正常访问 eMMC 资源
3. CP 侧启动 `disk_mem` 服务、`acomp` 框架和本地 Translation sample
4. CP 侧调用 `acomp_translation_do_prepare()`，向 AP 侧同步 20 个资源描述
5. AP 侧 Translation 服务按需加载 18 个 eMMC 模型资源，并从 Flash `resmgr` 获取 `trandb` 和 `dict`
6. CP 侧依次执行两次固定文本翻译：
   `CN2EN`：`这是一个 translation 算法 sample。`
   `EN2CN`：`This is a translation sample running on CP.`
7. AP 侧返回翻译状态和结果，CP 侧打印日志并清理 Translation 上下文

### AP/CP 分工

| 核心 | 主要职责 |
|------|----------|
| AP 侧 | 运行 Translation 算法服务、按需从 Flash/eMMC 加载资源、执行翻译、回传状态和结果 |
| CP 侧 | 初始化 `sdmmc0` / `disk_mem`、发送翻译方向与文本、等待结果回调、打印结果日志 |

### AP 侧配套说明

- 配套 AP 工程参考路径：`/home/hiker/listenai/voiceassistant/voiceassistant/apps/remote-ap`
- Translation 相关模块：
  `acomp/translation/translation.c`
  `acomp/translation/trans_algo.c`
  `acomp/translation/trans_algo.h`

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
I/trans_res       init ret:0
I/trans_res       prepare ret:0
I/trans_res       start ret:0
I/app_trans       CN2EN input: 这是一个 translation 算法 sample。
I/app_trans       translation result: This is a translation algorithm sample.
I/app_trans       EN2CN input: This is a translation sample running on CP.
I/app_trans       translation result: 这是一个运行在 CP 上的翻译示例。
I/app_trans       translation sample finished: 0
```

### AP 侧日志

```text
AP Hard ID: 0
I/translation     translation_prepare enter
I/trans_algo      trans_algo_prepare: res_cnt=20
I/trans_algo      pool: addr=..., size=7614976 (from shared pool)
I/trans_algo      loading dec_emb from eMMC ...
I/translation     translation_work_handler: text[...]=...
I/trans_algo      lstrans_process done, ret=0
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get("sdmmc0")` | 获取 SDMMC 设备实例 |
| `lisa_sdmmc_probe()` | 初始化 `sdmmc0`，保证 `disk_mem` 可正常访问 eMMC |
| `disk_mem_urpc_init()` | 启动 CP 侧 `disk_mem` 服务 |
| `acomp_translation_do_prepare()` | 初始化 Translation、同步资源描述并启动服务 |
| `acomp_translation_set_res_type()` | 设置翻译方向 |
| `acomp_translation_translate()` | 下发待翻译文本 |
| `acomp_translation_do_cleanup()` | 停止并清理 Translation 上下文 |
| `TRANS_CB_EVENT_RESULT` | 翻译结果回调 |
| `TRANS_CB_EVENT_STATUS` | 翻译状态回调 |

## 功能详解

### Translation 资源布局

本示例使用两类 Translation 资源：

1. Flash 常驻资源：`trandb`、`dict`，由 `algo.bin` 提供，并通过 `resmgr` 获取
2. eMMC 大资源：18 个模型资源，由 `algo_emmc.bin` 提供，按固定偏移从 eMMC 按需读取

当前使用的 eMMC 资源如下：

| 资源 | 偏移地址 | 大小（字节） |
|------|----------|-------------:|
| `ENCN_ENC_A` | `0x0` | 3306480 |
| `ENCN_ENC_B` | `0x500000` | 3286864 |
| `ENCN_ENC_C` | `0xA00000` | 3286864 |
| `ENCN_ENC_D` | `0xF00000` | 3286864 |
| `ENCN_ENC_E` | `0x1400000` | 3286864 |
| `ENCN_ENC_F` | `0x1900000` | 3561152 |
| `ENCN_DEC` | `0x1E00000` | 4429216 |
| `ENCN_DEC_EMB` | `0x2300000` | 1936384 |
| `ENCN_ENC_EMB` | `0x2800000` | 1936384 |
| `CNEN_ENC_A` | `0x2D00000` | 3306480 |
| `CNEN_ENC_B` | `0x3200000` | 3286864 |
| `CNEN_ENC_C` | `0x3700000` | 3286864 |
| `CNEN_ENC_D` | `0x3C00000` | 3286864 |
| `CNEN_ENC_E` | `0x4100000` | 3286864 |
| `CNEN_ENC_F` | `0x4600000` | 3561152 |
| `CNEN_DEC` | `0x4B00000` | 4429216 |
| `CNEN_DEC_EMB` | `0x5000000` | 1936384 |
| `CNEN_ENC_EMB` | `0x5500000` | 1936384 |

Translation 当前使用的 eMMC 资源总量为 `56634144` 字节，约 `54.01 MiB`。

### AP 侧算法资源占用

AP 侧 Translation 算法主要占用共享 `PSRAM` 资源池，关键数据如下：

| 项目 | 大小（字节） | 说明 |
|------|-------------:|------|
| `CONFIG_PSRAM_HEAP_SIZE` | 8349696 | AP 侧总 PSRAM heap |
| `CONFIG_ALGO_RESOURCE_PSRAM_BUFFER` | 7618560 | AP 侧算法资源缓冲池上限 |
| `PSRAM_SIZE` | 1228800 | Translation 工作区 |
| `TRANS_EMB_TOKENS_BUFFER_SIZE` | 4096 | Embedding tokens 输出缓冲 |
| `TRANS_EMB_POSITION_BUFFER_SIZE` | 16384 | Embedding position 输出缓冲 |
| `FORMAT_SIZE(CNEN_DEC_RES_SIZE, 512)` | 4429312 | Decoder 资源缓冲，对齐后大小 |
| `CNEN_EMB_RES_SIZE` | 1936384 | Decoder embedding 预加载缓冲 |
| 共享池峰值占用 | 7614976 | `trans_algo_prepare()` 实际申请大小 |
| 共享池剩余余量 | 3584 | 相对 `CONFIG_ALGO_RESOURCE_PSRAM_BUFFER` 的剩余空间 |
| `SHARE_SIZE` | 393216 | 固定使用 AP SRAM，不占共享 PSRAM 池 |
| `TRANS_RSLT_SIZE` | 1024 | 单次翻译临时结果缓冲，由 `psram_malloc_align()` 分配 |

按当前配置，Translation 的共享 `PSRAM` 资源池几乎被占满。如果 AP 侧继续叠加算法或调整资源大小，需要同步重新评估这部分预算。

## 关键代码

### CP 侧启动顺序

```c
ic_message_init();

sdmmc = lisa_device_get("sdmmc0");
ret = lisa_sdmmc_probe(sdmmc);

disk_mem_urpc_init();
acomp_init();
app_translation_init();
```

### Translation 执行流程

```c
ret = acomp_translation_do_prepare(translation_event_handler, NULL);

ret = acomp_translation_set_res_type(TRANS_TYPE_CN2EN);
ret = acomp_translation_translate(cn_text, strlen(cn_text));

ret = acomp_translation_set_res_type(TRANS_TYPE_EN2CN);
ret = acomp_translation_translate(en_text, strlen(en_text));

acomp_translation_do_cleanup();
```

## 配置说明

本示例的关键配置位于 `prj.conf`：

| 配置项 | 说明 |
|--------|------|
| `CONFIG_MEM_FLASH_BASE=0x308D0000` | 将 CP 固件链接到 Flash `0x8D0000` 偏移 |
| `CONFIG_BOOT_HART=n` / `CONFIG_BOOT=n` | 不生成 AP 启动 CP 的 boot 合并镜像 |
| `CONFIG_LISA_SDMMC_DEVICE=y` | 编入 `sdmmc0` 设备驱动 |
| `CONFIG_DISK_MEM=y` | 开启 `disk_mem` 组件 |
| `CONFIG_DISK_MEM_SERVER=y` | 在 CP 侧启用 `disk_mem` 服务 |
| `CONFIG_DISK_DRIVER=y` / `CONFIG_DISK_DRIVER_SDMMC=y` | 开启磁盘驱动与 SDMMC 后端 |
| `CONFIG_ACOMP_TRANSLATION=y` | 开启 Translation `acomp` 模块 |
| `CONFIG_ACOMP_TRANS_RES_*` | 固定 18 个 Translation eMMC 资源地址和大小 |

## 注意事项

1. `algo_emmc.bin` 必须正确烧录到 eMMC；若未烧录或资源内容不匹配，AP 侧 Translation 会在按需加载模型时失败
2. `disk_mem` 访问 eMMC 之前，CP 侧必须先完成 `lisa_sdmmc_probe()`；否则会出现 `Disk device not initialized`
3. 本 sample 只在 `prj.conf` 中固定 18 个 eMMC 资源地址，`trandb` 和 `dict` 仍来自 `algo.bin` 中的 Flash `resmgr`
4. AP 侧 Translation 共享 `PSRAM` 资源池只剩 `3584` 字节余量，修改 AP 配置或模型版本后应重新核算
5. AP 和 CP 固件、`algo.bin`、`algo_emmc.bin` 需要来自同一套资源版本，避免地址、大小或模型内容不匹配

## 相关文档

- [res_info.md](./res/res_info.md)：镜像烧录地址说明
- [main.c](./src/main.c)：CP 侧启动与 `sdmmc0` / `disk_mem` 初始化入口
- [app_translation.c](./src/app_translation/app_translation.c)：CP 侧 Translation 控制逻辑
