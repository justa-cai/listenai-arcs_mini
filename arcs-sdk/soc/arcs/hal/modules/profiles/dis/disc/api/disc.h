/**
 ****************************************************************************************
 *
 * @file disc.h
 *
 * @brief Header file - Device Information Service Client - Native API.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef DISC_H_
#define DISC_H_

/**
 ****************************************************************************************
 * @addtogroup DISC Device Information Service Client
 * @brief Device Information Service Client - Native API
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "disc_msg.h"

#include "ble_gatt.h"
#include "ble_prf.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * ENUMERATIONS
 ****************************************************************************************
 */

/*
 * TYPES DEFINITIONS
 ****************************************************************************************
 */
/// DIS client callback set
typedef struct disc_cb
{
    /**
     ****************************************************************************************
     * @brief This function is called when GATT server user has initiated event send to peer
     *        device or if an error occurs.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Client Enable status (@see enum hl_err)
     * @param[in] p_dis         Pointer to bond data information that describe peer database
     ****************************************************************************************
     */
    void (*cb_enable_cmp) (uint8_t conidx, uint16_t status, const disc_dis_content_t* p_dis);


    /**
     ****************************************************************************************
     * @brief This function is called when GATT server user has initiated event send to peer
     *        device or if an error occurs.
     *
     * @param[in] conidx        Connection index
     * @param[in] status        Read status (@see enum hl_err)
     * @param[in] val_id        Value identifer read (@see enum disc_val_id)
     * @param[in] length        Value data length
     * @param[in] p_data        Pointer to value data
     ****************************************************************************************
     */
    void (*cb_read_val_cmp) (uint8_t conidx, uint16_t status, uint8_t val_id, const uint8_t* p_data, uint16_t length);
} disc_cb_t;


/*
 * API
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Enable DIS client profile by doing a discovery or restoring bond data
 *
 * @param[in] conidx        Connection index
 * @param[in] con_type      Connection type (@see enum prf_con_type)
 * @param[in] p_dis         Pointer to bond data information (valid if con_type == PRF_CON_NORMAL)
 *
 * @return Function execution status (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t disc_enable(uint8_t conidx, uint8_t con_type, const disc_dis_content_t* p_dis);

/**
 ****************************************************************************************
 * @brief Read a DIS value in peer database
 *
 * @param[in] conidx   Connection index
 * @param[in] value_id Value identifier (@see enum disc_val_id)
 *
 * @return Function execution status (@see enum hl_err)
 ****************************************************************************************
 */
uint16_t disc_read_val(uint8_t conidx, uint8_t val_id);

/// @} DISC

#endif // DISC_H_
