/**
 ****************************************************************************************
 *
 * @file ble_gap.h
 *
 * @brief Header file - BLE GAP External API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BLE_GAP_H_
#define BLE_GAP_H_

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
#ifndef __ARRAY_EMPTY
#define __ARRAY_EMPTY
#endif


/*
 * DEFINES
 ****************************************************************************************
 */
#define GAP_BD_ADDR_LEN       (6)
#define GAP_NAME_LEN          (50)
#define GAP_KEY_LEN           (16)

/// No error
#define GAP_NO_ERROR                                0x00
/// Invalid parameters set
#define GAP_INVALID_PARAM                           0x40
/// Request not supported by software configuration
#define GAP_NOT_SUPPORTED                           0x42
/// Request not allowed in current state.
#define GAP_COMMAND_DISALLOWED                      0x43
/// Requested operation canceled.
#define GAP_CANCELED                                0x44
/// Requested operation timeout.
#define GAP_TIMEOUT                                 0x45
/// Link connection lost during operation.
#define GAP_DISCONNECTED                            0x46
/// Search algorithm finished, but no result found
#define GAP_NOT_FOUND                               0x47
/// Request rejected by peer device
#define GAP_REJECTED                                0x48
/// Problem with privacy configuration
#define GAP_PRIVACY_CFG_PB                          0x49
/// Duplicate or invalid advertising data
#define GAP_ADV_DATA_INVALID                        0x4A
/// Insufficient resources
#define GAP_INSUFF_RESOURCES                        0x4B
/// Unexpected Error
#define GAP_UNEXPECTED                              0x4C
/// Feature mismatch
#define GAP_MISMATCH                                0x4D
/// Buffer cannot be used due to invalid header or tail length
#define GAP_INVALID_BUFFER                          0x4E
/// busy, last command haven't finished
#define GAP_BUSY                                    0x4F

/// user use this code to deal private mode.
#define GAP_CO_ERROR_USER_DEFINED                   0x80

/// Pin code missing
#define GAP_PIN_MISSING                             0x96

/// The encryption procedure failed because the slave device didn't find the LTK
/// needed to start an encryption session.
#define SMP_ENC_KEY_MISSING                         0x22
/// The encryption procedure failed because the slave device doesn't support the
/// encryption feature.
#define SMP_ENC_NOT_SUPPORTED                       0x23



#define GAP_ADV_ID_0           (0)
#define GAP_ADV_ID_1           (1)
#define GAP_ADV_ID_2           (2)

#define GAP_SCAN_ID_0          (0)
#define GAP_SCAN_ID_1          (1)
#define GAP_SCAN_ID_2          (2)

#define GAP_INIT_ID_0          (0)
#define GAP_INIT_ID_1          (0)



enum gap_event_type
{
    GAPM_SET_WL_EVENT  = 0x53,
    GAPM_SET_RAL_EVENT = 0x54,
    GAPM_SET_PAL_EVENT = 0x55,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
enum bt_error
{
    BT_ERROR_NO_ERROR                        = 0x00,
    BT_ERROR_UNKNOWN_HCI_COMMAND             = 0x01,
    BT_ERROR_UNKNOWN_CONNECTION_ID           = 0x02,
    BT_ERROR_HARDWARE_FAILURE                = 0x03,
    BT_ERROR_PAGE_TIMEOUT                    = 0x04,
    BT_ERROR_AUTH_FAILURE                    = 0x05,
    BT_ERROR_PIN_MISSING                     = 0x06,
    BT_ERROR_MEMORY_CAPA_EXCEED              = 0x07,
    BT_ERROR_CON_TIMEOUT                     = 0x08,
    BT_ERROR_CON_LIMIT_EXCEED                = 0x09,
    BT_ERROR_SYNC_CON_LIMIT_DEV_EXCEED       = 0x0A,
    BT_ERROR_CON_ALREADY_EXISTS              = 0x0B,
    BT_ERROR_COMMAND_DISALLOWED              = 0x0C,
    BT_ERROR_CONN_REJ_LIMITED_RESOURCES      = 0x0D,
    BT_ERROR_CONN_REJ_SECURITY_REASONS       = 0x0E,
    BT_ERROR_CONN_REJ_UNACCEPTABLE_BDADDR    = 0x0F,
    BT_ERROR_CONN_ACCEPT_TIMEOUT_EXCEED      = 0x10,
    BT_ERROR_UNSUPPORTED                     = 0x11,
    BT_ERROR_INVALID_HCI_PARAM               = 0x12,
    BT_ERROR_REMOTE_USER_TERM_CON            = 0x13,
    BT_ERROR_REMOTE_DEV_TERM_LOW_RESOURCES   = 0x14,
    BT_ERROR_REMOTE_DEV_POWER_OFF            = 0x15,
    BT_ERROR_CON_TERM_BY_LOCAL_HOST          = 0x16,
    BT_ERROR_REPEATED_ATTEMPTS               = 0x17,
    BT_ERROR_PAIRING_NOT_ALLOWED             = 0x18,
    BT_ERROR_UNKNOWN_LMP_PDU                 = 0x19,
    BT_ERROR_UNSUPPORTED_REMOTE_FEATURE      = 0x1A,
    BT_ERROR_SCO_OFFSET_REJECTED             = 0x1B,
    BT_ERROR_SCO_INTERVAL_REJECTED           = 0x1C,
    BT_ERROR_SCO_AIR_MODE_REJECTED           = 0x1D,
    BT_ERROR_INVALID_LMP_PARAM               = 0x1E,
    BT_ERROR_UNSPECIFIED_ERROR               = 0x1F,
    BT_ERROR_UNSUPPORTED_LMP_PARAM_VALUE     = 0x20,
    BT_ERROR_ROLE_CHANGE_NOT_ALLOWED         = 0x21,
    BT_ERROR_LMP_RSP_TIMEOUT                 = 0x22,
    BT_ERROR_LMP_COLLISION                   = 0x23,
    BT_ERROR_LMP_PDU_NOT_ALLOWED             = 0x24,
    BT_ERROR_ENC_MODE_NOT_ACCEPT             = 0x25,
    BT_ERROR_LINK_KEY_CANT_CHANGE            = 0x26,
    BT_ERROR_QOS_NOT_SUPPORTED               = 0x27,
    BT_ERROR_INSTANT_PASSED                  = 0x28,
    BT_ERROR_PAIRING_WITH_UNIT_KEY_NOT_SUP   = 0x29,
    BT_ERROR_DIFF_TRANSACTION_COLLISION      = 0x2A,
    BT_ERROR_QOS_UNACCEPTABLE_PARAM          = 0x2C,
    BT_ERROR_QOS_REJECTED                    = 0x2D,
    BT_ERROR_CHANNEL_CLASS_NOT_SUP           = 0x2E,
    BT_ERROR_INSUFFICIENT_SECURITY           = 0x2F,
    BT_ERROR_PARAM_OUT_OF_MAND_RANGE         = 0x30,
    BT_ERROR_ROLE_SWITCH_PEND                = 0x32, /* LM_ROLE_SWITCH_PENDING               */
    BT_ERROR_RESERVED_SLOT_VIOLATION         = 0x34, /* LM_RESERVED_SLOT_VIOLATION           */
    BT_ERROR_ROLE_SWITCH_FAIL                = 0x35, /* LM_ROLE_SWITCH_FAILED                */
    BT_ERROR_EIR_TOO_LARGE                   = 0x36, /* LM_EXTENDED_INQUIRY_RESPONSE_TOO_LARGE */
    BT_ERROR_SP_NOT_SUPPORTED_HOST           = 0x37,
    BT_ERROR_HOST_BUSY_PAIRING               = 0x38,
    BT_ERROR_CONTROLLER_BUSY                 = 0x3A,
    BT_ERROR_UNACCEPTABLE_CONN_PARAM         = 0x3B,
    BT_ERROR_ADV_TO                          = 0x3C,
    BT_ERROR_TERMINATED_MIC_FAILURE          = 0x3D,
    BT_ERROR_CONN_FAILED_TO_BE_EST           = 0x3E,
    BT_ERROR_CCA_REJ_USE_CLOCK_DRAG          = 0x40,
    BT_ERROR_TYPE0_SUBMAP_NOT_DEFINED        = 0x41,
    BT_ERROR_UNKNOWN_ADVERTISING_ID          = 0x42,
    BT_ERROR_LIMIT_REACHED                   = 0x43,
    BT_ERROR_OPERATION_CANCELED_BY_HOST      = 0x44,
    BT_ERROR_PKT_TOO_LONG                    = 0x45,

    BT_ERROR_UNDEFINED                       = 0xFF,
};

enum bt_hl_err
{
    /// No error
    BT_GAP_ERR_NO_ERROR                                                               = 0x00,

    // ----------------------------------------------------------------------------------
    // -------------------------  ATT Specific Error ------------------------------------
    // ----------------------------------------------------------------------------------
    /// No error
    BT_ATT_ERR_NO_ERROR                                                               = 0x00,
    /// 0x01: H_andle is invalid
    BT_ATT_ERR_INVALID_HANDLE                                                         = 0x01,
    /// 0x02: Read permission disabled
    BT_ATT_ERR_READ_NOT_PERMITTED                                                     = 0x02,
    /// 0x03: Write permission disabled
    BT_ATT_ERR_WRITE_NOT_PERMITTED                                                    = 0x03,
    /// 0x04: Incorrect PDU
    BT_ATT_ERR_INVALID_PDU                                                            = 0x04,
    /// 0x05: Authentication privilege not enough
    BT_ATT_ERR_INSUFF_AUTHEN                                                          = 0x05,
    /// 0x06: Request not supported or not understood
    BT_ATT_ERR_REQUEST_NOT_SUPPORTED                                                  = 0x06,
    /// 0x07: Incorrect offset value
    BT_ATT_ERR_INVALID_OFFSET                                                         = 0x07,
    /// 0x08: Authorization privilege not enough
    BT_ATT_ERR_INSUFF_AUTHOR                                                          = 0x08,
    /// 0x09: Capacity queue for reliable write reached
    BT_ATT_ERR_PREPARE_QUEUE_FULL                                                     = 0x09,
    /// 0x0A: Attribute requested not existing
    BT_ATT_ERR_ATTRIBUTE_NOT_FOUND                                                    = 0x0A,
    /// 0x0B: Attribute requested not long
    BT_ATT_ERR_ATTRIBUTE_NOT_LONG                                                     = 0x0B,
    /// 0x0C: Encryption size not sufficient
    BT_ATT_ERR_INSUFF_ENC_KEY_SIZE                                                    = 0x0C,
    /// 0x0D: Invalid length of the attribute value
    BT_ATT_ERR_INVALID_ATTRIBUTE_VAL_LEN                                              = 0x0D,
    /// 0x0E: Operation not fit to condition
    BT_ATT_ERR_UNLIKELY_ERR                                                           = 0x0E,
    /// 0x0F: Attribute requires encryption before operation
    BT_ATT_ERR_INSUFF_ENC                                                             = 0x0F,
    /// 0x10: Attribute grouping not supported
    BT_ATT_ERR_UNSUPP_GRP_TYPE                                                        = 0x10,
    /// 0x11: Resources not sufficient to complete the request
    BT_ATT_ERR_INSUFF_RESOURCE                                                        = 0x11,
    /// 0x12: The server requests the client to rediscover the database.
    BT_ATT_ERR_DB_OUT_OF_SYNC                                                         = 0x12,
    /// 0x13: The attribute parameter value was not allowed.
    BT_ATT_ERR_VALUE_NOT_ALLOWED                                                      = 0x13,
    /// 0x80: Application error (also used in PRF Errors)
    BT_ATT_ERR_APP_ERROR                                                              = 0x80,

