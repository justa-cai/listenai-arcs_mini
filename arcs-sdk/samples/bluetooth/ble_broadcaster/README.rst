BLE 广播示例
============

功能说明
--------

本示例演示如何使用 LISA Bluetooth 组件实现 BLE 广播功能。
主要功能包括：

1. 自定义广播数据（Flags, Local Name）。
2. 自定义扫描响应数据（Manufacturer Specific Data）。
3. 自定义 GAP 配置（MAC 地址，设备名称）。

硬件连接
--------

无需外部连接，使用板载 BLE 功能。

示例内容
--------

1. 初始化 NVS 和 射频校准。
2. 初始化 LISA Bluetooth 组件。
3. 系统自动开始广播。


编译
--------
.. include:: /sample_build.rst

预期输出
--------

.. code-block:: text

   [I][sample] === BLE Broadcaster Example ===
   [I][lisa_bt] BLE Stack Initialized
   [I][lisa_bt] Advertising started
   ...

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``lisa_bluetooth_init()``
     - 初始化蓝牙协议栈
   * - ``lisa_bt_get_adv_data()``
     - 获取自定义广播数据（弱符号覆盖）
   * - ``lisa_bt_get_scan_rsp_data()``
     - 获取自定义扫描响应数据（弱符号覆盖）
   * - ``lisa_bt_gap_config()``
     - 自定义 GAP 配置（弱符号覆盖）

关键代码
--------

.. code-block:: c

   /* 自定义广播数据 */
   const uint8_t* lisa_bt_get_adv_data(uint8_t *len)
   {
       *len = sizeof(user_adv_data);
       return user_adv_data;
   }

   /* 自定义 GAP 配置 */
   void lisa_bt_gap_config(ble_gap_cfg_t *cfg)
   {
       // Set custom MAC address
       uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
       memcpy(cfg->addr.addr, mac, 6);
       // ...
   }

注意事项
--------

1. **MAC 地址**: 示例中使用了固定的 MAC 地址，实际产品中应从 NVS 或其他存储中读取。
2. **广播数据**: 广播数据长度有限制（通常 31 字节），请注意不要溢出。
