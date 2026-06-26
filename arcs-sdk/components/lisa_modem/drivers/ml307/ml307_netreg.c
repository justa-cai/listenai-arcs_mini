/**
 * @file ml307_netreg.c
 * @brief ML307 network registration and DNS helpers
 */

#include "drivers/ml307/ml307_netreg.h"
#include "drivers/ml307/ml307_endpoint_internal.h"
#include "core/modem_bits.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "semphr.h"
#include <string.h>
#include <lwip/inet.h>

#define TAG "ml307_netreg"
#include "lisa_log.h"

#define NETWORK_EVENT_REGISTERED  BIT0
#define NETWORK_EVENT_IP_READY    BIT1
#define ML307_DEFAULT_PDP_CID     1
#define ML307_NETREG_RETRY_COUNT  5
#define ML307_NETREG_WAIT_MS      100
#define ML307_DIAL_TIMEOUT_MS     10000

typedef struct { char *buf; size_t size; } str_out_t;
typedef struct { int *cid; int *status; char *ip; size_t ip_size; } mipcall_out_t;

static void ml307_status_set_error(ml307_endpoint_ctx_t *modem, lisa_modem_error_t error)
{
    int cme_error;

    if (!modem) {
        return;
    }

    cme_error = at_client_get_cme_error(modem->client);
    if (cme_error > 0) {
        lisa_modem_status_note_cme(&modem->status, cme_error);
    }
    if (modem->status.last_error != LISA_MODEM_ERR_TRAFFIC_EXCEEDED) {
        modem->status.last_error = error;
    }
}

static lisa_modem_error_t ml307_status_error_from_cpin(const char *status)
{
    if (!status || status[0] == '\0') {
        return LISA_MODEM_ERR_SIM_QUERY_FAILED;
    }
    if (strstr(status, "NOT INSERTED")) {
        return LISA_MODEM_ERR_SIM_NOT_INSERTED;
    }
    return LISA_MODEM_ERR_SIM_NOT_READY;
}

static bool parse_first_string(at_arg_value_t *args, size_t count, void *user_data)
{
    str_out_t *out = (str_out_t *)user_data;
    if (count < 1 || args[0].type != AT_ARG_TYPE_STRING || !args[0].data.string_val.value) {
        return false;
    }
    strncpy(out->buf, args[0].data.string_val.value, out->size - 1);
    out->buf[out->size - 1] = '\0';
    return true;
}

static bool parse_cereg(at_arg_value_t *args, size_t count, void *user_data)
{
    int *out = (int *)user_data;
    if (count < 2 || args[0].type != AT_ARG_TYPE_INT || args[1].type != AT_ARG_TYPE_INT) {
        return false;
    }
    out[0] = args[0].data.int_val;
    out[1] = args[1].data.int_val;
    return true;
}

static bool parse_mipcall(at_arg_value_t *args, size_t count, void *user_data)
{
    mipcall_out_t *out = (mipcall_out_t *)user_data;
    if (count < 3 || args[0].type != AT_ARG_TYPE_INT || args[1].type != AT_ARG_TYPE_INT) {
        return false;
    }
    if (out->cid) {
        *out->cid = args[0].data.int_val;
    }
    if (out->status) {
        *out->status = args[1].data.int_val;
    }
    if (out->ip && out->ip_size > 0 &&
        args[2].type == AT_ARG_TYPE_STRING && args[2].data.string_val.value) {
        strncpy(out->ip, args[2].data.string_val.value, out->ip_size - 1);
        out->ip[out->ip_size - 1] = '\0';
    }
    return true;
}

static bool parse_dns(at_arg_value_t *args, size_t count, void *user_data)
{
    str_out_t *out = (str_out_t *)user_data;
    const char *selected = NULL;
    const char *fallback = NULL;
    ip_addr_t addr;

    if (!out || !out->buf || out->size == 0 || count < 2) {
        return false;
    }

    for (size_t i = 1; i < count; ++i) {
        if (args[i].type != AT_ARG_TYPE_STRING || !args[i].data.string_val.value) {
            continue;
        }

        if (!fallback) {
            fallback = args[i].data.string_val.value;
        }

        if (ipaddr_aton(args[i].data.string_val.value, &addr) && !IP_IS_V6_VAL(addr)) {
            selected = args[i].data.string_val.value;
            break;
        }
    }

    if (!selected) {
        selected = fallback;
    }

    if (!selected) {
        return false;
    }

    strncpy(out->buf, selected, out->size - 1);
    out->buf[out->size - 1] = '\0';
    return true;
}

