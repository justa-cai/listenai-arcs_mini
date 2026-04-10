/**
 * @file ml307_modem.c
 * @brief ML307 4G Module modem Implementation
 * @details High-level ML307 module management
 *          Reference: c_version/ml307_at_modem.c
 */

#include "ml307_modem.h"
#include "at_uart.h"
#include "ml307_tcp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "semphr.h"

#define TAG "ml307_modem"
#include "lisa_log.h"



/* Bit position macros for event groups */
#ifndef BIT0
#define BIT0    (1 << 0)
#define BIT1    (1 << 1)
#define BIT2    (1 << 2)
#define BIT3    (1 << 3)
#define BIT4    (1 << 4)
#define BIT5    (1 << 5)
#define BIT6    (1 << 6)
#define BIT7    (1 << 7)
#define BIT8    (1 << 8)
#define BIT9    (1 << 9)
#define BIT10   (1 << 10)
#define BIT11   (1 << 11)
#define BIT12   (1 << 12)
#define BIT13   (1 << 13)
#define BIT14   (1 << 14)
#define BIT15   (1 << 15)
#endif

/* Maximum connection IDs (shared between TCP and UDP) */
#define MAX_CONNECT_IDS  5

/* Connection ID allocation bitmap (bit 1 = in use, bit 0 = free) */
static uint8_t connect_id_bitmap = 0;

/* Event bits for network status 如果bit位不够，可考虑新增一个事件标志组*/
#define NETWORK_EVENT_REGISTERED            BIT0
#define NETWORK_EVENT_IP_READY              BIT1
#define NETWORK_EVENT_IMEI_READY            BIT2
#define NETWORK_EVENT_ICCID_READY           BIT3
#define NETWORK_EVENT_CSQ_READY             BIT4
#define NETWORK_EVENT_CGMR_READY            BIT5
#define NETWORK_EVENT_COPS_READY            BIT6
#define NETWORK_EVENT_CEREG_QUERY_READY     BIT7
#define NETWORK_EVENT_MIPCALL_QUERY_READY   BIT8
#define NETWORK_EVENT_CGPADDR_READY         BIT9
#define NETWORK_EVENT_CPIN_READY            BIT10
#define NETWORK_EVENT_DNS_READY             BIT11

/**
 * @brief ML307 modem structure
 */
struct ml307_modem {
    bool initialized;                           /**< Initialization flag */
    network_status_t network_status;            /**< Current network status */
    bool network_ready;                         /**< Network ready flag */
    char ip_address[16];                        /**< IP address */

    /* Module information */
    char imei[20];                              /**< IMEI */
    char iccid[24];                             /**< ICCID */
    int rssi;                                   /**< Signal strength RSSI */
    int ber;                                    /**< Bit error rate */
    char module_revision[64];                   /**< Module revision */
    char carrier_name[32];                      /**< Carrier name */

    /* Query results */
    int cereg_n;                                /**< CEREG URC mode */
    int cereg_stat;                             /**< CEREG registration status */
    int mipcall_cid;                            /**< MIPCALL context ID */
    int mipcall_status;                         /**< MIPCALL status */
    char mipcall_ip[16];                        /**< MIPCALL IP address */
    char cgpaddr_ip[16];                        /**< CGPADDR IP address */
    char cpin_status[32];                       /**< CPIN status (READY, SIM PIN, etc.) */
    char dns_resolved_ip[16];                   /**< DNS resolved IP address */

    EventGroupHandle_t event_group;             /**< Event group for synchronization */
    at_urc_callback_node_t *urc_node;           /**< URC callback node */
    SemaphoreHandle_t dns_mutex;                /**< DNS query mutex to prevent concurrent queries */
};

/* Global modem instance */
static ml307_modem_t modem = {0};

/**
 * @brief URC callback handler
 */
