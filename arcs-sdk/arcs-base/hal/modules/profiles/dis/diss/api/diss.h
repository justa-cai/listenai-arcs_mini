/**
 ****************************************************************************************
 *
 * @file diss.h
 *
 * @brief Header file - Device Info Service Server - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

#ifndef DISS_H_
#define DISS_H_

/**
 ****************************************************************************************
 * @addtogroup DISS Device Information Service Server
 * @brief Device Information Service Server - Native API.
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "diss_msg.h"
#include "ble_gatt.h"
#include "ble_prf.h"

/*
 * DEFINES
 ****************************************************************************************
 */
 /// Maximal length for Characteristic values - 128 bytes
#define DIS_VAL_MAX_LEN                         (128)
///System ID string length
#define DIS_SYS_ID_LEN                          (0x08)
///IEEE Certif length (min 6 bytes)
#define DIS_IEEE_CERTIF_MIN_LEN                 (0x06)
///PnP ID length
#define DIS_PNP_ID_LEN                          (0x07)

/// DISS Attributes database handle list
enum diss_att_db_handles
{
    DIS_IDX_SVC,

    DIS_IDX_MANUFACTURER_NAME_CHAR,
    DIS_IDX_MANUFACTURER_NAME_VAL,

    DIS_IDX_MODEL_NB_STR_CHAR,
    DIS_IDX_MODEL_NB_STR_VAL,

    DIS_IDX_SERIAL_NB_STR_CHAR,
    DIS_IDX_SERIAL_NB_STR_VAL,

    DIS_IDX_HARD_REV_STR_CHAR,
    DIS_IDX_HARD_REV_STR_VAL,

    DIS_IDX_FIRM_REV_STR_CHAR,
    DIS_IDX_FIRM_REV_STR_VAL,

    DIS_IDX_SW_REV_STR_CHAR,
    DIS_IDX_SW_REV_STR_VAL,

    DIS_IDX_SYSTEM_ID_CHAR,
    DIS_IDX_SYSTEM_ID_VAL,

//    DIS_IDX_IEEE_CHAR,
//    DIS_IDX_IEEE_VAL,

    DIS_IDX_PNP_ID_CHAR,
    DIS_IDX_PNP_ID_VAL,

    DIS_IDX_NB,
};


/*
 * TYPES DEFINITIONS
 ****************************************************************************************
 */
/// DIS server callback set
typedef struct diss_cb
{
    /**
     ****************************************************************************************
     * @brief This function is called when GATT server user has initiated event send to peer
     *        device or if an error occurs.
     *
     * @param[in] token         Procedure token that must be returned in confirmation function
     * @param[in] val_id        Requested value identifier (@see enum diss_val_id)
     ****************************************************************************************
     */
    uint16_t (*cb_value_get) (uint8_t conidx, uint8_t att_idx, uint8_t *p_value, uint16_t max_len, uint16_t *ret_len);
} diss_cb_t;


/*
 * API
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Initialize DISS service
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t ble_diss_init(diss_cb_t *p_cb);


/// @} DISSTASK
#endif // DISS_H_
