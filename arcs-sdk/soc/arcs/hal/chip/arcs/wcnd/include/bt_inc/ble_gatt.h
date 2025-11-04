/**
 ****************************************************************************************
 *
 * @file ble_gatt.h
 *
 * @brief Header file - BLE GATT External API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BLE_GATT_H_
#define BLE_GATT_H_

/**
 ****************************************************************************************
 * @addtogroup GATT External API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdbool.h>
#include <stdint.h>
/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */

/// helper macro to define an attribute property
/// @param prop Property @see enum gatt_prop_bf
#define BLE_PROP(prop)          (BLE_GATT_PROP_##prop##_BIT)


/// helper macro to define an attribute option bit
/// @param option @see enum gatt_att_info_bf or @see enum gatt_att_ext_info_bf
#define BLE_OPT(opt)            (BLE_GATT_ATT_##opt##_BIT)

/// helper macro to set attribute security level on a specific permission
/// @param  lvl_name Security level @see enum gatt_sec_lvl
/// @param  perm     Permission @see enum gatt_att_info_bf (only RP, WP, NIP authorized)
#define BLE_SEC_LVL(perm, lvl_name)  (((BLE_GATT_SEC_##lvl_name) << (BLE_GATT_ATT_##perm##_LSB)) & (BLE_GATT_ATT_##perm##_MASK))

/*
 * DEFINES
 ****************************************************************************************
 */
/// Invalid GATT user local index
#define BLE_GATT_INVALID_USER_LID           (0xFF)

///// Invalid Attribute Handle
#define BLE_GATT_INVALID_HDL                (0x0000)

/// Length of 128-bit UUID in octets
#define BLE_GATT_UUID_128_LEN    (16)


/*
 * ENUMERATIONS
 ****************************************************************************************
 */
///    7    6    5   4   3    2    1    0
/// +-----+----+---+---+----+----+----+---+
/// | EXT | WS | I | N | WR | WC | RD | B |
/// +-----+----+---+---+----+----+----+---+
/// GATT Attribute properties Bit Field
enum ble_gatt_prop_bf
{
    /// Broadcast descriptor present
    BLE_GATT_PROP_B_BIT          = 0x0001,
    BLE_GATT_PROP_B_POS          = 0,
    /// Read Access Mask
    BLE_GATT_PROP_RD_BIT          = 0x0002,
    BLE_GATT_PROP_RD_POS          = 1,
    /// Write Command Enabled attribute Mask
    BLE_GATT_PROP_WC_BIT         = 0x0004,
    BLE_GATT_PROP_WC_POS         = 2,
    /// Write Request Enabled attribute Mask
    BLE_GATT_PROP_WR_BIT         = 0x0008,
    BLE_GATT_PROP_WR_POS         = 3,
    /// Notification Access Mask
    BLE_GATT_PROP_N_BIT          = 0x0010,
    BLE_GATT_PROP_N_POS          = 4,
    /// Indication Access Mask
    BLE_GATT_PROP_I_BIT          = 0x0020,
    BLE_GATT_PROP_I_POS          = 5,
    /// Write Signed Enabled attribute Mask
    BLE_GATT_PROP_WS_BIT         = 0x0040,
    BLE_GATT_PROP_WS_POS         = 6,
    /// Extended properties descriptor present
    BLE_GATT_PROP_EXT_BIT        = 0x0080,
    BLE_GATT_PROP_EXT_POS        = 7,
    /// Read security level permission (@see enum gatt_sec_lvl).
    BLE_GATT_ATT_RP_MASK        = 0x0300,
    BLE_GATT_ATT_RP_LSB         = 8,
    /// Write security level permission (@see enum gatt_sec_lvl).
    BLE_GATT_ATT_WP_MASK        = 0x0C00,
    BLE_GATT_ATT_WP_LSB         = 10,
    /// Notify and Indication security level permission (@see enum gatt_sec_lvl).
    BLE_GATT_ATT_NIP_MASK       = 0x3000,
    BLE_GATT_ATT_NIP_LSB        = 12,
    /// Type of attribute UUID (@see enum gatt_uuid_type)
    BLE_GATT_ATT_UUID_TYPE_MASK  = 0xC000,
    BLE_GATT_ATT_UUID_TYPE_LSB   = 14,
};