static void ml307_modem_urc_handler(const char *command, at_arg_value_t *arguments,
                                     size_t arg_count, void *user_data)
{
    (void)user_data;  /* Unused, use global modem instead */

    /* +CREG: <n>,<stat> - Network registration */
    if (strcmp(command, "CREG") == 0 && arg_count >= 2) {
        if (arguments[1].type == AT_ARG_TYPE_INT) {
            int stat = arguments[1].data.int_val;

            switch (stat) {
                case 0:
                    modem.network_status = NETWORK_STATUS_DISCONNECTED;
                    modem.network_ready = false;
                    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_REGISTERED);
                    LISA_LOGW(TAG, "Network disconnected");
                    break;
                case 1:
                    modem.network_status = NETWORK_STATUS_REGISTERED_HOME;
                    xEventGroupSetBits(modem.event_group, NETWORK_EVENT_REGISTERED);
                    LISA_LOGI(TAG, "Network registered (home)");
                    break;
                case 2:
                    modem.network_status = NETWORK_STATUS_SEARCHING;
                    LISA_LOGI(TAG, "Searching for network");
                    break;
                case 3:
                    modem.network_status = NETWORK_STATUS_DENIED;
                    LISA_LOGE(TAG, "Network registration denied");
                    break;
                case 5:
                    modem.network_status = NETWORK_STATUS_REGISTERED_ROAMING;
                    xEventGroupSetBits(modem.event_group, NETWORK_EVENT_REGISTERED);
                    LISA_LOGI(TAG, "Network registered (roaming)");
                    break;
                default:
                    modem.network_status = NETWORK_STATUS_UNKNOWN;
                    break;
            }
        }
    }
    /* +MIPCALL: <cid>,<status>,"<ip>" - PDP context status */
    else if (strcmp(command, "MIPCALL") == 0 && arg_count >= 3) {
        if (arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[1].type == AT_ARG_TYPE_INT &&
            arguments[2].type == AT_ARG_TYPE_STRING &&
            arguments[2].data.string_val.value) {  /* 添加NULL检查 */

            modem.mipcall_cid = arguments[0].data.int_val;
            modem.mipcall_status = arguments[1].data.int_val;
            strncpy(modem.mipcall_ip, arguments[2].data.string_val.value, sizeof(modem.mipcall_ip) - 1);
            modem.mipcall_ip[sizeof(modem.mipcall_ip) - 1] = '\0';

            /* If status is 1 (activated), update network ready state */
            if (modem.mipcall_status == 1) {
                strncpy(modem.ip_address, modem.mipcall_ip, sizeof(modem.ip_address) - 1);
                modem.ip_address[sizeof(modem.ip_address) - 1] = '\0';
                modem.network_ready = true;
                modem.network_status = NETWORK_STATUS_READY;
                LISA_LOGI(TAG, "PDP context activated, IP: %s", modem.mipcall_ip);
            }

            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_IP_READY);
            LISA_LOGD(TAG, "MIPCALL: cid=%d, status=%d, IP=%s",
                     modem.mipcall_cid, modem.mipcall_status, modem.mipcall_ip);
        }
    }
    /* +MATREADY - Module ready after reboot */
    else if (strcmp(command, "MATREADY") == 0) {
        LISA_LOGW(TAG, "Module restarted");
        modem.network_ready = false;
        modem.network_status = NETWORK_STATUS_DISCONNECTED;
        xEventGroupClearBits(modem.event_group,
                            NETWORK_EVENT_REGISTERED | NETWORK_EVENT_IP_READY);
    }
    /* +CSQ: <rssi>,<ber> - Signal quality */
    else if (strcmp(command, "CSQ") == 0 && arg_count >= 2) {
        if (arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[1].type == AT_ARG_TYPE_INT) {
            modem.rssi = arguments[0].data.int_val;
            modem.ber = arguments[1].data.int_val;
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_CSQ_READY);
            LISA_LOGD(TAG, "Signal quality: RSSI=%d, BER=%d", modem.rssi, modem.ber);
        }
    }
    /* +CGSN: <imei> - IMEI */
    else if (strcmp(command, "CGSN") == 0 && arg_count >= 1) {
        if (arguments[0].type == AT_ARG_TYPE_STRING &&
            arguments[0].data.string_val.value) {
            strncpy(modem.imei, arguments[0].data.string_val.value, sizeof(modem.imei) - 1);
            modem.imei[sizeof(modem.imei) - 1] = '\0';
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_IMEI_READY);
            LISA_LOGD(TAG, "IMEI: %s", modem.imei);
        }
    }
    /* +ICCID: <iccid> - SIM card ICCID */
    else if (strcmp(command, "ICCID") == 0 && arg_count >= 1) {
        if (arguments[0].type == AT_ARG_TYPE_STRING &&
            arguments[0].data.string_val.value) {
            strncpy(modem.iccid, arguments[0].data.string_val.value, sizeof(modem.iccid) - 1);
            modem.iccid[sizeof(modem.iccid) - 1] = '\0';
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_ICCID_READY);
            LISA_LOGD(TAG, "ICCID: %s", modem.iccid);
        }
    }
    /* +CGMR: <revision> - Module revision */
    else if (strcmp(command, "CGMR") == 0 && arg_count >= 1) {
        if (arguments[0].type == AT_ARG_TYPE_STRING &&
            arguments[0].data.string_val.value) {
            strncpy(modem.module_revision, arguments[0].data.string_val.value, sizeof(modem.module_revision) - 1);
            modem.module_revision[sizeof(modem.module_revision) - 1] = '\0';
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_CGMR_READY);
            LISA_LOGD(TAG, "Module revision: %s", modem.module_revision);
        }
    }
    /* +COPS: <mode>,<format>,"<oper>" - Carrier name */
    else if (strcmp(command, "COPS") == 0 && arg_count >= 3) {
        if (arguments[2].type == AT_ARG_TYPE_STRING &&
            arguments[2].data.string_val.value) {
            strncpy(modem.carrier_name, arguments[2].data.string_val.value, sizeof(modem.carrier_name) - 1);
            modem.carrier_name[sizeof(modem.carrier_name) - 1] = '\0';
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_COPS_READY);
            LISA_LOGD(TAG, "Carrier: %s", modem.carrier_name);
        }
    }
    /* +CEREG: <n>,<stat>[,<tac>,<ci>,<AcT>] - Network registration query response */
    else if (strcmp(command, "CEREG") == 0 && arg_count >= 2) {
        if (arguments[0].type == AT_ARG_TYPE_INT &&
            arguments[1].type == AT_ARG_TYPE_INT) {
            modem.cereg_n = arguments[0].data.int_val;
            modem.cereg_stat = arguments[1].data.int_val;
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_CEREG_QUERY_READY);
            LISA_LOGD(TAG, "CEREG query: n=%d, stat=%d", modem.cereg_n, modem.cereg_stat);
        }
    }
    /* +CGPADDR: <cid>,"<ip>" - IP address query response */
    else if (strcmp(command, "CGPADDR") == 0 && arg_count >= 2) {
        if (arguments[1].type == AT_ARG_TYPE_STRING &&
            arguments[1].data.string_val.value) {
            strncpy(modem.cgpaddr_ip, arguments[1].data.string_val.value, sizeof(modem.cgpaddr_ip) - 1);
            modem.cgpaddr_ip[sizeof(modem.cgpaddr_ip) - 1] = '\0';
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_CGPADDR_READY);
            LISA_LOGD(TAG, "CGPADDR query: IP=%s", modem.cgpaddr_ip);
        }
    }
    /* +CPIN: <status> - SIM card PIN status */
    else if (strcmp(command, "CPIN") == 0 && arg_count >= 1) {
        if (arguments[0].type == AT_ARG_TYPE_STRING &&
            arguments[0].data.string_val.value) {
            strncpy(modem.cpin_status, arguments[0].data.string_val.value, sizeof(modem.cpin_status) - 1);
            modem.cpin_status[sizeof(modem.cpin_status) - 1] = '\0';
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_CPIN_READY);
            LISA_LOGD(TAG, "CPIN status: %s", modem.cpin_status);
        }
    }
    /* +MDNSGIP: "<domain>","<ip>" - DNS resolution response */
    else if (strcmp(command, "MDNSGIP") == 0 && arg_count >= 2) {
        if (arguments[1].type == AT_ARG_TYPE_STRING &&
            arguments[1].data.string_val.value) {
            strncpy(modem.dns_resolved_ip, arguments[1].data.string_val.value, sizeof(modem.dns_resolved_ip) - 1);
            modem.dns_resolved_ip[sizeof(modem.dns_resolved_ip) - 1] = '\0';
            xEventGroupSetBits(modem.event_group, NETWORK_EVENT_DNS_READY);
            LISA_LOGI(TAG, "DNS resolved: %s", modem.dns_resolved_ip);
        }
    }
}

