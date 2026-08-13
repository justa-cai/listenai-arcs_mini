/**
 ****************************************************************************************
 *
 * @file ipc_mrpc_service.c
 *
 * @brief Built-in IPC MRPC service registration.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#include <stdint.h>
#include "ipc_mrpc_service.h"

#ifdef CFG_AMP_IPC_MRPC_SERVER_LWIP
#include "mrpc_lwip_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_NVS
#include "mrpc_nvs_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_FLASH_IF
#include "mrpc_flash_if_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M
#include "mrpc_utils_s2m_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_BT_S2M
#include "mrpc_bt_s2m_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_WIFI
#include "mrpc_wifi_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_BT
#include "mrpc_bt_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S
#include "mrpc_utils_m2s_api_server.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_LUNA
#include "mrpc_luna_api_server.h"
#endif
#ifdef CFG_AMP_IPC_BUS
#include "ipc_bus.h"
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_AP_PM_LOCK
#include "mrpc_ap_pm_lock_api_server.h"
#endif

#define IPC_MRPC_REGISTER_SERVICE(server, upper, lower) \
    mrpc_service_register(server, MRPC_SERVICE_TYPE_##upper, \
        mrpc_msg_##lower##_handlers, \
        MRPC_MSG_ID_##upper##_MAX - MRPC_MSG_ID_##upper##_START)

int32_t ipc_mrpc_register_master_services(struct mrpc_server_env *server)
{
    int32_t ret = 0;

    if (server == NULL)
        return -1;

#ifdef CFG_AMP_IPC_MRPC_SERVER_LWIP
    ret |= IPC_MRPC_REGISTER_SERVICE(server, LWIP, lwip);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_NVS
    ret |= IPC_MRPC_REGISTER_SERVICE(server, NVS, nvs);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_FLASH_IF
    ret |= IPC_MRPC_REGISTER_SERVICE(server, FLASH_IF, flash_if);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS_S2M
    ret |= IPC_MRPC_REGISTER_SERVICE(server, UTILS_S2M, utils_s2m);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_BT_S2M
    ret |= IPC_MRPC_REGISTER_SERVICE(server, BT_S2M, bt_s2m);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_LUNA
    ret |= IPC_MRPC_REGISTER_SERVICE(server, LUNA, luna);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_AP_PM_LOCK
    ret |= IPC_MRPC_REGISTER_SERVICE(server, AP_PM_LOCK, ap_pm_lock);
#endif

    return ret;
}

int32_t ipc_mrpc_register_slave_services(struct mrpc_server_env *server)
{
    int32_t ret = 0;

    if (server == NULL)
        return -1;

#ifdef CFG_AMP_IPC_MRPC_SERVER_WIFI
    ret |= IPC_MRPC_REGISTER_SERVICE(server, WIFI, wifi);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_BT
    ret |= IPC_MRPC_REGISTER_SERVICE(server, BT, bt);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_FLASH_IF
    ret |= IPC_MRPC_REGISTER_SERVICE(server, FLASH_IF, flash_if);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_UTILS_M2S
    ret |= IPC_MRPC_REGISTER_SERVICE(server, UTILS_M2S, utils_m2s);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_LUNA
    ret |= IPC_MRPC_REGISTER_SERVICE(server, LUNA, luna);
#endif
#ifdef CFG_AMP_IPC_MRPC_SERVER_AP_PM_LOCK
    ret |= IPC_MRPC_REGISTER_SERVICE(server, AP_PM_LOCK, ap_pm_lock);
#endif
    return ret;
}

#ifdef CFG_AMP_IPC_BUS
int32_t ipc_mrpc_register_bus_service(struct mrpc_server_env *server)
{
    if (server == NULL)
        return -1;

    return mrpc_service_register(server, MRPC_SERVICE_TYPE_IPC_BUS, ipc_bus_mrpc_handlers, IPC_BUS_MRPC_HANDLER_NUM);
}

int32_t ipc_mrpc_init_bus_client(void)
{
    return ipc_bus_mrpc_client_init();
}
#endif
