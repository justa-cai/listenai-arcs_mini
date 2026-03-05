.. _session_tetx_sample:

文本对话
######################

概述
****************************
该示例演示了如何将文本上传到云端并进行交互

.. toctree::
   :hidden:

   ../sample_build

.. include:: ../sample_build.rst

调用方法
****************************
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

        lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD 
            | LSC_GOT_TOKEN, lsc_event_cb, NULL);
        lsc_connect();

- 初始化文本会话

    .. code-block:: c

        session_text_init()

- 注册回调函数，并监听结果

    .. code-block:: c

        static void text_event_cb(session_text_event_e evt, void *data, uint32_t size, void *usr)
        {
            //获取事件
        }
        session_text_add_evt_callback(text_event_cb, SESSION_TEXT_TTS_URL | SESSION_TEXT_REPLY_URL, NULL);

- 发送文本"深圳天气如何"到云端

    .. code-block:: c

        session_text_send("深圳天气如何");

示例中所使用的API文档见 ::ref:`session_text`