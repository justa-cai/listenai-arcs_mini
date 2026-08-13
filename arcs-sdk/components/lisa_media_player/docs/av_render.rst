av_render
=========

概览
----

``av_render`` 是一个面向嵌入式的轻量渲染/同步组件，采用 **push-based** 模型：

- 上层（如 ``avi_player``\ ）把音频/视频包（带 PTS）推送进来。
- ``av_render`` 负责排队、多线程处理、（可选）解码、同步与最终渲染输出。

架构（逻辑）
------------

下图为播放器的模块架构：

.. code-block:: text

  Audio Data                Video Data
    |                         |
    v                         v
  +----------+              +----------+
  | A Decoder|              | V Decoder|
  +----+-----+              +----+-----+
      |                         |
      v                         v
  +----------+              +----------+
  | A Render |              | V Render |
  +----+-----+              +----+-----+
      \                       /
      \                     /
       v                   v
          +-----------+
          |   Sync    |
          +-----------+

职责与边界
----------

``av_render`` 负责：

- 接收 ``avi_player`` push 的音频/视频包，做队列缓冲。
- 多线程渲染：音频线程、视频线程（以及可选 MJPEG 解码线程）。
- 音视频同步：以音频为主时钟（audio master），控制视频的等待/丢帧/回压。
- 视频路径：MJPEG 解码 -> RGB565 ->（可选旋转）-> LCD 写屏。

线程模型（典型）
----------------

线程架构示意：

.. code-block:: text

   Audio packets (PCM)                 Video packets (MJPEG/RGB565)
       |                                     |
       v                                     v
     +-------------+                       +------------------+
     |   audio_q   |                       |  vdec_q / video_q |
     +------+------+                       +---------+--------+
       |                                        |
       |                                  +-----+-------------------+
       |                                  | MJPEG decode thread(*) |
       |                                  |  MJPEG -> RGB565       |
       |                                  +-----+-------------------+
       |                                        |
       v                                        v
   +-------------------+                    +-------------------+
   | Audio render thread|                    | Video render thread|
   | - write PCM to dev |                    | - sync wait (audio)|
   | - update audio clk |                    | - rotate (optional)|
   +---------+----------+                    | - LCD write        |
        |                               +---------+----------+
        v                                         |
   audio device                               display device

说明：上图中的 audio/display device 属于“具体输出实现”。

- ``audio_dev`` / ``display_dev`` 在配置里是 ``void *``\ （opaque handle），由具体 render_impl 解释。

- 默认情况下：当创建 ``av_render`` 时配置了 ``audio_dev`` / ``display_dev``\ ，会使用内置的 LISA render_impl（位于 ``components/lisa_media_player/av_render/src/render_impl``\ ）把 PCM/LCD 写入对应设备。
- 如果你注册了自定义 renderer（``av_render_register_audio_renderer`` / ``av_render_register_video_renderer``\ ），则可以不依赖 ``audio_dev`` / ``display_dev``\ （可置空），由自定义实现决定如何输出。

其中 (*) 表示可选：当推入 ``AV_RENDER_PKT_VIDEO_MJPEG`` 时启用 MJPEG 解码路径。

- Audio thread：

  - 消费音频包并写入音频设备
  - 更新“当前音频播放时间戳”作为主时钟

- Video thread：

  - 从视频队列取出解码后的帧
  - 根据音频主时钟做同步等待（小切片 sleep，避免长阻塞）
  - 执行 LCD 输出（可选固定 FPS）

- MJPEG decode thread（可选/实现相关）：

  - 从“待解码队列”取 MJPEG 包
  - 解码为 RGB565 帧并入视频队列

同步策略
--------

- 默认使用 ``AV_RENDER_SYNC_AUDIO``\ ：

  - 以 audio PTS 作为主时钟基准
  - video 在显示前根据 PTS 做等待
  - 当 video 过晚时允许丢帧

- ``AVI_PLAYER_MAX_VIDEO_AHEAD_MS``\ ：

  - 限制 video PTS 领先 audio 的最大值
  - 触发 push 侧/渲染侧的回压，降低端到端延迟并避免队列无限堆积

旋转与屏幕适配
--------------

提供旋转选项（尤其适合 240x320 LCD 播放 320x240 视频）：

- ``AVI_PLAYER_ROTATE_NONE``
- ``AVI_PLAYER_ROTATE_90_CW``
- ``AVI_PLAYER_ROTATE_90_CCW``

如果你在素材侧已经把视频预旋转成屏幕方向，推荐选择 ``AVI_PLAYER_ROTATE_NONE``\ 。

固定帧率
--------

- ``AVI_PLAYER_FIXED_FPS`` / ``AVI_PLAYER_FIXED_FPS_VALUE``

