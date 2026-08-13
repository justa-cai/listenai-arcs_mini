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
#include "bt_storage_port.h"
#include "log_print.h"
//#include "src_configure.h"

#ifndef CONFIG_LISA_BLUETOOTH_STORAGE_NVS
#define CONFIG_LISA_BLUETOOTH_STORAGE_NVS 0
#endif

#ifndef CONFIG_LISA_BLUETOOTH_STORAGE_KV
#define CONFIG_LISA_BLUETOOTH_STORAGE_KV 0
#endif

#define BT_NVDS_SUPPORT  (CONFIG_LISA_BLUETOOTH_STORAGE_NVS || CONFIG_LISA_BLUETOOTH_STORAGE_KV)

extern uint8_t lsip_nvds_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf);
extern uint8_t lsip_nvds_set(uint8_t param_id, uint8_t length, uint8_t *buf);
extern uint8_t lsip_nvds_del(uint8_t param_id);
extern void lsip_nvds_init(struct lsip_nvds_api* lsip_nvs_param);

uint8_t lsip_nvds_get(uint8_t param_id, uint8_t * lengthPtr, uint8_t *buf)
{
    uint8_t status = NVDS_FAIL;
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    status = bt_storage_port_get(param_id, lengthPtr, buf);
#elif CONFIG_LISA_BLUETOOTH_STORAGE_NVS
    uint32_t len = *lengthPtr;
    status = nvds_get(param_id, (size_t *)&len, buf);
    *lengthPtr = len;
#endif
    return (status);
}
uint8_t lsip_nvds_set(uint8_t param_id, uint8_t length, uint8_t *buf)
{
    uint8_t status = NVDS_FAIL;
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    status = bt_storage_port_set(param_id, length, buf);
#elif CONFIG_LISA_BLUETOOTH_STORAGE_NVS
    status =  nvds_put(param_id, length, buf);
#endif
    return status;
}
uint8_t lsip_nvds_del(uint8_t param_id)
{
    uint8_t status = NVDS_FAIL;
#if CONFIG_LISA_BLUETOOTH_STORAGE_KV
    status = bt_storage_port_del(param_id);
#elif CONFIG_LISA_BLUETOOTH_STORAGE_NVS
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

