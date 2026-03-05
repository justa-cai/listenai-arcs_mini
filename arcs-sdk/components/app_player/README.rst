.. _components_app_player:

app_player 音频播放组件
=======================
app_player 是一个轻量级的音频播放器模块，支持多种音频源的播放。

核心能力
::::::::

- **多种音频源支持**：

  - **网络音频文件**：支持 HTTP/HTTPS 协议的网络音频播放
  - **本地文件系统音频**：支持从 FAT32 文件系统（SD 卡）播放音频文件
  - **本地自定义打包音频**：支持从 Flash 播放预打包的提示音文件
  - **PCM 流式播放**：支持实时 PCM 数据流播放

- **多种音频格式**：支持 PCM、MP3、WAV、AAC、M4A 格式
- **内置重采样**：支持多种采样率自动转换，推荐 16 kHz 以获得最佳性能
- **播放控制**：播放、暂停、恢复、停止、跳转（seek）
- **事件回调**：支持准备完成、播放中、暂停、停止、播放完成、错误等事件通知
- **PA 功放管理**：外部注册 PA 硬件控制函数
- **高级选项**：支持跳过音频开头、跳过低能量段等高级播放选项
- **音量控制**：1-100 级音量调节
- **音频焦点管理**：通过 ``CONFIG_APP_PLAYER_AUDIO_FOCUS`` 启用，支持多播放器实例的焦点协调，基于优先级自动管理播放状态

Kconfig 配置
:::::::::::::

在使用 app_player 之前，需要通过 Kconfig 进行配置。

启用组件
--------

在 ``menuconfig`` 中启用 APP Player 组件：

.. code-block:: none

   Component config --->
       [*] Enable APP Player

或在 ``prj.conf`` 中添加：

.. code-block:: none

   CONFIG_APP_PLAYER=y

可配置项
--------

.. list-table:: 主要配置项
   :header-rows: 1
   :widths: 30 30 20 20

   * - 配置项
     - 说明
     - 默认值
     - 范围
   * - ``CONFIG_APP_PLAYER_PA_OFF_DELAY_MS``
     - PA 功放关闭延迟时间（毫秒）
     - 1000
     - 0-10000
   * - ``CONFIG_APP_PLAYER_CALLBACK_THREAD_STACK_SIZE``
     - 回调线程栈大小（字节）
     - 4096
     - 1024-16384
   * - ``CONFIG_APP_PLAYER_CALLBACK_THREAD_PRIORITY``
     - 回调线程优先级
     - 5
     - 0-32
   * - ``CONFIG_APP_PLAYER_AUDIO_FOCUS``
     - 启用音频焦点管理
     - n
     - -

配置说明
--------

1. **PA 功放关闭延迟 ``APP_PLAYER_PA_OFF_DELAY_MS``**

   - 播放停止后延迟关闭 PA 的时间，避免爆音
   - 设置为 0 表示立即关闭
   - 推荐保持默认值 1000 ms

2. **回调线程栈大小 ``APP_PLAYER_CALLBACK_THREAD_STACK_SIZE``**

   - 事件回调线程的栈空间大小
   - 如果回调函数中有复杂逻辑，可适当增大
   - 默认 4096 字节对大多数场景足够

3. **回调线程优先级 ``APP_PLAYER_CALLBACK_THREAD_PRIORITY``**

   - 数值越小优先级越高
   - 根据系统中其他任务的优先级进行调整
   - 默认优先级为 5

4. **音频焦点管理 ``APP_PLAYER_AUDIO_FOCUS``（可裁剪）**

   - 启用后支持多播放器实例的焦点协调
   - 基于优先级自动管理播放状态（前景/背景/无焦点）
   - 默认禁用（单播放器场景无需此功能）
   - 禁用后可减少代码体积，焦点相关 API 将不可用

依赖组件
--------

启用 ``CONFIG_APP_PLAYER`` 后，以下组件会自动被选中：

- LISA_OS - 操作系统抽象层
- LOG - 日志组件
- LISA_AUDIO_DEVICE - 音频设备驱动
- LISA_DEVICE - 设备抽象层
- SDK_MODULE_SPEEXDSP - SpeexDSP 音频处理库
- SDK_MODULE_MP3_DECODER - MP3 解码器
- SDK_MODULE_AAC_DECODER - AAC 解码器
- SDK_MODULE_MEMPOOL - 内存池管理
- MODULE_ZE_TLS - TLS 安全传输层
- MODULE_HTTPCLIENT - HTTP 客户端（用于网络音频）
- SDK_MODULE_COREHTTP - 核心 HTTP 库
- LISA_HTTP - HTTP 抽象层
- LWIP - 轻量级 TCP/IP 协议栈
- SYSLOG_PRINTK_REDIRECT - 系统日志重定向

快速开始
::::::::

1. 初始化模块
-------------

