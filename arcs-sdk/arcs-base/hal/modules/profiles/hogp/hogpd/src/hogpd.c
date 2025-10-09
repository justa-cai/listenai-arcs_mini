/**
 ****************************************************************************************
 * @addtogroup HOGPD
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#define BLE_HID_DEVICE 1

#if (BLE_HID_DEVICE)

#include "hogpd.h"

#include <string.h>

/*
 * MACROS
 ****************************************************************************************
 */



/*
 * DEFINES
 ****************************************************************************************
 */
#define HIDS_CFG_FLAG_MANDATORY_MASK    ((uint32_t)0x000FD)
#define HIDS_MANDATORY_ATT_NB           (7)
#define HIDS_CFG_FLAG_MAP_EXT_MASK      ((uint32_t)0x00102)
#define HIDS_MAP_EXT_ATT_NB             (2)
#define HIDS_CFG_FLAG_PROTO_MODE_MASK   ((uint32_t)0x00600)
#define HIDS_PROTO_MODE_ATT_NB          (2)
#define HIDS_CFG_FLAG_KEYBOARD_MASK     ((uint32_t)0x0F800)
#define HIDS_KEYBOARD_ATT_NB            (5)
#define HIDS_CFG_FLAG_MOUSE_MASK        ((uint32_t)0x70000)
#define HIDS_MOUSE_ATT_NB               (3)

#define HIDS_CFG_REPORT_MANDATORY_MASK  ((uint32_t)0x7)
#define HIDS_REPORT_MANDATORY_ATT_NB    (3)
#define HIDS_CFG_REPORT_IN_MASK         ((uint32_t)0x8)
#define HIDS_REPORT_IN_ATT_NB           (1)
// number of attribute index for a report
#define HIDS_REPORT_NB_IDX              (4)

/// Maximal length for Characteristic values - 128 bytes
#define HOGPD_VAL_MAX_LEN               (128)

/* HID information flags */
#define HID_FLAGS_REMOTE_WAKE           0x01 /* RemoteWake */
#define HID_FLAGS_NORMALLY_CONNECTABLE  0x02 /* NormallyConnectable */
#define HI_UINT16(a)                    (((a) >> 8) & 0xFF)
#define LO_UINT16(a)                    ((a) & 0xFF)

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */



/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

static uint16_t hogpd_get_att_handle(hogpd_env_t* hogpd_env, uint8_t svc_idx, uint8_t att_idx, uint8_t report_idx);

static uint8_t hogpd_get_att_idx(hogpd_env_t* hogpd_env, uint16_t handle, uint8_t *svc_idx, uint8_t *att_idx, uint8_t *report_idx);

static uint8_t hogpd_ntf_send(uint8_t conidx, const struct hogpd_report_info* report);

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct hogpd_env hogpd_env;
hogpd_env_t* p_hogpd_env = &hogpd_env;

