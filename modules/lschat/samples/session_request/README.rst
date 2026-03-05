.. _session_request_sample:

服务请求
######################

概述
****
该示例演示了如何使用云端的语音合成服务，将指定的本文进行语音合成，并获取URL

.. toctree::
   :hidden:

   ../sample_build

.. include:: ../sample_build.rst

调用方法
********
初始化lsc服务，并设置设备ID、产品ID、secret ID

  .. code-block:: c

        lsc_config_t cfg = {
            .device_id = DEVICE_ID_STRING,
            .product_id = PRODUCT_ID_STRING,
            .secret_id = SECRET_ID_STRING,
        };        
        lsc_init(lsc_config_t *cfg);

注册lsc事件回调函数，发起连接，并通过回调函数获取连接结果

  .. code-block:: c

        static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
        {
            //获取事件
        }

        lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD 
            | LSC_GOT_TOKEN, lsc_event_cb, NULL);
        lsc_connect();

初始化请求会话

    .. code-block:: c

        session_request_init()

请求语音合成, 传入文本和超时时间，云端将合成结果下发至request_url

    .. code-block:: c

        char text[] = "深圳天气";
        session_request_xtts(text, request_url, 30000);

示例中所使用的API文档见 ::ref:`session_request`