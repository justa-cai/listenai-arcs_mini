/**
 * @file modem_probe_utils.c
 * @brief Shared probe helpers for modem drivers
 */

#include "core/modem_probe_utils.h"
#include <string.h>

#define TAG "modem_probe"
#include "lisa_log.h"

void modem_probe_fill_field(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

bool modem_probe_common(at_client_t *client, modem_probe_result_t *result,
                        const char *driver_name, const char *model_match)
{
    char model[MODEM_PROBE_TEXT_LEN] = {0};
    char manufacturer[MODEM_PROBE_TEXT_LEN] = {0};
    char revision[MODEM_PROBE_TEXT_LEN] = {0};
    bool have_model = false;

    if (!client || !driver_name || !model_match) {
        return false;
    }

    if (at_client_exec_text_cmd(client, "AT+CGMM", model, sizeof(model), 1000)) {
        have_model = true;
        LISA_LOGI(TAG, "Probe %s got CGMM: %s", driver_name, model);
    } else if (at_client_exec_text_cmd(client, "ATI", model, sizeof(model), 1000)) {
        have_model = true;
        LISA_LOGI(TAG, "Probe %s got ATI: %s", driver_name, model);
    } else {
        LISA_LOGW(TAG, "Probe %s could not read modem model via CGMM/ATI", driver_name);
        return false;
    }

    if (!strstr(model, model_match)) {
        LISA_LOGI(TAG, "Probe %s model mismatch, expect '%s', got '%s'",
                  driver_name, model_match, model);
        return false;
    }

    if (!result) {
        return true;
    }

    memset(result, 0, sizeof(*result));
    modem_probe_fill_field(result->driver_name, sizeof(result->driver_name), driver_name);
    if (have_model) {
        modem_probe_fill_field(result->model, sizeof(result->model), model);
    }
    result->match_score = 100;

    if (at_client_exec_text_cmd(client, "AT+CGMI", manufacturer, sizeof(manufacturer), 1000)) {
        modem_probe_fill_field(result->manufacturer, sizeof(result->manufacturer), manufacturer);
    }

    if (at_client_exec_text_cmd(client, "AT+CGMR", revision, sizeof(revision), 1000)) {
        modem_probe_fill_field(result->revision, sizeof(result->revision), revision);
    }

    return true;
}
