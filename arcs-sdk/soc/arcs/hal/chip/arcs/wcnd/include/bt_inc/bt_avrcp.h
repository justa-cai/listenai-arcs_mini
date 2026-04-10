/**
 ****************************************************************************************
 *
 * @file bt_avrcp.h
 *
 * @brief Header file - BLE GAP External API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BT_AVRCP_H_
#define BT_AVRCP_H_

/**
 ****************************************************************************************
 * @addtogroup GAP External API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */


/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */
 typedef enum {
    BT_AVRCP_KEY_PRESS_SINGLE = 0x00,
    BT_AVRCP_KEY_PRESS_HOLD,
    BT_AVRCP_KEY_PRESS_RELEASE
} bt_avrcp_key_type_t;

 typedef enum {
    BT_AVRCP_KEY_PRESSED = 0x00,
    BT_AVRCP_KEY_RELEASED,
} bt_avrcp_key_press_type_t;

typedef enum {

    BT_AVRCP_KEY_ID_SELECT = 0x00,
    BT_AVRCP_KEY_ID_UP = 0x01,
    BT_AVRCP_KEY_ID_DOWN = 0x02,
    BT_AVRCP_KEY_ID_LEFT = 0x03,
    BT_AVRCP_KEY_ID_RIGHT = 0x04,
    BT_AVRCP_KEY_ID_ROOT_MENU = 0x09,

    BT_AVRCP_KEY_ID_0 = 0x20,
    BT_AVRCP_KEY_ID_1 = 0x21,
    BT_AVRCP_KEY_ID_2 = 0x22,
    BT_AVRCP_KEY_ID_3 = 0x23,
    BT_AVRCP_KEY_ID_4 = 0x24,
    BT_AVRCP_KEY_ID_5 = 0x25,
    BT_AVRCP_KEY_ID_6 = 0x26,
    BT_AVRCP_KEY_ID_7 = 0x27,
    BT_AVRCP_KEY_ID_8 = 0x28,
    BT_AVRCP_KEY_ID_9 = 0x29,
    BT_AVRCP_KEY_ID_DOT = 0x2a,
    BT_AVRCP_KEY_ID_ENTER = 0x2b,
    BT_AVRCP_KEY_ID_CLEAR = 0x2c,
    BT_AVRCP_KEY_ID_CHANNEL_UP = 0x30,
    BT_AVRCP_KEY_ID_CHANNEL_DOWN = 0x31,
    BT_AVRCP_KEY_ID_SKIP = 0x3c,
    BT_AVRCP_KEY_ID_VOLUME_UP = 0x41,
    BT_AVRCP_KEY_ID_VOLUME_DOWN = 0x42,
    BT_AVRCP_KEY_ID_MUTE = 0x43,
    BT_AVRCP_KEY_ID_PLAY = 0x44,
    BT_AVRCP_KEY_ID_STOP = 0x45,
    BT_AVRCP_KEY_ID_PAUSE = 0x46,
    BT_AVRCP_KEY_ID_REWIND = 0x48,
    BT_AVRCP_KEY_ID_FAST_FORWARD = 0x49,
    BT_AVRCP_KEY_ID_FORWARD = 0x4b,
    BT_AVRCP_KEY_ID_BACKWARD = 0x4c,
    BT_AVRCP_KEY_ID_ANGLE = 0x50,
    BT_AVRCP_KEY_ID_SUBPICTURE = 0x51,
    BT_AVRCP_KEY_ID_F1 = 0x71,
    BT_AVRCP_KEY_ID_F2 = 0x72,
    BT_AVRCP_KEY_ID_F3 = 0x73,
    BT_AVRCP_KEY_ID_F4 = 0x74,
    BT_AVRCP_KEY_ID_F5 = 0x75,
    BT_AVRCP_KEY_ID_VENDOR_UNIQUE = 0x7e,
    BT_AVRCP_KEY_ID_UNDEFINED = 0xff
} bt_avrcp_key_id_t;
    
typedef enum {
    BT_AVRCP_NOTIFI_PLAYBACK_STATUS_CHANGED             = 0x01,
    BT_AVRCP_NOTIFI_TRACK_CHANGED                       = 0x02,
    BT_AVRCP_NOTIFI_TRACK_REACHED_END                   = 0x03,
    BT_AVRCP_NOTIFI_TRACK_REACHED_START                 = 0x04,
    BT_AVRCP_NOTIFI_PLAYBACK_POS_CHANGED                = 0x05,
    BT_AVRCP_NOTIFI_BATT_STATUS_CHANGED                 = 0x06,
    BT_AVRCP_NOTIFI_SYSTEM_STATUS_CHANGED               = 0x07,
    BT_AVRCP_NOTIFI_PLAYER_APPLICATION_SETTING_CHANGED  = 0x08,
    BT_AVRCP_NOTIFI_NOW_PLAYING_CONTENT_CHANGED         = 0x09,
    BT_AVRCP_NOTIFI_AVAILABLE_PLAYERS_CHANGED           = 0x0a,
    BT_AVRCP_NOTIFI_ADDRESSED_PLAYER_CHANGED            = 0x0b,
    BT_AVRCP_NOTIFI_UIDS_CHANGED                        = 0x0c,
    BT_AVRCP_NOTIFI_VOLUME_CHANGED                      = 0x0d,
    BT_AVRCP_NOTIFI_MAX_VALUEEASE                       = 0x0e,
} bt_avrcp_notify_id_t;