/* HID Information characteristic value */
static const uint8_t hid_info[] =
{
    LO_UINT16 ( 0x0111 ), HI_UINT16 ( 0x0111 ),             /* bcdHID (USB HID version) */
    0x00,                                                   /* bCountryCode */
    HID_FLAGS_REMOTE_WAKE | HID_FLAGS_NORMALLY_CONNECTABLE  /* Flags */
};
/* define the HID report map */
static const uint8_t hid_report_value[] ={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


#if 0
static const uint8_t hid_report_map[] =
{
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

    //mouse
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    
    0x85, HIDS_MOUSE_REPORT_ID,    //   REPORT_ID (Mouse)
    0x09, 0x01,                    //   USAGE_PAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (PHYSICAL)
    0x05, 0x09,                    //   USAGE_PAGE (BUTTON)
    0x19, 0x01,                    //   USAGE_MINIMUM (1)
    0x29, 0x03,                    //   USAGE_MAXIMUM (5)
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
    0xc0,
    0xc0,

    //  media
    0x05, 0x0C,                     // USAGE_PAGE (Consumer Devices)
    0x09, 0x01,                     // USAGE (Consumer Control)
    0xA1, 0x01,                     // COLLECTION (Application)
    0x85, HIDS_MEDIA_REPORT_ID,     // REPORT_ID (3)
    0x19, 0x00,                     // USAGE_MINIMUM (0x00)
    0x2A, 0x9C, 0x02,               // USAGE_MAXIMUM (0x02 0xc9)
    0x15, 0x00,                     // LOGICAL_MINIMUM (0x00)
    0x26, 0x9C, 0x02,               // LOGICAL_MAXIMUM (0x02 0xc9)
    0x95, 0x01,                     // REPORT_COUNT (1)
    0x75, 0x10,                     // REPORT_SIZE (0x10)
    0x81, 0x00,                     //INPUT (Data,Ary,Abs)
    0xC0,                           //      END_COLLECTION

    //voice data report
    0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    0x09 , 0x01,                    //    Usage (Consumer Control)
    0xA1 , 0x01,                    //    Collection (Application)
    0x85 , HIDS_VOICE_DATA_IN_REPORT_ID,                               //    Report ID=0xFC
    0x95 , 0xff,                    //    REPORT_COUNT (20)
    0x75 , 0x08,                    //    REPORT_SIZE (8)
    0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    0xC0,                           //      END_COLLECTION

    //gde ack in
    0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    0x09 , 0x01,                    //    Usage (Consumer Control)
    0xA1 , 0x01,                    //    Collection (Application)
    0x85 , HIDS_GDE_ACK_IN_REPORT_ID,//   Report ID=0xF8
    0x95 , 0xff,                    //    REPORT_COUNT (20)
    0x75 , 0x08,                    //    REPORT_SIZE (8)
    0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    0xC0,                           //      END_COLLECTION

    //gde feedback
    0x05 , 0x0C,                    //      Usage Page (Consumer Devices)
    0x09 , 0x01,                    //    Usage (Consumer Control)
    0xA1 , 0x01,                    //    Collection (Application)
    0x85 , HIDS_GDE_FEEDBACK_IN_REPORT_ID,                             //    Report ID=0xF9
    0x95 , 0xff,                    //    REPORT_COUNT (1)
    0x75 , 0x08,                    //    REPORT_SIZE (8)
    0x15 , 0x00,                    //    LOGICAL_MINIMUM (0)
    0x26 , 0xFF , 0x00,             //    LOGICAL_MAXIMUM (255)
    0x81 , 0x00,                    //    INPUT (Data,Ary,Abs)
    0xC0,                           //      END_COLLECTION

};
#endif

static const uint8_t boot_mouse_rep_val[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

static const uint8_t boot_kb_in_rep_val[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

static const uint8_t boot_kb_out_rep_val[] = {0x89, 0x90};


/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */
/// Full HIDS Database Description - Used to add attributes into the database
const ble_gatt_att16_desc_t hids_att_db[HOGPD_IDX_NB] =
{
    // HID Service Declaration
    [HOGPD_IDX_SVC]                             = {BLE_GATT_DECL_PRIMARY_SERVICE,       BLE_PROP(RD),                           0                                           },

    // HID Service Declaration
    [HOGPD_IDX_INCL_SVC]                        = {BLE_GATT_DECL_INCLUDE,               BLE_PROP(RD),                           0                                           },

    // HID Information Characteristic Declaration
    [HOGPD_IDX_HID_INFO_CHAR]                   = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    // HID Information Characteristic Value
    [HOGPD_IDX_HID_INFO_VAL]                    = {BLE_GATT_CHAR_HID_INFO,              BLE_PROP(RD),                           sizeof(struct hids_hid_info)                },

    // HID Control Point Characteristic Declaration
    [HOGPD_IDX_HID_CTNL_PT_CHAR]                = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    // HID Control Point Characteristic Value
    [HOGPD_IDX_HID_CTNL_PT_VAL]                 = {BLE_GATT_CHAR_HID_CTNL_PT,           BLE_PROP(WC),                           sizeof(uint8_t)                             },

    // Report Map Characteristic Declaration
    [HOGPD_IDX_REPORT_MAP_CHAR]                 = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    // Report Map Characteristic Value
    //[HOGPD_IDX_REPORT_MAP_VAL]                  = {BLE_GATT_CHAR_REPORT_MAP,            (BLE_PROP(RD) | BLE_SEC_LVL(RP, NO_AUTH)),                         HOGPD_REPORT_MAP_MAX_LEN                    },
    [HOGPD_IDX_REPORT_MAP_VAL]                  = {BLE_GATT_CHAR_REPORT_MAP,            (BLE_PROP(RD)),                         HOGPD_REPORT_MAP_MAX_LEN                    },
    // Report Map Characteristic - External Report Reference Descriptor
    [HOGPD_IDX_REPORT_MAP_EXT_REP_REF]          = {BLE_GATT_DESC_EXT_REPORT_REF,        BLE_PROP(RD),                           sizeof(uint16_t)                            },

    // Protocol Mode Characteristic Declaration
    [HOGPD_IDX_PROTO_MODE_CHAR]                 = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    // Protocol Mode Characteristic Value
    [HOGPD_IDX_PROTO_MODE_VAL]                  = {BLE_GATT_CHAR_PROTOCOL_MODE,         (BLE_PROP(RD) | BLE_PROP(WC)),              sizeof(uint8_t)                             },

    // Boot Keyboard Input Report Characteristic Declaration
    [HOGPD_IDX_BOOT_KB_IN_REPORT_CHAR]          = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    // Boot Keyboard Input Report Characteristic Value
    [HOGPD_IDX_BOOT_KB_IN_REPORT_VAL]           = {BLE_GATT_CHAR_BOOT_KB_IN_REPORT,     (BLE_PROP(RD) | BLE_PROP(N)),               HOGPD_BOOT_REPORT_MAX_LEN                   },
    // Boot Keyboard Input Report Characteristic - Client Characteristic Configuration Descriptor
    [HOGPD_IDX_BOOT_KB_IN_REPORT_NTF_CFG]       = {BLE_GATT_DESC_CLIENT_CHAR_CFG,       BLE_PROP(RD)|BLE_PROP(WR),                  BLE_OPT(NO_OFFSET)                              },

    // Boot Keyboard Output Report Characteristic Declaration
    [HOGPD_IDX_BOOT_KB_OUT_REPORT_CHAR]         = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    // Boot Keyboard Output Report Characteristic Value
    [HOGPD_IDX_BOOT_KB_OUT_REPORT_VAL]          = {BLE_GATT_CHAR_BOOT_KB_OUT_REPORT,    (BLE_PROP(RD) | BLE_PROP(WR) | BLE_PROP(WC)),   BLE_OPT(NO_OFFSET)|HOGPD_BOOT_REPORT_MAX_LEN    },

    // Boot Mouse Input Report Characteristic Declaration
    [HOGPD_IDX_BOOT_MOUSE_IN_REPORT_CHAR]       = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    // Boot Mouse Input Report Characteristic Value
    [HOGPD_IDX_BOOT_MOUSE_IN_REPORT_VAL]        = {BLE_GATT_CHAR_BOOT_MOUSE_IN_REPORT,  (BLE_PROP(RD) | BLE_PROP(N)),               BLE_OPT(NO_OFFSET) | HOGPD_BOOT_REPORT_MAX_LEN  },
    // Boot Mouse Input Report Characteristic - Client Characteristic Configuration Descriptor
    [HOGPD_IDX_BOOT_MOUSE_IN_REPORT_NTF_CFG]    = {BLE_GATT_DESC_CLIENT_CHAR_CFG,       (BLE_PROP(RD) | BLE_PROP(WR) | BLE_PROP(WC)),   BLE_OPT(NO_OFFSET)                              },

    // Report Characteristic Declaration
    [HOGPD_IDX_REPORT_CHAR]                     = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    // Report Characteristic Value
    [HOGPD_IDX_REPORT_VAL]                      = {BLE_GATT_CHAR_REPORT,                (BLE_PROP(RD) | BLE_PROP(N)),               BLE_OPT(NO_OFFSET) | HOGPD_REPORT_MAX_LEN       },
    // Report Characteristic - Report Reference Descriptor
    [HOGPD_IDX_REPORT_REP_REF]                  = {BLE_GATT_DESC_REPORT_REF,            BLE_PROP(RD),                           BLE_OPT(NO_OFFSET) |  sizeof(struct hids_report_ref)},
    // Report Characteristic - Client Characteristic Configuration Descriptor
    [HOGPD_IDX_REPORT_NTF_CFG]                  = {BLE_GATT_DESC_CLIENT_CHAR_CFG,       (BLE_PROP(RD) | BLE_PROP(WR)),              0                                           },
};




/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Retrieve Attribute handle from service and attribute index
 *
 * @param[in] hogpd_env  HID Service environment
 * @param[in] svc_idx    HID Service index
 * @param[in] att_idx    Attribute index
 * @param[in] report_idx Report index
 *
 * @return HID attribute handle or INVALID HANDLE if nothing found
 ****************************************************************************************
 */
static uint16_t hogpd_get_att_handle(hogpd_env_t* p_hogpd_env, uint8_t svc_idx, uint8_t att_idx, uint8_t report_idx)
{
    uint16_t handle  = BLE_GATT_INVALID_HDL;
    uint8_t i = 0;

    // Sanity check
    if((svc_idx < p_hogpd_env->hids_nb) && (att_idx < HOGPD_IDX_NB)
           && ((att_idx < HOGPD_ATT_UNIQ_NB) || (report_idx < p_hogpd_env->svcs[svc_idx].nb_report)))
    {
        handle = p_hogpd_env->start_hdl;

        for(i = 0 ; i < svc_idx ; i++)
        {
            // update start handle for next service - only useful if multiple service, else not used.
            handle +=  p_hogpd_env->svcs[i].nb_att;
        }

        // increment index according to expected index
        if(att_idx < HOGPD_ATT_UNIQ_NB)
        {
            handle += att_idx;

            // check if Keyboard feature active
            if((p_hogpd_env->svcs[svc_idx].features & HOGPD_CFG_KEYBOARD) == 0)
            {
               if(att_idx > HOGPD_IDX_BOOT_KB_OUT_REPORT_VAL)
               {
                   handle -= HIDS_KEYBOARD_ATT_NB;
               }
               // Error Case
               else if ((att_idx >= HOGPD_IDX_BOOT_KB_IN_REPORT_CHAR)
                       && (att_idx <= HOGPD_IDX_BOOT_KB_OUT_REPORT_VAL))
               {
                   handle = BLE_GATT_INVALID_HDL;
               }
            }

            // check if Mouse feature active
            if((p_hogpd_env->svcs[svc_idx].features & HOGPD_CFG_MOUSE) == 0)
            {
               if(att_idx > HOGPD_IDX_BOOT_MOUSE_IN_REPORT_NTF_CFG)
               {
                   handle -= HIDS_MOUSE_ATT_NB;
               }
               // Error Case
               else if ((att_idx >= HOGPD_IDX_BOOT_MOUSE_IN_REPORT_CHAR)
                       && (att_idx <= HOGPD_IDX_BOOT_MOUSE_IN_REPORT_NTF_CFG))
               {
                   handle = BLE_GATT_INVALID_HDL;
               }
            }

            // check if Protocol Mode feature active
            if((p_hogpd_env->svcs[svc_idx].features & HOGPD_CFG_PROTO_MODE) == 0)
            {
               if(att_idx > HOGPD_IDX_PROTO_MODE_VAL)
               {
                   handle -= HIDS_PROTO_MODE_ATT_NB;
               }
               // Error Case
               else if ((att_idx >= HOGPD_IDX_PROTO_MODE_CHAR)
                       && (att_idx <= HOGPD_IDX_PROTO_MODE_VAL))
               {
                   handle = BLE_GATT_INVALID_HDL;
               }
            }

            // check if Ext Ref feature active
            if((p_hogpd_env->svcs[svc_idx].features & HOGPD_CFG_MAP_EXT_REF) == 0)
            {
               if(att_idx > HOGPD_IDX_REPORT_MAP_EXT_REP_REF)
               {
                   handle -= HIDS_MAP_EXT_ATT_NB;
               }
               else if(att_idx > HOGPD_IDX_INCL_SVC)
               {
                   handle -= 1;
               }
               // Error Case
               else if ((att_idx == HOGPD_IDX_INCL_SVC)
                       || (att_idx == HOGPD_IDX_REPORT_MAP_EXT_REP_REF))
               {
                   handle = BLE_GATT_INVALID_HDL;
               }
            }
        }
        else
        {
            handle += p_hogpd_env->svcs[svc_idx].report_hdl_offset;

            // increment attribute handle with other reports
            for(i = 0 ; i < report_idx; i++)
            {
                handle += HIDS_REPORT_MANDATORY_ATT_NB;
                // check if it's a Report input
                if((p_hogpd_env->svcs[svc_idx].features & (HOGPD_CFG_REPORT_NTF_EN << i)) != 0)
                {
                    handle += HIDS_REPORT_IN_ATT_NB;
                }
            }

            // Error check
            if((att_idx == HOGPD_IDX_REPORT_NTF_CFG)
                    && ((p_hogpd_env->svcs[svc_idx].features & (HOGPD_CFG_REPORT_NTF_EN << report_idx)) != 0))
            {
                handle = BLE_GATT_INVALID_HDL;
            }
            else
            {
                // update handle cursor
                handle += att_idx - HOGPD_ATT_UNIQ_NB;
            }
        }
    }

    return handle;
}

/**
 ****************************************************************************************
 * @brief Retrieve Service and attribute index form attribute handle
 *
 * @param[out] handle     Attribute handle
 * @param[out] svc_idx    HID Service index
 * @param[out] att_idx    Attribute index
 * @param[out] report_idx Report Index
 *
 * @return Success if attribute and service index found, else Application error
 ****************************************************************************************
 */
static uint8_t hogpd_get_att_idx(hogpd_env_t* p_hogpd_env, uint16_t handle, uint8_t *svc_idx, uint8_t *att_idx, uint8_t *report_idx)
{
    uint16_t hdl_cursor = p_hogpd_env->start_hdl;
    uint8_t status = BLE_PRF_APP_ERROR;

    // invalid index
    *att_idx = HOGPD_IDX_NB;

    // Browse list of services
    // handle must be greater than current index
    for(*svc_idx = 0 ; (*svc_idx < p_hogpd_env->hids_nb) && (handle >= hdl_cursor) ; (*svc_idx)++)
    {
        // check if handle is on current service
        if(handle >= (hdl_cursor + p_hogpd_env->svcs[*svc_idx].nb_att))
        {
            hdl_cursor += p_hogpd_env->svcs[*svc_idx].nb_att;
            continue;
        }

        // if we are here, we are sure that handle is valid
        status = BLE_GAP_ERR_NO_ERROR;
        *report_idx = 0;

        // check if handle is in reports or not
        if(handle < (hdl_cursor + p_hogpd_env->svcs[*svc_idx].report_hdl_offset))
        {
            if(handle == hdl_cursor)
            {
                *att_idx = HOGPD_IDX_SVC;
                break;
            }

            // check if Ext Ref feature active
            if((p_hogpd_env->svcs[*svc_idx].features & HOGPD_CFG_MAP_EXT_REF) != 0)
            {
                hdl_cursor += 1;
                if(handle == hdl_cursor)
                {
                    *att_idx = HOGPD_IDX_INCL_SVC;
                    break;
                }
            }

            // check if handle is in mandatory range
            hdl_cursor += HIDS_MANDATORY_ATT_NB;
            if(handle <= hdl_cursor)
            {
                *att_idx = HOGPD_IDX_REPORT_MAP_VAL - (hdl_cursor - handle - 1);
                break;
            }

            // check if Ext Ref feature active
            if((p_hogpd_env->svcs[*svc_idx].features & HOGPD_CFG_MAP_EXT_REF) != 0)
            {
                hdl_cursor += 1;
                if(handle == hdl_cursor)
                {
                    *att_idx = HOGPD_IDX_REPORT_MAP_EXT_REP_REF;
                    break;
                }
            }

            // check if Protocol Mode feature active
            if((p_hogpd_env->svcs[*svc_idx].features & HOGPD_CFG_PROTO_MODE) != 0)
            {
                hdl_cursor += HIDS_PROTO_MODE_ATT_NB;
                if(handle <= hdl_cursor)
                {
                    *att_idx = HOGPD_IDX_PROTO_MODE_VAL - (hdl_cursor - handle - 1);
                    break;
                }
            }

            // check if Keyboard feature active
            if((p_hogpd_env->svcs[*svc_idx].features & HOGPD_CFG_KEYBOARD) != 0)
            {
                hdl_cursor += HIDS_KEYBOARD_ATT_NB;
                if(handle <= hdl_cursor)
                {
                    *att_idx = HOGPD_IDX_BOOT_KB_OUT_REPORT_VAL - (hdl_cursor - handle - 1);
                    break;
                }
            }

            // check if Mouse feature active
            if((p_hogpd_env->svcs[*svc_idx].features & HOGPD_CFG_MOUSE) != 0)
            {
                hdl_cursor += HIDS_MOUSE_ATT_NB;
                if(handle <= hdl_cursor)
                {
                    *att_idx = HOGPD_IDX_BOOT_MOUSE_IN_REPORT_NTF_CFG - (hdl_cursor - handle - 1);
                    break;
                }
            }
        }
        else
        {
            // add handle offset
            hdl_cursor += p_hogpd_env->svcs[*svc_idx].report_hdl_offset;

            for(*report_idx = 0 ; (*report_idx < p_hogpd_env->svcs[*svc_idx].nb_report) ; (*report_idx)++)
            {
                hdl_cursor += HIDS_REPORT_MANDATORY_ATT_NB;
                if(handle <= hdl_cursor)
                {
                    *att_idx = HOGPD_IDX_REPORT_REP_REF - (hdl_cursor - handle - 1);
                    break;
                }

                if((p_hogpd_env->svcs[*svc_idx].features & (HOGPD_CFG_REPORT_NTF_EN << *report_idx)) != 0)
                {
                    hdl_cursor += HIDS_REPORT_IN_ATT_NB;
                    if(handle == hdl_cursor)
                    {
                        *att_idx = HOGPD_IDX_REPORT_NTF_CFG;
                        break;
                    }
                }
            }
        }
        // loop not expected here
        break;
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief Check if a report value can be notified to the peer central.
 *
 * @param[in] conidx Connection Index
 * @param[in] report Report information to notify
 *
 * @return Status Code to know if request succeed or not.
 ****************************************************************************************
 */
static uint8_t hogpd_ntf_send(uint8_t conidx, const struct hogpd_report_info* report)
{
    uint16_t  status = BLE_GAP_ERR_NO_ERROR;
    uint16_t handle = BLE_GATT_INVALID_HDL;
    uint8_t  att_idx = HOGPD_IDX_NB;
    uint16_t max_report_len= 0;
    uint16_t feature_mask = 0;
    uint8_t exp_prot_mode = 0;

    // According to the report type retrieve:
    // - Attribute index
    // - Attribute max length
    // - Feature to use
    // - Expected protocol mode
    switch(report->type)
    {
        // An Input Report
        case HOGPD_REPORT:
        {
            att_idx = HOGPD_IDX_REPORT_VAL;
            max_report_len = HOGPD_REPORT_MAX_LEN;
            feature_mask = HOGPD_CFG_REPORT_NTF_EN << report->idx;
            exp_prot_mode = HOGP_REPORT_PROTOCOL_MODE;
        }break;
        // Boot Keyboard input report
        case HOGPD_BOOT_KEYBOARD_INPUT_REPORT:
        {
            att_idx = HOGPD_IDX_BOOT_KB_IN_REPORT_VAL;
            max_report_len = HOGPD_BOOT_REPORT_MAX_LEN;
            feature_mask = HOGPD_CFG_KEYBOARD;
            exp_prot_mode = HOGP_BOOT_PROTOCOL_MODE;
        }break;
        // Boot Mouse input report
        case HOGPD_BOOT_MOUSE_INPUT_REPORT:
        {
            att_idx = HOGPD_IDX_BOOT_MOUSE_IN_REPORT_VAL;
            max_report_len = HOGPD_BOOT_REPORT_MAX_LEN;
            feature_mask = HOGPD_CFG_MOUSE;
            exp_prot_mode = HOGP_BOOT_PROTOCOL_MODE;
        }break;

        default: /* Nothing to do */ break;
    }

    handle = hogpd_get_att_handle(p_hogpd_env, report->hid_idx, att_idx, report->idx);

    // check if attribute is found
    if(handle == BLE_GATT_INVALID_HDL)
    {
        // check if it's an unsupported feature
        if((feature_mask != 0) && (report->hid_idx < p_hogpd_env->hids_nb)
                && (report->idx < p_hogpd_env->svcs[report->hid_idx].nb_report)
                && ((p_hogpd_env->svcs[report->hid_idx].features & feature_mask) == 0))
        {
            status = BLE_PRF_ERR_FEATURE_NOT_SUPPORTED;
        }
        // or an invalid param
        else
        {
            status = BLE_PRF_ERR_INVALID_PARAM;
        }
    }
    // check if length is valid
    else if(report->length > max_report_len)
    {
        status = BLE_PRF_ERR_UNEXPECTED_LEN;
    }
    // check if notification is enabled
    else if ((p_hogpd_env->svcs[report->hid_idx].ntf_cfg[conidx] & feature_mask) == 0)
    {
        status = BLE_PRF_ERR_NTF_DISABLED;
    }
    // check if protocol mode is valid
    else if((p_hogpd_env->svcs[report->hid_idx].proto_mode != exp_prot_mode)
            && ((p_hogpd_env->svcs[report->hid_idx].features & HOGPD_CFG_PROTO_MODE) != 0))
    {
        status = BLE_PRF_ERR_REQ_DISALLOWED;
    }

    if(status == BLE_GAP_ERR_NO_ERROR)
    {
        uint16_t dumy = (BLE_GATT_NOTIFY << 8) | (conidx);
        // send notify
        status = ble_gatt_srv_event_send(conidx, p_hogpd_env->user_lid, dumy,
                                BLE_GATT_NOTIFY, handle, report->value, report->length);
    }
    return (status);
}



/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief The function enables the HID Over GATT Profile Device Role.
 * @param[in] param Pointer to the parameters of the message @ref struct hogpd_enable_req.
 ****************************************************************************************
 */
void hogpd_enable(struct hogpd_enable_req const *param)
{
    // Counter for HIDS instance
    uint8_t svc_idx;

    for (svc_idx = 0; svc_idx < p_hogpd_env->hids_nb; svc_idx++)
    {
       // Retrieve notification configuration
       p_hogpd_env->svcs[svc_idx].ntf_cfg[param->conidx]   = param->ntf_cfg[svc_idx];
    }
}

/**
 ****************************************************************************************
 * @brief The function enables the HID Over GATT Profile Device Role.
 * @param[in] None.
 ****************************************************************************************
 */
void ble_hogpd_enable(uint8_t conidx)
{
    struct hogpd_enable_req enable_req_param;

    enable_req_param.conidx = conidx;
    enable_req_param.ntf_cfg[0] = 0xFFFF;
    enable_req_param.ntf_cfg[1] = 0xFFFF;
    hogpd_enable(&enable_req_param);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref HOGPD_REPORT_UPD_REQ message.
 *
 * @param[in] param Pointer to the parameters of the message @ref struct hogpd_report_upd_req.
 * @return Status Code to know if request succeed or not.
 ****************************************************************************************
 */
int hogpd_report_upd(struct hogpd_report_upd_req const *param)
{
    // Status
    uint8_t status = BLE_GAP_ERR_NO_ERROR;
    // Check provided values and check if connection exists
    status = hogpd_ntf_send(param->conidx, &(param->report));
    //p_hogpd_env->p_cb->cb_notify_cmp(0, 0);
    return (status);
}

/**
 ****************************************************************************************
 * @brief Send hogpd report
 *
 * @param[in] length send data length.
 * @param[in] value send data pointer.
 * @return Status Code to know if request succeed or not.
 ****************************************************************************************
 */
int ble_hogpd_report_upd(uint8_t conidx, uint8_t report_idx, uint8_t length, uint8_t* value)
{
    struct hogpd_report_upd_req *params;

    // Allocate report element
    params = (struct hogpd_report_upd_req *)plf_malloc_noret(sizeof(struct hogpd_report_upd_req) + length);

    params->conidx = conidx;
    params->report.hid_idx = 0;
    params->report.idx = report_idx;
    params->report.type = HOGPD_REPORT;
    params->report.length = length;
    memcpy(params->report.value, value, length);
    int ret = hogpd_report_upd(params);
    plf_free(params);

    return ret;
}


/**
 ****************************************************************************************
 * @brief report update notification callback function.
 *
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] user_lid      GATT user local identifier
 ****************************************************************************************
 */
void hogpd_cb_notify_cmp(uint32_t token, uint8_t val_id)
{
    //Todu: add notify complete to app
}


void hogpd_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    /// notify cmp.
    if((dummy >> 8) == BLE_GATT_NOTIFY)
    {
        if(p_hogpd_env->p_cb->cb_notify_cmp)
        {
            p_hogpd_env->p_cb->cb_notify_cmp(dummy, 0);
        }
    }
}

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
 * @param[in] offset        Data offset
 * @param[in] max_length    Maximum data length to return
 ****************************************************************************************
 */
static void hogpd_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, uint16_t max_length)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    uint32_t value;
    uint16_t length = 0;
    uint16_t finished = 0;

    if(p_hogpd_env != NULL)
    {
        uint8_t svc_idx = 0, att_idx = 0, report_idx = 0;

        status = hogpd_get_att_idx(p_hogpd_env, hdl, &svc_idx, &att_idx, &report_idx);

        if(status == BLE_GAP_ERR_NO_ERROR)
        {
            // check which attribute is requested by peer device
            switch(att_idx)
            {
                //  ------------ READ report value requested
                case HOGPD_IDX_BOOT_KB_IN_REPORT_VAL:
                {
                    // Send result to peer device
                    length = sizeof(boot_mouse_rep_val);
                    status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, length, length, boot_kb_in_rep_val);
                }break;
                case HOGPD_IDX_BOOT_KB_OUT_REPORT_VAL:
                {
                    // Send result to peer device
                    length = sizeof(boot_mouse_rep_val);
                    status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, length, length, boot_kb_out_rep_val);
                }break;
                case HOGPD_IDX_BOOT_MOUSE_IN_REPORT_VAL:
                {
                    // Send result to peer device
                    length = sizeof(boot_mouse_rep_val);
                    status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, length, length, boot_mouse_rep_val);
                }break;
                case HOGPD_IDX_REPORT_VAL:
                {
                    // Send result to peer device
                    length = sizeof(hid_report_value);
                    status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, length, length, hid_report_value);
                }break;
                case HOGPD_IDX_REPORT_MAP_VAL:
                {
                    if(offset == 0)
                    {
                        p_hogpd_env->svcs[svc_idx].report_map_info.remain_size = p_hogpd_env->svcs[svc_idx].report_map_info.size;
                    }
                    // Send result to peer device
                    if(p_hogpd_env->svcs[svc_idx].report_map_info.remain_size > max_length){
                        length = max_length;
                        p_hogpd_env->svcs[svc_idx].report_map_info.remain_size -= max_length;
                    }else{
                        length = p_hogpd_env->svcs[svc_idx].report_map_info.remain_size;
                        p_hogpd_env->p_cb->cb_read_cmp(token, svc_idx);
                    }
                    status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, p_hogpd_env->svcs[svc_idx].report_map_info.size, length, (p_hogpd_env->svcs[svc_idx].report_map_info.rep_map+offset));
                }break;
                case HOGPD_IDX_REPORT_MAP_EXT_REP_REF:
                {
                    value =  p_hogpd_env->svcs[svc_idx].ext_ref.inc_svc_hdl | ((p_hogpd_env->svcs[svc_idx].ext_ref.rep_ref_uuid)<<16);
                    length = sizeof(uint32_t);
                    finished = 1;
                }break;
                //  ------------ READ active protocol mode
                case HOGPD_IDX_PROTO_MODE_VAL:
                {
                    value =  p_hogpd_env->svcs[svc_idx].proto_mode;
                    length = sizeof(uint8_t);
                    finished = 1;
                }break;
                case HOGPD_IDX_REPORT_REP_REF:
                {
                    value =  ((p_hogpd_env->svcs[svc_idx].report_ref[report_idx].report_type)<<8) | (p_hogpd_env->svcs[svc_idx].report_ref[report_idx].report_id);
                    length = sizeof(uint16_t);
                    finished = 1;
                }break;
                //  ------------ READ Notification configuration
                case HOGPD_IDX_BOOT_KB_IN_REPORT_NTF_CFG:
                {
                    value = ((p_hogpd_env->svcs[svc_idx].ntf_cfg[conidx] & HOGPD_CFG_KEYBOARD) != 0)
                            ? BLE_PRF_CLI_START_NTF : BLE_PRF_CLI_STOP_NTFIND;
                    length = sizeof(uint16_t);
                    finished = 1;
                }break;
                case HOGPD_IDX_BOOT_MOUSE_IN_REPORT_NTF_CFG:
                {
                    value = ((p_hogpd_env->svcs[svc_idx].ntf_cfg[conidx] & HOGPD_CFG_MOUSE) != 0)
                            ? BLE_PRF_CLI_START_NTF : BLE_PRF_CLI_STOP_NTFIND;
                    length = sizeof(uint16_t);
                    finished = 1;
                }break;
                case HOGPD_IDX_REPORT_NTF_CFG:
                {
                    value = ((p_hogpd_env->svcs[svc_idx].ntf_cfg[conidx] & (HOGPD_CFG_REPORT_NTF_EN << report_idx)) != 0)
                            ? BLE_PRF_CLI_START_NTF : BLE_PRF_CLI_STOP_NTFIND;
                    length = sizeof(uint16_t);
                    finished = 1;
                }break;
                case HOGPD_IDX_HID_INFO_VAL:
                {
                    value = (p_hogpd_env->svcs[svc_idx].hid_info.bcdHID) | ((p_hogpd_env->svcs[svc_idx].hid_info.bCountryCode)<<16) |((p_hogpd_env->svcs[svc_idx].hid_info.flags)<<24);
                    length = sizeof(uint32_t);
                    finished = 1;
                }break;
                default:
                {
                    status = BLE_PRF_APP_ERROR;
                } break;
            }
        }
        if(finished == 1){
            // Send result to peer device
            status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, length, length, (uint8_t*)&value);
        }
    }
}


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
static void hogpd_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, void* p_data)
{
        uint8_t hid_idx, att_idx, report_idx;
        // retrieve attribute requested
        uint8_t status = hogpd_get_att_idx(p_hogpd_env, hdl, &hid_idx, &att_idx, &report_idx);
        uint16_t length = ble_co_buf_data_len(p_data);
        uint8_t* buff = ble_co_buf_data(p_data);

        // prepare operation info
        p_hogpd_env->op.conidx    = conidx;
        p_hogpd_env->op.handle    = hdl;

        if(status == BLE_GAP_ERR_NO_ERROR)
        {

            // check which attribute is requested by peer device
            switch(att_idx)
            {
                // Modification of protocol mode requested
                case HOGPD_IDX_PROTO_MODE_VAL:
                {
                    // Todu: Update protocal mode value
                    //p_data:co_buf_t
                }break;

                // Modification of report value requested
                case HOGPD_IDX_BOOT_KB_IN_REPORT_VAL:
                case HOGPD_IDX_BOOT_KB_OUT_REPORT_VAL:
                case HOGPD_IDX_BOOT_MOUSE_IN_REPORT_VAL:
                case HOGPD_IDX_REPORT_VAL:
                {
                    // Tudo: Update report value
                }break;

                // Notification configuration update
                case HOGPD_IDX_BOOT_KB_IN_REPORT_NTF_CFG:
                case HOGPD_IDX_BOOT_MOUSE_IN_REPORT_NTF_CFG:
                case HOGPD_IDX_REPORT_NTF_CFG:
                {
                      //Todu: Update ntf_cfg
                }break;

                default:
                {
                    status = BLE_PRF_APP_ERROR;
                } break;
            }
        }
        if(p_hogpd_env->p_cb->cb_write_ind != NULL)
        {
            p_hogpd_env->p_cb->cb_write_ind(conidx, report_idx, length, offset, buff);
        }
        ble_gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
}