static bool ml307_netreg_query_cpin(ml307_endpoint_ctx_t *modem, char *status, size_t size);
static bool ml307_netreg_query_cereg(ml307_endpoint_ctx_t *modem, int *n, int *stat);
static bool ml307_netreg_query_mipcall(ml307_endpoint_ctx_t *modem, int *cid, int *status, char *ip, size_t ip_size);

static bool ml307_netreg_activate_pdp(ml307_endpoint_ctx_t *modem, int cid)
{
    int active_cid = 0;
    int status = 0;
    char ip[sizeof(modem->ip_address)] = {0};
    EventBits_t bits;

    if (!modem || cid <= 0) {
        return false;
    }

    xEventGroupClearBits(modem->event_group, NETWORK_EVENT_IP_READY);
    if (!at_client_exec_cmdf(modem->client, &(at_cmd_desc_t){
            .cmd = "AT+MIPCALL=1,%d", .timeout_ms = 3000,
        }, NULL, cid)) {
        LISA_LOGE(TAG, "Failed to activate PDP cid=%d, cme=%d",
                  cid, at_client_get_cme_error(modem->client));
        modem->active_pdp_cid = 0;
        modem->network_ready = false;
        modem->ip_address[0] = '\0';
        ml307_status_set_error(modem, LISA_MODEM_ERR_PDP_ACTIVATE_FAILED);
        return false;
    }

    bits = xEventGroupWaitBits(modem->event_group, NETWORK_EVENT_IP_READY,
                               pdTRUE, pdFALSE, pdMS_TO_TICKS(ML307_DIAL_TIMEOUT_MS));
    if ((bits & NETWORK_EVENT_IP_READY) == 0) {
        LISA_LOGW(TAG, "Timed out waiting for PDP activation URC, fallback to query");
    }

    if (!ml307_netreg_query_mipcall(modem, &active_cid, &status, ip, sizeof(ip)) ||
        active_cid != cid || status != 1 || ip[0] == '\0') {
        LISA_LOGE(TAG, "PDP activation not ready, cid=%d status=%d ip='%s'",
                  active_cid, status, ip);
        ml307_status_set_error(modem, LISA_MODEM_ERR_PDP_ACTIVATE_FAILED);
        return false;
    }

    modem->active_pdp_cid = (uint8_t)cid;
    strncpy(modem->ip_address, ip, sizeof(modem->ip_address) - 1);
    modem->ip_address[sizeof(modem->ip_address) - 1] = '\0';
    modem->network_ready = true;
    modem->network_status = NETWORK_STATUS_READY;
    modem->status.last_error = LISA_MODEM_ERR_READY;
    LISA_LOGI(TAG, "PDP cid=%d active, ip=%s", cid, modem->ip_address);
    return true;
}