/**
 * @brief Check if network is ready
 */
bool ml307_modem_is_network_ready(void)
{
    return modem.network_ready;
}

/**
 * @brief Get network status
 */
network_status_t ml307_modem_get_network_status(void)
{
    return modem.network_status;
}

/**
 * @brief Allocate a free connection ID
 *
 * Connection IDs are shared between TCP and UDP (1-5)
 */
int ml307_modem_alloc_connect_id(void)
{
    for (int i = 0; i < MAX_CONNECT_IDS; i++) {
        if (!(connect_id_bitmap & (1 << i))) {
            connect_id_bitmap |= (1 << i);  /* Mark as in use */
            int conn_id = i + 1;  /* Return 1-5 instead of 0-4 */
            LISA_LOGD(TAG, "Allocated connection ID: %d", conn_id);
            return conn_id;
        }
    }
    LISA_LOGE(TAG, "No free connection ID available (all %d IDs in use)", MAX_CONNECT_IDS);
    return -1;
}

/**
 * @brief Free a connection ID
 */
void ml307_modem_free_connect_id(int connect_id)
{
    /* Convert from 1-5 range to 0-4 bitmap index */
    if (connect_id >= 1 && connect_id <= MAX_CONNECT_IDS) {
        int bitmap_index = connect_id - 1;
        connect_id_bitmap &= ~(1 << bitmap_index);  /* Mark as free */
        LISA_LOGD(TAG, "Freed connection ID: %d", connect_id);
    } else {
        LISA_LOGW(TAG, "Invalid connection ID to free: %d (valid range: 1-%d)", connect_id, MAX_CONNECT_IDS);
    }
}