用于在 LCD 带宽成为瓶颈时限制渲染帧率，降低卡顿概率（仍可能因为过晚而丢帧）。

配置项（Kconfig）
-----------------

本模块配置在：

- ``components/lisa_media_player/av_render/Kconfig``

常用选项：

- ``AVI_PLAYER_PROFILE_LOG``\ ：开启 profiling 日志（用于定位卡顿/饿死/队列策略）
- ``AVI_PLAYER_STATS_LOG``\ ：开启统计日志
- ``AVI_PLAYER_MAX_VIDEO_AHEAD_MS``\ ：同步回压窗口

包模型与所有权
--------------

``av_render`` 的统一入口是 ``av_render_push()``\ ，使用 ``av_render_pkt_t`` 描述一个音频/视频包：

- ``type``\ ：例如 ``AV_RENDER_PKT_AUDIO_PCM_S16LE``\ 、``AV_RENDER_PKT_VIDEO_MJPEG``\ 、``AV_RENDER_PKT_VIDEO_RGB565``\ 。
- ``pts_ms``\ ：时间戳（毫秒），用于同步与丢帧决策。
- ``buf``\ ：payload + 释放回调。

所有权规则（很重要）：

- ``av_render_push()`` 返回 0：payload 所有权转移给 ``av_render``\ （由 render/decoder 线程最终释放）。
- ``av_render_push()`` 返回非 0：不会接管 payload，函数内部会通过 ``free_fn``\ （或默认释放函数）释放。

音频解码（MP3）
---------------

当前 ``av_render`` 的音频输出统一是 **PCM s16le**（写入音频设备）。
当上游推入的是压缩音频（例如 MP3）时，需要由 ``av_render`` 的 **audio decoder** 把压缩数据解码成 PCM。

- 压缩音频包类型：``AV_RENDER_PKT_AUDIO_MP3``
- 音频解码器接口：``av_render_register_audio_decoder()``\ （运行期注册）

运行期注册（自定义 decoder）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

应用可以在推入第一包 MP3 之前注册 decoder：

.. code-block:: c

   static int my_mp3_push(av_render_t *r,
              uint8_t *encoded,
              size_t size,
              uint32_t pts_ms,
              av_render_free_fn_t free_fn,
              void *free_user,
              void *user)
   {
     /* 1) 解析/缓存 encoded 码流
    * 2) 解码为 PCM s16le
    * 3) 通过 av_render_push() 或内部队列把 PCM 送入音频渲染线程
    * 4) 成功时接管 encoded 并负责释放；失败时不接管，由上层释放
    */
     return 0;
   }

   static void my_mp3_close(av_render_t *r, void *user)
   {
     (void)r;
     (void)user;
   }

   static const av_render_audio_decoder_ops_t ops = {
     .push = my_mp3_push,
     .close = my_mp3_close,
   };

   av_render_register_audio_decoder(render, &ops, NULL);

说明：

- 如果应用注册了 decoder，则优先使用应用 decoder。
- 如果未注册，则会尝试使用内置 decoder（受 Kconfig 控制）。

内置 MP3 解码（modules/mp3dec）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

本工程提供基于 ``modules/mp3dec`` 的内置 MP3 解码实现，特点：

- 支持流式输入（AVI chunk 可能不是按 MP3 帧对齐）。
- 对 VBR/CBR 均可工作：PCM 的播放节奏以“解码输出样本数 / sample_rate”推进（更接近真实时长）。

启用方式（menuconfig）：

- ``LISA_MEDIA_PLAYER`` -> ``av_render`` -> ``Enable MP3 audio decoder (mp3dec)``
  - 对应配置：``CONFIG_AVI_PLAYER_AUDIO_MP3=y``

可调参数：

- ``CONFIG_AVI_PLAYER_MP3_DECODE_QUEUE_DEPTH``
- ``CONFIG_AVI_PLAYER_MP3_DECODE_THREAD_STACK_SIZE``
- ``CONFIG_AVI_PLAYER_MP3_DECODE_THREAD_PRIORITY``

关键 API（头文件）
------------------

- ``av_render_open()`` / ``av_render_close()``
- ``av_render_add_audio_stream()`` / ``av_render_add_video_stream()``
- ``av_render_push()``
- 运行期注册：
  - ``av_render_register_video_renderer()``
  - ``av_render_register_audio_renderer()``
  - ``av_render_register_mjpeg_decoder()``

提示：如果需要做 OSD/叠加，推荐用 ``av_render_register_video_renderer()`` 包一层，然后调用
``av_render_default_video_write_rgb565()`` 复用内置 LCD 输出（包含 rotate_mode）。

