掌静脉识别组件
================

简介
----

ACOMP PALM 是 ARCS SDK 的掌静脉识别组件，提供基于图像流的掌静脉检测、关键点输出、特征提取和特征比对功能。组件运行在 CP 核应用侧，通过 ACOMP IPC 与 AP 核算法侧通信，AP 核集成 LS-Palm 掌静脉算法引擎。

底层 LS-Palm 引擎能力包括：

- 掌静脉检测：输入图像，输出掌部检测框和 5 个关键点
- 掌静脉特征提取：基于检测框和关键点提取特征向量
- 掌静脉特征比对：计算输入特征与已注册特征的相似度得分

主要特性
--------

- **完整掌静脉识别流程**：支持掌静脉检测、关键点输出、特征提取和特征比对
- **多格式图像输入**：ACOMP PALM 支持 BGR888、YUYV422、RGB565 输入，AP 侧统一转换为 BGR888 后送入算法
- **特征管理**：支持导入外部掌静脉特征，最多支持 ``ACOMP_PALM_MAX_RESULT_CNT`` 个注册特征
- **事件回调机制**：通过回调异步返回掌静脉识别结果和写入错误事件
- **图像流管理**：通过 M2R stream 将 CP 核采集的图像发送到 AP 核算法组件
- **资源配置灵活**：算法资源可配置为 Flash、SD/eMMC 或 PSRAM 地址
- **生命周期管理**：支持 init、prepare、start、stop、cleanup、deinit 分阶段控制

组件架构
--------

ACOMP PALM 组件采用 CP/AP 双核协作架构：

.. code-block:: text

   ┌──────────────────────────────────────────────────────────────┐
   │                     CP 核 (Application Core)                  │
   │                                                              │
   │  用户应用                                                    │
   │   ├─ 采集图像数据                                            │
   │   ├─ 管理掌静脉注册特征                                      │
   │   └─ 处理 PALM 回调结果                                      │
   │                                                              │
   │  ACOMP PALM 组件                                             │
   │   ├─ 生命周期控制：init/prepare/start/stop/cleanup/deinit     │
   │   ├─ 参数配置：params_set                                    │
   │   ├─ 特征导入：features_load                                 │
   │   ├─ 事件回调：add_callback/remove_callback                  │
   │   └─ 图像流：stream tx buffer alloc/submit                   │
   └───────────────────────────────┬──────────────────────────────┘
                                   │
                                   │ IC Message + VirtQueue Stream
                                   │
   ┌───────────────────────────────▼──────────────────────────────┐
   │                      AP 核 (Algorithm Core)                   │
   │                                                              │
   │  ACOMP PALM Remote                                           │
   │   ├─ 接收 CP 控制命令                                        │
   │   ├─ 接收图像流并完成格式转换                                │
   │   ├─ 调用 LS-Palm Detect/Verify/Compare                      │
   │   └─ 通过 NOTIFY 返回检测框、关键点、特征和比对结果           │
   └──────────────────────────────────────────────────────────────┘

数据结构
--------

输入图像帧
~~~~~~~~~~

.. code-block:: c

   typedef struct {
       acomp_palm_pixel_format_t format; /* 图像格式 */
       uint32_t index;                   /* 帧索引 */
       uint16_t width;                   /* 图像宽度 */
       uint16_t height;                  /* 图像高度 */
       uint32_t length;                  /* 图像数据长度 */
       uint32_t resv[4];                 /* 保留字段 */
       uint8_t data[0];                  /* 图像数据 */
   } acomp_palm_input_frame_t;

支持的输入格式
~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 35 35 30

   * - 格式
     - 说明
     - 320x240 数据大小
   * - ``ACOMP_PALM_PIX_FMT_BGR888``
     - BGR 24 位真彩色
     - 230400 字节
   * - ``ACOMP_PALM_PIX_FMT_YUV422_YUYV_PACKED``
     - YUYV422 交织格式
     - 153600 字节
   * - ``ACOMP_PALM_PIX_FMT_RGB565``
     - RGB 16 位高彩色
     - 153600 字节

