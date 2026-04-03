/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file custom_service.c
 * @brief 自定义 BLE 服务实现
 *
 * 本文件实现了一个示例 BLE 服务，包含多种类型的特征值：
 * 1. 只读特征值
 * 2. 只写特征值
 * 3. 通知特征值
 * 4. 读写特征值
 * 5. 安全读取特征值（需要配对/认证）
 */

#include "custom_service.h"
#include "ble_gatt.h"
#include "hogpd_msg.h"
#include "hogpd.h"
#include <string.h>

#define LOG_TAG "custom_svc"
#include "lisa_log.h"

/* 自定义服务 UUID 定义 */
#define CUSTOM_SVC_UUID              0x1234
#define CUSTOM_CHAR_READ_UUID        0x1235
#define CUSTOM_CHAR_WRITE_UUID       0x1236
#define CUSTOM_CHAR_NOTIFY_UUID      0x1237
#define CUSTOM_CHAR_READ_WRITE_UUID  0x1238
#define CUSTOM_CHAR_SEC_READ_UUID    0x1239

// HID Report Map（完整版本，包含所有报告）
static const uint8_t hid_report_map[] = {
    // Keyboard
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)

    0x85, HIDS_KB_REPORT_ID,       //   REPORT_ID (Keyboard)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x4b,                    //   USAGE_MINIMUM (Keyboard PageUp)
    0x29, 0x52,                    //   USAGE_MAXIMUM (Keyboard UpArrow)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)

    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)

    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs)

    0x95, 0x6,                     //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0xff,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0xff,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0,                          //   END_COLLECTION

    // Mouse
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)

    0x85, HIDS_MOUSE_REPORT_ID,    //   REPORT_ID (Mouse)
    0x09, 0x01,                    //   USAGE_PAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (PHYSICAL)
    0x05, 0x09,                    //   USAGE_PAGE (BUTTON)
    0x19, 0x01,                    //   USAGE_MINIMUM (1)
    0x29, 0x05,                    //   USAGE_MAXIMUM (5)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x81, 0x01,                    //   INPUT (CONSTANT); 3 bit padding
    0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //   USAGE (X)
    0x09, 0x31,                    //   USAGE (Y)
    0x09, 0x38,                    //   USAGE (Wheel)
    0x15, 0x81,                    //   LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //   LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x03,                    //   REPORT_SIZE (3)
    0x81, 0x06,                    //   INPUT (Data,Var,Rel); 3 position bytes(X,Y,Wheel)
    0xc0,                          // END_COLLECTION
    0xc0,                          // END_COLLECTION

    // Media Consumer Control
    0x05, 0x0C,                    // USAGE_PAGE (Consumer Devices)
    0x09, 0x01,                    // USAGE (Consumer Control)
    0xA1, 0x01,                    // COLLECTION (Application)
    0x85, HIDS_MEDIA_REPORT_ID,    // REPORT_ID (3)
    0x19, 0x00,                    // USAGE_MINIMUM (0x00)
    0x2A, 0x9C, 0x02,              // USAGE_MAXIMUM (0x029C)
    0x15, 0x00,                    // LOGICAL_MINIMUM (0x00)
    0x26, 0x9C, 0x02,              // LOGICAL_MAXIMUM (0x029C)
    0x95, 0x01,                    // REPORT_COUNT (1)
    0x75, 0x10,                    // REPORT_SIZE (0x10)
    0x81, 0x00,                    // INPUT (Data,Ary,Abs)
    0xC0,                          // END_COLLECTION

    // Voice data report
    0x05, 0x0C,                    // Usage Page (Consumer Devices)
    0x09, 0x01,                    // Usage (Consumer Control)
    0xA1, 0x01,                    // Collection (Application)
    0x85, HIDS_VOICE_DATA_IN_REPORT_ID,  // Report ID=0xFC
    0x95, 0xff,                    // REPORT_COUNT (255)
    0x75, 0x08,                    // REPORT_SIZE (8)
    0x15, 0x00,                    // LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00,              // LOGICAL_MAXIMUM (255)
    0x81, 0x00,                    // INPUT (Data,Ary,Abs)
    0xC0,                          // END_COLLECTION

    // GDE ACK in
    0x05, 0x0C,                    // Usage Page (Consumer Devices)
    0x09, 0x01,                    // Usage (Consumer Control)
    0xA1, 0x01,                    // Collection (Application)
    0x85, HIDS_GDE_ACK_IN_REPORT_ID,   // Report ID=0xF8
    0x95, 0xff,                    // REPORT_COUNT (255)
    0x75, 0x08,                    // REPORT_SIZE (8)
    0x15, 0x00,                    // LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00,              // LOGICAL_MAXIMUM (255)
    0x81, 0x00,                    // INPUT (Data,Ary,Abs)
    0xC0,                          // END_COLLECTION

    // GDE Feedback
    0x05, 0x0C,                    // Usage Page (Consumer Devices)
    0x09, 0x01,                    // Usage (Consumer Control)
    0xA1, 0x01,                    // Collection (Application)
    0x85, HIDS_GDE_FEEDBACK_IN_REPORT_ID,  // Report ID=0xF9
    0x95, 0xff,                    // REPORT_COUNT (255)
    0x75, 0x08,                    // REPORT_SIZE (8)
    0x15, 0x00,                    // LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00,              // LOGICAL_MAXIMUM (255)
    0x81, 0x00,                    // INPUT (Data,Ary,Abs)
    0xC0,                          // END_COLLECTION
};