    // ----------------------------------------------------------------------------------
    // -------------------------- SDP Specific Error ------------------------------------
    // ----------------------------------------------------------------------------------
    /// Invalid/unsupported SDP version
    BT_SDP_ERR_INVALID_VERSION                                                        = 0x16,
    /// Invalid Service Record Handle
    BT_SDP_ERR_INVALID_SERVICE_HANDLE                                                 = 0x17,
    /// Invalid request syntax
    BT_SDP_ERR_INVALID_REQUEST_SYNTAX                                                 = 0x18,
    /// Invalid PDU Size
    BT_SDP_ERR_INVALID_PDU_SIZE                                                       = 0x19,
    /// Invalid Continuation State
    BT_SDP_ERR_INVALID_CONTINUE_STATE                                                 = 0x1A,

    // ----------------------------------------------------------------------------------
    // -------------------------- RFCOMM Specific Error ---------------------------------
    // ----------------------------------------------------------------------------------
    /// Invalid Service Record Handle
    BT_RFC_ERR_FLOW_CONTROL                                                           = 0x1B,
    /// Handsfree profile error
    BT_RFC_HFP_ERROR                                                                  = 0x1C,
    /// Handsfree profile error
    BT_RFC_LINK_ERROR                                                                 = 0x1D,

    // ----------------------------------------------------------------------------------
    // ------------------------- L2CAP Specific Error -----------------------------------
    // ----------------------------------------------------------------------------------
    /// Pending
    BT_L2CAP_ERR_PENDING                                                              = 0x2F,
    /// Message cannot be sent because connection lost. (disconnected)
    BT_L2CAP_ERR_CONNECTION_LOST                                                      = 0x30,
    /// MTU size exceed or invalid MTU proposed
    BT_L2CAP_ERR_INVALID_MTU                                                          = 0x31,
    /// MPS size exceed or invalid MPS proposed
    BT_L2CAP_ERR_INVALID_MPS                                                          = 0x32,
    /// Invalid Channel ID
    BT_L2CAP_ERR_INVALID_CID                                                          = 0x33,
    /// Invalid PDU
    BT_L2CAP_ERR_INVALID_PDU                                                          = 0x34,
    /// Connection refused - unacceptable parameters
    BT_L2CAP_ERR_UNACCEPTABLE_PARAM                                                   = 0x35,
    /// Connection refused - insufficient authentication
    BT_L2CAP_ERR_INSUFF_AUTHEN                                                        = 0x36,
    /// Connection refused - insufficient authorization
    BT_L2CAP_ERR_INSUFF_AUTHOR                                                        = 0x37,
    /// Connection refused - insufficient encryption key size
    BT_L2CAP_ERR_INSUFF_ENC_KEY_SIZE                                                  = 0x38,
    /// Connection Refused - insufficient encryption
    BT_L2CAP_ERR_INSUFF_ENC                                                           = 0x39,
    /// Connection refused - SPSM not supported
    BT_L2CAP_ERR_SPSM_NOT_SUPP                                                        = 0x3A,
    /// No more credit
    BT_L2CAP_ERR_INSUFF_CREDIT                                                        = 0x3B,
    /// Command not understood by peer device
    BT_L2CAP_ERR_NOT_UNDERSTOOD                                                       = 0x3C,
    /// Credit error, invalid number of credit received
    BT_L2CAP_ERR_CREDIT_ERROR                                                         = 0x3D,
    /// Channel identifier already allocated
    BT_L2CAP_ERR_CID_ALREADY_ALLOC                                                    = 0x3E,
    /// Unknown PDU
    BT_L2CAP_ERR_UNKNOWN_PDU                                                          = 0x3F,


    // ----------------------------------------------------------------------------------
    // -------------------------- GAP Specific Error ------------------------------------
    // ----------------------------------------------------------------------------------
    /// Invalid parameters set
    BT_GAP_ERR_INVALID_PARAM                                                          = 0x40,
    /// Problem with protocol exchange, get unexpected response
    BT_GAP_ERR_PROTOCOL_PROBLEM                                                       = 0x41,
    /// Request not supported by software configuration
    BT_GAP_ERR_NOT_SUPPORTED                                                          = 0x42,
    /// Request not allowed in current state.
    BT_GAP_ERR_COMMAND_DISALLOWED                                                     = 0x43,
    /// Requested operation canceled.
    BT_GAP_ERR_CANCELED                                                               = 0x44,
    /// Requested operation timeout.
    BT_GAP_ERR_TIMEOUT                                                                = 0x45,
    /// Link connection lost during operation.
    BT_GAP_ERR_DISCONNECTED                                                           = 0x46,
    /// Search algorithm finished, but no result found
    BT_GAP_ERR_NOT_FOUND                                                              = 0x47,
    /// Request rejected by peer device
    BT_GAP_ERR_REJECTED                                                               = 0x48,
    /// Problem with privacy configuration
    BT_GAP_ERR_PRIVACY_CFG_PB                                                         = 0x49,
    /// Duplicate or invalid advertising data
    BT_GAP_ERR_ADV_DATA_INVALID                                                       = 0x4A,
    /// Insufficient resources
    BT_GAP_ERR_INSUFF_RESOURCES                                                       = 0x4B,
    /// Unexpected Error
    BT_GAP_ERR_UNEXPECTED                                                             = 0x4C,
    /// Feature mismatch
    BT_GAP_ERR_MISMATCH                                                               = 0x4D,
    /// Buffer cannot be used due to invalid header or tail length
    BT_GAP_ERR_INVALID_BUFFER                                                         = 0x4E,
    /// busy, last command haven't finished
    BT_GAP_ERR_BUSY                                                                   = 0x4F,

    // ----------------------------------------------------------------------------------
    // ------------------------- GATT Specific Error ------------------------------------
    // ----------------------------------------------------------------------------------
    /// Problem with ATTC protocol response
    BT_GATT_ERR_INVALID_ATT_LEN                                                       = 0x50,
    /// Error in service search
    BT_GATT_ERR_INVALID_TYPE_IN_SVC_SEARCH                                            = 0x51,
    /// Invalid write data
    BT_GATT_ERR_WRITE                                                                 = 0x52,
    /// Signed write error
    BT_GATT_ERR_SIGNED_WRITE                                                          = 0x53,
    /// No attribute client defined
    BT_GATT_ERR_ATTRIBUTE_CLIENT_MISSING                                              = 0x54,
    /// No attribute server defined
    BT_GATT_ERR_ATTRIBUTE_SERVER_MISSING                                              = 0x55,
    /// Permission set in service/attribute are invalid
    BT_GATT_ERR_INVALID_PERM                                                          = 0x56,
    /// The Attribute bearer is closed
    BT_GATT_ERR_ATT_BEARER_CLOSE                                                      = 0x57,
    /// No more Attribute bearer available
    BT_GATT_ERR_NO_MORE_BEARER                                                        = 0x58,

    // ----------------------------------------------------------------------------------
    // ------------------------- SMP Specific Error -------------------------------------
    // ----------------------------------------------------------------------------------
    // SMP Protocol Errors detected on local device
    /// The user input of pass key failed, for example, the user canceled the operation.
    BT_SMP_ERR_LOC_PASSKEY_ENTRY_FAILED                                               = 0x61,
    /// The OOB Data is not available.
    BT_SMP_ERR_LOC_OOB_NOT_AVAILABLE                                                  = 0x62,
    /// The pairing procedure cannot be performed as authentication requirements cannot be met
    /// due to IO capabilities of one or both devices.
    BT_SMP_ERR_LOC_AUTH_REQ                                                           = 0x63,
    /// The confirm value does not match the calculated confirm value.
    BT_SMP_ERR_LOC_CONF_VAL_FAILED                                                    = 0x64,
    /// Pairing is not supported by the device.
    BT_SMP_ERR_LOC_PAIRING_NOT_SUPP                                                   = 0x65,
    /// The resultant encryption key size is insufficient for the security requirements of
    /// this device.
    BT_SMP_ERR_LOC_ENC_KEY_SIZE                                                       = 0x66,
    /// The SMP command received is not supported on this device.
    BT_SMP_ERR_LOC_CMD_NOT_SUPPORTED                                                  = 0x67,
    /// Pairing failed due to an unspecified reason.
    BT_SMP_ERR_LOC_UNSPECIFIED_REASON                                                 = 0x68,
    /// Pairing or Authentication procedure is disallowed because too little time has elapsed
    /// since last pairing request or security request.
    BT_SMP_ERR_LOC_REPEATED_ATTEMPTS                                                  = 0x69,
    /// The command length is invalid or a parameter is outside of the specified range.
    BT_SMP_ERR_LOC_INVALID_PARAM                                                      = 0x6A,
    /// Indicates to the remote device that the DHKey Check value received doesn't
    /// match the one calculated by the local device.
    BT_SMP_ERR_LOC_DHKEY_CHECK_FAILED                                                 = 0x6B,
    /// Indicates that the confirm values in the numeric comparison protocol do not match.
    BT_SMP_ERR_LOC_NUMERIC_COMPARISON_FAILED                                          = 0x6C,
    /// Indicates that the pairing over the LE transport failed due to a Pairing Request sent
    /// over the BR/EDR transport in process.
    BT_SMP_ERR_LOC_BREDR_PAIRING_IN_PROGRESS                                          = 0x6D,
    /// Indicates that the BR/EDR Link Key generated on the BR/EDR transport cannot be
    /// used to derive and distribute keys for the LE transport.
    BT_SMP_ERR_LOC_CROSS_TRANSPORT_KEY_GENERATION_NOT_ALLOWED                         = 0x6E,
    // SMP Protocol Errors detected by remote device
    /// The user input of passkey failed, for example, the user canceled the operation.
    BT_SMP_ERR_REM_PASSKEY_ENTRY_FAILED                                               = 0x71,
    /// The OOB Data is not available.
    BT_SMP_ERR_REM_OOB_NOT_AVAILABLE                                                  = 0x72,
    /// The pairing procedure cannot be performed as authentication requirements cannot be
    /// met due to IO capabilities of one or both devices.
    BT_SMP_ERR_REM_AUTH_REQ                                                           = 0x73,
    /// The confirm value does not match the calculated confirm value.
    BT_SMP_ERR_REM_CONF_VAL_FAILED                                                    = 0x74,
    /// Pairing is not supported by the device.
    BT_SMP_ERR_REM_PAIRING_NOT_SUPP                                                   = 0x75,
    /// The resultant encryption key size is insufficient for the security requirements of
    /// this device.
    BT_SMP_ERR_REM_ENC_KEY_SIZE                                                       = 0x76,
    /// The SMP command received is not supported on this device.
    BT_SMP_ERR_REM_CMD_NOT_SUPPORTED                                                  = 0x77,
    /// Pairing failed due to an unspecified reason.
    BT_SMP_ERR_REM_UNSPECIFIED_REASON                                                 = 0x78,
    /// Pairing or Authentication procedure is disallowed because too little time has elapsed
    /// since last pairing request or security request.
    BT_SMP_ERR_REM_REPEATED_ATTEMPTS                                                  = 0x79,
    /// The command length is invalid or a parameter is outside of the specified range.
    BT_SMP_ERR_REM_INVALID_PARAM                                                      = 0x7A,
    /// Indicates to the remote device that the DHKey Check value received doesn't
    /// match the one calculated by the local device.
    BT_SMP_ERR_REM_DHKEY_CHECK_FAILED                                                 = 0x7B,
    /// Indicates that the confirm values in the numeric comparison protocol do not match.
    BT_SMP_ERR_REM_NUMERIC_COMPARISON_FAILED                                          = 0x7C,
    /// Indicates that the pairing over the LE transport failed due to a Pairing Request sent
    /// over the BR/EDR transport in process.
    BT_SMP_ERR_REM_BREDR_PAIRING_IN_PROGRESS                                          = 0x7D,
    /// Indicates that the BR/EDR Link Key generated on the BR/EDR transport cannot be
    /// used to derive and distribute keys for the LE transport.
    BT_SMP_ERR_REM_CROSS_TRANSPORT_KEY_GENERATION_NOT_ALLOWED                         = 0x7E,
    // SMP Errors triggered by local device
    /// The provided resolvable address has not been resolved.
    BT_SMP_ERR_ADDR_RESOLV_FAIL                                                       = 0x20,
    /// The Signature Verification Failed
    BT_SMP_ERR_SIGN_VERIF_FAIL                                                        = 0x21,
    /// The encryption procedure failed because the slave device didn't find the LTK
    /// needed to start an encryption session.
    BT_SMP_ERR_ENC_KEY_MISSING                                                        = 0x22,
    /// The encryption procedure failed because the slave device doesn't support the
    /// encryption feature.
    BT_SMP_ERR_ENC_NOT_SUPPORTED                                                      = 0x23,
    /// A timeout has occurred during the start encryption session.
    BT_SMP_ERR_ENC_TIMEOUT                                                            = 0x24,

