BT 配对信息 KV 存储方案
=======================

目的
----

本文档用于评审 LISA Bluetooth 配对信息改用 Lisa KV 保存的实现方案，并说明后续在 ``components/lisa_bluetooth/lisa_bluetooth.c`` 中新增配对设备管理接口的设计。

目标接口包括：

.. code-block:: c

    int bt_paired_list_get(bt_paired_info_t *list, uint8_t max_count, uint8_t *out_count);
    int bt_paired_name_get(const gap_bdaddr_t *addr, char *name, size_t name_len);
    int bt_paired_remove(const gap_bdaddr_t *addr);

背景
----

当前 BT 配对信息主要通过 NVDS/NVS 保存，关键路径包括：

- ``soc/arcs/hal/chip/arcs/wcnd/bt_drv/bt_nvs_configure.c``：注册 ``lsip_nvds_api``，当前内部调用 ``nvds_get()``、``nvds_put()``、``nvds_del()``。
- ``soc/arcs/hal/chip/arcs/bt_hal/bt_stack_hal.c``：提供 ``bt_stack_nvs_get()``、``bt_stack_nvs_set()``、``bt_stack_nvs_del()``，当前也直接访问 NVS。
- ``components/lisa_bluetooth/bt_ble_user.c``：在 BLE 连接、配对、断开事件中读取或删除 ``NVS_ID_PEER_ADDRESS``。
- ``components/lisa_bluetooth/bt_classic_user.c``：在 Classic 连接、断开和 link key 保存流程中使用 ``NVS_ID_BT_PEER_ADDRESS`` 和 ``bt_gap_save_lk_mem_to_nvs()``。

BT stack 当前使用的是 NVDS 参数模型：

.. code-block:: c

    get(param_id, lengthPtr, buf)
    set(param_id, length, buf)
    del(param_id)

典型 ``param_id`` 定义在 ``soc/arcs/hal/chip/arcs/wcnd/include/bt_inc/ble_plf_config.h`` 中：

.. code-block:: c

    NVS_ID_BD_ADDRESS       = 0x01;
    NVS_ID_DEVICE_NAME      = 0x02;
    NVS_ID_LOC_IRK          = 0x03;
    NVS_ID_LK_INDEX         = 0x60;
    NVS_ID_LK_FIRST         = 0x61;
    NVS_ID_LK_LAST          = NVS_ID_LK_FIRST + NVS_COUNT_LK - 1;
    NVS_ID_LTK_INDEX        = 0x70;
    NVS_ID_LTK_FIRST        = 0x71;
    NVS_ID_LTK_LAST         = NVS_ID_LTK_FIRST + NVS_COUNT_LTK - 1;
    NVS_ID_PEER_ADDRESS     = 0x90;
    NVS_ID_BT_PEER_ADDRESS  = 0x91;

总体设计
--------

设计原则
~~~~~~~~

1. **保持 BT stack 的参数模型不变**：BT stack 仍然通过 ``param_id`` 读写配对信息，避免修改协议栈内部 link key / LTK 格式。
2. **使用 KV blob 保存原始配对数据**：每个 ``param_id`` 映射为一个 Lisa KV 字符串 key，value 使用 blob 保存。
3. **原始密钥与展示列表分离**：真实配对密钥按 ``param_id`` 保存；用户可见的配对列表、设备名称和类型保存为独立 metadata。
4. **明确新旧存储边界**：KV 后端不兼容迁移旧 NVS 数据；双核存储一致性由 ``lisa_kv`` 模块负责，BT 模块只通过 ``lisa_kv`` API 访问持久化数据。

整体结构如下：

.. code-block:: text

    BT stack / lsip_nvds_api / bt_stack_nvs_xxx
            |
            v
    BT storage adapter
            |
            +-- raw NVDS KV: param_id -> bt-nvds-%02x -> blob
            |
            +-- paired metadata KV: bt-paired-count / bt-paired-item-%u
            |
            v
    Lisa KV

Lisa KV key 映射
~~~~~~~~~~~~~~~~

``lisa_kv`` 不支持直接使用 ``param_id`` 作为 key，它的接口使用字符串 key：