/**
 * @brief Get module IMEI
 */
bool ml307_modem_get_imei(char *imei, size_t size)
{
    if (!imei || size == 0) return false;

    imei[0] = '\0';

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_IMEI_READY);

    /* Send query command */
    if (!at_uart_send_command("AT+CGSN=1", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send IMEI query command");
        return false;
    }

    /* Wait for URC response */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_IMEI_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_IMEI_READY) {
        strncpy(imei, modem.imei, size - 1);
        imei[size - 1] = '\0';
        LISA_LOGI(TAG, "Query IMEI: %s", modem.imei);
        return true;
    }

    LISA_LOGE(TAG, "Failed to get IMEI: timeout");
    return false;
}

/**
 * @brief Get SIM ICCID
 */
bool ml307_modem_get_iccid(char *iccid, size_t size)
{
    if (!iccid || size == 0) return false;

    iccid[0] = '\0';

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_ICCID_READY);

    /* Send query command */
    if (!at_uart_send_command("AT+ICCID", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send ICCID query command");
        return false;
    }

    /* Wait for URC response */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_ICCID_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_ICCID_READY) {
        strncpy(iccid, modem.iccid, size - 1);
        iccid[size - 1] = '\0';
        LISA_LOGI(TAG, "Query ICCID: %s", modem.iccid);
        return true;
    }

    LISA_LOGE(TAG, "Failed to get ICCID: timeout");
    return false;
}

/**
 * @brief Get signal quality (CSQ)
 */