    // ----------------------------------------------------------------------------------
    //------------------------ Profiles specific error codes ----------------------------
    // ----------------------------------------------------------------------------------
    /// Application Error
    BT_PRF_APP_ERROR                                                                  = 0x80,
    /// Invalid parameter in request
    BT_PRF_ERR_INVALID_PARAM                                                          = 0x81,
    /// Inexistent handle for sending a read/write characteristic request
    BT_PRF_ERR_INEXISTENT_HDL                                                         = 0x82,
    /// Discovery stopped due to missing attribute according to specification
    BT_PRF_ERR_STOP_DISC_CHAR_MISSING                                                 = 0x83,
    /// Too many SVC instances found -> protocol violation
    BT_PRF_ERR_MULTIPLE_SVC                                                           = 0x84,
    /// Discovery stopped due to found attribute with incorrect properties
    BT_PRF_ERR_STOP_DISC_WRONG_CHAR_PROP                                              = 0x85,
    /// Too many Char. instances found-> protocol violation
    BT_PRF_ERR_MULTIPLE_CHAR                                                          = 0x86,
    /// Attribute write not allowed
    BT_PRF_ERR_NOT_WRITABLE                                                           = 0x87,
    /// Attribute read not allowed
    BT_PRF_ERR_NOT_READABLE                                                           = 0x88,
    /// Request not allowed
    BT_PRF_ERR_REQ_DISALLOWED                                                         = 0x89,
    /// Notification Not Enabled
    BT_PRF_ERR_NTF_DISABLED                                                           = 0x8A,
    /// Indication Not Enabled
    BT_PRF_ERR_IND_DISABLED                                                           = 0x8B,
    /// Feature not supported by profile
    BT_PRF_ERR_FEATURE_NOT_SUPPORTED                                                  = 0x8C,
    /// Read value has an unexpected length
    BT_PRF_ERR_UNEXPECTED_LEN                                                         = 0x8D,
    /// Disconnection occurs
    BT_PRF_ERR_DISCONNECTED                                                           = 0x8E,
    /// Procedure Timeout
    BT_PRF_ERR_PROC_TIMEOUT                                                           = 0x8F,
    ///write request rejected
    BT_PRF_WRITE_REQ_REJECTED                                                         = 0xFC,
    /// Client characteristic configuration improperly configured
    BT_PRF_CCCD_IMPR_CONFIGURED                                                       = 0xFD,
    /// Procedure already in progress
    BT_PRF_PROC_IN_PROGRESS                                                           = 0xFE,
    /// Out of Range
    BT_PRF_OUT_OF_RANGE                                                               = 0xFF,