.. code-block:: c

    int lisa_kv_set_blob(const char *key, uint8_t *data, int len);
    int lisa_kv_get_blob(const char *key, uint8_t **data, int *len);
    int lisa_kv_del(const char *key);

因此需要新增适配层，将 ``param_id`` 转换为字符串 key：

.. code-block:: c

    #define BT_STORAGE_NVDS_KEY_MAX_LEN 16

    static int bt_storage_make_nvds_key(char *key, size_t key_len, uint8_t param_id)
    {
        int ret = snprintf(key, key_len, "bt-nvds-%02x", param_id);

        if (ret < 0 || (size_t)ret >= key_len) {
            return -ENAMETOOLONG;
        }

        return 0;
    }

映射示例：

.. list-table::
   :header-rows: 1

   * - ``param_id``
     - Lisa KV key
     - 说明
   * - ``0x01``
     - ``bt-nvds-01``
     - 本地 BT 地址
   * - ``0x03``
     - ``bt-nvds-03``
     - 本地 IRK
   * - ``0x60``
     - ``bt-nvds-60``
     - Classic link key index
   * - ``0x61`` ~ ``0x68``
     - ``bt-nvds-61`` ~ ``bt-nvds-68``
     - Classic link key slot
   * - ``0x70``
     - ``bt-nvds-70``
     - BLE LTK index
   * - ``0x71`` ~ ``0x78``
     - ``bt-nvds-71`` ~ ``bt-nvds-78``
     - BLE LTK slot
   * - ``0x90``
     - ``bt-nvds-90``
     - 最近一次 BLE peer 地址
   * - ``0x91``
     - ``bt-nvds-91``
     - 最近一次 Classic peer 地址

存储模型
--------

Raw NVDS KV
~~~~~~~~~~~

Raw NVDS KV 用于保存 BT stack 真实配对数据，外层不解析 blob 内容。

key 格式：

.. code-block:: text

    bt-nvds-%02x

value 类型：

.. code-block:: text

    blob

HAL 层只声明抽象 storage port，具体 KV 实现在 Lisa Bluetooth 组件内：

.. code-block:: c

    uint8_t bt_storage_port_get(uint8_t param_id, uint8_t *lengthPtr, uint8_t *buf);
    uint8_t bt_storage_port_set(uint8_t param_id, uint8_t length, uint8_t *buf);
    uint8_t bt_storage_port_del(uint8_t param_id);

返回值保持 NVDS 风格：

- ``NVDS_OK``：成功。
- ``NVDS_FAIL``：未找到或读写失败。
- ``NVDS_LENGTH_OUT_OF_RANGE``：用户 buffer 长度不足。

``get`` 行为建议：

.. code-block:: text

    1. 检查参数。
    2. 生成 bt-nvds-%02x key。
    3. 调用 lisa_kv_get_blob() 读取 blob。
    4. 如果 blob 长度大于 *lengthPtr，更新 *lengthPtr 并返回 NVDS_LENGTH_OUT_OF_RANGE。
    5. 拷贝 blob 到 buf，更新 *lengthPtr，返回 NVDS_OK。

Paired metadata KV
~~~~~~~~~~~~~~~~~~

Paired metadata KV 用于实现用户可见接口，不保存协议栈密钥内容。

建议 key：

.. code-block:: text

    bt-paired-count
    bt-paired-item-0
    bt-paired-item-1
    ...

推荐每个 item 使用 blob 保存，避免把字段拆成大量零散 key：

.. code-block:: c

    #define BT_PAIRED_NAME_MAX_LEN 248

    typedef enum {
        BT_PAIRED_TRANSPORT_UNKNOWN = 0,
        BT_PAIRED_TRANSPORT_BLE     = 1,
        BT_PAIRED_TRANSPORT_CLASSIC = 2,
    } bt_paired_transport_t;

    typedef struct {
        uint8_t version;
        uint8_t transport;
        uint8_t addr_type;
        uint8_t name_len;
        uint8_t addr[6];
        uint8_t name[BT_PAIRED_NAME_MAX_LEN];
    } bt_paired_storage_item_t;