// HOGPD 回调函数
static void ble_hid_read_cmp(uint32_t token, uint8_t val_id)
{
    LISA_LOGI(LOG_TAG, "HID read complete, val_id=%d", val_id);
}

static void ble_hid_send_cmp(uint32_t token, uint8_t val_id)
{
    LISA_LOGI(LOG_TAG, "HID send complete, val_id=%d", val_id);
}

static void ble_hid_rcv(uint8_t conidx, uint16_t index, uint16_t length,
                        uint16_t offset, uint8_t *data)
{
    LISA_LOGI(LOG_TAG, "HID received: idx=%d, len=%d", index, length);
}

// HOGPD 回调结构定义
const hogpd_cb_t ble_hogpd_cb = {
    .cb_read_cmp = ble_hid_read_cmp,
    .cb_notify_cmp = ble_hid_send_cmp,
    .cb_write_cmp = NULL,
    .cb_read_ind = NULL,
    .cb_notify_ind = NULL,
    .cb_write_ind = ble_hid_rcv,
};

/**
 * @brief 属性数据库索引枚举
 */
enum {
    IDX_SVC,                        /**< 服务声明 */
    
    IDX_CHAR_READ_DECL,             /**< 只读特征值声明 */
    IDX_CHAR_READ_VAL,              /**< 只读特征值数值 */
    
    IDX_CHAR_WRITE_DECL,            /**< 只写特征值声明 */
    IDX_CHAR_WRITE_VAL,             /**< 只写特征值数值 */
    
    IDX_CHAR_NOTIFY_DECL,           /**< 通知特征值声明 */
    IDX_CHAR_NOTIFY_VAL,            /**< 通知特征值数值 */
    IDX_CHAR_NOTIFY_CFG,            /**< 通知特征值 CCCD (Client Characteristic Configuration Descriptor) */
    
    IDX_CHAR_READ_WRITE_DECL,       /**< 读写特征值声明 */
    IDX_CHAR_READ_WRITE_VAL,        /**< 读写特征值数值 */
    
    IDX_CHAR_SEC_READ_DECL,         /**< 安全读特征值声明 */
    IDX_CHAR_SEC_READ_VAL,          /**< 安全读特征值数值 */
    
    IDX_NB,                         /**< 属性总数 */
};

/* GATT 用户回调 ID，注册时由协议栈分配 */
static uint8_t custom_user_lid = 0;
/* 服务起始句柄，添加服务时由协议栈分配 */
static uint16_t custom_start_hdl = 0;

/**
 * @brief 属性数据库定义
 * 
 * 定义服务和特征值的属性，包括 UUID、权限和最大长度。
 */
