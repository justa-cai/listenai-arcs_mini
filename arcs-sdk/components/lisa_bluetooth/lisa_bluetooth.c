#include "lisa_bluetooth.h"

#include <errno.h>
#include <bt_os_task.h>
#include <ble_drv.h>
#include <ble_gap.h>
#include <aud_os_task.h>
#include <os_task_init.h>
#include <rf_cali.h>
#include <string.h>

#include "bt_api.h"
#include "bt_app_hal.h"
#include "btos_al.h"
#include "lisa_ble_api.h"
#include "lisa_bt_classic_api.h"
#include "bt_stack_cfg.h"
#include "storage/bt_paired_storage.h"
#include "lisa_log.h"
#include "lisa_semaphore.h"
#include "sysutils.h"

#define PLATFORM_MEM_NOCACHE_MALLOC
#define TAG "LISA_BT"
#define LISA_BLUETOOTH_INIT_TIMEOUT_MS 5000
#define LISA_BLUETOOTH_CONNECT_CANCEL_TIMEOUT_MS 1000
#define LISA_BLUETOOTH_CLOSE_TIMEOUT_MS 3000
#define LISA_BLUETOOTH_PAGE_SCAN_ENABLE 2

#ifndef CONFIG_LISA_BLUETOOTH_CLASSIC
#define CONFIG_LISA_BLUETOOTH_CLASSIC 0
#endif

// BLE stack enable complete callback (set via lisa_bluetooth_init)
static lisa_bluetooth_enable_cmp_cb_t g_enable_cmp_cb = NULL;
static lisa_bluetooth_close_cmp_cb_t g_close_cmp_cb = NULL;

// Bluetooth device discovery callback
static lisa_bt_discovery_callback_t g_discovery_callback = NULL;
static lisa_bt_inquiry_stop_callback_t g_inquiry_stop_callback = NULL;

// BLE connection/disconnection callbacks
static lisa_ble_conn_cb_t g_ble_conn_cb = NULL;
static lisa_ble_disc_cb_t g_ble_disc_cb = NULL;
static lisa_ble_bond_cb_t g_ble_bond_cb = NULL;
static lisa_ble_key_req_cb_t g_ble_key_req_cb = NULL;

// Discovered device list
__psram_data__ static lisa_bt_discovery_info_t g_discovered_devices[MAX_DISCOVERED_DEVICES];
static uint8_t g_discovered_count = 0;
static gap_bdaddr_t g_pending_paired_name_addr;
static char g_pending_paired_name[BT_PAIRED_NAME_MAX_LEN + 1];
static bool g_pending_paired_name_valid = false;
static TimerHandle_t g_inquiry_timer = NULL;
static volatile bool g_inquiry_active = false;

typedef enum {
    LISA_BT_STATE_CLOSED = 0,
    LISA_BT_STATE_OPENING,
    LISA_BT_STATE_OPENED,
    LISA_BT_STATE_CLOSING,
} lisa_bluetooth_state_t;

typedef enum {
    LISA_BT_INIT_WAITING = 0,
    LISA_BT_INIT_TIMED_OUT,
    LISA_BT_INIT_COMPLETED,
} lisa_bluetooth_init_wait_state_t;

static bool g_bluetooth_opened = false;
static bool g_bluetooth_init_started = false;
static bool g_bluetooth_initialized = false;
static int g_bluetooth_init_result = -EINPROGRESS;
static int g_bluetooth_init_wait_state = LISA_BT_INIT_WAITING;
static lisa_semaphore_t *g_bluetooth_init_sem = NULL;
#if CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO
static bool g_bluetooth_aud_task_created = false;
#endif
static volatile lisa_bluetooth_state_t g_bluetooth_state = LISA_BT_STATE_CLOSED;
static volatile bool g_bluetooth_pending_open = false;
static TimerHandle_t g_close_timer = NULL;
static volatile bool g_classic_connecting = false;
static volatile bool g_close_wait_connect_cancel = false;

extern uint8_t lm_get_link_id(struct bd_addr *p_bd_addr);
extern uint8_t gapc_get_conidx(uint16_t conhdl);
#if CONFIG_LISA_BLUETOOTH_CLASSIC
extern void bt_gap_delete_bond(gap_bdaddr_t *bdaddr);
extern void bt_stack_bt_register_inquiry_stop_cb(void (*cb)(int16_t status));
extern void bt_stack_bt_register_connect_fail_cb(void (*cb)(uint8_t actv_id, int16_t status));
extern void bt_stack_bt_register_connect_actv_cb(void (*cb)(uint8_t actv, int16_t status));
extern void bt_stack_bt_register_link_auth_fail_cb(void (*cb)(uint8_t conidx, uint8_t reason));
#endif
extern void app_bt_register_close_cmp_cb(void (*cb)(uint8_t type, uint8_t status)) __attribute__((weak));

static void lisa_bluetooth_finish_close(uint8_t status);
static int lisa_bluetooth_request_stack_close(void);