metadata 与 raw NVDS KV 的关系：

- raw NVDS KV 负责回连、加密和协议栈内部配对恢复。
- paired metadata KV 负责 ``bt_paired_list_get()``、``bt_paired_name_get()`` 和用户展示。
- metadata 丢失不应影响已配对设备回连，但会影响用户列表展示。
- raw NVDS KV 丢失会导致配对失效。

公开 API 设计
-------------

数据结构
~~~~~~~~

建议在 ``components/lisa_bluetooth/lisa_bluetooth.h`` 中新增：

.. code-block:: c

    #define BT_PAIRED_NAME_MAX_LEN 248
    #define BT_PAIRED_MAX_COUNT    16

    typedef enum {
        BT_PAIRED_TRANSPORT_UNKNOWN = 0,
        BT_PAIRED_TRANSPORT_BLE     = 1,
        BT_PAIRED_TRANSPORT_CLASSIC = 2,
    } bt_paired_transport_t;

    typedef struct {
        gap_bdaddr_t addr;
        uint8_t transport;
        uint8_t name_len;
        uint8_t name[BT_PAIRED_NAME_MAX_LEN];
    } bt_paired_info_t;

字段说明：

- ``addr``：设备地址，复用已有 ``gap_bdaddr_t``。
- ``transport``：BLE、Classic 或未知类型。
- ``name_len``：设备名称长度，允许为 0。
- ``name``：设备名称，允许为空。

``bt_paired_list_get()``
~~~~~~~~~~~~~~~~~~~~~~~~

建议函数原型：

.. code-block:: c

    int bt_paired_list_get(bt_paired_info_t *list, uint8_t max_count, uint8_t *out_count);

功能：

获取当前已配对设备列表。

参数行为：

- ``list == NULL`` 且 ``max_count == 0``：只查询数量，``out_count`` 返回总数。
- ``list != NULL``：最多写入 ``max_count`` 个 item。
- 实际数量大于 ``max_count``：尽量填充 list，``out_count`` 返回实际总数，函数返回 ``-ENOSPC``。

返回值：

- ``0``：成功。
- ``-EINVAL``：参数错误。
- ``-ENOSPC``：用户 buffer 不足。
- ``-EIO``：KV 或 stack 查询失败。

建议内部流程：

.. code-block:: text

    1. 从 paired metadata KV 读取列表。
    2. 如果 BLE stack 已初始化，可调用 ble_gap_get_paired_addr() 同步 BLE paired address。
    3. metadata 中缺失的 BLE 地址补齐为 name_len = 0 的 item。
    4. metadata 中已不存在的 BLE bond 可以清理。
    5. Classic 第一版以 metadata 为主，首次重新配对或回连后补齐最近一次 peer。
    6. 返回合并后的 paired list。

``bt_paired_name_get()``
~~~~~~~~~~~~~~~~~~~~~~~~

建议函数原型：

.. code-block:: c

    int bt_paired_name_get(const gap_bdaddr_t *addr, char *name, size_t name_len);

功能：

按地址查询已配对设备名称。

返回值：

- ``0``：成功，``name`` 中包含以 ``\0`` 结尾的名称。
- ``-EINVAL``：参数错误。
- ``-ENOENT``：该地址不是已配对设备。
- ``-ENODATA``：设备已配对，但没有保存名称。

建议行为：

.. code-block:: text

    1. 扫描 paired metadata。
    2. 地址和 addr_type 都匹配时认为命中。
    3. name_len > 0 时拷贝名称并保证字符串结尾。
    4. name_len == 0 时返回 -ENODATA。
    5. 未找到返回 -ENOENT。

说明：BLE peripheral 被手机连接时，本地不一定能获取手机名称，因此允许名称为空。

``bt_paired_remove()``
~~~~~~~~~~~~~~~~~~~~~~

建议函数原型：

.. code-block:: c

    int bt_paired_remove(const gap_bdaddr_t *addr);

功能：

取消配对设备。

建议语义：

- ``addr != NULL``：删除指定地址的 BLE 和 Classic 配对信息。
- ``addr == NULL``：删除全部配对信息。