bool ml307_modem_get_signal_quality(int *rssi, int *ber)
{
    if (!rssi || !ber) return false;

    *rssi = 99;  /* Unknown */
    *ber = 99;   /* Unknown */

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_CSQ_READY);

    /* Send query command */
    if (!at_uart_send_command("AT+CSQ", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send CSQ query command");
        return false;
    }

    /* Wait for URC response */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_CSQ_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_CSQ_READY) {
        *rssi = modem.rssi;
        *ber = modem.ber;
        LISA_LOGI(TAG, "Query CSQ: RSSI=%d, BER=%d", modem.rssi, modem.ber);
        return true;
    }

    LISA_LOGE(TAG, "Failed to get signal quality: timeout");
    return false;
}

/**
 * @brief Get module revision
 */
bool ml307_modem_get_module_revision(char *revision, size_t size)
{
    if (!revision || size == 0) return false;

    revision[0] = '\0';

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_CGMR_READY);

    /* Send query command */
    if (!at_uart_send_command("AT+CGMR", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send module revision query command");
        return false;
    }

    /* Wait for URC response */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_CGMR_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_CGMR_READY) {
        strncpy(revision, modem.module_revision, size - 1);
        revision[size - 1] = '\0';
        LISA_LOGI(TAG, "Query Module Revision: %s", modem.module_revision);
        return true;
    }

    LISA_LOGE(TAG, "Failed to get module revision: timeout");
    return false;
}

/**
 * @brief Get carrier name
 */
bool ml307_modem_get_carrier_name(char *carrier, size_t size)
{
    if (!carrier || size == 0) return false;

    carrier[0] = '\0';

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_COPS_READY);

    /* Send query command */
    if (!at_uart_send_command("AT+COPS?", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send carrier name query command");
        return false;
    }

    /* Wait for URC response */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_COPS_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_COPS_READY) {
        strncpy(carrier, modem.carrier_name, size - 1);
        carrier[size - 1] = '\0';
        LISA_LOGI(TAG, "Query Carrier: %s", modem.carrier_name);
        return true;
    }

    LISA_LOGE(TAG, "Failed to get carrier name: timeout");
    return false;
}

/**
 * @brief Query SIM card PIN status (AT+CPIN?)
 */
bool ml307_modem_query_cpin(char *status, size_t size)
{
    if (!status || size == 0) return false;

    status[0] = '\0';

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_CPIN_READY);

    /* Send query command */
    if (!at_uart_send_command("AT+CPIN?", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send CPIN query command");
        return false;
    }

    /* Wait for URC response */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_CPIN_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_CPIN_READY) {
        strncpy(status, modem.cpin_status, size - 1);
        status[size - 1] = '\0';
        LISA_LOGI(TAG, "Query CPIN: %s", modem.cpin_status);
        return true;
    }

    LISA_LOGE(TAG, "Failed to query CPIN: timeout");
    return false;
}

/**
 * @brief Query network registration status (AT+CEREG?)
 */
bool ml307_modem_query_cereg(int *n, int *stat)
{
    if (!n || !stat) return false;

    *n = 0;
    *stat = 0;

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_CEREG_QUERY_READY);

    /* Send query command */
    if (!at_uart_send_command("AT+CEREG?", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send CEREG query command");
        return false;
    }

    /* Wait for URC response */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_CEREG_QUERY_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_CEREG_QUERY_READY) {
        *n = modem.cereg_n;
        *stat = modem.cereg_stat;
        LISA_LOGI(TAG, "Query CEREG: n=%d, stat=%d", modem.cereg_n, modem.cereg_stat);
        return true;
    }

    LISA_LOGE(TAG, "Failed to query CEREG: timeout");
    return false;
}

/**
 * @brief Query PDP context status (AT+MIPCALL?)
 */
