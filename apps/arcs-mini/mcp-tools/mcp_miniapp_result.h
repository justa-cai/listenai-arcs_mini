#ifndef MCP_MINIAPP_RESULT_H
#define MCP_MINIAPP_RESULT_H

#include <stdbool.h>
#include "cJSON.h"

static cJSON *miniapp_mcp_result(const char *name, const char *message, bool error)
{
    cJSON *result = cJSON_CreateObject();
    cJSON *content = result ? cJSON_AddArrayToObject(result, "content") : NULL;
    cJSON *item = cJSON_CreateObject();
    if (!result || !content || !item) {
        cJSON_Delete(item);
        cJSON_Delete(result);
        return NULL;
    }
    if (!cJSON_AddItemToArray(content, item)) {
        cJSON_Delete(item);
        cJSON_Delete(result);
        return NULL;
    }
    if (!cJSON_AddStringToObject(result, "tool", name) ||
        !cJSON_AddBoolToObject(result, "isError", error) ||
        !cJSON_AddStringToObject(item, "type", "text") ||
        !cJSON_AddStringToObject(item, "text", message)) {
        cJSON_Delete(result);
        return NULL;
    }
    return result;
}
#endif