static void lisa_bluetooth_finish_init(int status)
{
    int wait_state = LISA_BT_INIT_WAITING;

    g_bluetooth_initialized = status == 0;
    g_bluetooth_opened = status == 0;
    g_bluetooth_state = status == 0 ? LISA_BT_STATE_OPENED : LISA_BT_STATE_CLOSED;
    __atomic_store_n(&g_bluetooth_init_result, status, __ATOMIC_RELEASE);
    if (__atomic_compare_exchange_n(&g_bluetooth_init_wait_state, &wait_state,
                                    LISA_BT_INIT_COMPLETED, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        lisa_semaphore_give(g_bluetooth_init_sem);
    }
}

#if CONFIG_LISA_BLUETOOTH_CLASSIC
static bool lisa_bluetooth_stack_close_pending(void)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    return stack_env &&
           (stack_env->bt_open == BT_STATE_CLOSING_WAIT_DIS ||
            stack_env->bt_open == BT_STATE_CLOSING_WAIT_CTRL);
}

static void lisa_bluetooth_log_stack_close_pending(const char *reason)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if (!lisa_bluetooth_stack_close_pending()) {
        return;
    }

#if BT_STACK_PRESENT
    uint8_t classic_connected = stack_env->bt_classic_connected;
#else
    uint8_t classic_connected = 0;
#endif

    LISA_LOGE(TAG, "BT stack close pending timeout, reason:%s state:%d ble:%d classic:%d",
              reason, stack_env->bt_open, stack_env->bt_ble_connected,
              classic_connected);
}
#endif

static bool lisa_bluetooth_api_available(void)
{
    if (g_bluetooth_state != LISA_BT_STATE_OPENED || !g_bluetooth_opened) {
        LISA_LOGE(TAG, "Bluetooth is not opened, state:%d", (int)g_bluetooth_state);
        return false;
    }

    return true;
}

static void lisa_bluetooth_cancel_close_timer(void)
{
    if (g_close_timer) {
        xTimerStop(g_close_timer, 0);
        xTimerDelete(g_close_timer, 0);
        g_close_timer = NULL;
    }
}

static void lisa_bluetooth_close_timeout_cb(TimerHandle_t timer)
{
    if (g_close_timer == timer) {
        g_close_timer = NULL;
    }

    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
#if CONFIG_LISA_BLUETOOTH_CLASSIC
        if (g_close_wait_connect_cancel) {
            LISA_LOGE(TAG, "Bluetooth connect cancel timeout, close stack");
            g_classic_connecting = false;
            g_close_wait_connect_cancel = false;
            if (lisa_bluetooth_request_stack_close() == 0) {
                xTimerDelete(timer, 0);
                return;
            }
        }
#endif
        LISA_LOGE(TAG, "Bluetooth close timeout");
#if CONFIG_LISA_BLUETOOTH_CLASSIC
        lisa_bluetooth_log_stack_close_pending("close timeout");
#endif
        lisa_bluetooth_finish_close(1);
    }

    xTimerDelete(timer, 0);
}

static void lisa_bluetooth_start_close_timer(uint32_t timeout_ms)
{
    lisa_bluetooth_cancel_close_timer();

    g_close_timer = xTimerCreate("bt_close", pdMS_TO_TICKS(timeout_ms), pdFALSE,
                                 NULL, lisa_bluetooth_close_timeout_cb);
    if (!g_close_timer) {
        LISA_LOGE(TAG, "Failed to create bluetooth close timer");
        return;
    }

    if (xTimerStart(g_close_timer, 0) != pdPASS) {
        LISA_LOGE(TAG, "Failed to start bluetooth close timer");
        xTimerDelete(g_close_timer, 0);
        g_close_timer = NULL;
    }
}

static bool lisa_bluetooth_addr_invalid(const gap_bdaddr_t *addr)
{
    bool all_zero = true;
    bool all_ff = true;

    if (!addr || addr->addr_type > 1) {
        return true;
    }

    for (uint8_t i = 0; i < GAP_BD_ADDR_LEN; i++) {
        if (addr->addr[i] != 0x00) {
            all_zero = false;
        }
        if (addr->addr[i] != 0xFF) {
            all_ff = false;
        }
    }

    return all_zero || all_ff;
}

static void lisa_bluetooth_cancel_inquiry_timer(void)
{
    if (g_inquiry_timer) {
        xTimerStop(g_inquiry_timer, 0);
        xTimerDelete(g_inquiry_timer, 0);
        g_inquiry_timer = NULL;
    }
}

#if CONFIG_LISA_BLUETOOTH_CLASSIC
static void lisa_bluetooth_handle_inquiry_stopped(int16_t status)
{
    lisa_bluetooth_cancel_inquiry_timer();
    g_inquiry_active = false;

    LISA_LOGI(TAG, "BT inquiry stopped, status:%d", status);

    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        return;
    }

    if (g_inquiry_stop_callback) {
        g_inquiry_stop_callback();
    }
}
#endif