内部调用：

.. code-block:: c

    ble_gap_delete_bond((gap_bdaddr_t *)addr);

    #if CONFIG_LISA_BLUETOOTH_CLASSIC
    bt_gap_delete_bond((gap_bdaddr_t *)addr);
    #endif

    bt_paired_storage_remove(addr);

删除全部时：

.. code-block:: c

    ble_gap_delete_bond(NULL);

    #if CONFIG_LISA_BLUETOOTH_CLASSIC
    bt_gap_delete_bond(NULL);
    #endif

    bt_paired_storage_clear();

返回值：

- ``0``：成功。
- ``-EINVAL``：参数错误。
- ``-EIO``：metadata/KV 删除失败。

注意：当前 ``ble_gap_delete_bond()`` 返回 ``void``，因此公开 API 很难精确感知协议栈内部删除是否成功；返回值主要反映参数检查和 metadata/KV 操作结果。

文件修改建议
------------

新增文件
~~~~~~~~

建议新增 paired metadata 存储模块：

.. code-block:: text

    components/lisa_bluetooth/storage/bt_paired_storage.c
    components/lisa_bluetooth/storage/bt_paired_storage.h

建议职责：

.. code-block:: c

    int bt_paired_storage_load_list(bt_paired_info_t *list, uint8_t max_count, uint8_t *out_count);
    int bt_paired_storage_find_name(const gap_bdaddr_t *addr, char *name, size_t name_len);
    int bt_paired_storage_upsert(const bt_paired_info_t *item);
    int bt_paired_storage_remove(const gap_bdaddr_t *addr);
    int bt_paired_storage_clear(void);

建议新增 raw NVDS KV adapter：

.. code-block:: text

    components/lisa_bluetooth/storage/bt_storage_kv.c
    soc/arcs/hal/chip/arcs/wcnd/include/bt_inc/bt_storage_port.h

建议职责：

.. code-block:: c

    uint8_t bt_storage_port_get(uint8_t param_id, uint8_t *lengthPtr, uint8_t *buf);
    uint8_t bt_storage_port_set(uint8_t param_id, uint8_t length, uint8_t *buf);
    uint8_t bt_storage_port_del(uint8_t param_id);

修改文件
~~~~~~~~

建议修改：

- ``components/lisa_bluetooth/Kconfig``：增加 BT storage backend 配置。
- ``components/lisa_bluetooth/CMakeLists.txt``：加入 ``storage/bt_paired_storage.c`` 和 KV 模式下的 ``storage/bt_storage_kv.c``。
- ``components/lisa_bluetooth/lisa_bluetooth.h``：声明 ``bt_paired_info_t`` 和三个公开 API。
- ``components/lisa_bluetooth/lisa_bluetooth.c``：实现三个公开 API。
- ``components/lisa_bluetooth/bt_ble_user.c``：BLE 连接和配对事件中维护 metadata。
- ``components/lisa_bluetooth/bt_classic_user.c``：Classic 连接、认证、link key 保存路径中维护 metadata。
- ``soc/arcs/hal/chip/arcs/wcnd/bt_drv/bt_nvs_configure.c``：KV 模式下注册 storage port。
- ``soc/arcs/hal/chip/arcs/bt_hal/bt_stack_hal.c``：KV 模式下 ``bt_stack_nvs_xxx`` 走 storage port。
- ``soc/arcs/hal/chip/arcs/wcnd/include/bt_inc/bt_storage_port.h``：声明 HAL 可调用的 storage port 抽象接口。

Kconfig 方案
------------

建议在 ``components/lisa_bluetooth/Kconfig`` 中新增带 ``Disabled`` 默认项的后端选择。默认情况下 KV 和 NVS 保存都不开启，避免改变存量工程行为；需要配对持久化的工程显式选择一种后端。

.. code-block:: kconfig

    choice LISA_BLUETOOTH_STORAGE_BACKEND
        prompt "Bluetooth pairing storage backend"
        default LISA_BLUETOOTH_STORAGE_NONE

    config LISA_BLUETOOTH_STORAGE_NONE
        bool "Disabled"

    config LISA_BLUETOOTH_STORAGE_NVS
        bool "NVS"
        depends on ARCS_HAL_NVS || ARCS_HAL_IPC_MRPC_CLIENT_NVS

    config LISA_BLUETOOTH_STORAGE_KV
        bool "Lisa KV"
        select LISA_KV

    endchoice