static const ble_gatt_att16_desc_t custom_att_db[IDX_NB] = {
    // Service Declaration
    [IDX_SVC] = {BLE_GATT_DECL_PRIMARY_SERVICE, BLE_PROP(RD), 0},

    // Char 1: Read (只读)
    [IDX_CHAR_READ_DECL] = {BLE_GATT_DECL_CHARACTERISTIC, BLE_PROP(RD), 0},
    [IDX_CHAR_READ_VAL] = {CUSTOM_CHAR_READ_UUID, BLE_PROP(RD), 20},

    // Char 2: Write (只写，支持 Write Request 和 Write Command)
    [IDX_CHAR_WRITE_DECL] = {BLE_GATT_DECL_CHARACTERISTIC, BLE_PROP(RD), 0},
    [IDX_CHAR_WRITE_VAL] = {CUSTOM_CHAR_WRITE_UUID, BLE_PROP(WC) | BLE_PROP(WR), 20},

    // Char 3: Notify (通知)
    [IDX_CHAR_NOTIFY_DECL] = {BLE_GATT_DECL_CHARACTERISTIC, BLE_PROP(RD), 0},
    [IDX_CHAR_NOTIFY_VAL] = {CUSTOM_CHAR_NOTIFY_UUID, BLE_PROP(N), 20},
    [IDX_CHAR_NOTIFY_CFG] = {BLE_GATT_DESC_CLIENT_CHAR_CFG, BLE_PROP(RD) | BLE_PROP(WR), 0},

    // Char 4: Read/Write (可读可写)
    [IDX_CHAR_READ_WRITE_DECL] = {BLE_GATT_DECL_CHARACTERISTIC, BLE_PROP(RD), 0},
    [IDX_CHAR_READ_WRITE_VAL] = {CUSTOM_CHAR_READ_WRITE_UUID, BLE_PROP(RD) | BLE_PROP(WC) | BLE_PROP(WR), 20},

    // Char 5: Secure Read (安全读取，需要认证)
    [IDX_CHAR_SEC_READ_DECL] = {BLE_GATT_DECL_CHARACTERISTIC, BLE_PROP(RD), 0},
    [IDX_CHAR_SEC_READ_VAL] = {CUSTOM_CHAR_SEC_READ_UUID, BLE_PROP(RD) | BLE_SEC_LVL(RP, AUTH), 20},
};

/**
 * @brief 处理 GATT 读取请求的回调函数
 * 
 * @param conidx 连接索引
 * @param user_lid GATT 用户回调 ID
 * @param token 请求令牌
 * @param hdl 属性句柄
 * @param offset 读取偏移量
 * @param max_length 最大读取长度
 */
static void cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, uint16_t max_length)
{
    uint16_t idx = hdl - custom_start_hdl;
    uint8_t data[] = "Hello LISA"; // 示例数据
    uint16_t len = sizeof(data);
    
    LISA_LOGI(LOG_TAG, "Read request: idx=%d, offset=%d", idx, offset);

    // 根据索引判断读取哪个特征值
    if (idx == IDX_CHAR_READ_VAL || idx == IDX_CHAR_READ_WRITE_VAL || idx == IDX_CHAR_SEC_READ_VAL) {
        // 发送读取确认，返回数据
        ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, 0, len, len, data);
    } else if (idx == IDX_CHAR_NOTIFY_CFG) {
        // 读取 CCCD 配置 (通常由协议栈维护，这里仅作示例)
        uint16_t ccc_val = 0; 
        ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, 0, 2, 2, (uint8_t*)&ccc_val);
    } else {
        // 属性未找到或不可读
        ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, 0x0A /* ATT_ERR_ATTRIBUTE_NOT_FOUND */, 0, 0, NULL);
    }
}

/**
 * @brief 处理 GATT 写入请求的回调函数
 * 
 * @param conidx 连接索引
 * @param user_lid GATT 用户回调 ID
 * @param token 请求令牌
 * @param hdl 属性句柄
 * @param offset 写入偏移量
 * @param p_data 写入数据指针 (包含数据长度和内容)
 */