/// Default message callback handle from APP
const hogpd_cb_t hogpd_msg_cb =
{
    .cb_read_cmp = NULL,
    .cb_notify_cmp = hogpd_cb_notify_cmp,
    .cb_write_cmp = NULL,
};


/// Service callback hander from GATT
static const ble_gatt_srv_cb_t hogpd_cb =
{
    .cb_event_sent    = hogpd_cb_event_sent,
    .cb_att_read_get  = hogpd_cb_att_read_get,
    .cb_att_event_get = NULL,
    .cb_att_info_get  = NULL,
    .cb_att_val_set   = hogpd_cb_att_val_set,
};

uint16_t hogpd_init_report_map(uint8_t hids_nb, hogpd_report_map_t * p_map)
{
    for(uint8_t i=0; i<hids_nb; i++)
    {
        p_hogpd_env->svcs[i].report_map_info.size = p_map->size;
        p_hogpd_env->svcs[i].report_map_info.rep_map = p_map->rep_map;
    }
    return 0;
}

/**
 ****************************************************************************************
 * @brief Initialization of the HOGPD module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of database (if it's a service)
 *
 * @param[in|out] start_hdl  Service start handle (0 - dynamically allocated), only applies for services.
 * @param[in]     app_task   Application task number.
 * @param[in]     sec_lvl    Security level (AUTH, EKS and MI field of @see enum attm_value_perm_mask)
 * @param[in]     param      Configuration parameters of profile collector or service (32 bits aligned)
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t hogpd_init(uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                           const struct hogpd_db_cfg *p_params, hogpd_cb_t* p_cb, hogpd_report_map_t* p_map)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = BLE_GAP_ERR_NO_ERROR;

    uint8_t user_lid = BLE_GATT_INVALID_USER_LID;
    // Start handle used to allocate service.
    uint16_t shdl[HOGPD_NB_HIDS_INST_MAX];
    // Service Instance Counter, Counter
    uint8_t svc_idx=0, report_idx;
    // Total number of attributes
    uint8_t tot_nb_att = 0;
    // Report Char. Report Ref value
    struct hids_report_ref report_ref;

    // Service content flag - Without Report Characteristics
    uint32_t cfg_flag[HOGPD_NB_HIDS_INST_MAX][(HOGPD_ATT_MAX/32) +1];

    // array of service description used to allocate the service
    ble_gatt_att16_desc_t * hids_db[HOGPD_NB_HIDS_INST_MAX];

    //-------------------- allocate memory required for the profile  ---------------------
    // ensure that everything is initialized
    memset(hids_db, 0, sizeof(hids_db));
    memset(cfg_flag, 0, sizeof(cfg_flag));
    memset(p_hogpd_env, 0, sizeof(hogpd_env_t));

    do
    {
        if(p_cb == NULL)
        {
            p_cb = (hogpd_cb_t*)&(hogpd_msg_cb);
        }

        if((p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_notify_cmp == NULL))
        {
            status = BLE_GAP_ERR_INVALID_PARAM;
            break;
        }

        // Only one HID instance
        // register HOGPD user
        status = ble_gatt_user_register(HOGPD_VAL_MAX_LEN, user_prio, &hogpd_cb, &user_lid);
        if(status != BLE_GAP_ERR_NO_ERROR) break;


        // For each required HIDS instance
        for (svc_idx = 0; ((svc_idx < p_params->hids_nb) && (status == BLE_GAP_ERR_NO_ERROR)); svc_idx++)
        {
            // Check number of Report Char. instances
            if (p_params->cfg[svc_idx].report_nb > HOGPD_NB_REPORT_INST_MAX)
            {
                // Too many Report Char. Instances
                status = BLE_PRF_ERR_INVALID_PARAM;
                break;
            }

            // retrieve features
            p_hogpd_env->svcs[svc_idx].features  = p_params->cfg[svc_idx].svc_features & HOGPD_CFG_MASK;
            p_hogpd_env->svcs[svc_idx].nb_report = p_params->cfg[svc_idx].report_nb;
            memcpy(&(p_hogpd_env->svcs[svc_idx].hid_info), hid_info, sizeof(struct hids_hid_info));
            hids_db[svc_idx] = (ble_gatt_att16_desc_t *) plf_malloc_noret(HOGPD_ATT_MAX * sizeof(ble_gatt_att16_desc_t));

            cfg_flag[svc_idx][0]             = HIDS_CFG_FLAG_MANDATORY_MASK;
            p_hogpd_env->svcs[svc_idx].nb_att += HIDS_MANDATORY_ATT_NB;

            // copy default definition of the HID attribute database
            memcpy(hids_db[svc_idx], hids_att_db, sizeof(ble_gatt_att16_desc_t) * HOGPD_ATT_UNIQ_NB);

            //--------------------------------------------------------------------
            // Compute cfg_flag[i] without Report Characteristics
            //--------------------------------------------------------------------
            if ((p_params->cfg[svc_idx].svc_features & HOGPD_CFG_MAP_EXT_REF) == HOGPD_CFG_MAP_EXT_REF)
            {
                cfg_flag[svc_idx][0]            |= HIDS_CFG_FLAG_MAP_EXT_MASK;
                p_hogpd_env->svcs[svc_idx].nb_att += HIDS_MAP_EXT_ATT_NB;
            }

            if ((p_params->cfg[svc_idx].svc_features & HOGPD_CFG_PROTO_MODE) == HOGPD_CFG_PROTO_MODE)
            {
                cfg_flag[svc_idx][0]            |= HIDS_CFG_FLAG_PROTO_MODE_MASK;
                p_hogpd_env->svcs[svc_idx].nb_att += HIDS_PROTO_MODE_ATT_NB;
            }

            if ((p_params->cfg[svc_idx].svc_features & HOGPD_CFG_KEYBOARD) == HOGPD_CFG_KEYBOARD)
            {
                cfg_flag[svc_idx][0]            |= HIDS_CFG_FLAG_KEYBOARD_MASK;
                p_hogpd_env->svcs[svc_idx].nb_att += HIDS_KEYBOARD_ATT_NB;

                if ((p_params->cfg[svc_idx].svc_features & HOGPD_CFG_BOOT_KB_WR) == HOGPD_CFG_BOOT_KB_WR)
                {
                    // Adds write permissions on report
                    hids_db[svc_idx][HOGPD_IDX_BOOT_KB_IN_REPORT_VAL].info |= (BLE_PROP(WR));
                }
            }

            if ((p_params->cfg[svc_idx].svc_features & HOGPD_CFG_MOUSE) == HOGPD_CFG_MOUSE)
            {
                cfg_flag[svc_idx][0]            |= HIDS_CFG_FLAG_MOUSE_MASK;
                p_hogpd_env->svcs[svc_idx].nb_att += HIDS_MOUSE_ATT_NB;

                if ((p_params->cfg[svc_idx].svc_features & HOGPD_CFG_BOOT_MOUSE_WR) == HOGPD_CFG_BOOT_MOUSE_WR)
                {
                    // Adds write permissions on report
                    hids_db[svc_idx][HOGPD_IDX_BOOT_MOUSE_IN_REPORT_VAL].info |= (BLE_PROP(WR));
                }
            }

            // set report handle offset
            p_hogpd_env->svcs[svc_idx].report_hdl_offset = p_hogpd_env->svcs[svc_idx].nb_att;
            p_hogpd_env->svcs[svc_idx].report_map_info.size = p_map->size;
            p_hogpd_env->svcs[svc_idx].report_map_info.rep_map = p_map->rep_map;
            //--------------------------------------------------------------------
            // Update cfg_flag_rep[i] with Report Characteristics
            //--------------------------------------------------------------------
            for (report_idx = 0; report_idx < p_params->cfg[svc_idx].report_nb; report_idx++)
            {
                uint16_t perm = 0;
                uint16_t report_offset = HIDS_REPORT_NB_IDX*report_idx;

                // update config for current report
                cfg_flag[svc_idx][(HOGPD_IDX_REPORT_CHAR + report_offset) / 32]    |= (1 << ((HOGPD_IDX_REPORT_CHAR + report_offset) % 32));
                cfg_flag[svc_idx][(HOGPD_IDX_REPORT_VAL + report_offset) / 32]     |= (1 << ((HOGPD_IDX_REPORT_VAL + report_offset) % 32));
                cfg_flag[svc_idx][(HOGPD_IDX_REPORT_REP_REF + report_offset) / 32] |= (1 << ((HOGPD_IDX_REPORT_REP_REF + report_offset) % 32));
                p_hogpd_env->svcs[svc_idx].nb_att += HIDS_REPORT_MANDATORY_ATT_NB;
                p_hogpd_env->svcs[svc_idx].report_char_cfg[report_idx] = p_params->cfg[svc_idx].report_char_cfg[report_idx];

                // copy default definition of the HID report database
                memcpy(&(hids_db[svc_idx][HOGPD_IDX_REPORT_CHAR + report_offset]), &(hids_att_db[HOGPD_IDX_REPORT_CHAR]), sizeof(ble_gatt_att16_desc_t) * HIDS_REPORT_NB_IDX);

                // according to the report type, update value property
                switch (p_params->cfg[svc_idx].report_char_cfg[report_idx] & HOGPD_CFG_REPORT_FEAT)
                {
                    // Input Report
                    case HOGPD_CFG_REPORT_IN:
                    {
                        // add notification permission on report
                        perm = BLE_PROP(RD) | BLE_PROP(N);
                        // Report Char. supports NTF => Client Characteristic Configuration Descriptor
                        cfg_flag[svc_idx][(HOGPD_IDX_REPORT_NTF_CFG + report_offset) / 32] |= (1 << ((HOGPD_IDX_REPORT_NTF_CFG + report_offset) % 32));
                        p_hogpd_env->svcs[svc_idx].nb_att += HIDS_REPORT_IN_ATT_NB;

                        // update feature flag
                        p_hogpd_env->svcs[svc_idx].features |= (HOGPD_CFG_REPORT_NTF_EN << report_idx);
                        //Todu: Update report reference
                        p_hogpd_env->svcs[svc_idx].report_ref[report_idx].report_id = p_params->cfg[svc_idx].report_id[report_idx];
                        p_hogpd_env->svcs[svc_idx].report_ref[report_idx].report_type = HOGPD_CFG_REPORT_IN;

                        // check if attribute value could be written
                        if ((p_params->cfg[svc_idx].report_char_cfg[report_idx] & HOGPD_CFG_REPORT_WR) == HOGPD_CFG_REPORT_WR)
                        {
                            perm |= BLE_PROP(WR);
                        }
                    } break;

                    // Output Report
                    case HOGPD_CFG_REPORT_OUT:
                    {
                        perm = BLE_PROP(RD) | BLE_PROP(WR) | BLE_PROP(WC);
                        //Todu: Update report reference
                        p_hogpd_env->svcs[svc_idx].report_ref[report_idx].report_id = p_params->cfg[svc_idx].report_id[report_idx];
                        p_hogpd_env->svcs[svc_idx].report_ref[report_idx].report_type = HOGPD_CFG_REPORT_OUT;
                    } break;

                    // Feature Report
                    case HOGPD_CFG_REPORT_FEAT:
                    {
                        perm = BLE_PROP(RD) | BLE_PROP(WR);
                        //Todu: Update report reference
                        p_hogpd_env->svcs[svc_idx].report_ref[report_idx].report_id = p_params->cfg[svc_idx].report_id[report_idx];
                        p_hogpd_env->svcs[svc_idx].report_ref[report_idx].report_type = HOGPD_CFG_REPORT_FEAT;
                    } break;
                    default:
                    {
                        status = BLE_PRF_ERR_INVALID_PARAM;
                    } break;
                }
                hids_db[svc_idx][HOGPD_IDX_REPORT_VAL + (HIDS_REPORT_NB_IDX*report_idx)].info  = perm;
            }
            // increment total number of attributes to allocate.
            tot_nb_att += p_hogpd_env->svcs[svc_idx].nb_att;
        }


        // allocate services
        for (svc_idx = 0; ((svc_idx < p_params->hids_nb) && (status == BLE_GAP_ERR_NO_ERROR)); svc_idx++)
        {
            // Add HID service
            status = ble_gatt_db_svc16_add(user_lid, sec_lvl, BLE_GATT_SVC_HID, tot_nb_att+4,
                     (uint8_t *)&(cfg_flag[svc_idx][0]), hids_db[svc_idx], HOGPD_ATT_MAX,  p_start_hdl);
            if(status != BLE_GAP_ERR_NO_ERROR) break;

            // used start handle calculated when handle range reservation has been performed
            shdl[svc_idx] = *p_start_hdl;

            // by default in Report protocol mode.
            p_hogpd_env->svcs[svc_idx].proto_mode = HOGP_REPORT_PROTOCOL_MODE;
        }
        if(p_hogpd_env != NULL)
        {
            p_hogpd_env->start_hdl  = *p_start_hdl;
            p_hogpd_env->hids_nb   = p_params->hids_nb;
            p_hogpd_env->user_lid   = user_lid;

            p_hogpd_env->p_cb    = p_cb;
        }
    } while(0);

    if((status != BLE_GAP_ERR_NO_ERROR) && (user_lid != BLE_GATT_INVALID_USER_LID))
    {
        ble_gatt_user_unregister(user_lid);
    }

    // ensure that temporary allocated databases description is correctly free
    for (svc_idx = 0; svc_idx < HOGPD_NB_HIDS_INST_MAX; svc_idx++)
    {
        if(hids_db[svc_idx] != NULL)
        {
            plf_free(hids_db[svc_idx]);
        }
    }


    return (status);
}

/**
 ****************************************************************************************
 * @brief Initialization of the HOGPD module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of database (if it's a service)
 *
 * @param[in]     svc_features      Service Features (@see enum hogpd_cfg)
 * @param[in]     report_char_cfg   Report Char. Configuration (@see enum hogpd_report_cfg)
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t ble_hogpd_init(uint8_t svc_features, uint8_t report_char_cfg, hogpd_cb_t* p_cb, hogpd_report_map_t * p_map)
{
    struct hogpd_db_cfg db_cfg_params;
    uint16_t start_hdl=0;
    db_cfg_params.hids_nb = 1;
    db_cfg_params.cfg[0].svc_features = svc_features;
    db_cfg_params.cfg[0].report_nb = HIDS_INDEX_NUM;
    db_cfg_params.cfg[0].report_char_cfg[HIDS_KB_INDEX] = HOGPD_CFG_REPORT_IN;
    db_cfg_params.cfg[0].report_id[HIDS_KB_INDEX] = HIDS_KB_REPORT_ID;

    db_cfg_params.cfg[0].report_char_cfg[HIDS_MOUSE_INDEX] = HOGPD_CFG_REPORT_IN;
    db_cfg_params.cfg[0].report_id[HIDS_MOUSE_INDEX] = HIDS_MOUSE_REPORT_ID;

    db_cfg_params.cfg[0].report_char_cfg[HIDS_MEDIA_INDEX] = HOGPD_CFG_REPORT_IN;
    db_cfg_params.cfg[0].report_id[HIDS_MEDIA_INDEX] = HIDS_MEDIA_REPORT_ID;

    db_cfg_params.cfg[0].report_char_cfg[HIDS_VOICE_DATA_INDEX] = HOGPD_CFG_REPORT_IN;
    db_cfg_params.cfg[0].report_id[HIDS_VOICE_DATA_INDEX] = HIDS_VOICE_DATA_IN_REPORT_ID;

    db_cfg_params.cfg[0].report_char_cfg[HIDS_VOICE_CTRL_INDEX] = HOGPD_CFG_REPORT_OUT;
    db_cfg_params.cfg[0].report_id[HIDS_VOICE_CTRL_INDEX] = HIDS_VOICE_CTRL_OUT_REPORT_ID;

    db_cfg_params.cfg[0].report_char_cfg[HIDS_GDE_ACK_INDEX] = HOGPD_CFG_REPORT_IN;
    db_cfg_params.cfg[0].report_id[HIDS_GDE_ACK_INDEX] = HIDS_GDE_ACK_IN_REPORT_ID;

    db_cfg_params.cfg[0].report_char_cfg[HIDS_GDE_DATA_INDEX] = HOGPD_CFG_REPORT_OUT;
    db_cfg_params.cfg[0].report_id[HIDS_GDE_DATA_INDEX] = HIDS_GDE_DATA_OUT_REPORT_ID;

    db_cfg_params.cfg[0].report_char_cfg[HIDS_GDE_FEEDBACK_INDEX] = HOGPD_CFG_REPORT_IN;
    db_cfg_params.cfg[0].report_id[HIDS_GDE_FEEDBACK_INDEX] = HIDS_GDE_FEEDBACK_IN_REPORT_ID;

    return hogpd_init(&start_hdl, 0, 0, &db_cfg_params, p_cb, p_map);
}

/**
 ****************************************************************************************
 * @brief Destruction of the HOGPD module - due to a reset for instance.
 * This function clean-up allocated memory (attribute database is destroyed by another
 * procedure)
 *
 * @param[in]        reason     Detach reason
 ****************************************************************************************
 */
uint16_t hogpd_destroy(uint8_t reason)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;

    return (status);
}


/**
 ****************************************************************************************
 * @brief Handles Disconnection
 *
 * @param[in]        conidx     Connection index
 ****************************************************************************************
 */
void hogpd_cleanup(uint8_t conidx, uint16_t reason)
{
    uint8_t svc_idx;
//    ASSERT_ERR(conidx < BLE_CONNECTION_MAX);
    // Reset the notification configuration to ensure that no notification will be sent on
    // a disconnected link
    for (svc_idx = 0; svc_idx < p_hogpd_env->hids_nb; svc_idx++)
    {
        p_hogpd_env->svcs[svc_idx].ntf_cfg[conidx] = 0;
    }
}


#endif /* BLE_HID_DEVICE */

/// @} HOGPD