配置策略：

- ``CONFIG_LISA_BLUETOOTH_STORAGE_KV`` 默认关闭。
- ``CONFIG_LISA_BLUETOOTH_STORAGE_NVS`` 默认关闭。
- 通过 ``choice`` 保证 ``Disabled``、``NVS``、``Lisa KV`` 三者互斥。
- 两者都关闭时，BT 配对信息不持久化，``bt_paired_list_get()`` 返回空列表，``bt_paired_name_get()`` 返回 ``-ENOENT``，``bt_paired_remove()`` 只尝试清理协议栈内存态 bond。
- KV 后端不读取旧 NVS 数据；从 NVS 切换到 KV 后，需要重新配对生成 KV 记录。

示例工程如需验证 KV 保存，显式开启 ``CONFIG_LISA_BLUETOOTH_STORAGE_KV``，并采用 LSFS KV 后端。

CMake 方案
----------

``components/lisa_bluetooth/CMakeLists.txt`` 增加：

.. code-block:: cmake

    listenai_library_sources(
        storage/bt_paired_storage.c
    )

    if (CONFIG_LISA_BLUETOOTH_STORAGE_KV)
        listenai_library_sources(
            storage/bt_storage_kv.c
        )
    endif()

``storage/bt_storage_kv.c`` 放在 Lisa Bluetooth 组件目录，避免 HAL 直接依赖 ``lisa_kv``。HAL 目录只保留 ``bt_storage_port.h`` 抽象声明。

KV 初始化要求
-------------

``lisa_kv`` 使用前必须调用 ``lisa_kv_init()``。

推荐在 BT KV adapter 内部做一次性初始化：

.. code-block:: c

    static int bt_storage_kv_ensure_init(void)
    {
        static bool inited;

        if (inited) {
            return 0;
        }

        if (lisa_kv_init() != 0) {
            return -EIO;
        }

        inited = true;
        return 0;
    }

示例工程采用 ``CONFIG_LISA_KV_TYPE_LSFS`` 验证 BT 配对信息保存，便于和 WiFi KV 示例使用方式保持一致。使用 LSFS KV 时必须保证文件系统已经 mount，且 ``lisa_kv_init()`` 发生在 BT stack 访问配对信息之前。

产品工程可根据启动时序选择 KV 后端：

- ``CONFIG_LISA_KV_TYPE_LSFS``：推荐用于示例和已有文件系统初始化链路的应用，便于统一查看 KV 文件。
- ``CONFIG_LISA_KV_TYPE_EF``：适合初始化阶段较早、暂不依赖文件系统 mount 的应用。

设备名称来源
------------

``bt_paired_name_get()`` 依赖 metadata 中保存的名称，BT stack 的原始配对记录不一定包含名称。

设备名称通常只有 source 角色需要保存，例如 A2DP/HFP source 扫描并连接耳机或手机时，需要保存 sink 设备名称用于 UI 展示和按名称查询。Sink 角色或 BLE peripheral 场景通常不需要、也不一定能获取对端名称。

建议 source 场景名称来源优先级：

1. Classic inquiry 结果中的 ``struct gap_dev_name``。
2. ``lisa_bluetooth.c`` 中已发现设备缓存 ``g_discovered_devices``。
3. 连接成功时按地址查找最近一次发现结果并写入 metadata。
4. 非 source 场景无法获取或无需获取对端名称时，保存空名称。

因此 ``bt_paired_name_get()`` 需要支持 ``-ENODATA``，表示设备已配对但没有名称。

事件维护 metadata
-----------------

BLE 事件
~~~~~~~~

文件：``components/lisa_bluetooth/bt_ble_user.c``。

当前 ``bt_stack_ble_conn_ind()`` 有 ``peer_addr``，建议缓存当前 BLE peer：