/// GATT attribute security level
enum ble_gatt_sec_lvl
{
    /// Attribute value is accessible on an unencrypted link.
    BLE_GATT_SEC_NOT_ENC    = 0x00,
    /// Attribute value is accessible on an encrypted link or modified with using write signed procedure
    /// on unencrypted link if bonded using an unauthenticated pairing.
    BLE_GATT_SEC_NO_AUTH    = 0x01,
    /// Attribute value is accessible on an encrypted link or modified with using write signed procedure
    /// on unencrypted link if bonded using an authenticated pairing.
    BLE_GATT_SEC_AUTH       = 0x02,
    /// Attribute value is accessible on an encrypted link or modified with using write signed procedure
    /// on unencrypted link if bonded using a secure connection pairing.
    BLE_GATT_SEC_SECURE_CON = 0x03,
};

/// GATT Event Type
enum ble_gatt_evt_type
{
    /// Server initiated notification
    BLE_GATT_NOTIFY     = 0x00,
    /// Server initiated indication
    BLE_GATT_INDICATE   = 0x01,
};

///       15     14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
/// +-----------+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
/// | NO_OFFSET |               WRITE_MAX_SIZE               |
/// +-----------+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
/// |                     INC_SVC_HANDLE                     |
/// +-----------+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
/// |                     EXT_PROP_VALUE                     |
/// +-----------+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
///
/// GATT Attribute extended information Bit Field
enum ble_gatt_att_ext_info_bf
{
    /// Maximum value authorized for an attribute write.
    /// Automatically reduce to Maximum Attribute value (@see GATT_MAX_VALUE) if greater
    BLE_GATT_ATT_WRITE_MAX_SIZE_MASK  = 0x7FFF,
    BLE_GATT_ATT_WRITE_MAX_SIZE_LSB   = 0,
    /// 1: Do not authorize peer device to read or write an attribute with an offset != 0
    /// 0: Authorize offset usage
    BLE_GATT_ATT_NO_OFFSET_BIT        = 0x8000,
    BLE_GATT_ATT_NO_OFFSET_POS        = 15,
    /// Include Service handle value
    BLE_GATT_INC_SVC_HDL_BIT          = 0xFFFF,
    BLE_GATT_INC_SVC_HDL_POS          = 0,
    /// Characteristic Extended Properties value
    BLE_GATT_ATT_EXT_PROP_VALUE_MASK  = 0xFFFF,
    BLE_GATT_ATT_EXT_PROP_VALUE_LSB   = 0,
};


enum ble_gatt_char
{
    /*----------------- SERVICES ---------------------*/
    /// HID Service
    BLE_GATT_SVC_HID                                  = (0x1812),
    BLE_GATT_SVC_DEVICE_INFO                          = (0x180A),
    /// Battery Service
    BLE_GATT_SVC_BATTERY_SERVICE                      = (0x180F),
    /// Scan Parameters Service
    BLE_GATT_SVC_SCAN_PARAMETERS                      = (0x1813),

    /*---------------- DECLARATIONS -----------------*/
    /// Primary service Declaration
    BLE_GATT_DECL_PRIMARY_SERVICE                     = (0x2800),
    /// Include Declaration
    BLE_GATT_DECL_INCLUDE                             = (0x2802),
    /// Characteristic Declaration
    BLE_GATT_DECL_CHARACTERISTIC                      = (0x2803),

    /*----------------- DESCRIPTORS -----------------*/
    /// Client characteristic configuration
    BLE_GATT_DESC_CLIENT_CHAR_CFG                    = (0x2902),
    /// External Report Reference
    BLE_GATT_DESC_EXT_REPORT_REF                     = (0x2907),
    /// Report Reference
    BLE_GATT_DESC_REPORT_REF                         = (0x2908),