bool ml307_modem_query_mipcall(int *cid, int *status, char *ip, size_t ip_size)
{
    if (cid) *cid = 0;
    if (status) *status = 0;
    if (ip && ip_size > 0) ip[0] = '\0';

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_IP_READY);

    /* Send query command */
    if (!at_uart_send_command("AT+MIPCALL?", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send MIPCALL query command");
        return false;
    }

    /* Wait for URC response - use existing NETWORK_EVENT_IP_READY since MIPCALL response is already handled */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_IP_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_IP_READY) {
        if (cid) *cid = modem.mipcall_cid;
        if (status) *status = modem.mipcall_status;
        if (ip && ip_size > 0) {
            strncpy(ip, modem.mipcall_ip, ip_size - 1);
            ip[ip_size - 1] = '\0';
        }
        LISA_LOGI(TAG, "Query MIPCALL: cid=%d, status=%d, IP=%s", modem.mipcall_cid, modem.mipcall_status, modem.mipcall_ip);
        return true;
    }

    LISA_LOGE(TAG, "Failed to query MIPCALL: timeout");
    
    return false;
}

/**
 * @brief Query IP address (AT+CGPADDR)
 */
bool ml307_modem_query_cgpaddr(char *ip, size_t size)
{
    if (!ip || size == 0) return false;

    ip[0] = '\0';

    /* Clear event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_CGPADDR_READY);

    /* Send query command - default CID is 1 */
    if (!at_uart_send_command("AT+CGPADDR=1", 1000, true)) {
        LISA_LOGE(TAG, "Failed to send CGPADDR query command");
        return false;
    }

    /* Wait for URC response */
    EventBits_t bits = xEventGroupWaitBits(modem.event_group,
                                           NETWORK_EVENT_CGPADDR_READY,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(1000));

    if (bits & NETWORK_EVENT_CGPADDR_READY) {
        strncpy(ip, modem.cgpaddr_ip, size - 1);
        ip[size - 1] = '\0';
        LISA_LOGI(TAG, "Query CGPADDR: IP=%s", modem.cgpaddr_ip);
        return true;
    }

    LISA_LOGE(TAG, "Failed to query IP address: timeout");
    return false;
}

/**
 * @brief Reboot ML307 module
 */
void ml307_modem_reboot(void)
{
    LISA_LOGI(TAG, "Rebooting ML307 module...");
    at_uart_send_command("AT+MREBOOT=0", 1000, true);

    /* Clear network status */
    modem.network_ready = false;
    modem.network_status = NETWORK_STATUS_DISCONNECTED;
    xEventGroupClearBits(modem.event_group,
                        NETWORK_EVENT_REGISTERED | NETWORK_EVENT_IP_READY);
}

/**
 * @brief Set sleep mode
 */
bool ml307_modem_set_sleep_mode(bool enable, int delay_seconds)
{
    LISA_LOGI(TAG, "Setting sleep mode: %s (delay=%ds)", enable ? "enabled" : "disabled", delay_seconds);

    char command[64];

    if (enable) {
        if (delay_seconds > 0) {
            snprintf(command, sizeof(command), "AT+MLPMCFG=\"delaysleep\",%d", delay_seconds);
            at_uart_send_command(command, 1000, true);
        }
        return at_uart_send_command("AT+MLPMCFG=\"sleepmode\",2,0", 1000, true);
    } else {
        return at_uart_send_command("AT+MLPMCFG=\"sleepmode\",0,0", 1000, true);
    }
}

/**
 * @brief Wait for network ready
 */
