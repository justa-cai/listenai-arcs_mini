/*
 * wifi_config.c
 *
 *  Created on: 2024-10-16
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <stdbool.h>          // standard boolean definitions
#include <stdint.h>           // standard integer functions
#include <string.h>

#include "ls_misc.h"
#include "wifi_api.h"
#include "ls_utils.h"
#include "ls_wifi_type.h"
/**
 ****************************************************************************************
 * @addtogroup DRIVERS
 * @{
 *
 *
 * ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/*
 * MAIN FUNCTION
 ****************************************************************************************
 */



struct wifi_ops ops = {
    .get_mac = ls_get_wifi_mac,
    .temp_update = ls_temp_por_update,
};

void ls_wifi_init(void)
{
    wifi_ops_register(&ops);
    wifi_init();
}