static void lisa_bluetooth_handle_close_cmp(uint8_t type, uint8_t status)
{
    (void)type;

    LISA_LOGI(TAG, "Bluetooth close complete, status:%d", status);

    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        lisa_bluetooth_finish_close(status);
    }
}

static void lisa_bluetooth_inquiry_timeout_cb(TimerHandle_t timer)
{
    if (g_inquiry_timer == timer) {
        g_inquiry_timer = NULL;
    }

    LISA_LOGI(TAG, "BT inquiry timeout, stopping inquiry");
    if (app_bt_inq_stop() != pdTRUE) {
        LISA_LOGE(TAG, "Failed to send BT inquiry stop event");
    }
    xTimerDelete(timer, 0);
}

static const lisa_bt_discovery_info_t *lisa_bluetooth_find_device_by_name(const char *name)
{
    size_t name_len = strlen(name);

    for (int i = 0; i < g_discovered_count; i++) {
        if (g_discovered_devices[i].name_len == name_len &&
            memcmp(g_discovered_devices[i].name, name, name_len) == 0) {
            return &g_discovered_devices[i];
        }
    }

    return NULL;
}

static const lisa_bt_discovery_info_t *lisa_bluetooth_find_device_by_addr(const gap_bdaddr_t *addr)
{
    if (!addr) {
        return NULL;
    }

    for (int i = 0; i < g_discovered_count; i++) {
        if (g_discovered_devices[i].addr.addr_type == addr->addr_type &&
            memcmp(g_discovered_devices[i].addr.addr, addr->addr, sizeof(addr->addr)) == 0) {
            return &g_discovered_devices[i];
        }
    }

    return NULL;
}

static bool lisa_bluetooth_addr_equal(const gap_bdaddr_t *left, const gap_bdaddr_t *right)
{
    if (!left || !right) {
        return false;
    }

    return left->addr_type == right->addr_type &&
           memcmp(left->addr, right->addr, sizeof(left->addr)) == 0;
}

int bt_paired_list_get(bt_paired_info_t *list, uint8_t max_count, uint8_t *out_count)
{
    return bt_paired_storage_load_list(list, max_count, out_count);
}

int bt_paired_name_get(const gap_bdaddr_t *addr, char *name, size_t name_len)
{
    return bt_paired_storage_find_name(addr, name, name_len);
}

int bt_paired_remove(const gap_bdaddr_t *addr)
{
    int ret;

    ble_gap_delete_bond((gap_bdaddr_t *)addr);
#if CONFIG_LISA_BLUETOOTH_CLASSIC
    bt_gap_delete_bond((gap_bdaddr_t *)addr);
#endif

    ret = addr == NULL ? bt_paired_storage_clear() : bt_paired_storage_remove(addr);
    return ret;
}

int lisa_bluetooth_set_pending_paired_name(const gap_bdaddr_t *addr, const char *name)
{
    size_t name_len;

    if (!addr || !name || name[0] == '\0') {
        return -1;
    }

    name_len = strlen(name);
    if (name_len > BT_PAIRED_NAME_MAX_LEN) {
        name_len = BT_PAIRED_NAME_MAX_LEN;
    }

    g_pending_paired_name_addr = *addr;
    memcpy(g_pending_paired_name, name, name_len);
    g_pending_paired_name[name_len] = '\0';
    g_pending_paired_name_valid = true;
    return 0;
}

void bt_paired_record_update(const gap_bdaddr_t *addr, uint8_t transport)
{
    const lisa_bt_discovery_info_t *discovered;
    bt_paired_info_t item = {0};
    char name[BT_PAIRED_NAME_MAX_LEN + 1];

    if (!addr) {
        return;
    }

    item.addr = *addr;
    item.transport = transport;

    discovered = lisa_bluetooth_find_device_by_addr(addr);
    if (discovered && discovered->name_len > 0) {
        item.name_len = discovered->name_len;
        if (item.name_len > sizeof(item.name)) {
            item.name_len = sizeof(item.name);
        }
        memcpy(item.name, discovered->name, item.name_len);
    } else if (g_pending_paired_name_valid &&
               lisa_bluetooth_addr_equal(&g_pending_paired_name_addr, addr) &&
               g_pending_paired_name[0] != '\0') {
        item.name_len = strlen(g_pending_paired_name);
        if (item.name_len > sizeof(item.name)) {
            item.name_len = sizeof(item.name);
        }
        memcpy(item.name, g_pending_paired_name, item.name_len);
    } else if (bt_paired_storage_find_name(addr, name, sizeof(name)) == 0) {
        item.name_len = strlen(name);
        if (item.name_len > sizeof(item.name)) {
            item.name_len = sizeof(item.name);
        }
        memcpy(item.name, name, item.name_len);
    }

    (void)bt_paired_storage_upsert(&item);
    if (g_pending_paired_name_valid &&
        lisa_bluetooth_addr_equal(&g_pending_paired_name_addr, addr)) {
        g_pending_paired_name_valid = false;
        memset(g_pending_paired_name, 0, sizeof(g_pending_paired_name));
    }
}

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

    if (!g_inquiry_active || g_bluetooth_state != LISA_BT_STATE_OPENED) {
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
    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

    lisa_bluetooth_cancel_inquiry_timer();

    // 开始新的扫描前清空旧的设备列表
    lisa_bluetooth_clear_discovered_devices();
    
    if (app_bt_inq_start(mode, max_count) != pdTRUE) {
        return -1;
    }

    g_inquiry_active = true;
    return 0;
}

