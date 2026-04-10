#include "lisa_bluetooth.h"

#include <bt_os_task.h>
#include <ble_drv.h>
#include <aud_os_task.h>
#include <os_task_init.h>
#include <rf_cali.h>
#include <string.h>

#include "btos_al.h"
#include "lisa_log.h"

#define PLATFORM_MEM_NOCACHE_MALLOC
#define TAG "LISA_BT"

// BLE stack enable complete callback (set via lisa_bluetooth_init)
static lisa_bluetooth_enable_cmp_cb_t g_enable_cmp_cb = NULL;

// Bluetooth device discovery callback
static lisa_bt_discovery_callback_t g_discovery_callback = NULL;

// BLE connection/disconnection callbacks
static lisa_ble_conn_cb_t g_ble_conn_cb = NULL;
static lisa_ble_disc_cb_t g_ble_disc_cb = NULL;
static lisa_ble_bond_cb_t g_ble_bond_cb = NULL;
static lisa_ble_key_req_cb_t g_ble_key_req_cb = NULL;

// Discovered device list
static lisa_bt_discovery_info_t g_discovered_devices[MAX_DISCOVERED_DEVICES];
static uint8_t g_discovered_count = 0;

static void lsip_eif_empty_complete(ble_eif_callback callback, void *dummy)
{
    if (callback) {
        callback(dummy, 0);
    }
}

static void lsip_eif_empty_read(uint8_t *bufptr, uint32_t size, ble_eif_callback callback, void *dummy)
{
    (void)bufptr;
    (void)size;

    lsip_eif_empty_complete(callback, dummy);
}

static void lsip_eif_empty_write(uint8_t *bufptr, uint32_t size, ble_eif_callback callback, void *dummy)
{
    (void)bufptr;
    (void)size;

    lsip_eif_empty_complete(callback, dummy);
}

static void lsip_eif_empty_flow_on(void)
{
}

static bool lsip_eif_empty_flow_off(void)
{
    return true;
}

static const struct ble_eif_api lsip_eif_empty_api = {
    .read = lsip_eif_empty_read,
    .write = lsip_eif_empty_write,
    .flow_on = lsip_eif_empty_flow_on,
    .flow_off = lsip_eif_empty_flow_off,
};

const struct lsip_eif_api *lsip_eif_get(uint8_t index)
{
    (void)index;

    return (const struct lsip_eif_api *)&lsip_eif_empty_api;
}

void bt_stack_classic_discover_ind(gap_bdaddr_t *peer_addr, uint16_t clk_off, int8_t rssi, 
                                     uint8_t mode, uint32_t cod, struct gap_dev_name *name)
{
    if (!peer_addr) {
        return;
    }
    
    lisa_bt_discovery_info_t info = {0};
    
    // 填充设备地址
    info.addr = *peer_addr;
    
    // 填充其他信息
    info.clk_off = clk_off;
    info.rssi = rssi;
    info.mode = mode;
    info.cod = cod;
    
    // 填充设备名称
    if (name && name->value_length > 0) {
        info.name_len = name->value_length < sizeof(info.name) ? name->value_length : sizeof(info.name);
        memcpy(info.name, name->value, info.name_len);
    }
    
    // 检查设备是否已存在（通过地址比对）
    int found_index = -1;
    for (int i = 0; i < g_discovered_count; i++) {
        if (memcmp(&g_discovered_devices[i].addr, &info.addr, sizeof(gap_bdaddr_t)) == 0) {
            found_index = i;
            break;
        }
    }
    
    // 更新或添加设备到列表
    if (found_index >= 0) {
        // 更新已存在的设备信息（可能是RSSI变化或获取到名称）
        g_discovered_devices[found_index] = info;
    } else if (g_discovered_count < MAX_DISCOVERED_DEVICES) {
        // 添加新设备
        g_discovered_devices[g_discovered_count++] = info;
    }
    
    // 调用回调函数
    if (g_discovery_callback) {
        g_discovery_callback(&info);
    }
}

int lisa_bluetooth_inquiry_start(gapm_disc_type_e mode, uint8_t max_count)
{
    // 开始新的扫描前清空旧的设备列表
    lisa_bluetooth_clear_discovered_devices();
    
    return app_bt_inq_start(mode, max_count);
}

