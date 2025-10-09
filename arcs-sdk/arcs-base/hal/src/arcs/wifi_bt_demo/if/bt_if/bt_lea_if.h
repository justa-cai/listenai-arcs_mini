/**
 ****************************************************************************************
 *
 * @file bt_stack_if.h
 *
 * @brief Header file - BT STACK INTERFACE.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 *
 ****************************************************************************************
 */

#ifndef BT_LEA_IF_H_
#define BT_LEA_IF_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "os_task_init.h"
//#include "ble_lea.h"
/*
 * DEFINES
 ****************************************************************************************
 */
 #define LEA_SERVICE_MASK (1 << LEA_MICS_ID | \
                           1 << LEA_VOCS_ID  | \
                           1 << LEA_AICS_ID | \
                           1 << LEA_VCS_ID  | \
                           1 << LEA_MCS_ID | \
                           1 << LEA_GMCS_ID  | \
                           1 << LEA_TBS_ID | \
                           1 << LEA_GTBS_ID  | \
                           1 << LEA_OTS_ID | \
                           1 << LEA_PACS_ID  | \
                           1 << LEA_ASCS_ID | \
                           1 << LEA_BAASS_ID  | \
                           1 << LEA_CSIS_ID | \
                           1 << LEA_CAS_ID  | \
                           1 << LEA_TMAS_ID | \
                           1 << LEA_HAS_ID  | \
                           1 << LEA_IAS_ID)
/*
 * ENUMERATIONS
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */
void bt_stack_lea_enable(void);
/// @} BT STACK
#endif // BT_LEA_IF_H_
