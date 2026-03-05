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
    LISA_LOGI(LOG_TAG, "Initializing Custom Service");
    
    // 1. 注册 GATT 用户回调
    // pref_mtu: 512, prio: 0
    ble_gatt_user_register(512, 0, &custom_cb, &custom_user_lid);
    
    // 2. 添加服务
    // uuid: 0x1234 (CUSTOM_SVC_UUID)
    // nb_att: IDX_NB (属性数量)
    ble_gatt_db_svc16_add(custom_user_lid, 0, CUSTOM_SVC_UUID, IDX_NB, NULL, custom_att_db, IDX_NB, &custom_start_hdl);
    
    LISA_LOGI(LOG_TAG, "Custom Service added, start_hdl=%d", custom_start_hdl);
}