    // ----------------------------------------------------------------------------------
    //-------------------- LL Error codes conveyed to upper layer -----------------------
    // ----------------------------------------------------------------------------------
    /// Unknown HCI Command
    BT_CTRL_ERR_UNKNOWN_HCI_COMMAND                                                     = 0x91,
    /// Unknown Connection Identifier
    BT_CTRL_ERR_UNKNOWN_CONNECTION_ID                                                   = 0x92,
    /// Hardware Failure
    BT_CTRL_ERR_HARDWARE_FAILURE                                                        = 0x93,
    /// BT Page Timeout
    BT_CTRL_ERR_PAGE_TIMEOUT                                                            = 0x94,
    /// Authentication failure
    BT_CTRL_ERR_AUTH_FAILURE                                                            = 0x95,
    /// Pin code missing
    BT_CTRL_ERR_PIN_MISSING                                                             = 0x96,
    /// Memory capacity exceed
    BT_CTRL_ERR_MEMORY_CAPA_EXCEED                                                      = 0x97,
    /// Connection Timeout
    BT_CTRL_ERR_CON_TIMEOUT                                                             = 0x98,
    /// Connection limit Exceed
    BT_CTRL_ERR_CON_LIMIT_EXCEED                                                        = 0x99,
    /// Synchronous Connection limit exceed
    BT_CTRL_ERR_SYNC_CON_LIMIT_DEV_EXCEED                                               = 0x9A,
    /// ACL Connection exits
    BT_CTRL_ERR_ACL_CON_EXISTS                                                          = 0x9B,
    /// Command Disallowed
    BT_CTRL_ERR_COMMAND_DISALLOWED                                                      = 0x9C,
    /// Connection rejected due to limited resources
    BT_CTRL_ERR_CONN_REJ_LIMITED_RESOURCES                                              = 0x9D,
    /// Connection rejected due to security reason
    BT_CTRL_ERR_CONN_REJ_SECURITY_REASONS                                               = 0x9E,
    /// Connection rejected due to unacceptable BD Addr
    BT_CTRL_ERR_CONN_REJ_UNACCEPTABLE_BDADDR                                            = 0x9F,
    /// Connection rejected due to Accept connection timeout
    BT_CTRL_ERR_CONN_ACCEPT_TIMEOUT_EXCEED                                              = 0xA0,
    /// Not Supported
    BT_CTRL_ERR_UNSUPPORTED                                                             = 0xA1,
    /// invalid parameters
    BT_CTRL_ERR_INVALID_HCI_PARAM                                                       = 0xA2,
    /// Remote user terminate connection
    BT_CTRL_ERR_REMOTE_USER_TERM_CON                                                    = 0xA3,
    /// Remote device terminate connection due to low resources
    BT_CTRL_ERR_REMOTE_DEV_TERM_LOW_RESOURCES                                           = 0xA4,
    /// Remote device terminate connection due to power off
    BT_CTRL_ERR_REMOTE_DEV_POWER_OFF                                                    = 0xA5,
    /// Connection terminated by local host
    BT_CTRL_ERR_CON_TERM_BY_LOCAL_HOST                                                  = 0xA6,
    /// Repeated attempts
    BT_CTRL_ERR_REPEATED_ATTEMPTS                                                       = 0xA7,
    /// Pairing not Allowed
    BT_CTRL_ERR_PAIRING_NOT_ALLOWED                                                     = 0xA8,
    /// Unknown PDU Error
    BT_CTRL_ERR_UNKNOWN_LMP_PDU                                                         = 0xA9,
    /// Unsupported remote feature
    BT_CTRL_ERR_UNSUPPORTED_REMOTE_FEATURE                                              = 0xAA,
    /// Sco Offset rejected
    BT_CTRL_ERR_SCO_OFFSET_REJECTED                                                     = 0xAB,
    /// SCO Interval Rejected
    BT_CTRL_ERR_SCO_INTERVAL_REJECTED                                                   = 0xAC,
    /// SCO air mode Rejected
    BT_CTRL_ERR_SCO_AIR_MODE_REJECTED                                                   = 0xAD,
    /// Invalid LMP parameters
    BT_CTRL_ERR_INVALID_LMP_PARAM                                                       = 0xAE,
    /// Unspecified error
    BT_CTRL_ERR_UNSPECIFIED_ERROR                                                       = 0xAF,
    /// Unsupported LMP Parameter value
    BT_CTRL_ERR_UNSUPPORTED_LMP_PARAM_VALUE                                             = 0xB0,
    /// Role Change Not allowed
    BT_CTRL_ERR_ROLE_CHANGE_NOT_ALLOWED                                                 = 0xB1,
    /// LMP Response timeout
    BT_CTRL_ERR_LMP_RSP_TIMEOUT                                                         = 0xB2,
    /// LMP Collision
    BT_CTRL_ERR_LMP_COLLISION                                                           = 0xB3,
    /// LMP Pdu not allowed
    BT_CTRL_ERR_LMP_PDU_NOT_ALLOWED                                                     = 0xB4,
    /// Encryption mode not accepted
    BT_CTRL_ERR_ENC_MODE_NOT_ACCEPT                                                     = 0xB5,
    /// Link Key Cannot be changed
    BT_CTRL_ERR_LINK_KEY_CANT_CHANGE                                                    = 0xB6,
    /// Quality of Service not supported
    BT_CTRL_ERR_QOS_NOT_SUPPORTED                                                       = 0xB7,
    /// Error, instant passed
    BT_CTRL_ERR_INSTANT_PASSED                                                          = 0xB8,
    /// Pairing with unit key not supported
    BT_CTRL_ERR_PAIRING_WITH_UNIT_KEY_NOT_SUP                                           = 0xB9,
    /// Transaction collision
    BT_CTRL_ERR_DIFF_TRANSACTION_COLLISION                                              = 0xBA,
    /// Unacceptable parameters
    BT_CTRL_ERR_QOS_UNACCEPTABLE_PARAM                                                  = 0xBC,
    /// Quality of Service rejected
    BT_CTRL_ERR_QOS_REJECTED                                                            = 0xBD,
    /// Channel class not supported
    BT_CTRL_ERR_CHANNEL_CLASS_NOT_SUP                                                   = 0xBE,
    /// Insufficient security
    BT_CTRL_ERR_INSUFFICIENT_SECURITY                                                   = 0xBF,
    /// Parameters out of mandatory range
    BT_CTRL_ERR_PARAM_OUT_OF_MAND_RANGE                                                 = 0xC0,
    /// Role switch pending
    BT_CTRL_ERR_ROLE_SWITCH_PEND                                                        = 0xC2,
    /// Reserved slot violation
    BT_CTRL_ERR_RESERVED_SLOT_VIOLATION                                                 = 0xC4,
    /// Role Switch fail
    BT_CTRL_ERR_ROLE_SWITCH_FAIL                                                        = 0xC5,
    /// Error, EIR too large
    BT_CTRL_ERR_EIR_TOO_LARGE                                                           = 0xC6,
    /// Simple pairing not supported by host
    BT_CTRL_ERR_SP_NOT_SUPPORTED_HOST                                                   = 0xC7,
    /// Host pairing is busy
    BT_CTRL_ERR_HOST_BUSY_PAIRING                                                       = 0xC8,
    /// Controller is busy
    BT_CTRL_ERR_CONTROLLER_BUSY                                                         = 0xCA,
    /// Unacceptable connection initialization
    BT_CTRL_ERR_UNACCEPTABLE_CONN_INT                                                   = 0xCB,
    /// Direct Advertising Timeout
    BT_CTRL_ERR_DIRECT_ADV_TO                                                           = 0xCC,
    /// Connection Terminated due to a MIC failure
    BT_CTRL_ERR_TERMINATED_MIC_FAILURE                                                  = 0xCD,
    /// Connection failed to be established
    BT_CTRL_ERR_CONN_FAILED_TO_BE_EST                                                   = 0xCE,
    /// MAC Connection Failed
    BT_CTRL_ERR_MAC_CONN_FAILED                                                         = 0xCF,
    /// Coarse Clock Adjustment Rejected but Will Try to Adjust Using Clock Dragging
    BT_CTRL_ERR_CCA_REJ_USE_CLOCK_DRAG                                                  = 0xD0,
    /// Type0 Submap Not Defined
    BT_CTRL_ERR_TYPE0_SUBMAP_NOT_DEFINED                                                = 0xD1,
    /// Unknown Advertising Identifier
    BT_CTRL_ERR_UNKNOWN_ADVERTISING_ID                                                  = 0xD2,
    /// Limit Reached
    BT_CTRL_ERR_LIMIT_REACHED                                                           = 0xD3,
    /// Operation Cancelled by Host
    BT_CTRL_ERR_OPERATION_CANCELED_BY_HOST                                              = 0xD4,
    /// Packet Too Long
    BT_CTRL_ERR_PKT_TOO_LONG                                                            = 0xD5,

};

enum bt_sniff_mode
{
    BT_ACTIVE_MODE          = 0x00,
    BT_HOLD_MODE            = 0x01,
    BT_SNIFF_MODE           = 0x02,
    BT_PARK_MODE            = 0x03,
};

/// for get info
enum gap_dev_info_type
{
    GAP_INFO_BDADDR,
    GAP_INFO_NAME,
    GAP_INFO_APPEARANCE,
    GAP_INFO_VERSION,
    GAP_INFO_FETURES,
    GAP_INFO_RSSI,
    GAP_INFO_WHITE_LIST_SIZE,
    GAP_INFO_RAL_LIST_SIZE,
    GAP_INFO_PAL_LIST_SIZE,
};


/// IO Capability Values
enum gap_io_cap
{
    /// Display Only
    GAP_IO_CAP_DISPLAY_ONLY = 0x00,
    /// Display Yes No
    GAP_IO_CAP_DISPLAY_YES_NO,
    /// Keyboard Only
    GAP_IO_CAP_KB_ONLY,
    /// No Input No Output
    GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
    /// Keyboard Display
    GAP_IO_CAP_KB_DISPLAY,
    GAP_IO_CAP_LAST
};
/// GAP Advertising Flags
enum gap_adv_type
{
    /// Flag
    GAP_ADV_TYPE_FLAGS                      = 0x01,
    /// Use of more than 16 bits UUID
    GAP_ADV_TYPE_MORE_16_BIT_UUID           = 0x02,
    /// Complete list of 16 bit UUID
    GAP_ADV_TYPE_COMPLETE_LIST_16_BIT_UUID  = 0x03,
    /// Use of more than 32 bit UUD
    GAP_ADV_TYPE_MORE_32_BIT_UUID           = 0x04,
    /// Complete list of 32 bit UUID
    GAP_ADV_TYPE_COMPLETE_LIST_32_BIT_UUID  = 0x05,
    /// Use of more than 128 bit UUID
    GAP_ADV_TYPE_MORE_128_BIT_UUID          = 0x06,
    /// Complete list of 128 bit UUID
    GAP_ADV_TYPE_COMPLETE_LIST_128_BIT_UUID = 0x07,
    /// Shortened device name
    GAP_ADV_TYPE_SHORTENED_NAME             = 0x08,
    /// Complete device name
    GAP_ADV_TYPE_COMPLETE_NAME              = 0x09,
    /// Transmit power
    GAP_ADV_TYPE_TRANSMIT_POWER             = 0x0A,
    /// Class of device
    GAP_ADV_TYPE_CLASS_OF_DEVICE            = 0x0D,
    /// Simple Pairing Hash C
    GAP_ADV_TYPE_SP_HASH_C                  = 0x0E,
    /// Simple Pairing Randomizer
    GAP_ADV_TYPE_SP_RANDOMIZER_R            = 0x0F,
    /// Temporary key value
    GAP_ADV_TYPE_TK_VALUE                   = 0x10,
    /// Out of Band Flag
    GAP_ADV_TYPE_OOB_FLAGS                  = 0x11,
    /// Slave connection interval range
    GAP_ADV_TYPE_SLAVE_CONN_INT_RANGE       = 0x12,
    /// Require 16 bit service UUID
    GAP_ADV_TYPE_RQRD_16_BIT_SVC_UUID       = 0x14,
    /// Require 32 bit service UUID
    GAP_ADV_TYPE_RQRD_32_BIT_SVC_UUID       = 0x1F,
    /// Require 128 bit service UUID
    GAP_ADV_TYPE_RQRD_128_BIT_SVC_UUID      = 0x15,
    /// Service data 16-bit UUID
    GAP_ADV_TYPE_SERVICE_16_BIT_DATA        = 0x16,
    /// Service data 32-bit UUID
    GAP_ADV_TYPE_SERVICE_32_BIT_DATA        = 0x20,
    /// Service data 128-bit UUID
    GAP_ADV_TYPE_SERVICE_128_BIT_DATA       = 0x21,
    /// Public Target Address
    GAP_ADV_TYPE_PUB_TGT_ADDR               = 0x17,
    /// Random Target Address
    GAP_ADV_TYPE_RAND_TGT_ADDR              = 0x18,
    /// Appearance
    GAP_ADV_TYPE_APPEARANCE                 = 0x19,
    /// Advertising Interval
    GAP_ADV_TYPE_ADV_INTV                   = 0x1A,
    /// LE Bluetooth Device Address
    GAP_ADV_TYPE_LE_BT_ADDR                 = 0x1B,
    /// LE Role
    GAP_ADV_TYPE_LE_ROLE                    = 0x1C,
    /// Simple Pairing Hash C-256
    GAP_ADV_TYPE_SPAIR_HASH                 = 0x1D,
    /// Simple Pairing Randomizer R-256
    GAP_ADV_TYPE_SPAIR_RAND                 = 0x1E,
    /// List of 32-bit Service Solicitation UUIDs
    GAP_ADV_TYPE_LIST_UUID                  = 0x1F,
    /// Service Data - 32-bit UUID
    GAP_ADV_TYPE_SVC_32UUID                 = 0x20,
    /// Service Data - 128-bit UUID
    GAP_ADV_TYPE_SVC_128UUID                = 0x21,
    /// LE Secure Connections Confirmation Value
    GAP_ADV_TYPE_SEC_CFM                    = 0x22,
    /// LE Secure Connections Random Value
    GAP_ADV_TYPE_SEC_RAND                   = 0x23,
    /// URI
    GAP_ADV_TYPE_URI                        = 0x24,
    /// Indoor Positioning
    GAP_ADV_TYPE_INDOOR_POS                 = 0x25,
    /// Transport Discovery Data
    GAP_ADV_TYPE_TRANS_DISC                 = 0x26,
    /// LE Supported Features
    GAP_ADV_TYPE_LE_FEATS                   = 0x27,
    /// Channel Map Update Indication
    GAP_ADV_TYPE_MAP_IND                    = 0x28,
    /// Mesh profile PB-ADV
    GAP_ADV_TYPE_PB_ADV                     = 0x29,
    /// Mesh Message
    GAP_ADV_TYPE_MESH_MSG                   = 0x2A,
    /// Mesh Beacon
    GAP_ADV_TYPE_MESH_BEACON                = 0x2B,
    /// BIGInfo
    GAP_ADV_TYPE_BIG_INFO                   = 0x2C,
    /// Broadcast_Code
    GAP_ADV_TYPE_BROADCAST_CODE             = 0x2D,
    /// Resolvable Set Identifier
    GAP_ADV_TYPE_RSI                        = 0x2E,
    /// Advertising Interval - long
    GAP_ADV_TYPE_ADV_INTV_LONG              = 0x2F,
    /// Broadcast name
    GAP_ADV_TYPE_BROADCAST_NAME             = 0x30,
    /// 3D Information Data
    GAP_ADV_TYPE_3D_INFO                    = 0x3D,

    /// user private date
    GAP_ADV_TYPE_USER_PRIVATE               = 0xE0,