int lisa_bluetooth_inquiry_start_timed(gapm_disc_type_e mode, uint32_t timeout_ms)
{
    int ret;

    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

    if (timeout_ms == 0) {
        LISA_LOGE(TAG, "Invalid parameter: timeout_ms is 0");
        return -2;
    }

    lisa_bluetooth_cancel_inquiry_timer();
    g_inquiry_active = false;

    // 开始新的扫描前清空旧的设备列表
    lisa_bluetooth_clear_discovered_devices();

    ret = app_bt_inq_start(mode, MAX_DISCOVERED_DEVICES);
    if (ret != pdTRUE) {
        return -1;
    }

    g_inquiry_active = true;

    g_inquiry_timer = xTimerCreate("bt_inq", pdMS_TO_TICKS(timeout_ms), pdFALSE,
                                   NULL, lisa_bluetooth_inquiry_timeout_cb);
    if (!g_inquiry_timer) {
        g_inquiry_active = false;
        (void)app_bt_inq_stop();
        return -1;
    }

    if (xTimerStart(g_inquiry_timer, 0) != pdPASS) {
        xTimerDelete(g_inquiry_timer, 0);
        g_inquiry_timer = NULL;
        g_inquiry_active = false;
        (void)app_bt_inq_stop();
        return -1;
    }

    return 0;
}

int lisa_bluetooth_inquiry_stop(void)
{
    int ret;

    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        lisa_bluetooth_cancel_inquiry_timer();
        g_inquiry_active = false;
        return 0;
    }

    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

    ret = app_bt_inq_stop();
    if (ret != pdTRUE) {
        return -1;
    }

    /* The stack may not report a stop indication if inquiry is already stopping.
     * Clear the public active flag now so later close does not wait on stale state.
     */
    lisa_bluetooth_cancel_inquiry_timer();
    g_inquiry_active = false;
    return 0;
}

int lisa_bluetooth_connect_by_name(const char *name)
{
    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

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
                lisa_bluetooth_cancel_inquiry_timer();
                g_inquiry_active = false;
                app_bt_inq_stop();
                if (app_bt_conn(g_discovered_devices[i].addr, 2, g_discovered_devices[i].clk_off, 0)) {
                    g_classic_connecting = true;
                }
                return 0;
            }
        }
    }
    
    LISA_LOGE(TAG, "Device not found in list");
    return -1;  // 未找到设备
}

int lisa_bluetooth_connect_by_index(uint8_t index)
{
    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

    if (index >= g_discovered_count) {
        LISA_LOGE(TAG, "Invalid index: %d", index);
        return -2;  // 无效参数
    }
    
    LISA_LOGI(TAG, "Connecting to device at index: %d", index);
    LISA_LOGI(TAG, "Device name: %.*s", g_discovered_devices[index].name_len, g_discovered_devices[index].name);
    LISA_LOGI(TAG, "Device addr: %02X:%02X:%02X:%02X:%02X:%02X", 
            g_discovered_devices[index].addr.addr[0], g_discovered_devices[index].addr.addr[1],
            g_discovered_devices[index].addr.addr[2], g_discovered_devices[index].addr.addr[3],
            g_discovered_devices[index].addr.addr[4], g_discovered_devices[index].addr.addr[5]);
    lisa_bluetooth_cancel_inquiry_timer();
    g_inquiry_active = false;
    app_bt_inq_stop();
    if (app_bt_conn(g_discovered_devices[index].addr, 2, g_discovered_devices[index].clk_off, 0)) {
        g_classic_connecting = true;
    }
    return 0;
}

int lisa_bluetooth_connect_by_addr(const gap_bdaddr_t *addr)
{
    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

    if (!addr) {
        LISA_LOGE(TAG, "Invalid parameter: addr is NULL");
        return -2;
    }

    LISA_LOGI(TAG, "Connecting to device addr: %02X:%02X:%02X:%02X:%02X:%02X",
              addr->addr[0], addr->addr[1], addr->addr[2],
              addr->addr[3], addr->addr[4], addr->addr[5]);

    lisa_bluetooth_cancel_inquiry_timer();
    g_inquiry_active = false;
    (void)app_bt_inq_stop();

    if (!app_bt_conn(*addr, 2, 0, 1)) {
        LISA_LOGE(TAG, "Failed to send BT connect event");
        return -1;
    }

    g_classic_connecting = true;
    return 0;
}