network_status_t ml307_network_check(void)
{
    LISA_LOGI(TAG, "%s ...", __func__);

    /* Clear network ready state */
    modem.network_ready = false;
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_REGISTERED | NETWORK_EVENT_IP_READY);

    /* Check SIM card status */
    LISA_LOGI(TAG, "Checking SIM card status...");
    char cpin_status[32] = {0};
    bool pin_ready = false;
    for (int i = 0; i < 5; i++) {
        if (ml307_modem_query_cpin(cpin_status, sizeof(cpin_status))) {
            /* Check if status is "READY" */
            if (strcmp(cpin_status, "READY") == 0) {
                pin_ready = true;
                LISA_LOGI(TAG, "SIM card ready");
                break;
            } else {
                LISA_LOGW(TAG, "SIM card status: %s", cpin_status);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!pin_ready) {
        LISA_LOGE(TAG, "SIM card not ready, status: %s", cpin_status);
        return NETWORK_STATUS_ERROR;
    }

    /* Enable network registration URC with retry */
    bool cereg_enabled = false;
    for (int i = 0; i < 5; i++) {
        if (at_uart_send_command("AT+CEREG=2", 1000, true)) {
            cereg_enabled = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!cereg_enabled) {
        LISA_LOGE(TAG, "Failed to enable CEREG URC after 5 retries");
        return NETWORK_STATUS_ERROR;
    }

    /* Query network registration status with retry */
    int n = 0, stat = 0;
    bool cereg_queried = false;
    for (int i = 0; i < 5; i++) {
        if (ml307_modem_query_cereg(&n, &stat)) {
            cereg_queried = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!cereg_queried) {
        LISA_LOGE(TAG, "Failed to query network registration after 5 retries");
        return NETWORK_STATUS_ERROR;
    }

    
    /* Query and log RSSI, ICCID, and IMEI */
    // int rssi = 99, ber = 99;
    // char iccid[24] = {0};
    // char imei[20] = {0};

    // if (ml307_modem_get_signal_quality(&rssi, &ber)) {
    //     LISA_LOGI(TAG, "Signal Quality - RSSI: %d, BER: %d", rssi, ber);
    // } else {
    //     LISA_LOGW(TAG, "Failed to query signal quality");
    // }

    // if (ml307_modem_get_iccid(iccid, sizeof(iccid))) {
    //     LISA_LOGI(TAG, "ICCID: %s", iccid);
    // } else {
    //     LISA_LOGW(TAG, "Failed to query ICCID");
    // }

    // if (ml307_modem_get_imei(imei, sizeof(imei))) {
    //     LISA_LOGI(TAG, "IMEI: %s", imei);
    // } else {
    //     LISA_LOGW(TAG, "Failed to query IMEI");
    // }

    LISA_LOGI(TAG, "Network registered, waiting for IP...");
    /* Wait for IP address with retry，使用AT+MIPCALL查询IP地址和PDP状态 */
    int cid = 0, status = 0;
    char ip[16] = {0};
    for (int i = 0; i < 5; i++) {
        /* Query IP address using MIPCALL command */
        if (ml307_modem_query_mipcall(&cid, &status, ip, sizeof(ip))) {
            if (status == 1 && strlen(ip) > 0) {
                LISA_LOGI(TAG, "Network ready with IP: %s", ip);
                strncpy(modem.ip_address, ip, sizeof(modem.ip_address) - 1);
                modem.network_ready = true;
                return NETWORK_STATUS_READY;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    LISA_LOGW(TAG, "Network registered but no IP address assigned");

    return modem.network_status;
}

/**
 * @brief Initialize ML307 modem
 */
bool ml307_modem_init(const char *uart_dev)
{
    LISA_LOGI(TAG, "Initializing ML307 modem...");

    if (modem.initialized) {
        LISA_LOGW(TAG, "ML307 modem already initialized");
        return true;
    }

    /* Initialize AT UART */
    if (at_uart_init(uart_dev) != 0) {
        LISA_LOGE(TAG, "Failed to initialize AT UART");
        return false;
    }

    /* Create event group */
    modem.event_group = xEventGroupCreate();
    if (!modem.event_group) {
        LISA_LOGE(TAG, "Failed to create event group");
        at_uart_deinit();
        return false;
    }

    /* Create DNS mutex to prevent concurrent DNS queries */
    modem.dns_mutex = xSemaphoreCreateMutex();
    if (!modem.dns_mutex) {
        LISA_LOGE(TAG, "Failed to create DNS mutex");
        vEventGroupDelete(modem.event_group);
        at_uart_deinit();
        return false;
    }

    /* Register URC callback */
    modem.urc_node = at_uart_register_urc_callback(ml307_modem_urc_handler, NULL);
    if (!modem.urc_node) {
        LISA_LOGE(TAG, "Failed to register URC callback");
        vSemaphoreDelete(modem.dns_mutex);
        vEventGroupDelete(modem.event_group);
        at_uart_deinit();
        return false;
    }

    if (!uart_baudrate_adapt()) {
        LISA_LOGE(TAG, "4G network is not ready~~~.");
        return false;
    }

    if (NETWORK_STATUS_READY != ml307_network_check()) {
        LISA_LOGE(TAG, "4G network is not ready.");
        return false;
    }

    ml307_tcp_start_prefetch_task();
    ml307_udp_start_prefetch_task();
    
    modem.initialized = true;
    LISA_LOGI(TAG, "ML307 modem initialized successfully");

    return true;
}

/**
 * @brief Deinitialize ML307 modem
 */
bool ml307_modem_deinit(void)
{
    if (!modem.initialized) return false;

    LISA_LOGI(TAG, "Deinitializing ML307 modem...");

    /* Unregister URC callback */
    if (modem.urc_node) {
        at_uart_unregister_urc_callback(modem.urc_node);
        modem.urc_node = NULL;
    }

    /* Delete DNS mutex */
    if (modem.dns_mutex) {
        vSemaphoreDelete(modem.dns_mutex);
        modem.dns_mutex = NULL;
    }

    /* Delete event group */
    if (modem.event_group) {
        vEventGroupDelete(modem.event_group);
        modem.event_group = NULL;
    }

    /* Deinitialize AT UART */
    at_uart_deinit();

    modem.initialized = false;
    LISA_LOGI(TAG, "ML307 modem deinitialized");

    return true;
}

/**
 * @brief Resolve domain name to IP address using ML307
 *
 * Example response:
 * AT+MDNSGIP="www.baidu.com"
 * OK
 * +MDNSGIP: "www.baidu.com","183.232.231.172"
 */
bool ml307_dns_resolve(const char *domain, char *ip_addr, size_t size)
{
    if (!domain || !ip_addr || size < 16) {
        LISA_LOGE(TAG, "Invalid DNS resolve parameters");
        return false;
    }

    if (!modem.initialized || !modem.event_group) {
        LISA_LOGE(TAG, "Modem not initialized");
        return false;
    }

    /* Acquire DNS mutex to prevent concurrent DNS queries (avoid CME ERROR 4) */
    if (!modem.dns_mutex || xSemaphoreTake(modem.dns_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        LISA_LOGW(TAG, "DNS query in progress, wait timeout");
        return false;
    }

    ip_addr[0] = '\0';

    /* Clear the DNS ready event bit */
    xEventGroupClearBits(modem.event_group, NETWORK_EVENT_DNS_READY);

    /* Build DNS query command */
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+MDNSGIP=\"%s\"", domain);

    /* Send DNS query command */
    if (!at_uart_send_command(cmd, 5000, true)) {
        int cme_error = at_uart_get_cme_error_code();
        LISA_LOGE(TAG, "Failed to send DNS query command, CME error=%d", cme_error);
        xSemaphoreGive(modem.dns_mutex);
        return false;
    }

    /* Wait for +MDNSGIP URC response (handled in URC callback) */
    /* The URC will be in format: +MDNSGIP: "domain","ip_address" */
    EventBits_t bits = xEventGroupWaitBits(
        modem.event_group,
        NETWORK_EVENT_DNS_READY,
        pdTRUE,  /* Clear on exit */
        pdFALSE, /* Wait for all bits (only one bit in this case) */
        pdMS_TO_TICKS(10000)  /* 10 second timeout */
    );

    if (!(bits & NETWORK_EVENT_DNS_READY)) {
        LISA_LOGE(TAG, "DNS query timeout for domain: %s", domain);
        xSemaphoreGive(modem.dns_mutex);
        return false;
    }

    /* Copy resolved IP from modem structure */
    strncpy(ip_addr, modem.dns_resolved_ip, size - 1);
    ip_addr[size - 1] = '\0';

    LISA_LOGI(TAG, "DNS resolved: %s -> %s", domain, ip_addr);

    /* Release DNS mutex */
    xSemaphoreGive(modem.dns_mutex);
    return true;
}