掌静脉识别结果
~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       acomp_palm_rect_t        palm_rect;       /* 掌静脉检测矩形框 */
       float                    palm_score;      /* 检测得分 */
       acomp_palm_align_point_t align_points[5]; /* 5 个关键点 */
       int                      n_align_point;   /* 关键点个数 */
       int                      palm_id;         /* 掌静脉 ID */
       float                    features[ACOMP_PALM_MAX_FEATURE_CNT];
       int                      feature_cnt;     /* 特征维度，以实际回调为准 */
       float                    compare_scores[ACOMP_PALM_MAX_RESULT_CNT];
       int                      compare_cnt;     /* 已比对特征个数 */
   } acomp_palm_result_t;

.. note::

   每帧结果通过 ``acomp_palm_result_info_t`` 返回。``results_cnt`` 表示检测结果个数，``max_area_results_index`` 表示最大面积掌静脉结果索引。

配置选项
--------

基本配置
~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 40 40 20

   * - 配置项
     - 说明
     - 默认值
   * - ``CONFIG_ACOMP``
     - 启用 ACOMP 组件
     - n
   * - ``CONFIG_ACOMP_PALM``
     - 启用 ACOMP PALM 掌静脉识别组件
     - n

资源配置
~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 45 35 20

   * - 配置项
     - 说明
     - 默认值
   * - ``CONFIG_ACOMP_PALM_RES_DETECT_ADDRESS``
     - 掌静脉检测模型地址
     - 0x0
   * - ``CONFIG_ACOMP_PALM_RES_DETECT_LENGTH``
     - 掌静脉检测模型长度
     - 0
   * - ``CONFIG_ACOMP_PALM_RES_VERIFY_ADDRESS``
     - 掌静脉特征提取模型地址
     - 0x0
   * - ``CONFIG_ACOMP_PALM_RES_VERIFY_LENGTH``
     - 掌静脉特征提取模型长度
     - 0

典型配置示例：

.. code-block:: kconfig

   CONFIG_ACOMP=y
   CONFIG_ACOMP_PALM=y

   CONFIG_ACOMP_PALM_RES_DETECT_ADDRESS=0x30090000
   CONFIG_ACOMP_PALM_RES_DETECT_LENGTH=83288
   CONFIG_ACOMP_PALM_RES_VERIFY_ADDRESS=0x300B0000
   CONFIG_ACOMP_PALM_RES_VERIFY_LENGTH=92680

快速开始
--------

1. 初始化组件
~~~~~~~~~~~~~

.. code-block:: c

   #include "acomp.h"
   #include "palm/acomp_palm.h"

   int ret;

   ret = acomp_init();
   if (ret != ACOMP_ERR_OK) {
       return ret;
   }

   ret = acomp_palm_init();
   if (ret != ACOMP_ERR_OK) {
       return ret;
   }

2. 注册结果回调
~~~~~~~~~~~~~~~

.. code-block:: c

   static void palm_event_handler(uint32_t event, void *event_data,
                                  uint32_t event_data_len, void *priv)
   {
       (void)priv;

       if (event & PALM_CB_EVENT_ENGINE_RLT) {
           acomp_palm_result_info_t *info = (acomp_palm_result_info_t *)event_data;

           if (info == NULL || info->results_cnt == 0U) {
               return;
           }

           uint32_t idx = info->max_area_results_index;
           if (idx >= info->results_cnt) {
               idx = 0U;
           }

           acomp_palm_result_t *result = &info->results[idx];
           printf("Palm rect: [%d,%d,%d,%d], score=%f\n",
                  result->palm_rect.x, result->palm_rect.y,
                  result->palm_rect.w, result->palm_rect.h,
                  result->palm_score);
       } else if (event & PALM_CB_EVENT_ENGINE_WR_ERR) {
           printf("PALM write error\n");
       }
   }

   ret = acomp_palm_add_callback(PALM_CB_EVENT_ENGINE_RLT | PALM_CB_EVENT_ENGINE_WR_ERR,
                                 palm_event_handler, NULL);