    /*--------------- CHARACTERISTICS ---------------*/
    /// HID Information
    BLE_GATT_CHAR_HID_INFO                           = (0x2A4A),
    /// HID Control Point
    BLE_GATT_CHAR_HID_CTNL_PT                        = (0x2A4C),
    /// Report Map
    BLE_GATT_CHAR_REPORT_MAP                         = (0x2A4B),
    /// Protocol Mode
    BLE_GATT_CHAR_PROTOCOL_MODE                      = (0x2A4E),
    /// Boot Keyboard Input Report
    BLE_GATT_CHAR_BOOT_KB_IN_REPORT                  = (0x2A22),
    /// Boot Keyboard Output Report
    BLE_GATT_CHAR_BOOT_KB_OUT_REPORT                 = (0x2A32),
    /// Boot Mouse Input Report
    BLE_GATT_CHAR_BOOT_MOUSE_IN_REPORT               = (0x2A33),
    /// Report
    BLE_GATT_CHAR_REPORT                             = (0x2A4D),
    /// Manufacturer Name String
    BLE_GATT_CHAR_MANUF_NAME                         = (0x2A29),
    /// System ID
    BLE_GATT_CHAR_SYS_ID                             = (0x2A23),
    /// Model Number String
    BLE_GATT_CHAR_MODEL_NB                           = (0x2A24),
    /// Serial Number String
    BLE_GATT_CHAR_SERIAL_NB                          = (0x2A25),
    /// Firmware Revision String
    BLE_GATT_CHAR_FW_REV                             = (0x2A26),
    /// Hardware revision String
    BLE_GATT_CHAR_HW_REV                             = (0x2A27),
    /// Software Revision String
    BLE_GATT_CHAR_SW_REV                             = (0x2A28),
    /// Battery Level
    BLE_GATT_CHAR_BATTERY_LEVEL                      = (0x2A19),
    /// PnP ID
    BLE_GATT_CHAR_PNP_ID                             = (0x2A50),
    /// Characteristic Presentation Format
    BLE_GATT_DESC_CHAR_PRES_FORMAT                    =(0x2904),
    /// Scan Interval Window
    BLE_GATT_CHAR_SCAN_INTV_WD                       = (0x2A4F),
    /// Scan Refresh
    BLE_GATT_CHAR_SCAN_REFRESH                       = (0x2A31),
};


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
/// 16-bit UUID Attribute Description structure
typedef struct ble_gatt_att16_desc
{
    /// Attribute UUID (16-bit UUID - LSB First)
    uint16_t uuid16;
    /// Attribute information bit field (@see enum gatt_att_info_bf)
    uint16_t info;
    /// Attribute extended information bit field (@see enum gatt_att_ext_info_bf)
    /// Note:
    ///   - For Included Services and Characteristic Declarations, this field contains targeted handle.
    ///   - For Characteristic Extended Properties, this field contains 2 byte value
    ///   - For Client Characteristic Configuration and Server Characteristic Configuration, this field is not used.
    uint16_t ext_info;
} ble_gatt_att16_desc_t;


/// Attribute Description structure
/*@TRACE*/
typedef struct ble_gatt_att_desc
{
    /// Attribute UUID (LSB First)
    uint8_t  uuid[BLE_GATT_UUID_128_LEN];
    /// Attribute information bit field (@see enum gatt_att_info_bf)
    uint16_t info;
    /// Attribute extended information bit field (@see enum gatt_att_ext_info_bf)
    /// Note:
    ///   - For Included Services and Characteristic Declarations, this field contains targeted handle.
    ///   - For Characteristic Extended Properties, this field contains 2 byte value
    ///   - For Client Characteristic Configuration and Server Characteristic Configuration, this field is not used.
    uint16_t ext_info;
} ble_gatt_att_desc_t;


