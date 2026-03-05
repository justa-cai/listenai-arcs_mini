.. _lsc_connect_sample:

平台连接
================================

概述
---------------------------
该示例演示如何和云端大模型建立连接

.. toctree::
   :hidden:

   ../sample_build

.. include:: ../sample_build.rst

出现以下日志表示示例成功运行

.. code-block:: bash

    [lsc_conn] ws data: 81, {"action":"connected","cid":"9a0b421aef94","code":"0","data":"","desc":"success"}
    [lsc_core] [_lsc_core_conn_evt_cb] evt:conn data cjson 
    [main] evt : lsc connected


调用方法
---------------------------
- 初始化lsc服务，并设置设备ID、产品ID、secret ID以及是否自动重连

    .. code-block:: c

            lsc_config_t cfg = {
                .device_id = DEVICE_ID_STRING,
                .product_id = PRODUCT_ID_STRING,
                .secret_id = SECRET_ID_STRING,
                .if_auto_reconn = true,
                .reconn_interval_ms = 3000,
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

示例中所使用的API文档见 ::ref:`lsc_connect`