static void cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, void* p_data)
{
    uint16_t idx = hdl - custom_start_hdl;
    // 注意：p_data 结构依赖于具体的协议栈实现，这里假设已处理
    
    LISA_LOGI(LOG_TAG, "Write request: idx=%d", idx);
    
    // 发送写入确认
    ble_gatt_srv_att_val_set_cfm(conidx, user_lid, token, 0);
    
    // 如果是通知配置被写入，可以在这里处理通知开启/关闭逻辑
    if (idx == IDX_CHAR_NOTIFY_CFG) {
        LISA_LOGI(LOG_TAG, "CCCD updated");
    }
}

/**
 * @brief 通知/指示发送完成回调
 * 
 * @param conidx 连接索引
 * @param user_lid GATT 用户回调 ID
 * @param dummy 虚拟参数
 * @param status 发送状态
 */
static void cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    LISA_LOGI(LOG_TAG, "Event sent status: %d", status);
}

/**
 * @brief 获取属性信息回调 (用于 Read by Type 等请求)
 * 
 * @param conidx 连接索引
 * @param user_lid GATT 用户回调 ID
 * @param token 请求令牌
 * @param hdl 属性句柄
 */
static void cb_att_info_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl)
{
    uint16_t idx = hdl - custom_start_hdl;
    uint16_t length = 0;
    
    // 返回属性的当前长度
    if (idx == IDX_CHAR_READ_VAL) length = 20;
    else if (idx == IDX_CHAR_WRITE_VAL) length = 20;
    else if (idx == IDX_CHAR_NOTIFY_VAL) length = 20;
    else if (idx == IDX_CHAR_READ_WRITE_VAL) length = 20;
    else if (idx == IDX_CHAR_SEC_READ_VAL) length = 20;
    else if (idx == IDX_CHAR_NOTIFY_CFG) length = 2;
    
    ble_gatt_srv_att_info_get_cfm(conidx, user_lid, token, 0, length);
}

/* GATT 服务回调函数集合 */
static const ble_gatt_srv_cb_t custom_cb = {
    .cb_event_sent = cb_event_sent,
    .cb_att_read_get = cb_att_read_get,
    .cb_att_event_get = NULL, // 不使用 Reliable Write
    .cb_att_info_get = cb_att_info_get,
    .cb_att_val_set = cb_att_val_set,
};

/**
 * @brief 初始化自定义服务
 * 
 * 注册 GATT 用户回调并添加服务到数据库。
 * 此函数应在协议栈初始化完成后调用。
 */
void app_ble_init_cmp(void)
{
    LISA_LOGI(LOG_TAG, "Initializing BLE Services");

    // 1. 注册 GATT 用户回调
    // pref_mtu: 512, prio: 0
    ble_gatt_user_register(512, 0, &custom_cb, &custom_user_lid);

    // 2. 添加自定义服务
    // uuid: 0x1234 (CUSTOM_SVC_UUID)
    // nb_att: IDX_NB (属性数量)
    ble_gatt_db_svc16_add(custom_user_lid, 0, CUSTOM_SVC_UUID, IDX_NB, NULL, custom_att_db, IDX_NB, &custom_start_hdl);

    LISA_LOGI(LOG_TAG, "Custom Service added, start_hdl=%d", custom_start_hdl);

    // 3. 初始化 HOGP HID 服务
    uint8_t svc_features = HOGPD_CFG_KEYBOARD | HOGPD_CFG_MOUSE |
                          HOGPD_CFG_PROTO_MODE | HOGPD_CFG_REPORT_NTF_EN;
    uint8_t report_char_cfg = HOGPD_CFG_REPORT_IN;
    hogpd_report_map_t report_map = {
        .size = sizeof(hid_report_map),
        .remain_size = 0,
        .rep_map = (uint8_t *)hid_report_map
    };

    uint16_t ret = ble_hogpd_init(svc_features, report_char_cfg,
                                  (hogpd_cb_t *)&ble_hogpd_cb, &report_map);
    if (ret == 0) {
        LISA_LOGI(LOG_TAG, "HOGPD service initialized");
        // 不在这里启用，等待连接建立后再启用
    } else {
        LISA_LOGE(LOG_TAG, "Failed to init HOGPD: 0x%x", ret);
    }
}