/// GATT server user callback set
typedef struct ble_gatt_srv_cb
{
    /**
     ****************************************************************************************
     * @brief This function is called when GATT server user has initiated event send to peer
     *        device or if an error occurs.
     *
     * @param[in] conidx        Connection index
     * @param[in] user_lid      GATT user local identifier
     * @param[in] dummy         Dummy parameter provided by upper layer for command execution
     * @param[in] status        Status of the procedure (@see enum hl_err)
     ****************************************************************************************
     */
    void (*cb_event_sent) (uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status);

    /**
     ****************************************************************************************
     * @brief This function is called when peer want to read local attribute database value.
     *
     *        @see gatt_srv_att_read_get_cfm shall be called to provide attribute value
     *
     * @param[in] conidx        Connection index
     * @param[in] user_lid      GATT user local identifier
     * @param[in] token         Procedure token that must be returned in confirmation function
     * @param[in] hdl           Attribute handle
     * @param[in] offset        Value offset
     * @param[in] max_length    Maximum value length to return
     ****************************************************************************************
     */
    void (*cb_att_read_get) (uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                             uint16_t max_length);

    /**
     ****************************************************************************************
     * @brief This function is called when GATT server user has initiated event send procedure,
     *
     *        @see gatt_srv_att_event_get_cfm shall be called to provide attribute value
     *
     * @param[in] conidx        Connection index
     * @param[in] user_lid      GATT user local identifier
     * @param[in] token         Procedure token that must be returned in confirmation function
     * @param[in] dummy         Dummy parameter provided by upper layer for command execution.
     * @param[in] hdl           Attribute handle
     * @param[in] max_length    Maximum value length to return
     ****************************************************************************************
     */
    void (*cb_att_event_get) (uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t dummy, uint16_t hdl,
                              uint16_t max_length);

    /**
     ****************************************************************************************
     * @brief This function is called during a write procedure to get information about a
     *        specific attribute handle.
     *
     *        @see gatt_srv_att_info_get_cfm shall be called to provide attribute information
     *
     * @param[in] conidx        Connection index
     * @param[in] user_lid      GATT user local identifier
     * @param[in] token         Procedure token that must be returned in confirmation function
     * @param[in] hdl           Attribute handle
     ****************************************************************************************
     */
    void (*cb_att_info_get) (uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl);

    /**
     ****************************************************************************************
     * @brief This function is called during a write procedure to modify attribute handle.
     *
     *        @see gatt_srv_att_val_set_cfm shall be called to accept or reject attribute
     *        update.
     *
     * @param[in] conidx        Connection index
     * @param[in] user_lid      GATT user local identifier
     * @param[in] token         Procedure token that must be returned in confirmation function
     * @param[in] hdl           Attribute handle
     * @param[in] offset        Value offset
     * @param[in] p_data        Pointer to buffer that contains data to write starting from offset
     ****************************************************************************************
     */
    void (*cb_att_val_set) (uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                            void* p_data);
} ble_gatt_srv_cb_t;


/*@TRACE*/
typedef struct ble_gatt_att
{
    /// Attribute handle
    uint16_t hdl;
    /// Value length
    uint16_t length;
} ble_gatt_att_t;



/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */




/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
/**
 * @brief Command used to register a GATT user. This must be done prior to any GATT
 *        procedure execution.
 *
 * @param[in]  pref_mtu     Preferred MTU for attribute exchange.
 * @param[in]  prio_level   User attribute priority level
 * @param[in]  p_cb         Pointer to set of callback functions to be used for communication
 *                          with the GATT server user
 * @param[out] p_user_lid   Pointer where GATT user local identifier will be set
 *
 * @return Status of the function execution (@see enum hl_err)
 */
uint16_t ble_gatt_user_register(uint16_t pref_mtu, uint8_t prio_level, const ble_gatt_srv_cb_t* p_cb, uint8_t* p_user_lid);


