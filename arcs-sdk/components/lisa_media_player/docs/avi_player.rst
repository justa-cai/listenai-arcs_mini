avi_player
==========

概览
----

``avi_player`` 提供 AVI（RIFF）容器的 demux 能力：

- 从数据源（本地文件或 HTTP Range）读取字节流。
- 解析 header/idx1/movi chunk，识别音频/视频流。
- 生成（或修正）音频/视频 PTS（毫秒）。
- 以 push 方式把音频/视频包送入 ``av_render``\ 。

当前支持的典型组合：

- 视频：MJPEG
- 音频：PCM s16le（``WAVE_FORMAT_PCM`` / ``AVI_WAVE_FORMAT_PCM``\ ）或 MP3（``WAVE_FORMAT_MPEGLAYER3`` / ``AVI_WAVE_FORMAT_MPEGLAYER3``\ ，由 ``av_render`` 侧解码为 PCM）

职责与边界
----------

``avi_player`` 负责：

- 解析 AVI/RIFF 结构（header/idx1/movi），识别音频/视频流。
- 从数据源读取并 seek（本地文件或 HTTP Range）。
- 为音频/视频 chunk 生成/修正 PTS（毫秒）。
- 以 push 方式把数据包送入 ``av_render``\ （不做解码/渲染）。

它不负责：

- MJPEG 解码、RGB565 转换、LCD 输出。
- 音视频同步策略（由 ``av_render`` 决定）。

数据流
------

典型播放路径如下（简化）：

1. open 数据源（LSFS/SD 或 HTTP Range）
2. 解析 AVI header，拿到帧率、音频格式等元信息
3. 循环读取 ``movi`` 中的 chunk：

    - 视频 chunk：封装为视频包（包含 PTS、codec、payload）并调用 ``av_render_push(...)``
    - 音频 chunk：根据编码类型自动选择包类型并调用 ``av_render_push(...)``\ ：

       - PCM：``AV_RENDER_PKT_AUDIO_PCM_S16LE``
       - MP3：``AV_RENDER_PKT_AUDIO_MP3``\ （需要 ``av_render`` 的 MP3 解码器：内置或运行期注册）

4. 结束或出错后关闭数据源，通知 render 侧退出

流程图（demux → push）
----------------------

.. code-block:: text

   +-----------------------+
   |      avi_player       |
   |  RIFF/AVI demux loop  |
   +-----------+-----------+
         |
         | avi_io_read()/avi_io_seek()
         v
    +-------------------+
    |   AVI headers      |
    |  avih/strh/strf... |
    +-------------------+
         |
         v
    +-------------------+
    |   movi chunks      |
    |  00dc / 01wb ...   |
    +--------+----------+
         |
      +------+------+
      |             |
      v             v
   video pkt       audio pkt
   (MJPEG)         (PCM s16)
      |             |
      +------v------+ 
         |
      av_render_push()
         |
         v
       +--------+
       |av_render|
       +--------+

数据源（avi_io）
----------------

``avi_player`` 通过 ``avi_io`` 抽象屏蔽底层来源差异：

- 本地文件：通常走 LSFS/FATFS（如 ``/SD:/default.avi``\ ）。
- HTTP Range：通过 ``Range: bytes=...-`` 重连实现 seek，适合“可随机访问”的远端 AVI。

``avi_io`` 接口很小（见 ``avi_io.h``\ ）：

- ``read``\ ：顺序读取
- ``seek``\ ：支持 ``AVI_IO_SEEK_SET`` 与 ``AVI_IO_SEEK_CUR``
- ``close``\ ：关闭

示例中提供了两个 opener：

- ``avi_io_open_lsfs()``\ ：本地文件（LSFS/FATFS）
- ``avi_io_open_http_range()``\ ：HTTP Range（需启用 Kconfig）

启用 HTTP Range
~~~~~~~~~~~~~~~

需要在 menuconfig 打开：

- ``LISA_MEDIA_PLAYER``
- ``AVI_PLAYER_HTTP_RANGE``

并配置：

- ``AVI_PLAYER_HTTP_URL``
- ``AVI_PLAYER_WIFI_SSID`` / ``AVI_PLAYER_WIFI_PWD``

依赖：

- httpclient 模块（Kconfig 中通过 ``select MODULE_HTTPCLIENT``\ ）
- WiFi 管理（Kconfig 中通过 ``select WIFI_MANAGER``\ ）

注意：HTTP Range 仅影响数据源部分；demux/PTS/push 逻辑保持一致。

使用 FFmpeg 生成 MJPEG + MP3 音频的 AVI
---------------------------------------

下面命令示例与现有工程实践一致（320x240 + 顺时针旋转 90° + 20fps + MP3 64kbps）：

.. code-block:: bash

   ffmpeg -i Unit1.MyFamily.mp4 \
      -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2,transpose=1" \
      -r 20 \
      -c:v mjpeg -q:v 2 \
      -ar 16000 -ac 1 -c:a mp3 -b:a 64k \
      -y default.avi

提示：

- 如果你希望继续用 PCM 音频，把 ``-c:a mp3 -b:a 64k`` 改为 ``-c:a pcm_s16le``\ 。
- 如果素材侧已预旋转为屏幕方向，则不需要 ``transpose``\ ，并建议在 ``av_render`` 里选择不旋转。

典型用法（示例工程）
--------------------

示例工程在 ``samples/media/avi_player``\ ，通常会：

- 初始化显示/音频设备
- ``av_render_open(...)`` 创建渲染实例
- ``avi_player_play_file("/SD:/default.avi", render, 0)`` 或 ``avi_player_play_url(url, render, 0)``

``max_frames`` 参数：

- 0：不限帧数（一直播放直到结束/出错）
- >0：播放指定帧数后退出（用于压力测试/定位问题）

最小伪代码（以源码为准）：

.. code-block:: c

   av_render_cfg_t cfg = {
      .display_dev = (void *)lcd,
      .audio_dev = (void *)audio,
      .sync_mode = AV_RENDER_SYNC_AUDIO,
   };
   av_render_t *render = NULL;
   av_render_open(&cfg, &render);

   /* Local file */
   avi_player_play_file("/SD:/default.avi", render, 0);

   av_render_close(render);

具体函数名以源码为准；若你已经把视频源预先旋转为屏幕方向，建议在 ``av_render`` 侧选择
``AVI_PLAYER_ROTATE_NONE``\ 。

播放控制（暂停/继续/停止）
--------------------------

如果你需要在播放过程中控制暂停/继续/停止，可以使用 ``avi_player_ctrl_t`` 并调用 ``*_ex`` 版本：

.. code-block:: c

   avi_player_ctrl_t *ctrl = avi_player_ctrl_create();

   /* 通常在独立线程/任务里调用（该函数为阻塞式） */
   int ret = avi_player_play_file_ex("/SD:/default.avi", render, 0, ctrl);
   /* ret == -ECANCELED 表示外部请求了 stop */

   /* 另一个线程里： */
   avi_player_pause(ctrl);
   avi_player_resume(ctrl);
   avi_player_stop(ctrl);

   avi_player_ctrl_destroy(ctrl);

说明：pause/stop 为“协作式”生效点，主要在 demux/push 的循环边界处检查。

配置项（Kconfig）
-----------------

本模块配置在：

- ``components/lisa_media_player/avi_player/Kconfig``

常用选项：

- ``AVI_PLAYER_HTTP_RANGE``\ ：启用 HTTP Range 数据源
- ``AVI_PLAYER_MJPEG_POOL`` / ``AVI_PLAYER_AUDIO_POOL``\ ：启用包缓冲池（降低 malloc 压力）