static int lisa_bluetooth_disconnect_by_addr_internal(const gap_bdaddr_t *addr)
{
    struct bd_addr remote_addr = {0};
    uint8_t link_id;
    uint8_t conidx;

    if (!addr) {
        LISA_LOGE(TAG, "Invalid parameter: addr is NULL");
        return -2;
    }

    memcpy(remote_addr.addr, addr->addr, sizeof(remote_addr.addr));
    link_id = lm_get_link_id(&remote_addr);
    if (link_id == 0xFF) {
        LISA_LOGE(TAG, "Device is not connected");
        return -1;
    }

    conidx = gapc_get_conidx(link_id + 0x80);
    ble_gap_disconnect(conidx, 0x13);
    return 0;
}

int lisa_bluetooth_disconnect_by_addr(const gap_bdaddr_t *addr)
{
    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

    return lisa_bluetooth_disconnect_by_addr_internal(addr);
}

int lisa_bluetooth_disconnect_by_name(const char *name)
{
    const lisa_bt_discovery_info_t *device;

    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

    if (!name || strlen(name) == 0) {
        LISA_LOGE(TAG, "Invalid parameter: name is NULL or empty");
        return -2;
    }

    device = lisa_bluetooth_find_device_by_name(name);
    if (!device) {
        LISA_LOGE(TAG, "Device not found in list");
        return -1;
    }

    return lisa_bluetooth_disconnect_by_addr(&device->addr);
}

int lisa_bluetooth_disconnect_by_index(uint8_t index)
{
    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

    if (index >= g_discovered_count) {
        LISA_LOGE(TAG, "Invalid index: %d", index);
        return -2;
    }

    return lisa_bluetooth_disconnect_by_addr(&g_discovered_devices[index].addr);
}

int lisa_bluetooth_get_discovered_devices(const lisa_bt_discovery_info_t **list, uint8_t *count)
{
    if (!lisa_bluetooth_api_available()) {
        return -1;
    }

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

void lisa_bluetooth_register_inquiry_stop_callback(lisa_bt_inquiry_stop_callback_t callback)
{
    g_inquiry_stop_callback = callback;
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
    lisa_bluetooth_enable_cmp_cb_t cb = __atomic_load_n(&g_enable_cmp_cb, __ATOMIC_ACQUIRE);

    if (cb) {
        cb(status);
    }

#if !CONFIG_LISA_BLUETOOTH_CLASSIC
    if (g_bluetooth_state == LISA_BT_STATE_OPENING) {
        lisa_bluetooth_finish_init(status);
    }
#endif
}

void lisa_ble_notify_connected(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr)
{
    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        return;
    }

    if (g_bluetooth_state != LISA_BT_STATE_OPENED) {
        return;
    }

    if (g_ble_conn_cb) {
        g_ble_conn_cb(conidx, conhdl, peer_addr);
    }
}

void lisa_ble_notify_disconnected(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        return;
    }

    if (g_bluetooth_state != LISA_BT_STATE_OPENED) {
        return;
    }

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
    if (!lisa_bluetooth_api_available()) {
        return;
    }

    ble_gap_key_cfm(conidx, accept, passkey);
}

/* ======== BT Classic callbacks ======== */

static lisa_bt_classic_conn_cb_t s_bt_classic_conn_cb = NULL;
static lisa_bt_classic_disc_cb_t s_bt_classic_disc_cb = NULL;
static lisa_bt_classic_conn_fail_cb_t s_bt_classic_conn_fail_cb = NULL;
static lisa_bt_classic_link_auth_fail_cb_t s_bt_classic_link_auth_fail_cb = NULL;
static lisa_bt_classic_avrcp_cb_t s_bt_classic_avrcp_cb = NULL;
static lisa_bt_classic_avrcp_notify_cb_t s_bt_classic_avrcp_notify_cb = NULL;

void lisa_bt_classic_register_conn_cb(lisa_bt_classic_conn_cb_t cb)
{
    s_bt_classic_conn_cb = cb;
}

void lisa_bt_classic_register_disc_cb(lisa_bt_classic_disc_cb_t cb)
{
    s_bt_classic_disc_cb = cb;
}

void lisa_bt_classic_register_conn_fail_cb(lisa_bt_classic_conn_fail_cb_t cb)
{
    s_bt_classic_conn_fail_cb = cb;
}

static void lisa_bluetooth_handle_connect_failed(uint8_t actv_id, int16_t status)
{
    g_classic_connecting = false;
    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        if (g_close_wait_connect_cancel) {
            g_close_wait_connect_cancel = false;
            (void)lisa_bluetooth_request_stack_close();
        }
        return;
    }

    if (s_bt_classic_conn_fail_cb) {
        s_bt_classic_conn_fail_cb(actv_id, status);
    }
}

static void lisa_bluetooth_handle_connect_actv(uint8_t actv, int16_t status)
{
    (void)status;

    if (actv != 0) {
        return;
    }

    g_classic_connecting = false;
    if (g_bluetooth_state == LISA_BT_STATE_CLOSING && g_close_wait_connect_cancel) {
        g_close_wait_connect_cancel = false;
        (void)lisa_bluetooth_request_stack_close();
    }
}

void lisa_bt_classic_register_link_auth_fail_cb(lisa_bt_classic_link_auth_fail_cb_t cb)
{
    s_bt_classic_link_auth_fail_cb = cb;
}

