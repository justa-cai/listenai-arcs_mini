/**
 ****************************************************************************************
 *
 * @file spp.h
 *
 * @brief Header file - Serial Port Profile - Native API.
 *
 * Copyright (C) Listenai.com 2021
 *
 ****************************************************************************************
 */

#ifndef _SPP_H_
#define _SPP_H_

/**
 ****************************************************************************************
 * @addtogroup SPP
 * @ingroup Profile
 * @brief Serial Port Profile - Native API.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "spp_msg.h"


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

typedef void (*spp_data_cb)(uint8_t conidx, uint8_t port, co_buf_t* p_sdu);

/*
 * NATIVE API CALLBACKS
 ****************************************************************************************
 */

/*
 * NATIVE API FUNCTIONS
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief set spp data receive callback
 *
 * @param[in] conidx    connection index
 * @param[in] port      Port id
 * @param[in] cb        data receive callback
 *
 * @return              error code
 ****************************************************************************************
 */
uint16_t spp_set_data_cb(uint8_t conidx, uint8_t port, spp_data_cb cb);

/**
 ****************************************************************************************
 * @brief Send data to spp port
 *
 * @param[in] conidx    connection index
 * @param[in] port      Port id
 * @param[in] p_sdu     data to send
 *
 * @return              error code
 ****************************************************************************************
 */
uint16_t spp_send_data(uint8_t conidx, uint8_t port, co_buf_t* p_sdu);

/// @} SPP

#endif /* _SPP_H_ */
