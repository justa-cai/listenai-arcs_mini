BLE 配网示例
============

功能说明
--------

本示例演示如何通过 BLE 将 WiFi 凭据传递给设备，实现无屏配网。
主要功能包括：

1. 通过 BLE netcfg profile 接收 WiFi SSID 和密码。
2. 使用 ``lisa_ble_netcfg_set_handler()`` 注册回调，实现依赖反转（BLE 组件不直接调用 WiFi API）。
3. WiFi Manager 自动管理连接和 DHCP。

硬件连接
--------

无需外部连接，使用板载 WiFi + BLE 功能。

示例内容
--------

1. 初始化 NVS 和 MAC Manager。
2. 初始化 WiFi Manager 并注册连接状态回调。
3. 初始化 LISA Bluetooth 并注册 netcfg + bass 服务。
4. 通过 ``lisa_ble_netcfg_set_handler()`` 注册 WiFi 连接回调。
5. 开始 BLE 广播，等待手机 APP 下发 WiFi 凭据。

编译
--------
.. include:: /sample_build.rst

预期输出
--------

.. code-block:: text

   [I][netcfg] lisa_wifi_init_done
   [I][lisa_bt] BLE Stack Initialized
   [I][lisa_bt] Advertising started
   ...

   (手机 APP 下发 WiFi 凭据后)
   [I][netcfg] BLE netcfg: connecting to 'YourSSID'
   [I][netcfg] WiFi connection status: 0
   [I][netcfg] DHCP OK VIF-0: IP=192.168.1.100

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``lisa_bluetooth_init()``
     - 初始化蓝牙协议栈
   * - ``lisa_ble_netcfg_set_handler()``
     - 注册 BLE 配网 WiFi 连接回调（依赖反转）
   * - ``lisa_ble_adv_start()``
     - 启动 BLE 广播
   * - ``lisa_wifi_init()``
     - 初始化 WiFi 子系统
   * - ``wifi_mgr_sta_connect()``
     - 发起 WiFi STA 连接

关键代码
--------

.. code-block:: c

   /* WiFi connect handler invoked by BLE netcfg profile */
   static int netcfg_wifi_connect_handler(const char *ssid, const char *pwd)
   {
       wifi_mgr_sta_config_t cfg = {0};
       strncpy(cfg.ssid, ssid, sizeof(cfg.ssid) - 1);
       strncpy(cfg.pwd, pwd, sizeof(cfg.pwd) - 1);
       return wifi_mgr_sta_connect(&cfg, false);
   }

   int main(int argc, char **argv)
   {
       // ... init WiFi, BLE ...

       /* Register WiFi connect handler (dependency inversion) */
       lisa_ble_netcfg_set_handler(netcfg_wifi_connect_handler);

       lisa_ble_adv_start(0, LISA_BLE_ADV_GEN);
       // ...
   }

注意事项
--------

1. **依赖反转**: BLE 组件通过回调获取 WiFi 连接能力，不直接依赖 WiFi 头文件，使得纯 BLE 示例无需链接 WiFi 库。
2. **文件系统**: ``WIFI_MANAGER`` 通过 ``LISA_KV`` 间接依赖 ``LSFS``，因此 ``prj.conf`` 中必须启用 ``CONFIG_FILE_SYSTEM`` 和 ``CONFIG_DISK_DRIVER``。
3. **Lisa Bluetooth 配置**: ``CONFIG_LISA_BLUETOOTH_BLE_NETCFG`` 会启用本示例所需的 BLE netcfg 能力，底层依赖的 ``BLE_PROFILE_*`` 由 ``lisa_bluetooth`` 组件内部自动选择，无需在 ``prj.conf`` 单独配置。