int lisa_bluetooth_connect_by_name(const char *name)
{
    if (!name || strlen(name) == 0) {
        LISA_LOGE(TAG, "Invalid parameter: name is NULL or empty");
        return -2;  // 无效参数
    }
    
    LISA_LOGI(TAG, "Trying to connect to device: %s", name);
    LISA_LOGI(TAG, "Device list count: %d", g_discovered_count);
    
    // 在已发现的设备列表中查找匹配的设备
    for (int i = 0; i < g_discovered_count; i++) {
        if (g_discovered_devices[i].name_len > 0) {
            LISA_LOGI(TAG, "Device[%d]: %.*s", i, g_discovered_devices[i].name_len, g_discovered_devices[i].name);
            LISA_LOGI(TAG, "addr: %02X:%02X:%02X:%02X:%02X:%02X", 
                      g_discovered_devices[i].addr.addr[0], g_discovered_devices[i].addr.addr[1],
                      g_discovered_devices[i].addr.addr[2], g_discovered_devices[i].addr.addr[3],
                      g_discovered_devices[i].addr.addr[4], g_discovered_devices[i].addr.addr[5]);
            // 比较设备名称
            if (strncmp((char *)g_discovered_devices[i].name, name, g_discovered_devices[i].name_len) == 0) {
                LISA_LOGI(TAG, "Device found! Connecting...");
                app_bt_inq_stop();
                app_bt_conn(g_discovered_devices[i].addr, 2, g_discovered_devices[i].clk_off, 0);
                return 0;
            }
        }
    }
    
    LISA_LOGE(TAG, "Device not found in list");
    return -1;  // 未找到设备
}

int lisa_bluetooth_get_discovered_devices(const lisa_bt_discovery_info_t **list, uint8_t *count)
{
    if (!list || !count) {
        return -1;
    }
    
    *list = g_discovered_devices;
    *count = g_discovered_count;
    return 0;
}

void lisa_bluetooth_clear_discovered_devices(void)
{
    g_discovered_count = 0;
    memset(g_discovered_devices, 0, sizeof(g_discovered_devices));
}

void lisa_bluetooth_register_discovery_callback(lisa_bt_discovery_callback_t callback)
{
    g_discovery_callback = callback;
}

void lisa_ble_register_conn_cb(lisa_ble_conn_cb_t cb)
{
    g_ble_conn_cb = cb;
}

void lisa_ble_register_disc_cb(lisa_ble_disc_cb_t cb)
{
    g_ble_disc_cb = cb;
}

void lisa_ble_register_bond_cb(lisa_ble_bond_cb_t cb)
{
    g_ble_bond_cb = cb;
}

void lisa_ble_register_key_req_cb(lisa_ble_key_req_cb_t cb)
{
    g_ble_key_req_cb = cb;
}

void lisa_ble_notify_enable_cmp(uint16_t status)
{
    if (g_enable_cmp_cb) {
        g_enable_cmp_cb(status);
    }
}

void lisa_ble_notify_connected(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr)
{
    if (g_ble_conn_cb) {
        g_ble_conn_cb(conidx, conhdl, peer_addr);
    }
}

void lisa_ble_notify_disconnected(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    if (g_ble_disc_cb) {
        g_ble_disc_cb(conidx, conhdl, reason);
    }
}

void lisa_ble_notify_bond(uint8_t conidx, uint8_t info, uint8_t value)
{
    if (g_ble_bond_cb) {
        g_ble_bond_cb(conidx, info, value);
    }
}

bool lisa_ble_notify_key_req(uint8_t conidx, uint8_t key_type, uint32_t passkey)
{
    if (g_ble_key_req_cb) {
        g_ble_key_req_cb(conidx, key_type, passkey);
        return true;
    }
    return false;
}

void lisa_ble_key_confirm(uint8_t conidx, uint8_t accept, uint32_t passkey)
{
    ble_gap_key_cfm(conidx, accept, passkey);
}

int lisa_bluetooth_init(lisa_bluetooth_enable_cmp_cb_t cb)
{
    g_enable_cmp_cb = cb;

    ls_rf_cali_proc();

    ls_crypto_init();

    extern os_task_cb_t *bt_stack_if_get_cb(void);
    bt_os_init((os_task_cb_t *)bt_stack_if_get_cb());

#if CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO
    aud_os_init(bt_audio_adapter_get_os_task_cb());
    // aud_pro_os_init(aud_pro_if_get_cb());

    btos_task_create(aud_os_task, AUD_OS_TASK_NAME, OS_TASK_ID_AUD,
                     CONFIG_LISA_BLUETOOTH_AUD_TASK_STACK_SIZE, NULL,
                     CONFIG_LISA_BLUETOOTH_AUD_TASK_PRIORITY, NULL);

    // btos_task_create(aud_pro_os_task, AUD_PRO_OS_TASK_NAME, OS_TASK_ID_AUD_PRO,
    //                  CONFIG_LISA_BLUETOOTH_AUD_PRO_TASK_STACK_SIZE, NULL,
    //                  CONFIG_LISA_BLUETOOTH_AUD_PRO_TASK_PRIORITY, NULL);
#endif

    btos_task_create(bt_os_task, BT_OS_TASK_NAME, OS_TASK_ID_BT, CONFIG_LISA_BLUETOOTH_TASK_STACK_SIZE, NULL,
                     CONFIG_LISA_BLUETOOTH_TASK_PRIORITY, NULL);

    return 0;
}