    /// Manufacturer specific data
    GAP_ADV_TYPE_MANU_SPECIFIC_DATA         = 0xFF,
};

/// Appearance type
enum gap_appearance_type
{
    GAP_APP_UNKNOWN                    =  0,
    GAP_APP_GENERIC_PHONE              =  64,
    GAP_APP_GENERIC_COMPUTER           =  128,
    GAP_APP_GENERIC_WATCH              =  192,
    GAP_APP_SPORTS_WATCH               =  193,
    GAP_APP_GENERIC_CLOCK              =  256,
    GAP_APP_GENERIC_DISPLAY            =  320,
    GAP_APP_GENERIC_REMOTE_CONTROL     =  384,
    GAP_APP_GENERIC_EYE_GLASSES        =  448,
    GAP_APP_GENERIC_TAG                =  512,
    GAP_APP_GENERIC_KEYRING            =  576,
    GAP_APP_GENERIC_MEDIA_PLAYER       =  640,
    GAP_APP_GENERIC_BARCODE_SCANNER    =  704,
    GAP_APP_GENERIC_THERMOMETER        =  768,
    GAP_APP_EAR_THERMOMETER            =  769,
    GAP_APP_GENERIC_HEART_RATE_SENSOR  =  832,
    GAP_APP_HEART_RATE_BELT            =  833,
    GAP_APP_GENERIC_BLOOD_PRESSURE     =  896,
    GAP_APP_BLOOD_PRESSURE_ARM         =  897,
    GAP_APP_BLOOD_PRESSURE_WRIST       =  898,
    GAP_APP_HUMAN_INTERFACE_DEVICE     =  960,
    GAP_APP_HID_KEYBOARD               =  961,
    GAP_APP_HID_MOUSE                  =  962,
    GAP_APP_HID_JOYSTICK               =  963,
    GAP_APP_HID_GAMEPAD                =  964,
    GAP_APP_HID_DIGITIZER_TABLET       =  965,
    GAP_APP_HID_CARD_READER            =  966,
    GAP_APP_HID_DIGITAL_PEN            =  967,
    GAP_APP_HID_BARCODE_SCANNER        =  968,
    GAP_APP_GLUCOSE_METER              =  1024,
    GAP_APP_RUNNING_WALKING_SENSOR     =  1088,
    GAP_APP_RUNNING_WALKING_IN_SHOE    =  1089,
    GAP_APP_RUNNING_WALKING_ON_SHOE    =  1090,
    GAP_APP_RUNNING_WALKING_ON_HIP     =  1091,
    GAP_APP_CYCLING                    =  1152,
    GAP_APP_CYCLING_COMPUTER           =  1153,
    GAP_APP_CYCLING_SPEED_SENSOR       =  1154,
    GAP_APP_CYCLING_CADENCE_SENSOR     =  1155,
    GAP_APP_CYCLING_POWER_SENSOR       =  1156,
    GAP_APP_CYCLING_SPEED_AND_CADENCE  =  1157,
    GAP_APP_GENERIC_PULSE_OXIMETER     =  3136,
    GAP_APP_FINGERTIP_PULSE_OXIMETER   =  3137,
    GAP_APP_WRISTWORN_PULSE_OXIMETER   =  3138,
    GAP_APP_GENERIC_OUTDOOR_SPORTS     =  5184,
    GAP_APP_LOCDISPDEV_OUTDOOR_SPORTS  =  5185,
    GAP_APP_LOCNAVIDEV_OUTDOOR_SPORTS  =  5186,
    GAP_APP_LOCPOD_OUTDOOR_SPORTS      =  5187,
    GAP_APP_LOCNAVIPOD_OUTDOOR_SPORTS  =  5188,
};
/// class bt of device type
enum gap_class_bt_type
{
    /// bt handfree
    GAP_APP_HANDSFREE                  = 0x200408,
    /// bt speaker
    GAP_APP_SPEAKER                    = 0x200414,
    /// LE headphones
    GAP_APP_LE_HEADPHONES              = 0x204418,
    /// LE audio HIFI
    GAP_APP_LE_HIFI                    = 0x204428,
    /// bt helmet
    GAP_APP_HELMET                     = 0x200710,
    /// bt remote controller
    GAP_APP_REMOTE_CONTROL             = 0x28054c,
    /// bt joystick with audio
    GAP_APP_JOYSTICK                   = 0x2805c4,
};

/// TK Type
enum gap_tk_type
{
    ///  TK get from out of band method
    GAP_TK_OOB         = 0x00,
    /// Displayed given TK in local device
    GAP_TK_DISPLAY,
    /// TK generated and shall be displayed by local device
    GAP_TK_GEN,
    /// TK shall be entered by user using device keyboard
    GAP_TK_KEY_ENTRY,
    /// display and compare the given TK
    GAP_TK_COMPARE,
};

/// Bit field use to select the preferred TX or RX LE PHY. 0 means no preferences
enum gap_phy
{
    /// No preferred PHY
    GAP_PHY_ANY               = 0x00,
    /// LE 1M PHY preferred for an active link
    GAP_PHY_LE_1MBPS          = (1 << 0),
    /// LE 2M PHY preferred for an active link
    GAP_PHY_LE_2MBPS          = (1 << 1),
    /// LE Coded PHY preferred for an active link
    GAP_PHY_LE_CODED          = (1 << 2),
};


/// Enumeration of TX/RX PHY values
enum gap_phy_val
{
    /// LE 1M PHY (TX or RX)
    GAP_PHY_1MBPS        = 1,
    /// LE 2M PHY (TX or RX)
    GAP_PHY_2MBPS        = 2,
    /// LE Coded PHY (RX Only)
    GAP_PHY_CODED        = 3,
    /// LE Coded PHY with S=8 data coding (TX Only)
    GAP_PHY_125KBPS      = 3,
    /// LE Coded PHY with S=2 data coding (TX Only)
    GAP_PHY_500KBPS      = 4,
};

///Advertising filter policy
enum gap_adv_filter_policy
{
    ///Allow both scan and connection requests from anyone
    GAP_ADV_SCAN_ANY_CON_ANY    = 0x00,
    ///Allow both scan req from White List devices only and connection req from anyone
    GAP_ADV_SCAN_WLST_CON_ANY,
    ///Allow both scan req from anyone and connection req from White List devices only
    GAP_ADV_SCAN_ANY_CON_WLST,
    ///Allow scan and connection requests from White List devices only
    GAP_ADV_SCAN_WLST_CON_WLST,
};

/// Generic Security key structure
typedef struct gap_sec_key
{
    /// Key value MSB -> LSB
    uint8_t key[GAP_KEY_LEN];
} gap_sec_key_t;

/// Bluetooth address
/*@TRACE*/
typedef struct gap_addr
{
    /// BD Address of device
    uint8_t addr[GAP_BD_ADDR_LEN];
} gap_addr_t;

/// Address information about a device address
/*@TRACE*/
typedef struct gap_bdaddr
{
    /// BD Address of device
    uint8_t addr[GAP_BD_ADDR_LEN];
    /// Address type of the device 0=public/1=private random
    uint8_t addr_type;
} gap_bdaddr_t;

typedef struct gap_rolv_info
{
    /// Identify address of device
    gap_bdaddr_t addr;
    /// 
    uint8_t irk[GAP_KEY_LEN];
} gap_rolv_info_t;

/// Periodic advertising address information
/*@TRACE*/
typedef struct gap_per_adv_bdaddr
{
    /// BD Address of device
    uint8_t addr[GAP_BD_ADDR_LEN];
    /// Address type of the device 0=public/1=private random
    uint8_t addr_type;
    /// Advertising SID
    uint8_t adv_sid;
} gap_per_adv_bdaddr_t;

/// Device name
/*@TRACE*/
struct gap_dev_name
{
    /// Length of provided value
    uint16_t value_length;
    /// name value starting from offset to maximum length
    uint8_t  value[__ARRAY_EMPTY];
};

struct gap_adv_report_data
{
    /// Length of provided value
    uint16_t value_length;
    /// name value starting from offset to maximum length
    uint8_t  value[__ARRAY_EMPTY];
};

/// Sniff mode
/*@TRACE*/
struct gap_sniff_mode
{
    /// Maximum interval (in slots)
    uint16_t    max_int;
    /// Minimum interval (in slots)
    uint16_t    min_int;
    /// Attempts (number of receive slots) (in slots)
    uint16_t    attempt;
    /// Timeout (number of receive slots) (in slots)
    uint16_t    timeout;
};
/// Sub Sniff mode
/*@TRACE*/
struct gap_sniff_sub
{
    /// Maximum latency used to calculate the maximum sniff subrate that the remote device may use (in slots)
    uint16_t    max_lat;
    /// Minimum base sniff subrate timeout that the remote device may use (in slots)
    uint16_t    min_rem_to;
    /// Minimum base sniff subrate timeout that the local device may use (in slots)
    uint16_t    min_loc_to;
};

/// Authentication mask
enum gap_auth_mask
{
    /// No Flag set
    GAP_AUTH_NONE    = 0,
    /// Bond authentication
    GAP_AUTH_BOND    = (1 << 0),
    /// Man In the middle protection
    GAP_AUTH_MITM    = (1 << 2),
    /// Secure Connection
    GAP_AUTH_SEC_CON = (1 << 3),
    /// Key Notification
    GAP_AUTH_KEY_NOTIF = (1 << 4),
    /// Support h7 function
    GAP_AUTH_CT2      = (1 << 5),
};

/// Security Link Level
enum gap_sec_lvl
{
    /// Service accessible through an un-encrypted link
    GAP_SEC_NOT_ENC             = 0,
    /// Service require an unauthenticated pairing (just work pairing)
    GAP_SEC_UNAUTH,
    /// Service require an authenticated pairing (Legacy pairing with pin code or OOB)
    GAP_SEC_AUTH,
    /// Service require a secure connection pairing
    GAP_SEC_SECURE_CON,
};

enum gapm_pairing_mode
{
    /// No pairing authorized
    GAPM_PAIRING_DISABLE  = 0,
    /// Legacy pairing Authorized
    GAPM_PAIRING_LEGACY   = (1 << 0),
    /// Secure Connection pairing Authorized
    GAPM_PAIRING_SEC_CON  = (1 << 1),
};

/// Type of advertising that can be created
enum gapm_adv_type
{
    /// Legacy advertising
    GAPM_ADV_TYPE_LEGACY = 0,
    /// Extended advertising
    GAPM_ADV_TYPE_EXTENDED,
    /// Periodic advertising
    GAPM_ADV_TYPE_PERIODIC,
};

/// Advertising discovery mode
enum gapm_adv_disc_mode
{
    /// Mode in non-discoverable
    GAPM_ADV_MODE_NON_DISC = 0,
    /// Mode in general discoverable
    GAPM_ADV_MODE_GEN_DISC,
    /// Mode in limited discoverable
    GAPM_ADV_MODE_LIM_DISC,
    /// Broadcast mode without presence of AD_TYPE_FLAG in advertising data
    GAPM_ADV_MODE_BEACON,
    GAPM_ADV_MODE_MAX,
};

/// Advertising properties bit field bit positions
enum gapm_adv_flag
{
    /// Indicate that advertising is connectable, reception of CONNECT_REQ or AUX_CONNECT_REQ
    /// PDUs is accepted. Not applicable for periodic advertising.
    GAPM_ADV_PROP_CONNECTABLE     = 0x0001,

    /// Indicate that advertising is scannable, reception of SCAN_REQ or AUX_SCAN_REQ PDUs is
    /// accepted
    GAPM_ADV_PROP_SCANNABLE       = 0x0002,
    /// Indicate that advertising targets a specific device. Only apply in following cases:
    ///   - Legacy advertising: if connectable
    ///   - Extended advertising: connectable or (non connectable and non discoverable)
    GAPM_ADV_PROP_DIRECTED        = 0x0004,

    /// Indicate that High Duty Cycle has to be used for advertising on primary channel
    /// Apply only if created advertising is not an extended advertising
    GAPM_ADV_PROP_HDC             = 0x0008,

    /// Enable anonymous mode. Device address won't appear in send PDUs
    /// Valid only if created advertising is an extended advertising
    GAPM_ADV_PROP_ANONYMOUS       = 0x0020,

    /// Include TX Power in the extended header of the advertising PDU.
    /// Valid only if created advertising is not a legacy advertising
    GAPM_ADV_PROP_TX_PWR          = 0x0040,

    /// Include TX Power in the periodic advertising PDU.
    /// Valid only if created advertising is a periodic advertising
    GAPM_ADV_PROP_PER_TX_PWR      = 0x0080,

    /// Indicate if application must be informed about received scan requests PDUs
    GAPM_ADV_PROP_SCAN_REQ_NTF_EN = 0x0100,
};

/// Advertising report information
enum ble_gapm_adv_report_info
{
    /// Report Type
        /// Extended advertising report
    BLE_GAPM_REPORT_TYPE_ADV_EXT = 0,
    /// Legacy advertising report
    BLE_GAPM_REPORT_TYPE_ADV_LEG,
    /// Extended scan response report
    BLE_GAPM_REPORT_TYPE_SCAN_RSP_EXT,
    /// Legacy scan response report
    BLE_GAPM_REPORT_TYPE_SCAN_RSP_LEG,
    /// Periodic advertising report
    BLE_GAPM_REPORT_TYPE_PER_ADV,
    
    BLE_GAPM_REPORT_INFO_REPORT_TYPE_MASK    = 0x07,
    /// Report is complete
    BLE_GAPM_REPORT_INFO_COMPLETE_BIT        = (1 << 3),
    /// Connectable advertising
    BLE_GAPM_REPORT_INFO_CONN_ADV_BIT        = (1 << 4),
    /// Scannable advertising
    BLE_GAPM_REPORT_INFO_SCAN_ADV_BIT        = (1 << 5),
    /// Directed advertising
    BLE_GAPM_REPORT_INFO_DIR_ADV_BIT         = (1 << 6),
};

/// Scanning Types
enum gapm_scan_type
{
    /// General discovery
    GAPM_SCAN_TYPE_GEN_DISC = 0,
    /// Limited discovery
    GAPM_SCAN_TYPE_LIM_DISC,
    /// Observer
    GAPM_SCAN_TYPE_OBSERVER,
    /// Selective observer
    GAPM_SCAN_TYPE_SEL_OBSERVER,
    /// Connectable discovery
    GAPM_SCAN_TYPE_CONN_DISC,
    /// Selective connectable discovery
    GAPM_SCAN_TYPE_SEL_CONN_DISC,
};

/// Scanning Types
enum ble_gapm_scan_filter
{
    /// Controller scan filter disable
    BLE_SCAN_FILTER_CONTROLLER_DIS = 0,
    /// Controller scan filter enable
    BLE_SCAN_FILTER_CONTROLLER_EN = 1,
    /// Controller scan filter enable
    BLE_SCAN_FILTER_CONTROLLER_DUR_EN = 2,