static void lisa_bluetooth_handle_link_auth_failed(uint8_t conidx, uint8_t reason)
{
    if (s_bt_classic_link_auth_fail_cb) {
        s_bt_classic_link_auth_fail_cb(conidx, reason);
    }
}

void lisa_bt_classic_register_avrcp_cb(lisa_bt_classic_avrcp_cb_t cb)
{
    s_bt_classic_avrcp_cb = cb;
}

void lisa_bt_classic_register_avrcp_notify_cb(lisa_bt_classic_avrcp_notify_cb_t cb)
{
    s_bt_classic_avrcp_notify_cb = cb;
}

void lisa_bt_classic_notify_connected(uint8_t conidx, uint16_t conhdl, const gap_bdaddr_t *peer_addr)
{
    g_classic_connecting = false;

    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        return;
    }

    if (g_bluetooth_state != LISA_BT_STATE_OPENED) {
        return;
    }

    if (s_bt_classic_conn_cb) {
        s_bt_classic_conn_cb(conidx, conhdl, peer_addr);
    }
}

void lisa_bt_classic_notify_disconnected(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    g_classic_connecting = false;

    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        return;
    }

    if (g_bluetooth_state != LISA_BT_STATE_OPENED) {
        return;
    }

    if (s_bt_classic_disc_cb) {
        s_bt_classic_disc_cb(conidx, conhdl, reason);
    }
}

void lisa_bt_classic_notify_avrcp_key(uint8_t conidx, uint8_t key_id)
{
    if (g_bluetooth_state != LISA_BT_STATE_OPENED) {
        return;
    }

    if (s_bt_classic_avrcp_cb) {
        s_bt_classic_avrcp_cb(conidx, key_id);
    }
}

void lisa_bt_classic_notify_avrcp_event(uint8_t conidx, uint8_t c_r, uint8_t event_id, uint8_t event_value)
{
    if (g_bluetooth_state != LISA_BT_STATE_OPENED) {
        return;
    }

    if (s_bt_classic_avrcp_notify_cb) {
        s_bt_classic_avrcp_notify_cb(conidx, c_r, event_id, event_value);
    }
}

static lisa_bt_classic_profile_cb_t s_bt_classic_profile_cb = NULL;

void lisa_bt_classic_register_profile_cb(lisa_bt_classic_profile_cb_t cb)
{
    s_bt_classic_profile_cb = cb;
}

void lisa_bt_classic_notify_profile(uint8_t conidx, int profile, bool connected)
{
    if (g_bluetooth_state != LISA_BT_STATE_OPENED) {
        return;
    }

    if (s_bt_classic_profile_cb) {
        s_bt_classic_profile_cb(conidx, profile, connected);
    }
}

int lisa_bluetooth_set_local_addr(const gap_bdaddr_t *addr)
{
    if (lisa_bluetooth_addr_invalid(addr)) {
        LISA_LOGE(TAG, "Invalid bluetooth address");
        return -2;
    }

    if (g_bluetooth_init_started) {
        LISA_LOGE(TAG, "Bluetooth stack init already started");
        return -3;
    }

    if (bt_stack_cfg_set_local_addr(addr) != 0) {
        return -1;
    }

    return 0;
}

