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

#include "log_print.h"
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


static int wifi_pwr_tbl_set_example(void)
{
    int res = 0;
    struct pwr_table pwr={18,18,18,18,
                          16,16,16,16,16,16,16,16,
                          17,17,17,16,16,16,16,15,
                          17,17,17,16,16,16,16,15,15,15};
    int8_t type = CHAN_ALL;

    CLOGI("11b pwr:%d %d %d %d", pwr.pwr_11b[0],pwr.pwr_11b[1],pwr.pwr_11b[2],pwr.pwr_11b[3]);
    CLOGI("11g pwr: %d %d %d %d %d %d %d %d", pwr.pwr_11g[0],pwr.pwr_11g[1],pwr.pwr_11g[2],pwr.pwr_11g[3],pwr.pwr_11g[4],pwr.pwr_11g[5],pwr.pwr_11g[6],pwr.pwr_11g[7]);
    CLOGI("11n pwr: %d %d %d %d %d %d %d %d ", pwr.pwr_11n_ht20[0],pwr.pwr_11n_ht20[1],pwr.pwr_11n_ht20[2],pwr.pwr_11n_ht20[3],pwr.pwr_11n_ht20[4],pwr.pwr_11n_ht20[5],pwr.pwr_11n_ht20[6],pwr.pwr_11n_ht20[7]);
    CLOGI("11ax pwr: %d %d %d %d %d %d %d %d %d %d", pwr.pwr_11ax_he20[0], pwr.pwr_11ax_he20[1],pwr.pwr_11ax_he20[2],pwr.pwr_11ax_he20[3],pwr.pwr_11ax_he20[4],pwr.pwr_11ax_he20[5],pwr.pwr_11ax_he20[6],pwr.pwr_11ax_he20[7],pwr.pwr_11ax_he20[8],pwr.pwr_11ax_he20[9]);


    res = wifi_set_max_tx_pwr(&pwr, type);

    return res;
}

void ls_wifi_init(void)
{

    wifi_ops_register(&ops);
    wifi_init();

    //wifi_pwr_tbl_set_example();
}