    BLE_SCAN_FILTER_CONTROLLER_MASK = 0x0f,
    
    BLE_SCAN_FILTER_HOST_ROUND_EN_BIT = (1 << 4),

};

/// Packet Payload type for test mode
enum gap_pkt_pld_type
{
    /// PRBS9 sequence "11111111100000111101..." (in transmission order)
    GAP_PKT_PLD_PRBS9,
    /// Repeated "11110000" (in transmission order)
    GAP_PKT_PLD_REPEATED_11110000,
    /// Repeated "10101010" (in transmission order)
    GAP_PKT_PLD_REPEATED_10101010,
    /// PRBS15 sequence
    GAP_PKT_PLD_PRBS15,
    /// Repeated "11111111" (in transmission order) sequence
    GAP_PKT_PLD_REPEATED_11111111,
    /// Repeated "00000000" (in transmission order) sequence
    GAP_PKT_PLD_REPEATED_00000000,
    /// Repeated "00001111" (in transmission order) sequence
    GAP_PKT_PLD_REPEATED_00001111,
    /// Repeated "01010101" (in transmission order) sequence
    GAP_PKT_PLD_REPEATED_01010101,
};


/// Modulation index
enum gap_modulation_idx
{
    /// Assume transmitter will have a standard modulation index
    GAP_MODULATION_STANDARD,
    /// Assume transmitter will have a stable modulation index
    GAP_MODULATION_STABLE,
};

/// Bit field of enabled advertising reports
enum gapm_report_en_bf
{
    /// Periodic advertising reports reception enabled
    GAPM_REPORT_ADV_EN_BIT      = 0x01,
    GAPM_REPORT_ADV_EN_POS      = 0,
    /// BIG Info advertising reports reception enabled
    GAPM_REPORT_BIGINFO_EN_BIT  = 0x02,
    GAPM_REPORT_BIGINFO_EN_POS  = 1,
};

/// Periodic synchronization types
enum gapm_per_sync_type
{
    /// Do not use periodic advertiser list for synchronization. Use advertiser information provided
    /// in the GAPM_ACTIVITY_START_CMD.
    GAPM_PER_SYNC_TYPE_GENERAL = 0,
    /// Use periodic advertiser list for synchronization
    GAPM_PER_SYNC_TYPE_SELECTIVE,
    /// Use Periodic advertising sync transfer information send through connection for synchronization
    GAPM_PER_SYNC_TYPE_PAST,
};

/// bt Discoverability modes
enum gapm_discover_mode
{
    /// can not be discovered
    GAPM_NON_DISCOVERABLE     = 0,
    /// can be discovered, default general discoverable
    GAPM_DISCOVERABLE         = 1,
    /// general discoverable
    GAPM_GEN_DISCOVERABLE     = 1,
    /// limited discoverable, include general discoverable
    GAPM_LIM_DISCOVERABLE     = 2,
};

/// bt Connectability modes
enum gapm_connect_mode
{
    /// can not be connected
    GAPM_NON_CONNECTABLE = 0,
    /// can be connected
    GAPM_CONNECTABLE,
};

enum gapm_actv_type
{
    /// Advertising activity
    GAPM_ACTV_TYPE_ADV = 0,
    /// Scanning activity
    GAPM_ACTV_TYPE_SCAN,
    /// Initiating activity
    GAPM_ACTV_TYPE_INIT,
    /// Periodic synchronization activity
    GAPM_ACTV_TYPE_PER_SYNC,
	/// BR/EDR device discovery
	GAPM_ACTV_TYPE_DISCOVERY,
	/// BT/EDR device connect
	GAPM_ACTV_TYPE_CONNECT,
};

enum gap_actv_type
{
    GAP_TYPE_GAPM                                    = 0x00,
    /// Set content of resolving list
    GAP_TYPE_GAPC                                    = 0x01,
};

enum gap_gapm_actv_type
{
    /// Set content of white list
    GAP_GAPM_SET_WL                                    = 0x53,
    /// Set content of resolving list
    GAP_GAPM_SET_RAL                                   = 0x54,
};

enum gap_gapc_actv_type
{
    /// Start bonding procedure.
    GAP_GAPC_BOND                                     = 0x50,
    /// Start encryption procedure.
    GAP_GAPC_ENCRYPT                                  = 0x51,
    /// Start security request procedure
    GAP_GAPC_SECURITY_REQ                             = 0x52,
    /// Request to inform the remote device when keys have been entered or erased
    GAP_GAPC_KEY_PRESS_NOTIFICATION                   = 0x53,
    /// auth request for classic bt
    GAP_GAPC_AUTH_REQ                                 = 0x54,
};

/// Privacy configuration
enum gap_priv_cfg
{
    /// Indicate if identity address is a public (0) or static private random (1) address
    GAP_PRIV_CFG_PRIV_ADDR_BIT = (1 << 0),
    GAP_PRIV_CFG_PRIV_ADDR_POS = 0,
    /// Reserved
    GAP_PRIV_CFG_RSVD_BIT      = (1 << 1),
    GAP_PRIV_CFG_RSVD_BIT_POS  = 1,
    /// Indicate if controller privacy is enabled
    GAP_PRIV_CFG_PRIV_EN_BIT   = (1 << 2),
    GAP_PRIV_CFG_PRIV_EN_POS   = 2,
};

enum gap_att_cfg_flag
{
    /// Device Name write permission requirements for peer device
    GAP_ATT_NAME_PERM_MASK                  = 0x0007,
    GAP_ATT_NAME_PERM_LSB                   = 0,
    /// Device Appearance write permission requirements for peer device
    GAP_ATT_APPEARENCE_PERM_MASK            = 0x0038,
    GAP_ATT_APPEARENCE_PERM_LSB             = 3,
    /// Slave Preferred Connection Parameters present in GAP attribute database.
    GAP_ATT_SLV_PREF_CON_PAR_EN_MASK        = 0x0040,
    GAP_ATT_SLV_PREF_CON_PAR_EN_LSB         = 6,
    /// Disable automatic MTU exchange at connection establishment
    GAP_ATT_CLI_DIS_AUTO_MTU_EXCH_MASK      = 0x0080,
    GAP_ATT_CLI_DIS_AUTO_MTU_EXCH_LSB       = 7,
    /// Disable automatic client feature enable setup at connection establishment
    GAP_ATT_CLI_DIS_AUTO_FEAT_EN_MASK       = 0x0100,
    GAP_ATT_CLI_DIS_AUTO_FEAT_EN_LSB        = 8,
    /// Disable automatic establishment of Enhanced ATT bearers
    GAP_ATT_CLI_DIS_AUTO_EATT_MASK          = 0x0200,
    GAP_ATT_CLI_DIS_AUTO_EATT_LSB           = 9,
    /// Enable presence of Resolvable private address only.
    /// This means that after a bond, device must only use resolvable private address
    GAP_ATT_RSLV_PRIV_ADDR_ONLY_MASK        = 0x0400,
    GAP_ATT_RSLV_PRIV_ADDR_ONLY_LSB         = 10,
    /// EATT permission requirements for peer device
    GAP_ATT_EATT_SEC_MASK                   = 0x1800,
    GAP_ATT_EATT_SEC_LSB                    = 11,
    /// Trigger bond information to application even if devices are not bonded
    GAP_DBG_BOND_INFO_TRIGGER_BIT           = 0x8000,
    GAP_DBG_BOND_INFO_TRIGGER_POS           = 15,
};

enum bt_gap_cfg_flag
{
    /// Accept con req role set
    BT_GAP_CON_REQ_ROLE_REMIAN_SLAVE_MASK                  = 0x0001,
    BT_GAP_CON_REQ_ROLE_REMIAN_SLAVE_LSB                   = 0,

};

enum gap_encrypt_type
{
    GAP_BOND_IND = 0x00,
    GAP_ENCRYPT_REQ = 0x80,
};

enum bt_gap_scan_type
{
    BT_GAP_SCAN_DIS = 0x00,
    BT_GAP_ISCAN_EN = 0x01,
    BT_GAP_PSCAN_EN = 0x02,
    BT_GAP_ISCAN_PSCAN_EN = 0x03,
};

enum bt_gap_role_type
{
    ///Master role
    BT_ROLE_MASTER,
    ///Slave role
    BT_ROLE_SLAVE,
};

enum gap_exit_latency_type
{
    GAP_EXIT_LATENCY_CONNECT  = (1 << 0),
    GAP_EXIT_LATENCY_AUDIO    = (1 << 1),
    GAP_EXIT_LATENCY_OTA      = (1 << 2),

    GAP_EXIT_LATENCY_ALL      = 0xff,
};

/// Bond event type.
/*@TRACE*/
enum gap_bond_encrypt_info
{
    /// Pairing Finished information
    GAP_PAIRING_SUCCEED,
    /// Pairing Failed information
    GAP_PAIRING_FAILED,

    /// Used to retrieve pairing Temporary Key
    GAP_TK_EXCH,
    /// Used for Identity Resolving Key exchange
    GAP_IRK_EXCH,
    /// Used for Connection Signature Resolving Key exchange
    GAP_CSRK_EXCH,
    /// Used for Long Term Key exchange
    GAP_LTK_EXCH,
    /// Used for classic bt link key exchange
    GAP_LK_EXCH,

    /// Bond Pairing request issue, Repeated attempt
    GAP_REPEATED_ATTEMPT,

    /// Out of Band - exchange of confirm and rand.
    GAP_OOB_EXCH,

    /// Numeric Comparison - Exchange of Numeric Value -
    GAP_NC_EXCH,

    /// Link encrypted.
    GAP_LINK_ENCRYPTED,

    /// link encrypt request info.
    GAP_LINK_ENCRYPT_REQ,

