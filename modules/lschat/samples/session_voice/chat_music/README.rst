.. _chat_music_sample:

音乐播放
######################

概述
****
该示例程序介绍如何通过闲聊会话来使用云端音乐服务

.. toctree::
   :hidden:

   ../../sample_build

.. include:: ../../sample_build.rst

调用方法
********
- 初始化lsc服务，并设置设备ID、产品ID、secret ID

  .. code-block:: c

        lsc_config_t cfg = {
            .device_id = DEVICE_ID_STRING,
            .product_id = PRODUCT_ID_STRING,
            .secret_id = SECRET_ID_STRING,
        };        
        lsc_init(lsc_config_t *cfg);

- 注册lsc事件回调函数，发起连接，并通过回调函数获取连接结果

  .. code-block:: c

        static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
        {
            //获取事件
        }

        lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb, NULL);
        lsc_connect();

- 激活云端音乐技能

  .. code-block:: c

      lsc_activate_music();

- 初始化闲聊会话

  .. code-block:: c

      session_voice_init()

- 注册回调函数，云端返回的结果会以事件的形式下发

  .. code-block:: c

      static void voice_event_cb(session_voice_event_e evt, void *data, uint32_t size, void *usr)
      {
          //获取事件
      }
      session_voice_add_evt_callback(voice_event_cb, SESSION_VOICE_TTS | SESSION_VOICE_IAT | SESSION_VOICE_REPLY_URL | SESSION_VOICE_DRAW | SESSION_VOICE_MUSIC_INSTR | SESSION_VOICE_MUSIC_INSTR, NULL);

- 设置会话参数，使能云端vad功能，并配置设备ID

  .. code-block:: c

      session_voice_config_t config = {
      .vad_enable = true,
      .device_id = "F97CE114C70DE8E2",
      .speex_size = 0,
      .aue = "raw",
    };
    session_voice_set_config(&config);

- 启动会话，并上传语音数据

  .. note:: play_music_inc_file[]数组中存放的是录音数据("播放一首歌")，格式为16Khz, 16bit单通道PCM

  .. code-block:: c

      session_voice_start();
      mock_record_audio_stream_send((uint8_t *)play_music_inc_file, sizeof(play_music_inc_file));

- 获取音乐列表，请求音乐URL

  .. note:: 获取到的音乐列表中，包含有音乐id，通过该id可以请求音乐的URL

  .. code-block:: c

      static void voice_event_cb(session_voice_event_e evt, void *data, uint32_t size, void *usr)
      {
          case SESSION_VOICE_MUSIC_LISTS: {
              session_voice_music_lists_t *item = (session_voice_music_lists_t *)data;
              // 获取播放列表中第一首歌的URL
              char music_url[256];
              lsc_music_request_url(item->items[0].id, music_url);
          } break;
      }

示例中所使用的API文档见 ::ref:`session_voice`