typedef enum {
    BT_AVRCP_META_GET_CAPABILITIES_INFO = 0x10,
    BT_AVRCP_META_GET_PLAY_STATUS_INFO = 0x11,
    BT_AVRCP_META_GET_ELEMENT_ATTRIBUTE_INFO = 0x12,
} bt_avrcp_get_meta_info_id_t;
    
typedef enum {
    BT_AVRCP_PRESS_ID_SELECT = 0x00,
    BT_AVRCP_PRESS_ID_UP = 0x01,
    BT_AVRCP_PRESS_ID_DOWN = 0x02,
    BT_AVRCP_PRESS_ID_LEFT = 0x03,
    BT_AVRCP_PRESS_ID_RIGHT = 0x04,
    BT_AVRCP_PRESS_ID_ROOT_MENU = 0x09,

    BT_AVRCP_PRESS_ID_CHANNEL_UP = 0x30,
    BT_AVRCP_PRESS_ID_CHANNEL_DOWN = 0x31,

    BT_AVRCP_PRESS_ID_SKIP = 0x3C,
    BT_AVRCP_PRESS_ID_VOLUME_UP = 0x41,
    BT_AVRCP_PRESS_ID_VOLUME_DOWN = 0x42,
    BT_AVRCP_PRESS_ID_MUTE = 0x43,
    
    BT_AVRCP_PRESS_ID_PLAY = 0x44,
    BT_AVRCP_PRESS_ID_STOP = 0x45,
    BT_AVRCP_PRESS_ID_PAUSE = 0x46,
    BT_AVRCP_PRESS_ID_REWIND = 0x48,
    BT_AVRCP_PRESS_ID_FAST_FORWARD = 0x49,
    BT_AVRCP_PRESS_ID_FORWARD = 0x4B,
    BT_AVRCP_PRESS_ID_BACKWARD = 0x4C,

    BT_AVRCP_PRESS_ID_SET_ABSOLUTE_VOLUME = 0x50,

    BT_AVRCP_PRESS_ID_NEXT_GROUP = 0x60,
    BT_AVRCP_PRESS_ID_PREVIOUS_GROUP = 0x61,

    BT_AVRCP_PRESS_ID_SEND_CUSTOM_COMMAND = 0x80,

    BT_AVRCP_PRESS_ID_UNDEFINED = 0xFF
}bt_avrcp_press_id_t;

typedef enum{
    BT_AVRCP_PLAYBACK_STATUS_STOPPED = 0x00,
    BT_AVRCP_PLAYBACK_STATUS_PLAYING,
    BT_AVRCP_PLAYBACK_STATUS_PAUSED,
    BT_AVRCP_PLAYBACK_STATUS_FWD_SEEK,
    BT_AVRCP_PLAYBACK_STATUS_REV_SEEK,
    BT_AVRCP_PLAYBACK_STATUS_ERROR = 0xFF
} bt_avrcp_playback_status_t;

typedef struct bt_avrcp_cb
{
    /**
     ****************************************************************************************
     * @brief Reception of avrcp connect complete.
     ****************************************************************************************
     */
    void (*cb_avrcp_connect_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Reception of a2dp avrcp disconnect complete.
     ****************************************************************************************
     */
    void (*cb_avrcp_disconnect_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief avrcp press complete.
     ****************************************************************************************
     */
    void (*cb_avrcp_press_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief avrcp press complete.
     ****************************************************************************************
     */
    void (*cb_avrcp_notify_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief a2dp media indicate.
     ****************************************************************************************
     */
    void (*cb_avrcp_media_cmp)(uint8_t conidx, uint16_t status);


    /**
     ****************************************************************************************
     * @brief avrcp press indicate.
     ****************************************************************************************
     */
    void (*cb_avrcp_press_ind)(uint8_t conidx, uint8_t key_type, uint8_t key_id, uint8_t key_value);

    /**
     ****************************************************************************************
     * @brief avrcp notify indicate.
     ****************************************************************************************
     */
    void (*cb_avrcp_notify_ind)(uint8_t conidx, uint8_t c_r, uint8_t event_id, uint8_t event_value);

    /**
     ****************************************************************************************
     * @brief a2dp media indicate.
     ****************************************************************************************
     */
    void (*cb_avrcp_media_ind)(uint8_t conidx, uint8_t info_id);

}bt_avrcp_cb_t;

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/**
 * avrcp register
 *
 * @param cb:callback function.
 *
 * @return None.
 */
void app_avrcp_register(const bt_avrcp_cb_t *cb);

/**
 * avrcp unregister
 *
 * @param cb:callback function.
 *
 * @return None.
 */
void app_avrcp_unregister(void);

/**
 * avrcp press req
 *
 * @param conidx:connect id. key_type:enum@avrcp_key_type_t.key_id:enum@avrcp_key_id_t.key_value:key value.
 *
 * @return None.
 */
void app_avrcp_press_req(uint8_t conidx, uint8_t key_type, uint8_t key_id, uint8_t key_value);

/**
 * avrcp notify req
 *
 * @param conidx:connect id. event_id:enum@avrcp_notify_id_t.
 *
 * @return None.
 */
void app_avrcp_notify_req(uint8_t conidx, uint8_t event_id);

/**
 * avrcp notify req
 *
 * @param conidx:connect id. info_id:enum@avrcp_get_meta_info_id_t.
 *
 * @return None.
 */
void app_avrcp_meta_req(uint8_t conidx, uint8_t info_id);


/**
 * avrcp play status set
 *
 * @param conidx:connect id. play_status:enum@avrcp_playback_status_t.
 *
 * @return None.
 */
void app_avrcp_play_status_set(uint8_t conidx, uint8_t play_status);

#endif



