LISA Bluetooth 组件
===================

简介
----

LISA Bluetooth 是一个蓝牙初始化和管理组件，封装了 BLE 协议栈的初始化流程，提供统一的 API 接口，支持自定义广播数据、扫描响应数据以及 GAP 配置。

主要特性
--------

- **统一初始化**：提供 ``lisa_bluetooth_init()`` 接口，简化协议栈启动流程。
- **自定义广播数据**：支持通过弱符号覆盖机制自定义广播数据。
- **自定义扫描响应**：支持通过弱符号覆盖机制自定义扫描响应数据。
- **自定义 GAP 配置**：支持自定义 MAC 地址、设备名称等 GAP 参数。
- **广播控制**：提供简单的广播开启和停止接口。

API 参考
--------

核心函数
~~~~~~~~

.. c:function:: int lisa_bluetooth_init(void)

   初始化蓝牙协议栈。

   :return: 0 成功，非 0 失败。

.. c:function:: uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type)

   开启 BLE 广播。

   :param adv_id: 广播 ID。
   :param adv_type: 广播类型 (参考 ``enum app_adv_type``)。
   :return: 状态码。

.. c:function:: uint8_t app_ble_adv_stop(uint8_t adv_id)

   停止 BLE 广播。

   :param adv_id: 广播 ID。
   :return: 状态码。

自定义接口 (Weak Symbols)
~~~~~~~~~~~~~~~~~~~~~~~~~

用户可以在应用程序中实现以下函数来覆盖默认行为。

.. c:function:: const uint8_t* lisa_bt_get_adv_data(uint8_t *len)

   获取自定义广播数据。

   :param len: [输出] 数据长度指针。
   :return: 指向广播数据的指针。

.. c:function:: const uint8_t* lisa_bt_get_scan_rsp_data(uint8_t *len)

   获取自定义扫描响应数据。

   :param len: [输出] 数据长度指针。
   :return: 指向扫描响应数据的指针。

.. c:function:: void lisa_bt_gap_config(ble_gap_cfg_t *cfg)

   配置 GAP 参数（如 MAC 地址、设备名称）。

   :param cfg: [输入/输出] GAP 配置结构体指针。

使用示例
--------

自定义广播数据和 GAP 配置
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "lisa_bluetooth.h"
    #include "bt_app_if.h"
    #include <string.h>

    // 自定义广播数据
    static const uint8_t user_adv_data[] = {
        0x02, 0x01, 0x06, // Flags
        0x09, 0x09, 'M', 'Y', '_', 'B', 'L', 'E', // Local Name
    };

    const uint8_t* lisa_bt_get_adv_data(uint8_t *len)
    {
        *len = sizeof(user_adv_data);
        return user_adv_data;
    }

    // 自定义 GAP 配置
    void lisa_bt_gap_config(ble_gap_cfg_t *cfg)
    {
        // 设置自定义 MAC 地址
        uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        memcpy(cfg->addr.addr, mac, 6);
        
        // 设置设备名称
        const char *name = "MY_DEVICE";
        cfg->name_len = strlen(name);
        if (cfg->name_len > sizeof(cfg->name)) {
            cfg->name_len = sizeof(cfg->name);
        }
        memcpy(cfg->name, name, cfg->name_len);
    }

    int main(void)
    {
        // 初始化蓝牙
        lisa_bluetooth_init();

        // 开启广播
        app_ble_adv_start(0, BLE_ADV_GEN);

        // ...
    }
