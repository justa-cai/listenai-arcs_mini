/**
 * @file ec801e_netreg.c
 * @brief EC801E network registration and DNS helpers
 */

#include "drivers/ec801e/ec801e_netreg.h"
#include "drivers/ec801e/ec801e_dnsgip.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define TAG "ec801e_netreg"
#include "lisa_log.h"

#define EC801E_DEFAULT_PDP_CID     1
#define EC801E_NETREG_RETRY_COUNT  5
#define EC801E_NETREG_WAIT_MS      100
#define EC801E_DIAL_TIMEOUT_MS     150000
#define EC801E_DNS_WAIT_MS         10000

typedef struct {
    char *buf;
    size_t size;
} str_out_t;

typedef struct {
    int *n;
    int *stat;
} cereg_out_t;

typedef struct {
    int *cid;
    int *status;
    char *ip;
    size_t ip_size;
} qiact_out_t;

static bool parse_first_string(at_arg_value_t *args, size_t count, void *user_data)
{
    str_out_t *out = (str_out_t *)user_data;

    if (!out || !out->buf || out->size == 0 ||
        count < 1 || args[0].type != AT_ARG_TYPE_STRING || !args[0].data.string_val.value) {
        return false;
    }

    strncpy(out->buf, args[0].data.string_val.value, out->size - 1);
    out->buf[out->size - 1] = '\0';
    return true;
}

static bool parse_cereg(at_arg_value_t *args, size_t count, void *user_data)
{
    cereg_out_t *out = (cereg_out_t *)user_data;

    if (!out || !out->n || !out->stat || count < 2 ||
        args[0].type != AT_ARG_TYPE_INT || args[1].type != AT_ARG_TYPE_INT) {
        return false;
    }

    *out->n = args[0].data.int_val;
    *out->stat = args[1].data.int_val;
    return true;
}

static bool parse_qiact(at_arg_value_t *args, size_t count, void *user_data)
{
    qiact_out_t *out = (qiact_out_t *)user_data;

    if (!out || count < 3 || args[0].type != AT_ARG_TYPE_INT || args[1].type != AT_ARG_TYPE_INT) {
        return false;
    }

    if (out->cid) {
        *out->cid = args[0].data.int_val;
    }
    if (out->status) {
        *out->status = args[1].data.int_val;
    }
    if (out->ip && out->ip_size > 0 && count >= 4 &&
        args[3].type == AT_ARG_TYPE_STRING && args[3].data.string_val.value) {
        strncpy(out->ip, args[3].data.string_val.value, out->ip_size - 1);
        out->ip[out->ip_size - 1] = '\0';
    }
    return true;
}

static bool ec801e_netreg_query_cpin(ec801e_endpoint_ctx_t *ctx, char *status, size_t size)
{
    str_out_t out = { status, size };

    return ctx && status && size > 0 &&
           at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
               .cmd = "AT+CPIN?", .expect_urc = "CPIN",
               .parse = parse_first_string, .timeout_ms = 1000,
           }, &out);
}

static bool ec801e_netreg_query_cereg(ec801e_endpoint_ctx_t *ctx, int *n, int *stat)
{
    cereg_out_t out = { n, stat };

    return ctx && n && stat &&
           at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
               .cmd = "AT+CEREG?", .expect_urc = "CEREG",
               .parse = parse_cereg, .timeout_ms = 1000,
           }, &out);
}

static bool ec801e_netreg_query_qiact(ec801e_endpoint_ctx_t *ctx, int *cid, int *status, char *ip, size_t ip_size)
{
    qiact_out_t out = { cid, status, ip, ip_size };

    return ctx &&
           at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
               .cmd = "AT+QIACT?", .expect_urc = "QIACT",
               .parse = parse_qiact, .timeout_ms = 2000,
           }, &out);
}