void ml307_netreg_handle_modem_urc(ml307_endpoint_ctx_t *modem, const char *command,
                                      at_arg_value_t *arguments, size_t arg_count)
{
    if (!modem || !command) {
        return;
    }

    if (strcmp(command, "CREG") == 0 && arg_count >= 2) {
        if (arguments[1].type == AT_ARG_TYPE_INT) {
            int stat = arguments[1].data.int_val;
            switch (stat) {
                case 0:
                    modem->network_status = NETWORK_STATUS_DISCONNECTED;
                    modem->network_ready = false;
                    xEventGroupClearBits(modem->event_group, NETWORK_EVENT_REGISTERED);
                    break;
                case 1:
                    modem->network_status = NETWORK_STATUS_REGISTERED_HOME;
                    xEventGroupSetBits(modem->event_group, NETWORK_EVENT_REGISTERED);
                    break;
                case 2:
                    modem->network_status = NETWORK_STATUS_SEARCHING;
                    break;
                case 3:
                    modem->network_status = NETWORK_STATUS_DENIED;
                    break;
                case 5:
                    modem->network_status = NETWORK_STATUS_REGISTERED_ROAMING;
                    xEventGroupSetBits(modem->event_group, NETWORK_EVENT_REGISTERED);
                    break;
                default:
                    modem->network_status = NETWORK_STATUS_UNKNOWN;
                    break;
            }
        }
    } else if (strcmp(command, "MIPCALL") == 0 && arg_count >= 3) {
        if (arguments[1].type == AT_ARG_TYPE_INT &&
            arguments[2].type == AT_ARG_TYPE_STRING &&
            arguments[2].data.string_val.value) {
            int status = arguments[1].data.int_val;
            if (status == 1) {
                strncpy(modem->ip_address, arguments[2].data.string_val.value,
                        sizeof(modem->ip_address) - 1);
                modem->ip_address[sizeof(modem->ip_address) - 1] = '\0';
                modem->active_pdp_cid = (arguments[0].type == AT_ARG_TYPE_INT)
                                      ? (uint8_t)arguments[0].data.int_val
                                      : ML307_DEFAULT_PDP_CID;
                modem->network_ready = true;
                modem->network_status = NETWORK_STATUS_READY;
            }
            xEventGroupSetBits(modem->event_group, NETWORK_EVENT_IP_READY);
        }
    } else if (strcmp(command, "MATREADY") == 0) {
        modem->network_ready = false;
        modem->network_status = NETWORK_STATUS_DISCONNECTED;
        modem->active_pdp_cid = 0;
        modem->ip_address[0] = '\0';
        xEventGroupClearBits(modem->event_group, NETWORK_EVENT_REGISTERED | NETWORK_EVENT_IP_READY);
    }
}

static bool ml307_netreg_query_cpin(ml307_endpoint_ctx_t *modem, char *status, size_t size)
{
    str_out_t out = { status, size };
    return modem && status && size > 0 &&
           at_client_exec_cmd(modem->client, &(at_cmd_desc_t){
               .cmd = "AT+CPIN?", .expect_urc = "CPIN",
               .parse = parse_first_string, .timeout_ms = 1000,
           }, &out);
}

static bool ml307_netreg_query_cereg(ml307_endpoint_ctx_t *modem, int *n, int *stat)
{
    int result[2] = { 0, 0 };
    bool ok;
    if (!modem || !n || !stat) {
        return false;
    }
    ok = at_client_exec_cmd(modem->client, &(at_cmd_desc_t){
        .cmd = "AT+CEREG?", .expect_urc = "CEREG",
        .parse = parse_cereg, .timeout_ms = 1000,
    }, result);
    *n = result[0];
    *stat = result[1];
    return ok;
}

static bool ml307_netreg_query_mipcall(ml307_endpoint_ctx_t *modem, int *cid, int *status, char *ip, size_t ip_size)
{
    mipcall_out_t out = { cid, status, ip, ip_size };
    return modem &&
           at_client_exec_cmd(modem->client, &(at_cmd_desc_t){
               .cmd = "AT+MIPCALL?", .expect_urc = "MIPCALL",
               .parse = parse_mipcall, .timeout_ms = 1000,
           }, &out);
}