.. code-block:: c

   #include "app_player.h"  /* 公开 API 头文件 */

   /* PA 控制回调函数（控制功放开关） */
   static int pa_control_callback(int onoff) {
       /* onoff: 1=打开 PA, 0=关闭 PA */
       /* 返回 0 表示成功 */
       return lisa_gpio_write_pin(gpio_dev, PA_PIN_NUM,
                                  onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
   }

   /* 基本初始化（未启用焦点管理） */
   app_player_config_t config = {
       .pa_ctrl_callback = pa_control_callback  /* 必填 */
   };
   app_player_init(&config);

2. 创建播放器实例
-----------------

.. code-block:: c

   /* 创建播放器 */
   app_player_t *player = app_player_create("my_player");

   /* 注册事件回调（可选，建议在首次播放前调用） */
   app_player_register_callback(player, event_callback, user_data);

3. 播放音频
-----------

.. code-block:: c

   /* 播放网络音频 */
   app_player_play(player, "https://example.com/audio.mp3");

   /* 播放本地文件系统音频 */
   app_player_play(player, "/SD:/music.mp3");

   /* 播放 Flash 提示音（需先初始化提示音系统） */
   app_tone_init(0x30100000);
   const char *url = app_tone_get_url(TONE_ID_0);
   app_player_play(player, url);

4. 播放控制
-----------

.. code-block:: c

   /* 暂停 */
   app_player_pause(player);

   /* 恢复 */
   app_player_resume(player);

   /* 停止 */
   app_player_stop(player);

   /* 跳转到指定位置 */
   app_player_seek(player, 5000);  /* 跳转到 5 秒位置 */

   /* 设置音量 */
   app_player_set_volume(player, 80);  /* 音量 80/100 */

5. 事件处理
-----------

.. code-block:: c

   static void event_callback(app_player_t *player,
                              app_player_event_t event,
                              void *user_data) {
       switch (event) {
       case APP_PLAYER_EVENT_PREPARED:
           /* 准备完成，即将开始播放 */
           break;
       case APP_PLAYER_EVENT_PLAYING:
           /* 正在播放 */
           break;
       case APP_PLAYER_EVENT_COMPLETED:
           /* 播放完成 */
           break;
       case APP_PLAYER_EVENT_ERROR:
           /* 播放出错 */
           break;
       }
   }

6. 清理资源
-----------

.. code-block:: c

   /* 销毁播放器 */
   app_player_destroy(player);

播放器状态机
::::::::::::::

.. code-block:: text

   IDLE → PREPARING → PREPARED → PLAYING ⇄ PAUSED
                        ↓           ↓
                     STOPPED ← ─ ─ ┘
                        ↓
                      ERROR

- **IDLE**：初始空闲状态
- **PREPARING**：正在准备音频资源
- **PREPARED**：准备完成，即将开始播放
- **PLAYING**：正在播放
- **PAUSED**：已暂停
- **STOPPED**：已停止
- **ERROR**：错误状态

主要 API
::::::::

.. list-table:: 核心 API
   :header-rows: 1
   :widths: 40 60

   * - 函数
     - 说明
   * - ``app_player_init()``
     - 初始化模块（必须最先调用）
   * - ``app_player_create()``
     - 创建播放器实例
   * - ``app_player_destroy()``
     - 销毁播放器实例
   * - ``app_player_register_callback()``
     - 注册事件回调
   * - ``app_player_play()``
     - 播放指定 URL
   * - ``app_player_play_ex()``
     - 高级播放（支持更多选项）
   * - ``app_player_stop()``
     - 停止播放（异步）
   * - ``app_player_stop_sync()``
     - 停止播放（同步）
   * - ``app_player_pause()``
     - 暂停播放
   * - ``app_player_resume()``
     - 恢复播放（异步）
   * - ``app_player_resume_sync()``
     - 恢复播放（同步）
   * - ``app_player_seek()``
     - 跳转到指定位置
   * - ``app_player_set_volume()``
     - 设置音量（1-100）
   * - ``app_player_get_state()``
     - 获取播放器状态
   * - ``app_player_get_position()``
     - 获取当前播放位置
   * - ``app_player_get_duration()``
     - 获取音频总时长
   * - ``app_player_reset()``
     - 重置播放器到初始状态

注意事项
::::::::

1. **必须先初始化**：调用 ``app_player_init()`` 必须在创建播放器实例之前。
2. **PA 回调必填**：初始化时必须提供 ``pa_ctrl_callback``，用于控制功放。
3. **多实例支持**：

   - 支持创建多个播放器实例。
   - **不支持混音**：同一时刻只能有一个播放器处于播放状态。
   - 多播放器场景建议启用音频焦点管理（``CONFIG_APP_PLAYER_AUDIO_FOCUS=y``）来自动协调播放状态。
   - 单播放器场景建议禁用焦点管理以减少代码体积。

4. **音频格式要求**：

   - **位深度**：16 bit（硬件固定，不可更改）。
   - **采样率**：16 kHz（推荐）。播放器内部支持重采样，可处理其他采样率，但为了最佳性能建议使用 16 kHz。
   - **格式**：支持 PCM、MP3、WAV、AAC、M4A。

5. **同步 vs 异步**：``stop_sync()`` 和 ``resume_sync()`` 会阻塞等待操作完成，适合需要确保状态切换完成的场景。
6. **音量范围**：音量值范围是 1-100。
7. **资源清理**：使用完毕后记得调用 ``app_player_destroy()`` 释放资源。

错误码
::::::

.. list-table:: 常见错误码
   :header-rows: 1
   :widths: 40 60

   * - 错误码
     - 说明
   * - ``APP_PLAYER_OK``
     - 成功
   * - ``APP_PLAYER_ERR_INVALID_PARAM``
     - 无效参数
   * - ``APP_PLAYER_ERR_NO_MEMORY``
     - 内存不足
   * - ``APP_PLAYER_ERR_INVALID_STATE``
     - 状态错误
   * - ``APP_PLAYER_ERR_NOT_SUPPORTED``
     - 不支持的操作
   * - ``APP_PLAYER_ERR_TIMEOUT``
     - 超时
   * - ``APP_PLAYER_ERR_IO``
     - IO 错误

扩展文档
::::::::

.. toctree::
   :maxdepth: 1

   audio_focus.md
   streaming.md
   local_tone.md

示例代码
::::::::

完整示例代码请参考：

- ``samples/modules/app_player/`` —— 演示用例。
