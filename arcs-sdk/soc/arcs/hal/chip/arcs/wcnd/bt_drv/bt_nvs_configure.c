/*
 * nvs_configure.c
 *
 *  Created on: Feb 3, 2023
 *      Author: USER
 */
#include "arcs_ap.h" 
#include "nvs.h"
#include "nvs_priv.h"
#include "spiflash.h"
#include "ble_plf_config.h"
#include "log_print.h"
//#include "src_configure.h"


#define BT_NVDS_SUPPORT  (CFG_NVS)


extern uint8_t lsip_nvds_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
extern uint8_t lsip_nvds_set(uint8_t param_id, uint8_t length, uint8_t *buf);
extern uint8_t lsip_nvds_del(uint8_t param_id);
extern void lsip_nvds_init(struct lsip_nvds_api* lsip_nvs_param);

uint8_t lsip_nvds_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf)
{
    uint8_t status = NVDS_FAIL;
#if (BT_NVDS_SUPPORT)
    size_t len = *lengthPtr;
    status = nvds_get(param_id, &len, buf);
    *lengthPtr = len;
    //CLOGD("lsip_nvs_get,id:0x%x, status:%d", param_id, status);
#endif
    return (status);
}
uint8_t lsip_nvds_set(uint8_t param_id, uint8_t length, uint8_t *buf)
{
    uint8_t status = NVDS_FAIL;
#if (BT_NVDS_SUPPORT)
    status =  nvds_put(param_id, length, buf);
    //CLOGD("lsip_nvs_set,id:0x%x, ret:%d", param_id, status);
#endif
    return status;
}
uint8_t lsip_nvds_del(uint8_t param_id)
{
    uint8_t status = NVDS_FAIL;
#if (BT_NVDS_SUPPORT)
    status = nvds_del(param_id);
#endif
    return  status;
}

int bt_nvs_init(void)
{
    struct lsip_nvds_api lsip_nvs_env={
        .get = lsip_nvds_get,
        .set = lsip_nvds_set,
        .del = lsip_nvds_del
        };
#if (BT_NVDS_SUPPORT)
    lsip_nvds_init(&lsip_nvs_env);
#endif

    return 0;
}

