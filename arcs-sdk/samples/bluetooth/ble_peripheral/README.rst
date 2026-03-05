BLE 外设示例
============

功能说明
--------

本示例演示如何使用 LISA Bluetooth 组件实现 BLE 外设功能，并添加一个自定义 GATT 服务。
主要功能包括：

1. 自定义广播数据（Flags, Local Name）。
2. 自定义扫描响应数据（Manufacturer Specific Data）。
3. 自定义 GAP 配置（MAC 地址，设备名称）。
4. 启动 BLE 协议栈（外设模式）。
5. 自定义 GATT 服务 (UUID: 0x1234)，包含以下特征值：
    - **只读特征值** (UUID: 0x1235): 仅支持读取。
    - **只写特征值** (UUID: 0x1236): 支持 Write Request 和 Write Command。
    - **通知特征值** (UUID: 0x1237): 支持 Notify，带有 CCCD。
    - **读写特征值** (UUID: 0x1238): 支持读取和写入。
    - **安全读特征值** (UUID: 0x1239): 需要配对/认证后才能读取。

硬件连接
--------

无需外部连接，使用板载 BLE 功能。

示例内容
--------

1. 初始化 NVS 和 射频校准。
2. 初始化 LISA Bluetooth 组件。
3. 注册自定义 GATT 服务。
4. 系统自动开始广播（作为外设）。

编译
--------

.. include:: /sample_build.rst

预期输出
--------

.. code-block:: text

   [I][sample] === BLE Peripheral Example ===
   [I][lisa_bt] BLE Stack Initialized
   [I][custom_svc] Initializing Custom Service
   [I][custom_svc] Custom Service added, start_hdl=...
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
   * - ``ble_gatt_user_register()``
     - 注册 GATT 回调
   * - ``ble_gatt_db_svc16_add()``
     - 添加 16-bit UUID 服务到数据库

关键代码
--------

**自定义广播数据**

.. code-block:: c

   /* 自定义广播数据 */
   const uint8_t* lisa_bt_get_adv_data(uint8_t *len)
   {
       *len = sizeof(user_adv_data);
       return user_adv_data;
   }

**自定义服务属性表**

.. code-block:: c

   static const ble_gatt_att16_desc_t custom_att_db[IDX_NB] = {
       // Service Declaration
       [IDX_SVC] = {BLE_GATT_DECL_PRIMARY_SERVICE, BLE_PROP(RD), 0},

       // Char 1: Read (只读)
       [IDX_CHAR_READ_DECL] = {BLE_GATT_DECL_CHARACTERISTIC, BLE_PROP(RD), 0},
       [IDX_CHAR_READ_VAL] = {CUSTOM_CHAR_READ_UUID, BLE_PROP(RD), 20},

       // ... (其他特征值)
       
       // Char 5: Secure Read (安全读取，需要认证)
       [IDX_CHAR_SEC_READ_DECL] = {BLE_GATT_DECL_CHARACTERISTIC, BLE_PROP(RD), 0},
       [IDX_CHAR_SEC_READ_VAL] = {CUSTOM_CHAR_SEC_READ_UUID, BLE_PROP(RD) | BLE_SEC_LVL(RP, AUTH), 20},
   };

注意事项
--------

1. **MAC 地址**: 示例中使用了固定的 MAC 地址，实际产品中应从 NVS 或其他存储中读取。
2. **广播数据**: 广播数据长度有限制（通常 31 字节），请注意不要溢出。
3. **安全特征值**: 访问安全特征值（如 UUID 0x1239）时，手机端通常会弹出配对请求，需确认配对后方可读取。
