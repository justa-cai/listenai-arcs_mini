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
static void wifi_mgmt_frame_process_example(uint8_t *frame, uint32_t len, void *arg)
{
    struct wifi_mac_hdr *hdr;

    //hdr = (struct wifi_mac_hdr *)frame;
    //CLOGV("frame type %x len %d \n", hdr->fctl, len);
    // customer_cb(frame, len, arg, other para);
}

/*
 * MAIN FUNCTION
 ****************************************************************************************
 */



struct wifi_ops ops = {
    .get_mac = ls_get_wifi_mac,
    .get_temp = ls_read_temp_voltage,
};

void ls_wifi_init(void)
{
    wifi_mgmt_frame_cb_register(wifi_mgmt_frame_process_example, NULL);

    wifi_ops_register(&ops);
    wifi_init();
}