int lisa_bluetooth_init(lisa_bluetooth_enable_cmp_cb_t cb)
{
    int ret;
    bool init_started = false;
#if CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO
    bool aud_task_created = false;
#endif

    __atomic_store_n(&g_enable_cmp_cb, cb, __ATOMIC_RELEASE);
    if (!__atomic_compare_exchange_n(&g_bluetooth_init_started, &init_started, true, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        int result = __atomic_load_n(&g_bluetooth_init_result, __ATOMIC_ACQUIRE);
        return result == -EINPROGRESS ? -EINPROGRESS : (result == 0 ? 0 : -1);
    }

    lisa_semaphore_t *init_sem = lisa_semaphore_create(1);
    if (!init_sem) {
        __atomic_store_n(&g_bluetooth_init_started, false, __ATOMIC_RELEASE);
        return -ENOMEM;
    }

    g_bluetooth_init_sem = init_sem;
    g_bluetooth_state = LISA_BT_STATE_OPENING;
    __atomic_store_n(&g_bluetooth_init_result, -EINPROGRESS, __ATOMIC_RELEASE);
    __atomic_store_n(&g_bluetooth_init_wait_state, LISA_BT_INIT_WAITING, __ATOMIC_RELEASE);
    g_bluetooth_opened = false;
#if CONFIG_LISA_BLUETOOTH_CLASSIC
    bt_stack_bt_register_inquiry_stop_cb(lisa_bluetooth_handle_inquiry_stopped);
    bt_stack_bt_register_connect_fail_cb(lisa_bluetooth_handle_connect_failed);
    bt_stack_bt_register_connect_actv_cb(lisa_bluetooth_handle_connect_actv);
    bt_stack_bt_register_link_auth_fail_cb(lisa_bluetooth_handle_link_auth_failed);
#endif
    if (app_bt_register_close_cmp_cb) {
        app_bt_register_close_cmp_cb(lisa_bluetooth_handle_close_cmp);
    }

    ls_rf_cali_proc();

    ls_crypto_init();

    extern os_task_cb_t *bt_stack_if_get_cb(void);
    bt_os_init((os_task_cb_t *)bt_stack_if_get_cb());

#if CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO
    aud_os_init(bt_audio_adapter_get_os_task_cb());
    // aud_pro_os_init(aud_pro_if_get_cb());

    if (!g_bluetooth_aud_task_created) {
        ret = btos_task_create(aud_os_task, AUD_OS_TASK_NAME, OS_TASK_ID_AUD,
                               CONFIG_LISA_BLUETOOTH_AUD_TASK_STACK_SIZE, NULL,
                               CONFIG_LISA_BLUETOOTH_AUD_TASK_PRIORITY, NULL);
        if (ret != pdTRUE) {
            LISA_LOGE(TAG, "Failed to create Bluetooth audio task:%d", ret);
            goto task_create_failed;
        }
        g_bluetooth_aud_task_created = true;
        aud_task_created = true;
    }

    // btos_task_create(aud_pro_os_task, AUD_PRO_OS_TASK_NAME, OS_TASK_ID_AUD_PRO,
    //                  CONFIG_LISA_BLUETOOTH_AUD_PRO_TASK_STACK_SIZE, NULL,
    //                  CONFIG_LISA_BLUETOOTH_AUD_PRO_TASK_PRIORITY, NULL);
#endif

    ret = btos_task_create(bt_os_task, BT_OS_TASK_NAME, OS_TASK_ID_BT,
                           CONFIG_LISA_BLUETOOTH_TASK_STACK_SIZE, NULL,
                           CONFIG_LISA_BLUETOOTH_TASK_PRIORITY, NULL);
    if (ret != pdTRUE) {
        LISA_LOGE(TAG, "Failed to create Bluetooth task:%d", ret);
task_create_failed:
#if CONFIG_LISA_BLUETOOTH_CLASSIC_AUDIO
        if (aud_task_created) {
            btos_task_delete(OS_TASK_ID_AUD);
            g_bluetooth_aud_task_created = false;
        }
#endif
        g_bluetooth_init_sem = NULL;
        __atomic_store_n(&g_bluetooth_init_result, -EINPROGRESS, __ATOMIC_RELEASE);
        g_bluetooth_initialized = false;
        g_bluetooth_opened = false;
        g_bluetooth_state = LISA_BT_STATE_CLOSED;
        lisa_semaphore_delete(init_sem);
        __atomic_store_n(&g_bluetooth_init_started, false, __ATOMIC_RELEASE);
        return -1;
    }

    if (lisa_semaphore_take(g_bluetooth_init_sem, LISA_BLUETOOTH_INIT_TIMEOUT_MS) != LISA_OK) {
        int result = -EINPROGRESS;
        int wait_state = LISA_BT_INIT_WAITING;

        if (!__atomic_compare_exchange_n(&g_bluetooth_init_result, &result, -ETIMEDOUT, false,
                                         __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            lisa_semaphore_take(g_bluetooth_init_sem, LISA_OS_WAIT_FOREVER);
            g_bluetooth_init_sem = NULL;
            lisa_semaphore_delete(init_sem);
            return result == 0 ? 0 : -1;
        }
        if (!__atomic_compare_exchange_n(&g_bluetooth_init_wait_state, &wait_state,
                                         LISA_BT_INIT_TIMED_OUT, false,
                                         __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            lisa_semaphore_take(g_bluetooth_init_sem, LISA_OS_WAIT_FOREVER);
            g_bluetooth_init_sem = NULL;
            lisa_semaphore_delete(init_sem);
            result = __atomic_load_n(&g_bluetooth_init_result, __ATOMIC_ACQUIRE);
            return result == 0 ? 0 : -1;
        }
        g_bluetooth_init_sem = NULL;
        lisa_semaphore_delete(init_sem);
        LISA_LOGE(TAG, "Bluetooth stack init timeout");
        return -ETIMEDOUT;
    }

    if (!g_bluetooth_initialized) {
        g_bluetooth_init_sem = NULL;
        lisa_semaphore_delete(init_sem);
        return -1;
    }

    g_bluetooth_init_sem = NULL;
    lisa_semaphore_delete(init_sem);
    return 0;
}

void lisa_bluetooth_notify_classic_enabled(uint16_t status)
{
    int ret;

    if (g_bluetooth_state != LISA_BT_STATE_OPENING) {
        return;
    }

    if (status != 0) {
        LISA_LOGE(TAG, "Bluetooth classic enable failed:%d", status);
        if (!g_bluetooth_initialized) {
            lisa_bluetooth_finish_init(status);
        } else {
            g_bluetooth_opened = false;
            g_bluetooth_state = LISA_BT_STATE_CLOSED;
        }
        return;
    }

    ret = lisa_bt_scan(LISA_BLUETOOTH_PAGE_SCAN_ENABLE);
    if (ret != 0) {
        LISA_LOGE(TAG, "Bluetooth open page scan failed:%d", ret);
        if (!g_bluetooth_initialized) {
            lisa_bluetooth_finish_init(ret);
        } else {
            g_bluetooth_opened = false;
            g_bluetooth_state = LISA_BT_STATE_CLOSED;
        }
        return;
    }

    if (!g_bluetooth_initialized) {
        lisa_bluetooth_finish_init(0);
    } else {
        g_bluetooth_opened = true;
        g_bluetooth_state = LISA_BT_STATE_OPENED;
    }
}

int lisa_bluetooth_open(void)
{
    int ret;
    bool was_opened;

    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        g_bluetooth_pending_open = true;
        LISA_LOGI(TAG, "Bluetooth is closing, defer open");
        return 0;
    }

    if (!g_bluetooth_initialized) {
        LISA_LOGE(TAG, "Bluetooth stack is not initialized");
        return -1;
    }

#if CONFIG_LISA_BLUETOOTH_CLASSIC
    if (lisa_bluetooth_stack_close_pending()) {
        g_bluetooth_pending_open = true;
        g_bluetooth_state = LISA_BT_STATE_CLOSING;
        g_bluetooth_opened = false;
        LISA_LOGI(TAG, "Bluetooth stack is closing, defer open");
        lisa_bluetooth_start_close_timer(LISA_BLUETOOTH_CLOSE_TIMEOUT_MS);
        return 0;
    }
#endif

    was_opened = lisa_bluetooth_is_opened();
    if (was_opened) {
        return 0;
    }

    g_bluetooth_state = LISA_BT_STATE_OPENING;
    g_bluetooth_opened = false;

#if CONFIG_LISA_BLUETOOTH_CLASSIC
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    if (stack_env->bt_open == BT_STATE_OPENED) {
        lisa_bluetooth_notify_classic_enabled(0);
        return lisa_bluetooth_is_opened() ? 0 : -1;
    }
#endif

    if (app_bt_open(0) != pdTRUE) {
        LISA_LOGE(TAG, "Failed to send Bluetooth open event");
        g_bluetooth_state = LISA_BT_STATE_CLOSED;
        return -1;
    }

    return 0;
}

bool lisa_bluetooth_is_opened(void)
{
    return g_bluetooth_state == LISA_BT_STATE_OPENED && g_bluetooth_opened;
}

static void lisa_bluetooth_finish_close(uint8_t status)
{
    lisa_bluetooth_cancel_close_timer();
    lisa_bluetooth_cancel_inquiry_timer();
    g_inquiry_active = false;
    lisa_bluetooth_clear_discovered_devices();
    g_classic_connecting = false;
    g_close_wait_connect_cancel = false;
    g_bluetooth_opened = false;
    g_bluetooth_state = LISA_BT_STATE_CLOSED;

    if (g_close_cmp_cb) {
        g_close_cmp_cb(status);
    }

    if (g_bluetooth_pending_open) {
        g_bluetooth_pending_open = false;
        (void)lisa_bluetooth_open();
    }
}

static int lisa_bluetooth_request_stack_close(void)
{
    if (app_bt_close(0) != pdTRUE) {
        LISA_LOGE(TAG, "Failed to send Bluetooth close event");
        return -1;
    }

    lisa_bluetooth_start_close_timer(LISA_BLUETOOTH_CLOSE_TIMEOUT_MS);
    return 0;
}

int lisa_bluetooth_close(void)
{
    int ret = 0;

    if (g_bluetooth_state == LISA_BT_STATE_CLOSING) {
        g_bluetooth_pending_open = false;
        return 0;
    }

    if (g_bluetooth_state == LISA_BT_STATE_CLOSED || !g_bluetooth_opened) {
        LISA_LOGE(TAG, "Bluetooth is already closed");
        return -1;
    }

    lisa_bluetooth_cancel_inquiry_timer();
    g_inquiry_active = false;
    g_bluetooth_state = LISA_BT_STATE_CLOSING;
    g_bluetooth_opened = false;

    if (g_classic_connecting) {
        LISA_LOGI(TAG, "Bluetooth close waits for connect cancel");
        g_close_wait_connect_cancel = true;
        if (app_bt_conn_cancel() != pdTRUE) {
            LISA_LOGE(TAG, "Failed to send BT connect cancel event");
            g_close_wait_connect_cancel = false;
        } else {
            lisa_bluetooth_start_close_timer(LISA_BLUETOOTH_CONNECT_CANCEL_TIMEOUT_MS);
            return 0;
        }
    }

    ret = lisa_bluetooth_request_stack_close();
    if (ret != 0) {
        g_bluetooth_state = LISA_BT_STATE_OPENED;
        g_bluetooth_opened = true;
    }
    return ret;
}

void lisa_bluetooth_register_close_cmp_cb(lisa_bluetooth_close_cmp_cb_t cb)
{
    g_close_cmp_cb = cb;
}