3. 准备并启动算法
~~~~~~~~~~~~~~~~~

.. code-block:: c

   ret = acomp_palm_prepare();
   if (ret != ACOMP_ERR_OK) {
       return ret;
   }

   ret = acomp_palm_start();
   if (ret != ACOMP_ERR_OK) {
       return ret;
   }

4. 创建图像输入流
~~~~~~~~~~~~~~~~~

.. code-block:: c

   #define PALM_STREAM_CH_INDEX 0

   acomp_stream_chn_create_desc_t desc = {
       .cname = "stream.palm_image",
       .direction = ACOMP_STREAM_DIRECTION_M2R,
       .index = PALM_STREAM_CH_INDEX,
       .buffer_size = 320 * 240 * 3 + sizeof(acomp_palm_input_frame_t),
       .num_descs = 4,
       .kick_policy = 1,
   };

   ret = acomp_palm_stream_ch_enable(PALM_STREAM_CH_INDEX, &desc);

5. 发送图像帧
~~~~~~~~~~~~~

.. code-block:: c

   uint32_t buf_size;
   uint16_t desc_idx;
   uint8_t *buffer = acomp_palm_stream_tx_buffer_alloc(PALM_STREAM_CH_INDEX,
                                                       &buf_size, &desc_idx);
   if (buffer != NULL) {
       acomp_palm_input_frame_t *frame = (acomp_palm_input_frame_t *)buffer;

       frame->format = ACOMP_PALM_PIX_FMT_BGR888;
       frame->index = frame_index++;
       frame->width = 320;
       frame->height = 240;
       frame->length = 320 * 240 * 3;
       memset(frame->resv, 0, sizeof(frame->resv));
       memcpy(frame->data, image_data, frame->length);

       ret = acomp_palm_stream_tx_buffer_submit(PALM_STREAM_CH_INDEX,
                                                buffer,
                                                sizeof(*frame) + frame->length,
                                                desc_idx);
   }

6. 停止并释放资源
~~~~~~~~~~~~~~~~~

.. code-block:: c

   acomp_palm_stop();
   acomp_palm_cleanup();
   acomp_palm_stream_ch_disable(PALM_STREAM_CH_INDEX);
   acomp_palm_remove_callback(palm_event_handler);
   acomp_palm_deinit();

API 参考
--------

生命周期管理
~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - API
     - 说明
   * - ``acomp_palm_init()``
     - 初始化 CP 侧 PALM 组件，建立 IPC 回调和 stream 管理对象
   * - ``acomp_palm_prepare()``
     - 使用 Kconfig 资源配置准备 AP 侧算法资源
   * - ``acomp_palm_prepare_with_resources()``
     - 使用调用方传入的资源地址和大小准备 AP 侧算法资源
   * - ``acomp_palm_start()``
     - 启动 AP 侧算法处理
   * - ``acomp_palm_stop()``
     - 停止 AP 侧算法处理，不释放 CP 侧 stream 和回调对象
   * - ``acomp_palm_cleanup()``
     - 清理 AP 侧算法运行资源
   * - ``acomp_palm_deinit()``
     - 释放 CP 侧 PALM 组件对象、回调和 stream 资源

参数与特征管理
~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - API
     - 说明
   * - ``acomp_palm_params_set()``
     - 设置掌静脉检测和比对参数，参数键见 ``acomp_palm_params.h``
   * - ``acomp_palm_params_get()``
     - 预留接口，当前暂不支持
   * - ``acomp_palm_features_load()``
     - 导入掌静脉注册特征，用于后续相似度比对

