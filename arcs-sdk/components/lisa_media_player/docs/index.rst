lisa_media_player
=================

概览
----

``lisa_media_player`` 是一个面向嵌入式的轻量媒体播放组件集合，目前包含两部分：

- ``avi_player``\ ：AVI 容器 demux + PTS 生成 + 数据源抽象（本地文件/可选 HTTP Range）。
- ``av_render``\ ：音视频渲染管线（音频为主时钟）+ MJPEG 解码 + RGB565 LCD 输出。

目标是把“容器解析/取流（player）”与“渲染/同步（render）”解耦，便于未来并列扩展
例如 ``mp4_player``\ 、``flv_player`` 等，而复用同一套 ``av_render``\ 。

整体框架（端到端）
------------------

.. code-block:: text

    +--------------------+        +--------------------+
    |   Data Source      |        |    Render/Device   |
    |  (avi_io backend)  |        |  (display / audio) |
    +---------+----------+        +---------+----------+
                   |                             ^
                   v                             |
    +--------------------+   push packets   |
    |     avi_player     +-----------------+
    |  - RIFF/AVI demux  |                 |
    |  - PTS generation  |                 |
    +---------+----------+                 |
                   |                            |
                   v                            |
    +--------------------+                 |
    |     av_render      |-----------------+
    |  - queues/threads  |
    |  - A/V sync        |
    |  - MJPEG decode    |
    |  - RGB565 output   |
    +--------------------+

依赖关系（构建/运行）
---------------------

- ``avi_player``
   - 必选：``av_render``\ （推送目标）、AVI 解析（RIFF/movi/idx1）
   - 数据源：
      - 本地文件：LSFS/FATFS（示例默认 ``/SD:/default.avi``\ ）
      - HTTP Range（可选）：需要 ``CONFIG_AVI_PLAYER_HTTP_RANGE=y``\ ，并在 Kconfig 中 ``select`` httpclient 与 WiFi 管理
- ``av_render``
   - 必选：LISA 线程/队列/内存
   - 可选：若使用内置 LISA ``render_impl`` 输出（音频/显示），则需要对应的 LISA 设备栈（例如 ``lisa_audio`` / ``lisa_display``\ ）
   - 视频：RGB565 输出到 display；MJPEG 解码为内置实现（可被运行期注册替换）
   - 同步：默认 ``AV_RENDER_SYNC_AUDIO``\ （音频为主时钟）

文档
----

.. toctree::
   :maxdepth: 2

   avi_player
   av_render