static bool ec801e_netreg_activate_pdp(ec801e_endpoint_ctx_t *ctx, int cid)
{
    int active_cid = 0;
    int status = 0;
    char ip[sizeof(ctx->ip_address)] = {0};

    if (!ctx || cid <= 0) {
        return false;
    }

    (void)at_client_exec_cmdf(ctx->client, &(at_cmd_desc_t){
        .cmd = "AT+QICSGP=%d,1,\"\",\"\",\"\",1",
        .timeout_ms = 2000,
    }, NULL, cid);

    if (!at_client_exec_cmdf(ctx->client, &(at_cmd_desc_t){
            .cmd = "AT+QIACT=%d", .timeout_ms = EC801E_DIAL_TIMEOUT_MS,
        }, NULL, cid)) {
        if (!ec801e_netreg_query_qiact(ctx, &active_cid, &status, ip, sizeof(ip)) ||
            active_cid != cid || status != 1) {
            LISA_LOGE(TAG, "Failed to activate PDP cid=%d", cid);
            return false;
        }
    }

    if (!ec801e_netreg_query_qiact(ctx, &active_cid, &status, ip, sizeof(ip)) ||
        active_cid != cid || status != 1) {
        LISA_LOGE(TAG, "PDP not ready after activation, cid=%d status=%d", active_cid, status);
        return false;
    }

    ctx->active_pdp_cid = (uint8_t)cid;
    ctx->network_ready = true;
    ctx->network_status = NETWORK_STATUS_READY;
    strncpy(ctx->ip_address, ip, sizeof(ctx->ip_address) - 1);
    ctx->ip_address[sizeof(ctx->ip_address) - 1] = '\0';
    return true;
}

void ec801e_netreg_handle_modem_urc(ec801e_endpoint_ctx_t *ctx, const char *command,
                                    at_arg_value_t *arguments, size_t arg_count)
{
    const char *urc_type;
    bool dns_done = false;
    char dns_ip[sizeof(ctx->dns_result)] = {0};

    if (!ctx || !command) {
        return;
    }

    if (strcmp(command, "CEREG") == 0 && arg_count >= 2 && arguments[1].type == AT_ARG_TYPE_INT) {
        switch (arguments[1].data.int_val) {
            case 0:
                ctx->network_status = NETWORK_STATUS_DISCONNECTED;
                ctx->network_ready = false;
                break;
            case 1:
                ctx->network_status = NETWORK_STATUS_REGISTERED_HOME;
                break;
            case 2:
                ctx->network_status = NETWORK_STATUS_SEARCHING;
                break;
            case 3:
                ctx->network_status = NETWORK_STATUS_DENIED;
                break;
            case 5:
                ctx->network_status = NETWORK_STATUS_REGISTERED_ROAMING;
                break;
            default:
                ctx->network_status = NETWORK_STATUS_UNKNOWN;
                break;
        }
        return;
    }

    if (strcmp(command, "QIURC") != 0 || arg_count < 1 ||
        arguments[0].type != AT_ARG_TYPE_STRING || !arguments[0].data.string_val.value) {
        return;
    }

    if (ctx->dns_pending) {
        if (ec801e_dnsgip_try_extract_ip(arguments, arg_count, dns_ip, sizeof(dns_ip), &dns_done)) {
            strncpy(ctx->dns_result, dns_ip, sizeof(ctx->dns_result) - 1);
            ctx->dns_result[sizeof(ctx->dns_result) - 1] = '\0';
            ctx->dns_success = true;
            ctx->dns_pending = false;
            return;
        }

        if (dns_done) {
            ctx->dns_success = false;
            ctx->dns_pending = false;
            return;
        }
    }

    urc_type = arguments[0].data.string_val.value;
    if (strcmp(urc_type, "pdpdeact") == 0) {
        ctx->network_ready = false;
        ctx->active_pdp_cid = 0;
        ctx->network_status = NETWORK_STATUS_DISCONNECTED;
    }
}

