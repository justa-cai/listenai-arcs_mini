# 扫描笔视觉算法示例

## 功能说明

本示例演示 ARCS 双核架构下扫描笔视觉算法的完整调用链路。CP 侧 sample 从 eMMC 顺序读取 100 张 `96x240` 的 mock 灰度图，通过 `acomp_cv` 图像流发送给 AP 侧；AP 侧依次执行 `stitch`、`cutline`、`ocr` 三段处理，最终将整次扫描的 OCR 结果通过 IPC 回传给 CP 侧。

本示例同时覆盖以下关键能力：

- CP 侧 `sdmmc0` / `disk_mem` 初始化
- 100 张 mock 图的 eMMC 顺序读取与流式发送
- AP/CP 间扫描图像流、状态和 OCR 结果回传
- 扫描笔视觉算法 Flash/eMMC 资源布局与烧录说明
- AP 侧运行时资源占用评估

## 硬件连接

- 无需额外连接 AP/CP，两核均运行在同一颗芯片内部
- `sdmmc0`：用于访问 eMMC/SD 存储上的 `algo_emmc.bin`，其中包含 100 张 mock 图和扫描笔视觉模型资源
- 调试串口：默认查看 AP、CP 两侧日志

## 示例内容

1. CP 侧初始化 `ic_message`
2. CP 侧获取 `sdmmc0` 并执行 `lisa_sdmmc_probe()`，确保 AP 侧后续 `disk_mem` 可访问 eMMC 模型资源
3. CP 侧启动 `disk_mem` 服务、`acomp` 框架和当前 sample
4. CP 侧调用 `acomp_cv_prepare()`，向 AP 侧同步 `stitch` / `cutline` / `ocr` 资源描述
5. CP 侧从 eMMC `0x9600000` 顺序读取 100 张 `96x240` mock 图，并通过 `stream.cv_image` 发送给 AP 侧
6. AP 侧对 100 张图完成 `stitch -> cutline -> ocr`
7. AP 侧返回最终 OCR 结果，CP 侧打印结果日志并结束 sample

### AP/CP 分工

| 核心 | 主要职责 |
|------|----------|
| AP 侧 | 运行扫描笔视觉算法服务、加载 `cutline` / `ocr` 模型资源、执行拼接与识别、回传 OCR 结果与状态 |
| CP 侧 | 初始化 `sdmmc0` / `disk_mem`、从 eMMC 读取 100 张 mock 图、按扫描笔协议逐帧发送、接收最终 OCR 结果 |

### AP 侧配套说明

