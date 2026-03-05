.. _speex-nb_encode_sample:

speex-nb音频编码后天气对话
############################

概述
************************
该示例程序介绍如何通过speex-nb编码方式对音频数据进行编码，并通过闲聊会话和云端进行交互，获取天气信息。

.. toctree::
   :hidden:

   ../../sample_build

.. include:: ../../sample_build.rst

调用方法
************************
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

- 初始化闲聊会话

  .. code-block:: c

      session_voice_init();

- 注册回调函数，云端返回的结果会以事件的形式下发

  .. code-block:: c

      static void voice_event_cb(session_voice_event_e evt, void *data, uint32_t size, void *usr)
      {
          //获取事件
      }
      session_voice_add_evt_callback(voice_event_cb, SESSION_VOICE_TTS | SESSION_VOICE_IAT | SESSION_VOICE_REPLY_URL | SESSION_VOICE_DRAW,NULL);

- 设置会话参数，使能云端vad功能

  .. code-block:: c
    
      session_voice_config_t config = {
      .vad_enable = true,
      .speex_size = 38,
      .aue = "speex",
    };
    session_voice_set_config(&config);

- 启动会话，对原始音频数据进行speex narrowband编码，然后上传编码后的语音数据

  .. note:: draw_inc_file[]数组中存放的是录音("深圳天气怎么样")数据，格式为16Khz, 16bit单通道PCM

  .. code-block:: c

      session_voice_start();
      mock_record_audio_stream_send((uint8_t *)draw_inc_file, sizeof(draw_inc_file))

示例中所使用的API文档见 ::ref:`session_voice`