    /// bt link auth req
    GAP_BT_LINK_AUTH_REQ,
};
/// BT Discovery Types
enum bt_gapm_disc_type
{
    /// General discovery
    BT_GAPM_DISC_TYPE_GEN_DISC = 0,
    /// Limited discovery
    BT_GAPM_DISC_TYPE_LIM_DISC,
};

typedef struct ble_gap_cfg
{
    /// device address
    gap_bdaddr_t addr;
    /// device name length
    uint8_t name_len;
    /// device name
    uint8_t name[GAP_NAME_LEN];
    /// ble appearance @see enum gap_appearance_type
    uint16_t appearance;
    /// auth io capability @see enum gap_io_cap
    uint8_t iocap;
    /// auth request @see enum gap_auth_mask
    uint8_t auth;
    /// Pairing mode authorized (@see enum gapm_pairing_mode)
    uint8_t pairing_mode;
    /// Privacy configuration bit field (@see enum gap_priv_cfg for bit signification)
    uint8_t         privacy_cfg;
    /// Duration before regenerate device address when privacy is enabled. - in seconds
    uint16_t        renew_dur;

    /// Attribute database configuration (@see enum gap_att_cfg_flag)
    uint16_t        att_cfg;

} ble_gap_cfg_t;

typedef struct bt_gap_cfg
{
    /// Local device class
    uint32_t        cod;
    // -------------- BT Modes Config -----------------------
    /// Discoverability modes @see enum gapm_discover_mode
    uint8_t         discover_mode;
    /// Connectability modes @see enum gapm_connect_mode
    uint8_t         connect_mode;
    /// Inquiry scan interval
    uint16_t        iscan_interval;
    /// Page scan interval
    uint16_t        pscan_interval;
    /// bt classic feature config flag
    uint16_t        bt_cfg_flag;
} bt_gap_cfg_t;

typedef struct bt_gap_peer_info
{
    uint8_t  phy_2m;
    uint8_t  bt_core;
    uint16_t comp_id;
} bt_gap_peer_info_t;


typedef union ble_info_data
{
    /// address
    gap_bdaddr_t addr;
    /// device name
    struct {
        uint16_t length;
        uint8_t *name;
    };
    /// appearance
    uint16_t appearance;
    /// version
    struct {
        uint8_t lmp_version;
        uint16_t subvers;
        uint16_t compid;
    };
    /// feature
    struct {
        uint8_t page;
        uint8_t features[8];
    };
    /// list size
    struct {
        uint8_t operation;
        uint8_t size;
    };
    /// rssi
    int8_t rssi;
} ble_info_data_t;

typedef struct ble_gap_cb
{
    /**
     ****************************************************************************************
     * @brief Reception of stack activate complete
     ****************************************************************************************
     */
    void (*cb_ble_enable_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief Handles adv report event from the GAP
     *
     * @param[in] flag              @see enum ble_gapm_adv_report_info
     * @param[in] peer_addr         peer device address
     * @param[in] rssi              rssi of the adv report
     * @param[in] len               ADV Report Data Length
     * @param[in] data              ADV Report Data
     ****************************************************************************************
     */
    void (*cb_ble_adv_report_ind)(uint8_t flag, gap_bdaddr_t *peer_addr, int8_t rssi, uint8_t len, uint8_t *data);

    /**
     ****************************************************************************************
     * @brief Handles active start indicate event from the GAP
     *
     * @param[in] type              active type, @see enum gapm_actv_type
     * @param[in] actv_id           active id
     * @param[in] status            status of the start report
     ****************************************************************************************
     */
    void (*cb_ble_actv_start_ind)(uint8_t type, uint8_t actv_id, int16_t status);

    /**
     ****************************************************************************************
     * @brief Handles active stop indicate event from the GAP
     *
     * @param[in] type              active type, @see enum gapm_actv_type
     * @param[in] actv_id           active id
     * @param[in] status            status of the stop report
     ****************************************************************************************
     */
    void (*cb_ble_actv_stop_ind)(uint8_t type, uint8_t actv_id, int16_t status);

    /**
     ****************************************************************************************
     * @brief Handles connection complete event from the GAP
     ****************************************************************************************
     */
    void (*cb_ble_conn_ind)(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);

    /**
     ****************************************************************************************
     * @brief Handles connection update event from the GAP
     ****************************************************************************************
     */
    void (*cb_ble_conn_update)(uint8_t conidx, uint16_t interval, uint16_t latency, uint16_t super_to);


    /**
     ****************************************************************************************
     * @brief Handles connection disconnection event from the GAP
     ****************************************************************************************
     */
    void (*cb_ble_disc_ind)(uint8_t conidx, uint16_t conhdl, uint16_t reason);

    /**
     ****************************************************************************************
     * @brief Handles secure key request event from the GAP
     *
     * @param[in] conidx            Connection index
     * @param[in] key_type          Request key type @See gap_tk_type
     * @param[in] key               key value for display
     ****************************************************************************************
     */
    void (*cb_ble_key_req)(uint8_t conidx, uint8_t key_type, uint32_t key);

    /**
     ****************************************************************************************
     * @brief Handles bond complete event from the GAP
     ****************************************************************************************
     */
    void (*cb_ble_bond_ind)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief callback for the get_dev_info/get_con_info
     *
     * @param[in] conidx            Connection index, GAP_INVALID_CONIDX(0xff) for the info is for local device
     * @param[in] type              Information type @see enum gap_dev_info_type
     * @param[in] data              Pointer to the data @see ble_info_data_t
     ****************************************************************************************
     */
    void (*cb_ble_info_ind)(uint8_t conidx, uint8_t type, ble_info_data_t *data);

    /**
     ****************************************************************************************
     * @brief Reception of stack event complete
     ****************************************************************************************
     */
    void (*cb_ble_event_cmp)(uint8_t type, uint16_t status);

} ble_gap_cb_t;

typedef struct bt_gap_cb
{
    /**
     ****************************************************************************************
     * @brief Reception of stack activate complete
     ****************************************************************************************
     */
    void (*cb_bt_enable_cmp)(uint16_t status);
    /**
     ****************************************************************************************
     * @brief Handles connection complete event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_conn_ind)(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr);
     /**
     ****************************************************************************************
     * @brief Handles sco connection event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_aud_conn_ind)(uint8_t conidx, uint16_t type, uint16_t status);
    /**
     ****************************************************************************************
     * @brief Handles sco connection disconnection event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_aud_disc_ind)(uint8_t conidx, uint16_t conhdl, uint16_t reason);
    /**
     ****************************************************************************************
     * @brief Handles discover report event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_discover_ind)(gap_bdaddr_t *peer_addr, uint16_t clk_off, int8_t rssi, uint8_t mode, uint32_t cod, struct gap_dev_name *name);
    /**
     ****************************************************************************************
     * @brief Handles sniff mode change event from the GAP
     ****************************************************************************************
     */
    void (*cb_bt_sniff_change_ind)(uint8_t conidx, uint8_t status, uint8_t cur_mode, uint16_t interv);

}bt_gap_cb_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/**
 * Enable gap ble stack
 *
 * @param cfg               Configure for gap
 * @param cb                Call back for gap
 *
 * @return None.
 */
void ble_gap_enable(const ble_gap_cfg_t *cfg, const ble_gap_cb_t *cb);


/**
 * Get local device info.
 *
 * @param info_type   Get local device info command  (@see enum gap_dev_info_type)
 *
 * @return None.
 */
void ble_gap_get_dev_info(uint8_t info_type);

/**
 * Prepare advertising activity parameter.
 *
 * @param tpye        Advertising type (@see enum gapm_adv_type)
 * @param disc_mode   Discovery mode (@see enum gapm_adv_disc_mode)
 * @param flags       Advertising flags (@see enum gapm_adv_flag)
 * @param peer_addr   Peer address configuration (only used in case of directed advertising)
 * @param max_adv_evt Maximum number of extended advertising events the controller shall attempt to send prior to
 *                    terminating the extending advertising
 *                    Valid only if extended advertising
 *
 * @return None.
 */
void ble_gap_adv_prepare(uint8_t adv_id, uint8_t type, uint8_t disc_mode, uint16_t flags, uint8_t filter_pol, uint32_t intv_min, uint32_t intv_max,
                                        gap_bdaddr_t peer_addr, uint8_t max_adv_evt);
/**
 * Generate advertising data.
 *
 * @param nb_uuid       Number of uuid list
 * @param uuids         UUID List
 *
 * @return None.
 */
void ble_gap_adv_gen_data(uint8_t adv_id, uint8_t nb_uuid, uint16_t *uuids);

/**
 * Set advertising data.
 *
 * @param adv_len       Advertising Data Length
 * @param avd_data      Advertising Data
 * @param rsp_len       Scan Response Data Length
 * @param rsp_data      Scan Response Data
 *
 * @return None.
 */
void ble_gap_adv_set_data(uint8_t adv_id, uint16_t adv_len, uint8_t *avd_data, uint16_t rsp_len, uint8_t *rsp_data);


/**
 * Start advertising activity.
 *
 * @return None.
 */

void ble_gap_adv_start(uint8_t adv_id);

/**
 * Stop advertising activity.
 *
 * @return None.
 */
void ble_gap_adv_stop(uint8_t adv_id);

/**
 * Stop advertising activity.
 *
 * @return Adv state.
 */
uint8_t ble_gap_get_adv_state(uint8_t adv_id);

/**
 * Start scan activity.
 *
 * @param type          Type of scanning to be started (@see enum gapm_scan_type)
 * @param phy           PHY select for the connection, @see enum gap_phy
 * @param scan_intv     Scan interval 1:0.625us, rang 0x0004(2.5ms)-0x4000(10.24sec)
 * @param scan_win      Scan window must not lager than scan interval.1:0.625us, rang 0x0004(2.5ms)-0x4000(10.24sec)
 * @param dup_filter    Duplicate packet filtering policy,0=disabled, 1=enabled, 2=enabled & reset each scan period,bit4:host round filter enable.
 *                      @see enum ble_gapm_scan_filter
 *
 * @return None.
 */
void ble_gap_scan_start(uint8_t scan_id, uint8_t type, uint8_t phy, uint16_t scan_intv, uint16_t scan_win, uint8_t dup_filter);

/**
 * Stop scan activity
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_scan_stop(uint8_t scan_id);

/**
 * Start create connection
 *
 * @param addr    	        Peer device address
 * @param phy               PHY select for the connection, @see enum gap_phy
 * @param conn_intv_min     Minimum value for the connection interval 7.5ms to 4s (in unit of 1.25ms).
 * @param conn_intv_max     Maximum value for the connection interval 7.5ms to 4s (in unit of 1.25ms).
 * @param latency           Connection Latency
 * @param super_to          Supervision timeout
 *
 * @return None.
 */
void ble_gap_connect(gap_bdaddr_t addr, uint8_t phy, uint16_t conn_intv_min, uint16_t conn_intv_max,
                    uint16_t latency, uint16_t super_to);

/**
 * Stop create connection
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_connect_cancel();

/**
 * Update connection param
 *
 * @param conidx            Connection index
 * @param conn_intv_min     Minimum value for the connection interval 7.5ms to 4s (in unit of 1.25ms).
 * @param conn_intv_max     Maximum value for the connection interval 7.5ms to 4s (in unit of 1.25ms).
 * @param latency           Connection Latency
 * @param super_to          Supervision timeout
 *
 * @return None.
 */
void ble_gap_connect_update(uint8_t conidx, uint16_t conn_intv_min, uint16_t conn_intv_max, uint16_t latency, uint16_t super_to);

/**
 * Disconnect connection
 *
 * @param conidx            Connection index
 * @param reasion           Disconnect reason,but bt classic dis reason only {0x05,0x13,0x14,0x15,0x1A,0x29}
 *
 * @return None.
 */
void ble_gap_disconnect(uint8_t conidx, uint8_t reason);

/**
 * Get connection information
 *
 * @param conidx            Connection index
 * @param type              Information type @see enum gap_dev_info_type
 *
 * @return None.
 */
void ble_gap_get_con_info(uint8_t conidx, uint8_t type);

/**
 * Start auth request
 *
 * @param conidx            Connection index
 * @param sec_lvl           Security level @see enum gap_sec_lvl
 *
 * @return None.
 */
void ble_gap_auth_req(uint8_t conidx, uint8_t sec_lvl);

/**
 ****************************************************************************************
 * @brief Handles secure key request event from the GAP
 *
 * @param[in] conidx            Connection index
 * @param[in] accept            Accept the key or not
 * @param[in] key               Key value, only for GAP_TK_GEN/GAP_TK_KEY_ENTRY
 ****************************************************************************************
 */
void ble_gap_key_cfm(uint8_t conidx, uint8_t accept, uint32_t key);

/**
 * Delete device bond information
 *
 * @param addr              BDADDR of the device to delete bond information, NULL for delete all bond information
 *
 * @return None.
 */
void ble_gap_delete_bond(gap_bdaddr_t *addr);

/**
 * Start tx test mode
 *
 * @param channel           Tx Channel (Range 0x00 to 0x27)
 * @param tx_pkt_payload    Packet Payload type (@see enum gap_pkt_pld_type)
 * @param tx_data_length    Length in bytes of payload data in each packet
 * @param phy               Test PHY rate (@see enum gap_phy_val)
 * @param tx_pwr_lvl        Transmit power level in dBm (range: -127 to +20)
 *
 * @return None.
 */
void ble_gap_test_mode_tx(uint8_t channel, uint8_t tx_pkt_payload, uint8_t tx_data_length, uint8_t phy, int8_t tx_pwr_lvl);

/**
 * Start rx test mode
 *
 * @param channel           Rx Channel (Range 0x00 to 0x27)
 * @param modulation_idx    Modulation Index (@see enum gap_modulation_idx)
 * @param slot_dur          Slot durations
 * @param phy               Test PHY rate (@see enum gap_phy_val)
 *
 * @return None.
 */
void ble_gap_test_mode_rx(uint8_t channel, uint8_t modulation_idx, uint8_t slot_dur, uint8_t phy);

/**
 * Stop create connection
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_stop_test();

/**
 * Set le connect phy mode
 *
 * @param conidx           Connect index
 * @param rx_phy           Supported LE PHY for data reception (@see enum gap_phy)
 * @param tx_phy           Supported LE PHY for data reception (@see enum gap_phy)
 * @param phy_opt          PHY options (@see enum gapc_phy_option)
 *
 * @return None.
 */
uint8_t ble_gap_set_phy(uint8_t conidx, uint8_t rx_phy, uint8_t tx_phy, uint8_t phy_opt);

/**
 * Dis connect param req
 *
 * @param flag
 *
 * @return None.
 */
void ble_gap_set_con_param_dis(uint8_t flag);

/**
 * Set 1 to make connect exit latency.
 *
 * @param flag
 *
 * @return None.
 */

void ble_gap_set_con_exit_latency(uint8_t flag);

/**
 * entry latency apply.
 *
 * @param type (@see enum gap_exit_latency_type)
 *
 * @return None.
 */
void ble_gap_entry_latency(uint8_t type);

/**
 * exit latency apply.
 *
 * @param type (@see enum gap_exit_latency_type)
 *
 * @return None.
 */
void ble_gap_exit_latency(uint8_t type);

/**
 * Get peer rx mtu.
 *
 * @param conidx
 *
 * @return Mtu.
 */
uint16_t ble_gap_get_peer_mtu(uint8_t conidx);

/**
 * mtu exchange,this is not a standard interface when gatt service.
 *
 * @param conidx, user_lid
 *
 * @return status.
 */
uint8_t ble_gap_mtu_exch(uint8_t conidx, uint8_t user_lid);

/**
 * gap set ltk nvs default_id.
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_set_ltk_nvs_default_id(void);

/**
 * gap set ltk use addr.
 *
 * @param addr            Device addr
 * @param p_ltk           store ltk pointer
 *
 * @return None.
 */
//uint8_t ble_gap_get_ltk_nocon(gap_addr_t * addr, uint8_t *p_ltk);

/**
 * gap get paired addr.
 *
 * @param addr            Device addr
 *
 * @return Number of paired addr.
 */
uint8_t ble_gap_get_paired_addr(gap_bdaddr_t *addr_buf);

/**
 * gap get ral paired info.
 *
 * @param addr            Ral device info
 *
 * @return Number of ral paired addr.
 */
//uint8_t ble_gap_get_paired_ral_info(struct gap_ral_dev_info *ral_info);

/**
 * gap set white list.
 *
 * @param size            list count.
 * @param addr            white list addr.
 *
 * @return None.
 */
void ble_gap_wl_list_set(uint8_t size, gap_bdaddr_t* addr);

/**
 * gap set ral list.
 *
 * @param size            list count.
 * @param addr            ral list addr.
 *
 * @return None.
 */
//void ble_gap_ral_list_set(uint8_t size, struct gap_ral_dev_info *ral_info);

/**
 * gap add paired dev to ral list.
 *
 * @param size            list count.
 * @param addr            white list addr.
 *
 * @return None.
 */
uint8_t ble_gap_add_paired_rpa_to_rlist(void);

/**
 * gap get connect parameter disable.
 *
 * @param None
 *
 * @return flag of disable.
 */
uint8_t ble_gap_get_con_param_dis(void);

/**
 * Gap set user adv data
 *
 * @param adv_id
 * @param len
 * @param adv_data
 *
 * @return None.
 */
void ble_gap_adv_user_data(uint8_t adv_id, uint8_t len , uint8_t *adv_data);

/**
 * Gap set data len
 *
 * @param conidx
 * @param tx_octets
 * @param tx_time
 *
 * @return None.
 */
void ble_gap_set_data_len(uint16_t conidx, uint16_t tx_octets, uint16_t tx_time);


/**
 * Prepare scan activity.
 *
 * @param scan id       ID number of scan.
 *
 * @return None.
 */
void ble_gap_scan_prepare(uint8_t scan_id);

/**
 * Start periodic synchronization activity.
 *
 * @param type          Type of Periodic synchronization to be started (@see enum gapm_per_sync_type)
 * @param adv_addr      Periodic advertising address information
 * @param report_en     report enable bitfile @see enum gapm_report_en_bf
 * @param past_conidx   only valid for GAPM_PER_SYNC_TYPE_PAST 
 * @param time_out      in unit of 10ms between 100ms and 163.84s
 *
 * @return None.
 */
void ble_gap_per_sync_start(uint8_t type, gap_per_adv_bdaddr_t *adv_addr, uint8_t report_en, uint8_t past_conidx,uint16_t time_out);

/**
 * Stop scan activity
 *
 * @param None
 *
 * @return None.
 */
void ble_gap_per_sync_stop();

/**
 * gap set local public addr.
 *
 * @param addr, public addr.
 *
 * @return None.
 */
void ble_gap_set_loc_pub_addr(uint8_t *addr);

/**
 * gap update device name.
 *
 * @param name length, name.
 *
 * @return None.
 */
void ble_gap_update_device_name(uint8_t name_len, uint8_t  *name);

/**
 * gap set ltk nvs by num id.
 *
 * @param num, dev number from 0,max is 2.save_ltk_max, should be not larger than NVS_COUNT_LTK.
 *
 * @return None.
 */
void ble_gap_set_ltk_nvs_num(uint8_t num, uint8_t save_ltk_max);

/**
 * gap set disable adv data check for some user.
 *
 * @param dis_check, 0:enable check, 1:disable check.defaut is 0.
 *
 * @return None.
 */
void ble_gap_set_adv_data_dis_check(uint8_t dis_check);

/**
 * gap get disable adv data check for some user.
 *
 * @param dis_check, 0:enable check, 1:disable check.defaut is 0.
 *
 * @return dis_check, 0:enable check, 1:disable check.
 */
uint8_t gapm_adv_get_data_dis_check(void);

/**
 * gap update local irk.
 *
 * @param index.
 *
 * @return None.
 */
void ble_gap_update_irk(uint8_t index);

/**
 * gap use temp local irk.
 *
 * @param name length, name.
 *
 * @return None.
 */
void ble_gap_use_temp_irk(void);

/**
 * gap save short_time local irk to nvs.
 *
 * @param index.
 *
 * @return None.
 */
void ble_gap_save_temp_irk(uint8_t index);

/**
 * gap set 2M phy feature support.
 *
 * @param feature 0:not support 2M, 1:support 2M.
 *
 * @return None.
 */
void ble_gap_2m_phy_feature_set(uint8_t flag);


#if (BT_STACK_PRESENT)
/**
 * Enable gap bt stack
 *
 * @param cfg               Configure for gap
 * @param cb                Call back for gap
 *
 * @return None.
 */
void bt_gap_enable(const bt_gap_cfg_t *cfg, const bt_gap_cb_t *cb);

/**
 * Enable gap bt discover create
 *
 * @param own_addr_type               addr type
 *
 * @return None.
 */
void bt_gap_discover_create(uint8_t own_addr_type);

/**
 * Enable gap bt discover start
 *
 * @param disc_mode              discover mode
 * @param max_count              max count
 * @param get_name               get name flag
 *
 * @return None.
 */
void bt_gap_discover_start(uint8_t disc_mode, uint8_t max_count, bool get_name);


/**
 * Enable gap bt discover stop
 *
 * @param  None
 *
 * @return None.
 */
void bt_gap_discover_stop(void);

/**
 * Enable gap bt connect
 *
 * @param p_addr                 addr point
 * @param type                   type
 * @param clk_off                clk_off
 * @param page_scan_rep_mode     page_scan_rep_mode
 *
 * @return None.
 */
void bt_gap_connect(gap_bdaddr_t addr, uint8_t type, uint16_t clk_off, uint8_t page_scan_rep_mode);

/**
 * Stop gap bt connect
 *
 * @param None.
 *
 * @return None.
 */
void bt_gap_connect_cancel(void);

/**
 * Bt scan enable
 *
 * @param enable,0:dia, 1:i_scan, 2:p_scan, 3:p_scan&i_scan @see enmu bt_gap_scan_type
 *
 * @return None.
 */
void bt_classic_scan_enable(uint8_t enable);

/**
 * Get bt role
 *
 * @param conidx
 *
 * @return role, @see enum bt_gap_role_type
 */
uint8_t bt_gap_get_role(uint8_t conidx);

/**
 * Set bt role
 *
 * @param conidx
 * @param role, @see enum bt_gap_role_type
 *
 * @return None.
 */
void bt_gap_set_role(uint8_t conidx, uint8_t role);

/**
 * Enable bt gap auth req
 *
 * @param conidx
 * @param sec_lvl
 *
 * @return None.
 */
void bt_gap_auth_req(uint8_t conidx, uint8_t sec_lvl);

/**
 * Enable bt gap save lk mem to nvs
 *
 * @param conidx
 *
 * @return None.
 */
void bt_gap_save_lk_mem_to_nvs(uint8_t conidx);

/**
 * Delete Bt device bond information
 *
 * @param addr              BDADDR of the device to delete bond information, NULL for delete all bond information
 *
 * @return None.
 */
void bt_gap_delete_bond(gap_bdaddr_t *bdaddr);

/**
 * Enable bt set asic cvsd en
 *
 * @param en
 *
 * @return None.
 */
void app_bt_set_asic_cvsd_en(uint8_t en);
/**
 * Enable bt set asic cvsd en
 *
 * @param flag,0:enable check,1:disable check
 *
 * @return None.
 */
void app_bt_rfcomm_dis_tx_credit_check(uint8_t flag);

/**
 * Bt exit sniff
 *
 * @param conidx,connect index.
 *
 * @return None.
 */
void bt_gap_exit_sniff(uint8_t conidx);
/**
 * Bt entry sniff mode
 *
 * @param conidx,connect index.
 * @param max_int,maximum interval (in slots).
 * @param min_int,minimum interval (in slots).
 * @param attempt,attempts (number of receive slots) (in slots).
 * @param timeout, timeout (number of receive slots) (in slots).
 *
 * @return None.
 */
void bt_gap_sniff_mode(uint8_t conidx, uint16_t max_int, uint16_t min_int, uint16_t attempt, uint16_t timeout);

/**
 * Bt entry sniff sub
 *
 * @param conidx,connect index.
 * @param max_int,maximum latency used to calculate the maximum sniff subrate that the remote device may use (in slots).
 * @param min_int,minimum base sniff subrate timeout that the remote device may use (in slots).
 * @param attempt,attempts (number of receive slots) (in slots).
 * @param timeout, timeout (number of receive slots) (in slots).
 *
 * @return None.
 */
void bt_gap_sniff_sub(uint8_t conidx, uint16_t max_lat, uint16_t min_rem_to, uint16_t min_loc_to);

/**
 * Bt set link super timeout
 *
 * @param conidx,connect index.
 * @param timeout, link timeout (in slots).
 *
 * @return None.
 */
void bt_gap_set_sup_timeout(uint8_t conidx, uint16_t super_timeout);

#endif//(BT_STACK_PRESENT)

#endif//BLE_GAP_H_