- 配套 AP 固件镜像为 `res/ap.bin`
- AP 侧扫描笔视觉服务负责模块为 `cv` / `cv_algo`

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
I/app_cv_in       send 100 mock images from eMMC: base=0x9600000, frame=96x240
I/app_cv_in       all mock images submitted
I/app_cv          ocr result: ...
```

### AP 侧日志

```text
AP Hard ID: 0
boot cp from address: 0x308d0000
I/cv              cv_prepare enter
I/cv_algo         res[1]: type=1, storage=0, addr=0x30420100, size=461952
I/cv_algo         res[2]: type=2, storage=1, addr=0xa000000, size=4586672
I/cv_algo         stitch done, oframe: ...
I/cv_algo         cutline out: ...
I/cv_algo         ocr result[...]: ...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get("sdmmc0")` | 获取 SDMMC 设备实例 |
| `lisa_sdmmc_probe()` | 初始化 `sdmmc0`，保证 AP 侧 `disk_mem` 和 CP 侧 mock 图读取都可正常访问 eMMC |
| `disk_mem_urpc_init()` | 启动 CP 侧 `disk_mem` 服务 |
| `acomp_cv_prepare()` | 初始化扫描笔视觉资源描述并同步给 AP 侧 |
| `acomp_cv_stream_ch_enable()` | 建立图像输入通道 |
| `acomp_cv_stream_tx_buffer_alloc()` | 分配图像帧头缓冲 |
| `acomp_cv_stream_tx_buffer_submit()` | 提交图像帧头到 AP 侧 |
| `CV_CB_EVENT_FRAME_DONE` | AP 侧处理完当前帧，允许 CP 侧复用单帧缓冲 |
| `CV_CB_EVENT_OCR_RESULT` | AP 侧回传最终 OCR 结果 |

## 功能详解

### 扫描笔链路

本示例走的是扫描笔完整处理链，而不是单张 OCR：

1. CP 侧从 eMMC 读取第 1 张 mock 图，发送 `BEGIN`
2. CP 侧连续发送第 2~99 张 mock 图，发送 `CONTINUE`
3. CP 侧发送第 100 张 mock 图，发送 `END`
4. AP 侧在 `END` 到达后完成拼接图生成
5. AP 侧继续执行 `cutline`
6. AP 侧对裁切结果执行 `ocr`
7. AP 侧回传整次扫描的最终识别文本

### mock 图与模型资源布局

本示例依赖三类 eMMC 数据：

| 名称 | 偏移地址 | 大小（字节） | 说明 |
|------|----------|-------------:|------|
| `OCR_MOCK_IMAGE` | `0x9600000` | 2304000 | 100 张 mock 图，总输入数据 |
| `CUTLINE` | `0x9B00000` | 461952 | `cutline` 模型 |
| `OCR_THINKER` | `0xA000000` | 4586672 | OCR 模型 |

mock 图规格如下：

| 项目 | 值 |
|------|----|
| 单张宽度 | `96` |
| 单张高度 | `240` |
| 单张大小 | `23040` 字节 |
| 图片张数 | `100` |
| 总大小 | `2304000` 字节 |

### AP 侧算法资源占用

AP 侧扫描笔视觉算法在运行时主要占用共享 `PSRAM` 资源池。配套 AP 工程当前关键配置和计算如下：

| 项目 | 大小（字节） | 说明 |
|------|-------------:|------|
| `CONFIG_PSRAM_HEAP_SIZE` | 8349696 | AP 侧总 PSRAM heap |
| `CONFIG_ALGO_RESOURCE_PSRAM_BUFFER` | 7733248 | AP 侧算法资源缓冲池上限 |
| `BUFFER_PSRAMSIZE` | 2097152 | Stitch / cutline 输出大缓冲 |
| `CUTLINE_PSRAM_SIZE + OCR_PERMANENT_PSRAM_SIZE + OCR_TEMP_PSRAM_SIZE` | 1044480 | 算法实例 PSRAM 区 |
| `OCR_BUFFER_SIZE` | 1024 | OCR 文本输出缓冲 |
| `CUTLINE` 资源 | 461952 | 当 cutline 走 eMMC 时的额外 PSRAM 资源拷贝 |
| `OCR_THINKER` 资源 | 4586672 | OCR 模型 PSRAM 资源拷贝 |
| 共享池峰值占用（cutline 走 flash XIP） | 7729328 | 低于当前池上限 `3920` 字节 |
| 共享池峰值占用（cutline 走 eMMC） | 8191280 | 超出当前池上限 `458032` 字节 |

> 当前 sample 联调要求 `cutline` 资源走 Flash、`ocr` 资源走 eMMC。若 `cutline` 退化为从 eMMC 装载，AP 侧现有资源池仍会溢出。

### 镜像文件占用

`res/` 目录下当前配套镜像大小如下：

| 文件 | 大小（字节） | 说明 |
|------|-------------:|------|
| `boot.bin` | 136780 | Boot 固件 |
| `ap.bin` | 1216848 | AP 固件 |
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
app_cv_init();
```

### 100 帧 mock 图发送流程

```c
for (index = 0; index < 100; index++) {
    app_cv_image_read_mock_image(index);
    cv_send_frame((uint32_t)(uintptr_t)g_cv_image_buf,
                  index,
                  96,
                  240,
                  app_cv_mock_frame_status(index));

    if (index != 99) {
        xSemaphoreTake(g_cv_frame_done_sem, ...);
    }
}
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
| `CONFIG_ACOMP_CV_RES_CUTLINE_EMMC_ADDR=0x9B00000` | 固定 `cutline` 模型地址 |
| `CONFIG_ACOMP_CV_RES_OCR_EMMC_ADDR=0xA000000` | 固定 OCR 模型地址 |

## 注意事项

1. `algo_emmc.bin` 必须正确烧录到 eMMC；否则 CP 侧读不到 100 张 mock 图，AP 侧也无法加载 `cutline` / `ocr` 模型
2. 本 sample 的 mock 图读取走 CP 侧 `lisa_sdmmc_read()`，AP 侧模型加载走 `disk_mem`，两者都依赖 `sdmmc0` 已被正确初始化
3. 100 张 mock 图必须按 `BEGIN -> CONTINUE -> END` 顺序发送；若中途乱序或提前停止，AP 侧 `stitch` 流程不会得到有效完整图
4. sample 当前使用单帧缓冲，必须等待 `CV_CB_EVENT_FRAME_DONE` 后才能复用该缓冲发送下一张图
5. 当前配套 AP 固件已将 `CONFIG_ALGO_RESOURCE_PSRAM_BUFFER` 调整为 `0x760000`，可覆盖“`cutline` 走 Flash、`ocr` 走 eMMC”的联调路径，但资源余量很小
6. 若 `cutline` 未命中 Flash 资源而退化为从 eMMC 装载，AP 侧现有资源池仍会溢出，联调前需先确认资源装载路径

## 相关文档

- [res_info.md](./res/res_info.md)：镜像烧录地址说明
- [main.c](./src/main.c)：CP 侧启动与 `sdmmc0` / `disk_mem` 初始化入口
- [app_cv.c](./src/app_cv/app_cv.c)：CP 侧扫描笔视觉控制逻辑
- [app_cv_image_input.c](./src/app_cv/app_cv_image_input.c)：100 张 mock 图读取与发送逻辑