事件回调
~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - 事件
     - 说明
   * - ``PALM_CB_EVENT_ENGINE_RLT``
     - 掌静脉识别结果返回，``event_data`` 指向 ``acomp_palm_result_info_t``
   * - ``PALM_CB_EVENT_ENGINE_WR_ERR``
     - 算法引擎图像数据写入出错

图像流管理
~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - API
     - 说明
   * - ``acomp_palm_stream_ch_enable()``
     - 使能 stream 通道
   * - ``acomp_palm_stream_ch_disable()``
     - 禁用 stream 通道
   * - ``acomp_palm_stream_tx_buffer_alloc()``
     - 分配 TX 缓冲区，用于从 CP 向 AP 发送图像帧
   * - ``acomp_palm_stream_tx_buffer_submit()``
     - 提交 TX 缓冲区
   * - ``acomp_palm_stream_rx_buffer_get()``
     - 获取 RX 缓冲区，供 R2M 场景使用
   * - ``acomp_palm_stream_rx_buffer_release()``
     - 释放 RX 缓冲区，供 R2M 场景使用

算法资源与内存
--------------

当前 palm 组件配套资源包括：

.. list-table::
   :header-rows: 1
   :widths: 35 35 30

   * - 资源
     - 文件
     - 典型大小
   * - 掌静脉检测模型
     - ``palm_detect_thinker.bin``
     - 83288 B
   * - 掌静脉特征提取模型
     - ``palm_feature_thinker.bin``
     - 92680 B

内存占用参考：

- AP 侧算法实例 PSRAM 实测约 60KB，建议预留一定动态内存余量
- AP 侧图像转换使用 BGR888 缓冲区，320x240 输入约 225KB
- AP 侧 palm 算法使用 384KB LUNA SRAM 运行窗口，检测和特征提取分时复用
- 算法资源如拷贝到 PSRAM，约需 176KB

注意事项
--------

1. **初始化顺序**：必须先调用 ``acomp_init()`` 初始化 ACOMP 框架，再调用 ``acomp_palm_init()``。
2. **资源匹配**：Kconfig 中的模型地址和长度必须与 Flash 中实际烧录的资源一致。
3. **图像尺寸**：stream buffer 必须能容纳 ``sizeof(acomp_palm_input_frame_t) + frame->length``。
4. **图像格式**：底层 LS-Palm 引擎输入为 BGR888，非 BGR888 图像会在 AP 侧转换。
5. **特征有效期**：回调中的 ``features`` 已复制到结果结构，可按 ``feature_cnt`` 持久化保存。
6. **回调约束**：回调运行在组件内部线程上下文中，不要执行长时间阻塞操作。
7. **分时复用**：palm 与其他算法复用 LUNA SRAM 时，应由应用层控制送帧节奏，避免多个算法同时占用运行窗口。
8. **双核协作**：本组件需要 AP 固件提供 ``acomp.palm`` remote 设备，CP 初始化失败时应先检查 AP 固件和日志。

常见问题
--------

**Q: ``acomp_palm_init()`` 返回 ``ACOMP_ERR_NOT_FOUND`` 怎么处理？**

A: 通常表示 CP 未发现 AP 侧 ``acomp.palm`` 设备。检查 AP 固件是否包含 palm remote 组件、AP 是否启动成功，以及 IPC 初始化是否正常。

**Q: ``acomp_palm_prepare()`` 失败怎么办？**

A: 检查 palm detect/verify 资源地址和长度是否配置正确，资源是否已经烧录到对应 Flash 地址，AP 侧日志中通常会打印更具体的算法初始化错误。

**Q: 为什么没有检测到掌静脉？**

A: 检查输入图像格式、宽高、长度是否匹配；确认手掌在画面中足够清晰且无遮挡；必要时调整检测阈值参数。

**Q: 如何做 1:N 掌静脉识别？**

A: 应用层保存多组 ``acomp_palm_feature_result_t``，通过 ``acomp_palm_features_load()`` 导入 AP 侧注册库。每帧识别结果中的 ``compare_scores`` 返回输入掌静脉与已注册特征的相似度。