/**
 ****************************************************************************************
 * @brief Command used to unregister a GATT user (client or server).
 *
 * @param[in]  user_lid     GATT User Local identifier
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_user_unregister(uint8_t user_lid);


/**
 ****************************************************************************************
 * @brief Command used to add a service into local attribute database.
 *
 *        Service and attributes UUIDs in service must be 16-bit
 *
 *        If start handle is set to zero (invalid attribute handle), GATT looks for a
 *        free handle block matching with number of attributes to reserve.
 *        Else, according to start handle, GATT checks if attributes to reserve are
 *        not overlapping part of existing database.
 *
 *        An added service is automatically visible for peer device.
 *
 *        @note First attribute in attribute array must be a Primary or a Secondary service
 *
 * @param[in]     user_lid     GATT User Local identifier
 * @param[in]     info         Service Information bit field (@see enum gatt_svc_info_bf)
 * @param[in]     uuid16       Service UUID (16-bit UUID - LSB First)
 * @param[in]     nb_att       Number of attribute(s) in service
 * @param[in]     p_att_mask   Pointer to mask of attribute to insert in database:
 *                               - If NULL insert all attributes
 *                               - If bit set to 1: attribute inserted
 *                               - If bit set to 0: attribute not inserted
 * @param[in]     p_atts       Pointer to List of attribute (with 16-bit uuid) description present in service.
 * @param[in]     nb_att_rsvd  Number of attribute(s) reserved for the service (shall be equals or greater nb_att)
 *                             Prevent any services to be inserted between start_hdl and (start_hdl + nb_att_rsvd - 1)
 * @param[in|out] p_start_hdl  Pointer to Service Start Handle (0 = chosen by GATT module)
 *                             Pointer updated with service start handle associated to
 *                             created service.
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_db_svc16_add(uint8_t user_lid, uint8_t info, uint16_t uuid16, uint8_t nb_att, const uint8_t* p_att_mask,
        const ble_gatt_att16_desc_t* p_atts, uint8_t nb_att_rsvd, uint16_t* p_start_hdl);

/**
 ****************************************************************************************
 * @brief Command used to add a service into local attribute database.
 *
 *        If start handle is set to zero (invalid attribute handle), GATT looks for a
 *        free handle block matching with number of attributes to reserve.
 *        Else, according to start handle, GATT checks if attributes to reserve are
 *        not overlapping part of existing database.
 *
 *        An added service is automatically visible for peer device.
 *
 *        @note First attribute in attribute array must be a Primary or a Secondary service
 *
 * @param[in]     user_lid     GATT User Local identifier
 * @param[in]     info         Service Information bit field (@see enum gatt_svc_info_bf)
 * @param[in]     p_uuid       Pointer to service UUID (LSB first)
 * @param[in]     nb_att       Number of attribute(s) in service
 * @param[in]     p_att_mask   Pointer to mask of attribute to insert in database:
 *                               - If NULL insert all attributes
 *                               - If bit set to 1: attribute inserted
 *                               - If bit set to 0: attribute not inserted
 * @param[in]     p_atts       Pointer to List of attribute description present in service.
 * @param[in]     nb_att_rsvd  Number of attribute(s) reserved for the service (shall be equals or greater nb_att)
 *                             Prevent any services to be inserted between start_hdl and (start_hdl + nb_att_rsvd -1)
 * @param[in|out] p_start_hdl  Pointer to Service Start Handle (0 = chosen by GATT module)
 *                             Pointer updated with service start handle associated to
 *                             created service.
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_db_svc_add(uint8_t user_lid, uint8_t info, const uint8_t* p_uuid, uint8_t nb_att, const uint8_t* p_att_mask,
                         const ble_gatt_att_desc_t* p_atts, uint8_t nb_att_rsvd, uint16_t* p_start_hdl);


/**
 ****************************************************************************************
 * @brief Command used to remove a service from local attribute database.
 *
 *        Only GATT user responsible of service can remove it.
 *
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  start_hdl    Service Start Handle
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_db_svc_remove(uint8_t user_lid, uint16_t start_hdl);


/**
 ****************************************************************************************
 * @brief Command used to control visibility and usage authorization of a local service.
 *        A hidden service is present in database but cannot be discovered or manipulated
 *        by a peer device.
 *        A disabled service can be discovered by a peer device but it is not authorized to
 *        use it.
 *
 *        Only GATT user responsible of service can update its properties
 *
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  start_hdl    Service Start Handle
 * @param[in]  enable       True: Authorize usage of the service
 *                          False: reject usage of the service
 * @param[in]  visible      Service visibility (@see enum gatt_svc_visibility)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_db_svc_ctrl(uint8_t user_lid, uint16_t start_hdl, uint8_t enable, uint8_t visible);


/**
 ****************************************************************************************
 * @brief Command used by a GATT server user to send notifications or indications for
 *        some attributes values to peer device.
 *        Number of attributes must be set to one for GATT_INDICATE event type.
 *
 *        This function is consider reliable because GATT user is aware of maximum packet
 *        size that can be transmitted over the air.
 *
 *        Attribute value will be requested by GATT using @see cb_att_event_get function
 *        Wait for @see cb_event_sent execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  evt_type     Event type to trigger (@see enum gatt_evt_type)
 * @param[in]  nb_att       Number of attribute
 * @param[in]  p_atts       Pointer to List of attribute
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_srv_event_reliable_send(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint8_t evt_type,
        uint8_t nb_att, const ble_gatt_att_t* p_atts);


/**
 ****************************************************************************************
 * @brief Command used by a GATT server user to send notifications or indications.
 *
 *        Since user is not aware of MTU size of the bearer used for attribute
 *        transmission it cannot be considered reliable. If size of the data buffer is too
 *        big, data is truncated to max supported length.
 *
 *        Wait for @see cb_event_sent execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  evt_type     Event type to trigger (@see enum gatt_evt_type)
 * @param[in]  hdl          Attribute handle
 * @param[in]  p_data       Data buffer that must be transmitted
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_srv_event_send(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint8_t evt_type, uint16_t hdl,
        const uint8_t* p_data, uint16_t data_len);


/**
 ****************************************************************************************
 * @brief Command used by a GATT server user to cancel a multi connection event transmission
 *
 *        @note Once procedure is done, @see cb_event_sent function is called.
 *
 * @param[in]  user_lid     GATT User Local identifier used in @see gatt_srv_event_mtp_send
 * @param[in]  dummy        Dummy parameter used in @see gatt_srv_event_mtp_send
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_srv_event_mtp_cancel(uint8_t user_lid, uint16_t dummy);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to discover primary or secondary services,
 *        exposed by peer device in its attribute database.
 *
 *        All services can be discovered or filtering services having a specific UUID.
 *        The discovery is done between start handle and end handle range.
 *        For a complete discovery start handle must be set to 0x0001 and end handle to
 *        0xFFFF.
 *
 *        Wait for @see cb_discover_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  disc_type    GATT Service discovery type (@see enum gatt_svc_discovery_type)
 * @param[in]  full         Perform discovery of all information present in the service
 *                          (True: enable, False: disable)
 * @param[in]  start_hdl    Search start handle
 * @param[in]  end_hdl      Search end handle
 * @param[in]  uuid_type    UUID Type (@see enum gatt_uuid_type)
 * @param[in]  p_uuid       Pointer to searched Service UUID (meaningful only for
 *                          discovery by UUID)
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_discover_svc(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint8_t disc_type, bool full,
        uint16_t start_hdl, uint16_t end_hdl, uint8_t uuid_type, const uint8_t* p_uuid);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to discover included services, exposed
 *        by peer device in its attribute database.
 *
 *        The discovery is done between start handle and end handle range.
 *        For a complete discovery start handle must be set to 0x0001 and end handle to
 *        0xFFFF.
 *
 *        Wait for @see cb_discover_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  start_hdl    Search start handle
 * @param[in]  end_hdl      Search end handle
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_discover_inc_svc(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t start_hdl, uint16_t end_hdl);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to discover all or according to a specific
 *        UUID characteristics exposed by peer device in its attribute database.
 *
 *        The discovery is done between start handle and end handle range.
 *        For a complete discovery start handle must be set to 0x0001 and end handle to
 *        0xFFFF.
 *
 *        Wait for @see cb_discover_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  disc_type    GATT characteristic discovery type (@see enum gatt_char_discovery_type)
 * @param[in]  start_hdl    Search start handle
 * @param[in]  end_hdl      Search end handle
 * @param[in]  uuid_type    UUID Type (@see enum gatt_uuid_type)
 * @param[in]  p_uuid       Pointer to searched Attribute Value UUID (meaningful only
 *                          for discovery by UUID)
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_discover_char(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint8_t disc_type, uint16_t start_hdl, uint16_t end_hdl, uint8_t uuid_type, const uint8_t* p_uuid);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to discover characteristic descriptor
 *        exposed by peer device in its attribute database.
 *
 *        The discovery is done between start handle and end handle range.
 *        For a complete discovery start handle must be set to 0x0001 and end handle to
 *        0xFFFF.
 *
 *        Wait for @see cb_discover_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  start_hdl    Search start handle
 * @param[in]  end_hdl      Search end handle
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_discover_desc(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t start_hdl, uint16_t end_hdl);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to cancel an on-going discovery procedure.
 *        The dummy parameter in the request must be equals to dummy parameter used for
 *        service discovery command.
 *
 *        The discovery is aborted as soon as on-going discovery attribute transaction
 *        is over.
 *
 *        Wait for @see cb_discover_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_discover_cancel(uint8_t conidx, uint8_t user_lid, uint16_t dummy);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to read value of an attribute (identified
 *        by its handle) present in peer database.
 *
 *        Wait for @see cb_read_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  hdl          Attribute handle
 * @param[in]  offset       Value offset
 * @param[in]  length       Value length to read (0 = read all)
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_read(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset, uint16_t length);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to read value of an attribute with a given
 *        UUID in peer database.
 *
 *        Wait for @see cb_read_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  start_hdl    Search start handle
 * @param[in]  end_hdl      Search end handle
 * @param[in]  uuid_type    UUID Type (@see enum gatt_uuid_type)
 * @param[in]  p_uuid       Pointer to searched attribute UUID (LSB First)
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_read_by_uuid(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t start_hdl, uint16_t end_hdl, uint8_t uuid_type, const uint8_t* p_uuid);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to read multiple attribute at the same time.
 *        If one of attribute length is unknown, the read multiple variable length
 *        procedure is used.
 *
 *        Wait for @see cb_read_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  nb_att       Number of attribute
 * @param[in]  p_atts       Pointer to list of attribute
 *                          If Attribute length is zero (consider length unknown):
  *                            - Attribute protocol read multiple variable length
  *                              procedure used
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_read_multiple(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint8_t nb_att, const ble_gatt_att_t* p_atts);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to request to write value of an attribute
 *        in peer database.
 *
 *        This function is consider reliable because GATT user is aware of maximum packet
 *        size that can be transmitted over the air.
 *
 *        Attribute value will be requested by GATT using @see cb_att_val_get function
 *
 *        Wait for @see cb_write_cmp execution before starting a new procedure
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  write_type   GATT write type (@see enum gatt_write_type)
 * @param[in]  write_mode   Write execution mode (@see enum gatt_write_mode).
 *                          Valid only for GATT_WRITE.
 * @param[in]  hdl          Attribute handle
 * @param[in]  offset       Value offset, valid only for GATT_WRITE
 * @param[in]  length       Value length to write
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_write_reliable(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint8_t write_type, uint8_t write_mode, uint16_t hdl, uint16_t offset, uint16_t length);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to request peer server to execute prepare
 *        write queue.
 *
 *        Wait for @see cb_write_cmp execution before starting a new procedure
 *
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  dummy        Dummy parameter whose meaning is upper layer dependent and
 *                          which is returned in command complete.
 * @param[in]  execute      True: Perform pending write operations
 *                          False: Cancel pending write operations
 *
 * @return Status of the function execution (@see enum hl_err)
 *         Consider status only if an error occurs; else wait for execution completion
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_write_exe(uint8_t conidx, uint8_t user_lid, uint16_t dummy, bool execute);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to register for reception of events
 *        (notification / indication) for a given handle range.
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  start_hdl    Attribute start handle
 * @param[in]  end_hdl      Attribute end handle
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_event_register(uint8_t conidx, uint8_t user_lid, uint16_t start_hdl, uint16_t end_hdl);


/**
 ****************************************************************************************
 * @brief Command used by a GATT client user to stop reception of events (notification /
 *        indication) onto a specific handle range.
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  start_hdl    Attribute start handle
 * @param[in]  end_hdl      Attribute end handle
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_event_unregister(uint8_t conidx, uint8_t user_lid, uint16_t start_hdl, uint16_t end_hdl);


/**
 ****************************************************************************************
 * @brief Request a MTU exchange on legacy attribute bearer.
 *        There is no callback executed when the procedure is over.
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_mtu_update(uint8_t conidx, uint8_t user_lid);


/**
 ****************************************************************************************
 * @brief Upper layer provide attribute value requested by GATT Layer for a read procedure
 *        If rejected, value is not used.
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  token        Procedure token provided in corresponding callback
 * @param[in]  status       Status of attribute value get (@see enum hl_err)
 * @param[in]  att_length   Complete Length of the attribute value
 * @param[in]  p_data       Pointer to buffer that contains attribute data
 *                          (starting from offset and does not exceed maximum size provided)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_srv_att_read_get_cfm(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t status,
                                   uint16_t att_total_length, uint16_t att_send_length, const uint8_t* p_data);


/**
 ****************************************************************************************
 * @brief Upper layer provide information about attribute requested by GATT Layer.
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  token        Procedure token provided in corresponding callback
 * @param[in]  status       Status of attribute info get (@see enum hl_err)
 * @param[in]  att_length   Attribute value length
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_srv_att_info_get_cfm(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t status, uint16_t att_length);


/**
 ****************************************************************************************
 * @brief Upper layer provide status of attribute value modification by GATT server user.
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  token        Procedure token provided in corresponding callback
 * @param[in]  status       Status of attribute value set (@see enum hl_err)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_srv_att_val_set_cfm(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t status);


/**
 ****************************************************************************************
 * @brief Upper layer provide status of attribute event handled by GATT client user.
 *
 * @param[in]  conidx       Connection index
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  token        Procedure token provided in corresponding callback
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_cli_att_event_cfm(uint8_t conidx, uint8_t user_lid, uint16_t token);

/**
 ****************************************************************************************
 * @brief Command used to set information of an attribute.
 *
 * @param[in]  user_lid     GATT User Local identifier
 * @param[in]  hdl          Attribute Handle
 * @param[in]  info         Attribute information bit field (@see enum gatt_att_info_bf)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_db_att_info_set(uint8_t user_lid, uint16_t hdl, uint16_t info);

/**
 ****************************************************************************************
 * @brief Command used to retrieve information of an attribute.
 *
 * @param[in]  user_lid     GATT User Local identifier
 * @param[out] p_info       Attribute information bit field
 *                          (@see enum gatt_att_info_bf)
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_gatt_db_att_info_get(uint8_t user_lid, uint16_t hdl, uint16_t* p_info);

///////////////////////////////////////////////////////////////////////////////////////////
uint16_t ble_co_buf_data_len(void *p_buf);
uint8_t* ble_co_buf_data(void *p_buf);
uint16_t ble_co_btohs(uint16_t btshort);
uint16_t ble_co_read16p(void const *ptr16);

#endif