.. code-block:: c

    static gap_bdaddr_t s_ble_peer_addr;
    static bool s_ble_peer_valid;

    void bt_stack_ble_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
    {
        s_ble_peer_addr = *peer_addr;
        s_ble_peer_valid = true;
        ...
    }

在 ``GAP_PAIRING_SUCCEED`` 时写入 metadata：

.. code-block:: c

    if (s_ble_peer_valid) {
        bt_paired_info_t item = {0};
        item.addr = s_ble_peer_addr;
        item.transport = BT_PAIRED_TRANSPORT_BLE;
        bt_paired_storage_upsert(&item);
    }

在 ``GAP_PAIRING_FAILED`` 或删除 bond 时同步删除 metadata。

Classic 事件
~~~~~~~~~~~~

文件：``components/lisa_bluetooth/bt_classic_user.c``。

当前 ``bt_stack_classic_conn_ind()`` 有 ``peer_addr``，建议缓存 Classic peer，并尝试从发现列表中补充名称。

在以下时机 upsert metadata：

- Classic 连接成功。
- ``GAP_BT_LINK_AUTH_REQ`` 且 ``value == 0``，表示认证成功。
- ``bt_gap_save_lk_mem_to_nvs(conidx)`` 后，确认 link key 已保存。

NVS 切换说明
------------

KV 后端不读取、不迁移旧 NVS 配对数据。工程从 ``CONFIG_LISA_BLUETOOTH_STORAGE_NVS`` 切换到
``CONFIG_LISA_BLUETOOTH_STORAGE_KV`` 后，需要重新配对生成新的 ``bt-nvds-*`` raw 记录和
``bt-paired-*`` metadata 记录。

这样可以避免 BT 模块同时维护两套持久化后端和旧数据解析逻辑，升级兼容策略由产品侧根据实际需求单独处理。

双核场景
--------

当前存在 ``CFG_AMP_IPC_MRPC_CLIENT_NVS``，说明旧 NVS 路径已经考虑过跨核访问。切到 Lisa KV 后，跨核存储 owner 和同步机制应由 ``lisa_kv`` 模块统一解决，BT 模块不单独实现 KV MRPC，也不直接感知 KV 实际运行在哪个核。

BT 模块侧原则：

.. code-block:: text

    BT -> lisa_kv API -> lisa_kv 内部后端/跨核机制

如果某个 AP/CP 组合下 ``lisa_kv`` 尚不支持安全跨核访问，则该限制应记录为 ``lisa_kv`` 模块能力缺口；BT KV 后端只依赖 ``lisa_kv`` 对外承诺。

与 WiFi KV 方案的差异
---------------------

WiFi 当前将每个字段拆成独立 KV，例如：

.. code-block:: text

    wifi-kv-count
    wifi-kv-item-0-ssid
    wifi-kv-item-0-pwd
    wifi-kv-item-0-pmk

BT 不建议照搬字段拆分方式保存真实配对密钥，原因：

1. BT LTK/LK blob 是协议栈私有结构。
2. 外层不应解析 link key 内部字段。
3. 按 ``param_id`` 保存 blob 更贴近原 NVDS 语义，风险更低。

推荐差异化设计：

.. code-block:: text

    真实配对数据：param_id -> bt-nvds-%02x -> blob
    展示列表数据：bt-paired-count / bt-paired-item-%u -> metadata blob

风险与待确认项
--------------

1. **NVS 切换到 KV 后需重新配对**：KV 后端不迁移旧 NVS 记录，产品升级时如果需要保留旧配对关系，需要单独设计迁移策略。
2. **设备名称缺失**：设备名主要面向 source 角色保存 sink 名称；非 source 或 BLE peripheral 场景可能没有名称，``bt_paired_name_get()`` 需要允许返回 ``-ENODATA``。
3. **双核 KV 能力**：双核存储 owner 和跨核同步由 ``lisa_kv`` 模块实现；BT 侧只通过 ``lisa_kv`` API 访问。如果 ``lisa_kv`` 尚不支持某个双核场景，需要在 ``lisa_kv`` 模块补齐。
4. **KV 后端选择**：示例采用 LSFS KV；产品工程需保证文件系统 mount 和 ``lisa_kv_init()`` 时序，或根据场景切换 EF KV。
5. **删除接口返回值**：``ble_gap_delete_bond()`` 返回 ``void``，公开 API 只能反映 metadata 删除状态，无法完全表达协议栈内部删除结果。
6. **默认配置策略**：``CONFIG_LISA_BLUETOOTH_STORAGE_KV`` 和 ``CONFIG_LISA_BLUETOOTH_STORAGE_NVS`` 默认都不开启，避免默认改变存量工程持久化行为。