network_status_t ml307_netreg_network_check(ml307_endpoint_ctx_t *modem)
{
    char cpin_status[32] = {0};
    int n = 0;
    int stat = 0;
    int cid = 0;
    int status = 0;
    char ip[16] = {0};

    if (!modem) {
        return NETWORK_STATUS_ERROR;
    }

    modem->status.last_error = LISA_MODEM_ERR_NOT_INITIALIZED;
    modem->network_ready = false;
    modem->active_pdp_cid = 0;
    modem->ip_address[0] = '\0';
    xEventGroupClearBits(modem->event_group, NETWORK_EVENT_REGISTERED | NETWORK_EVENT_IP_READY);

    for (int i = 0; i < ML307_NETREG_RETRY_COUNT; i++) {
        bool cpin_ok = ml307_netreg_query_cpin(modem, cpin_status, sizeof(cpin_status));

        if (cpin_ok && strcmp(cpin_status, "READY") == 0) {
            break;
        }
        if (i == ML307_NETREG_RETRY_COUNT - 1) {
            LISA_LOGE(TAG, "SIM not ready, CPIN='%s'", cpin_status);
            ml307_status_set_error(modem, cpin_ok
                                      ? ml307_status_error_from_cpin(cpin_status)
                                      : LISA_MODEM_ERR_SIM_QUERY_FAILED);
            return NETWORK_STATUS_ERROR;
        }
        vTaskDelay(pdMS_TO_TICKS(ML307_NETREG_WAIT_MS));
    }

    for (int i = 0; i < ML307_NETREG_RETRY_COUNT; i++) {
        if (at_client_exec_cmd(modem->client, &(at_cmd_desc_t){
                .cmd = "AT+CEREG=2", .timeout_ms = 1000,
            }, NULL)) {
            break;
        }
        if (i == ML307_NETREG_RETRY_COUNT - 1) {
            ml307_status_set_error(modem, LISA_MODEM_ERR_AT_COMMAND_FAILED);
            return NETWORK_STATUS_ERROR;
        }
        vTaskDelay(pdMS_TO_TICKS(ML307_NETREG_WAIT_MS));
    }

    for (int i = 0; i < 30; i++) {
        bool cereg_ok = ml307_netreg_query_cereg(modem, &n, &stat);

        if (cereg_ok && (stat == 1 || stat == 5)) {
            modem->network_status = (stat == 1)
                                  ? NETWORK_STATUS_REGISTERED_HOME
                                  : NETWORK_STATUS_REGISTERED_ROAMING;
            break;
        }
        if (i == 29) {
            LISA_LOGE(TAG, "Network registration not ready, CEREG=%d,%d", n, stat);
            ml307_status_set_error(modem, stat == 3
                                      ? LISA_MODEM_ERR_NETWORK_DENIED
                                      : LISA_MODEM_ERR_NETWORK_REGISTER_FAILED);
            return NETWORK_STATUS_ERROR;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    for (int i = 0; i < ML307_NETREG_RETRY_COUNT; i++) {
        if (ml307_netreg_query_mipcall(modem, &cid, &status, ip, sizeof(ip)) &&
            status == 1 && strlen(ip) > 0) {
            modem->active_pdp_cid = (uint8_t)cid;
            strncpy(modem->ip_address, ip, sizeof(modem->ip_address) - 1);
            modem->ip_address[sizeof(modem->ip_address) - 1] = '\0';
            modem->network_ready = true;
            modem->network_status = NETWORK_STATUS_READY;
            modem->status.last_error = LISA_MODEM_ERR_READY;
            return NETWORK_STATUS_READY;
        }
        vTaskDelay(pdMS_TO_TICKS(ML307_NETREG_WAIT_MS));
    }

    if (ml307_netreg_activate_pdp(modem, ML307_DEFAULT_PDP_CID)) {
        return NETWORK_STATUS_READY;
    }

    if (modem->status.last_error == LISA_MODEM_ERR_NOT_INITIALIZED) {
        ml307_status_set_error(modem, LISA_MODEM_ERR_PDP_ACTIVATE_FAILED);
    }
    return modem->network_status;
}

bool ml307_netreg_dns_resolve(ml307_endpoint_ctx_t *modem, const char *domain, char *ip_addr, size_t size)
{
    str_out_t out = { ip_addr, size };
    bool result;

    if (!modem || !domain || !ip_addr || size < 16 || !modem->initialized) {
        return false;
    }

    if (!modem->dns_mutex || xSemaphoreTake(modem->dns_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return false;
    }

    result = at_client_exec_cmdf(modem->client, &(at_cmd_desc_t){
        .cmd = "AT+MDNSGIP=\"%s\",%d", .expect_urc = "MDNSGIP",
        .parse = parse_dns, .timeout_ms = 10000,
    }, &out, domain, modem->active_pdp_cid > 0 ? modem->active_pdp_cid : ML307_DEFAULT_PDP_CID);

    xSemaphoreGive(modem->dns_mutex);
    return result;
}