network_status_t ec801e_netreg_network_check(ec801e_endpoint_ctx_t *ctx)
{
    char cpin_status[32] = {0};
    int n = 0;
    int stat = 0;
    int cid = 0;
    int pdp_status = 0;
    char ip[sizeof(ctx->ip_address)] = {0};

    if (!ctx) {
        return NETWORK_STATUS_ERROR;
    }

    ctx->network_ready = false;
    ctx->active_pdp_cid = 0;

    for (int i = 0; i < EC801E_NETREG_RETRY_COUNT; ++i) {
        if (ec801e_netreg_query_cpin(ctx, cpin_status, sizeof(cpin_status)) &&
            strcmp(cpin_status, "READY") == 0) {
            break;
        }
        if (i == EC801E_NETREG_RETRY_COUNT - 1) {
            LISA_LOGE(TAG, "SIM not ready, CPIN='%s'", cpin_status);
            return NETWORK_STATUS_ERROR;
        }
        vTaskDelay(pdMS_TO_TICKS(EC801E_NETREG_WAIT_MS));
    }

    (void)at_client_exec_cmd(ctx->client, &(at_cmd_desc_t){
        .cmd = "AT+CEREG=2", .timeout_ms = 1000,
    }, NULL);

    for (int i = 0; i < 30; ++i) {
        if (ec801e_netreg_query_cereg(ctx, &n, &stat) && (stat == 1 || stat == 5)) {
            ctx->network_status = (stat == 1)
                                ? NETWORK_STATUS_REGISTERED_HOME
                                : NETWORK_STATUS_REGISTERED_ROAMING;
            break;
        }
        if (i == 29) {
            LISA_LOGE(TAG, "Network registration not ready, CEREG=%d,%d", n, stat);
            return NETWORK_STATUS_ERROR;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (ec801e_netreg_query_qiact(ctx, &cid, &pdp_status, ip, sizeof(ip)) &&
        cid == EC801E_DEFAULT_PDP_CID && pdp_status == 1) {
        ctx->active_pdp_cid = (uint8_t)cid;
        ctx->network_ready = true;
        ctx->network_status = NETWORK_STATUS_READY;
        strncpy(ctx->ip_address, ip, sizeof(ctx->ip_address) - 1);
        ctx->ip_address[sizeof(ctx->ip_address) - 1] = '\0';
        return NETWORK_STATUS_READY;
    }

    return ec801e_netreg_activate_pdp(ctx, EC801E_DEFAULT_PDP_CID)
         ? NETWORK_STATUS_READY
         : NETWORK_STATUS_ERROR;
}

bool ec801e_netreg_dns_resolve(ec801e_endpoint_ctx_t *ctx, const char *domain, char *ip_addr, size_t size)
{
    bool result;
    int waited_ms = 0;

    if (!ctx || !ctx->initialized || !ctx->network_ready ||
        !domain || !ip_addr || size < 16 || !ctx->dns_mutex) {
        return false;
    }

    if (xSemaphoreTake(ctx->dns_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return false;
    }

    ctx->dns_pending = true;
    ctx->dns_success = false;
    ctx->dns_result[0] = '\0';

    result = at_client_exec_cmdf(ctx->client, &(at_cmd_desc_t){
        .cmd = "AT+QIDNSGIP=%d,\"%s\"",
        .timeout_ms = 2000,
    }, NULL, ctx->active_pdp_cid > 0 ? ctx->active_pdp_cid : EC801E_DEFAULT_PDP_CID, domain);

    while (result && ctx->dns_pending && waited_ms < EC801E_DNS_WAIT_MS) {
        vTaskDelay(pdMS_TO_TICKS(100));
        waited_ms += 100;
    }

    if (result && ctx->dns_success && ctx->dns_result[0] != '\0') {
        strncpy(ip_addr, ctx->dns_result, size - 1);
        ip_addr[size - 1] = '\0';
    } else {
        result = false;
    }

    ctx->dns_pending = false;
    ctx->dns_success = false;

    xSemaphoreGive(ctx->dns_mutex);
    return result;
}