验证计划
--------

编译验证
~~~~~~~~

至少覆盖以下工程：

.. code-block:: bash

    ./build.sh -S samples/bluetooth/ble/peripheral/pairing -DBOARD=arcs_evb -C
    ./build.sh -S samples/bluetooth/classic/a2dp_source -DBOARD=arcs_evb -C
    ./build.sh -S samples/bluetooth/classic/hfp_source -DBOARD=arcs_evb -C

BLE 配对验证
~~~~~~~~~~~~

1. 烧录 BLE pairing sample。
2. 手机连接并完成配对。
3. 调用 ``bt_paired_list_get()``，应返回手机地址。
4. 调用 ``bt_paired_name_get()``：如果有名称返回 ``0``，否则返回 ``-ENODATA``。
5. 重启设备后再次调用 ``bt_paired_list_get()``，地址仍存在。
6. 调用 ``bt_paired_remove(addr)``。
7. 重启设备后，``bt_paired_list_get()`` 不再返回该地址。

Classic 配对验证
~~~~~~~~~~~~~~~~

1. 使用 A2DP/HFP sample 扫描并连接手机或耳机。
2. 完成 Classic pairing 并保存 link key。
3. 调用 ``bt_paired_list_get()``，应返回 Classic 设备地址。
4. 如果扫描阶段获取到名称，``bt_paired_name_get()`` 应返回名称。
5. 重启设备，验证无需重新配对即可回连。
6. 调用 ``bt_paired_remove(addr)`` 后再次连接，应触发重新配对。

实施阶段
--------

阶段 1：KV 后端替换
~~~~~~~~~~~~~~~~~~~

目标：BT raw pairing data 从 NVS 切到 Lisa KV。

任务：

1. 新增 ``bt_storage_port`` 抽象和组件侧 ``bt_storage_kv`` 实现。
2. 修改 ``bt_nvs_configure.c``。
3. 修改 ``bt_stack_hal.c``。
4. 增加 Kconfig 和 CMake。
5. 验证配对、重启、回连正常。

阶段 2：公开 paired API
~~~~~~~~~~~~~~~~~~~~~~~

目标：新增 ``bt_paired_list_get()``、``bt_paired_name_get()``、``bt_paired_remove()``。

任务：

1. 新增 ``bt_paired_storage``。
2. 在 ``lisa_bluetooth.c`` 实现三个 API。
3. 在 ``lisa_bluetooth.h`` 声明结构和函数。
4. 在 BLE/Classic 事件路径维护 metadata。

阶段 3：双核场景完善
~~~~~~~~~~~~~~~~~~~~

目标：确认 ``lisa_kv`` 已覆盖 AP/CP 场景的 KV owner 和跨核访问能力。

可能任务：

1. 梳理 BT 所在核调用 ``lisa_kv`` 的实际路径。
2. 如果 ``lisa_kv`` 已支持目标双核场景，BT 侧无需额外实现。
3. 如果 ``lisa_kv`` 不支持目标双核场景，在 ``lisa_kv`` 模块补齐跨核后端能力。

结论
----

推荐采用如下设计：

.. code-block:: text

    真实配对数据：BT stack param_id -> bt-nvds-%02x -> lisa_kv blob
    公开配对列表：bt-paired-count / bt-paired-item-%u -> metadata blob
    用户 API：bt_paired_list_get() / bt_paired_name_get() / bt_paired_remove()

该方案可以在不修改 BT stack 内部配对数据格式的前提下，将持久化后端切换到 Lisa KV，并为应用层提供稳定的配对设备管理接口。
