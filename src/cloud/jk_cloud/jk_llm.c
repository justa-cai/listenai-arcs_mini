#define TAG "jk_llm"

#include "jk_llm.h"
#include "jk_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

static void llm_on_event(jk_websocket_t *ws, jk_ws_event_type_e event, void *user) {
    LISA_LOGI(TAG, "llm_on_event called: event=%d, user=%p", event, user);

    jk_llm_t *llm = (jk_llm_t *)user;
    if (!llm) {
        LISA_LOGE(TAG, "llm_on_event: llm is NULL!");
        return;
    }

    LISA_LOGI(TAG, "llm_on_event: processing event=%d, current_state=%d", event, llm->state);

    switch (event) {
    case JK_WS_EVENT_CONNECTED:
        LISA_LOGI(TAG, "LLM CONNECTED - calling on_connected callback");
        llm->state = JK_LLM_STATE_CONNECTED;
        if (llm->cbs.on_connected) {
            LISA_LOGI(TAG, "LLM CONNECTED - on_connected callback exists, calling...");
            llm->cbs.on_connected(llm);
        } else {
            LISA_LOGW(TAG, "LLM CONNECTED - on_connected callback is NULL!");
        }
        break;
    case JK_WS_EVENT_DISCONNECTED:
        LISA_LOGI(TAG, "LLM DISCONNECTED");
        llm->state = JK_LLM_STATE_DISCONNECTED;
        if (llm->cbs.on_disconnected) {
            llm->cbs.on_disconnected(llm);
        }
        break;
    case JK_WS_EVENT_ERROR:
        LISA_LOGE(TAG, "LLM ERROR event received");
        llm->state = JK_LLM_STATE_DISCONNECTED;
        if (llm->cbs.on_error) {
            LISA_LOGI(TAG, "LLM ERROR - calling on_error callback");
            llm->cbs.on_error(llm, "WebSocket error");
        }
        break;
    default:
        LISA_LOGW(TAG, "llm_on_event: unknown event type=%d", event);
        break;
    }
}

static void parse_llm_message(jk_llm_t *llm, const char *data, uint32_t len) {
    cJSON *json = cJSON_ParseWithLength(data, len);
    if (!json) {
        LISA_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (!type_item || !cJSON_IsString(type_item)) {
        cJSON_Delete(json);
        return;
    }

    jk_llm_message_t msg = {0};
    const char *type_str = type_item->valuestring;

    if (strcmp(type_str, "status") == 0) {
        msg.type = JK_LLM_MSG_TYPE_STATUS;
        cJSON *data_obj = cJSON_GetObjectItem(json, "data");
        if (data_obj) {
            cJSON *sid = cJSON_GetObjectItem(data_obj, "session_id");
            if (sid && cJSON_IsString(sid)) {
                strncpy(llm->session_id, sid->valuestring, JK_LLM_SESSION_ID_LEN - 1);
                msg.session_id = llm->session_id;
                LISA_LOGI(TAG, "Session: %s", llm->session_id);
            }
        }
    } else if (strcmp(type_str, "llm_response") == 0) {
        msg.type = JK_LLM_MSG_TYPE_LLM_RESPONSE;
        cJSON *content = cJSON_GetObjectItem(json, "content");
        if (content && cJSON_IsString(content)) {
            msg.content = content->valuestring;
        }
    } else if (strcmp(type_str, "tool_call") == 0) {
        msg.type = JK_LLM_MSG_TYPE_TOOL_CALL;
        cJSON *tool_name = cJSON_GetObjectItem(json, "tool_name");
        cJSON *args = cJSON_GetObjectItem(json, "arguments");
        cJSON *result = cJSON_GetObjectItem(json, "result");
        if (tool_name) msg.tool_name = tool_name->valuestring;
        if (args) msg.arguments = cJSON_PrintUnformatted(args);
        if (result) msg.result = cJSON_PrintUnformatted(result);
    } else if (strcmp(type_str, "tool_callback") == 0) {
        /* v1.2.0 服务端回调客户端工具 */
        msg.type = JK_LLM_MSG_TYPE_TOOL_CALLBACK;
        cJSON *call_id = cJSON_GetObjectItem(json, "call_id");
        cJSON *tool_name = cJSON_GetObjectItem(json, "tool_name");
        cJSON *args = cJSON_GetObjectItem(json, "arguments");
        if (call_id) msg.call_id = call_id->valuestring;
        if (tool_name) msg.tool_name = tool_name->valuestring;
        if (args) msg.arguments = cJSON_PrintUnformatted(args);
    } else if (strcmp(type_str, "tools_registered") == 0) {
        /* v1.2.0 工具注册确认 */
        msg.type = JK_LLM_MSG_TYPE_TOOLS_REGISTERED;
        cJSON *count = cJSON_GetObjectItem(json, "count");
        if (count) {
            /* 将 count 存储在 result 字段中传递 */
            char count_buf[16];
            snprintf(count_buf, sizeof(count_buf), "%d", count->valueint);
            LISA_LOGI(TAG, "tools_registered: server confirmed count=%d (raw type=%d)",
                      count->valueint, count->type);
            msg.result = lisa_mem_calloc(1, strlen(count_buf) + 1);
            if (msg.result) {
                strcpy((char *)msg.result, count_buf);
            }
        } else {
            LISA_LOGW(TAG, "tools_registered: count field not found in server response");
        }
    } else if (strcmp(type_str, "error") == 0) {
        msg.type = JK_LLM_MSG_TYPE_ERROR;
        cJSON *code = cJSON_GetObjectItem(json, "code");
        cJSON *message = cJSON_GetObjectItem(json, "message");
        if (code) msg.error_code = code->valuestring;
        if (message) msg.error_message = message->valuestring;
    } else if (strcmp(type_str, "pong") == 0) {
        msg.type = JK_LLM_MSG_TYPE_PONG;
    }

    if (llm->cbs.on_message) {
        llm->cbs.on_message(llm, &msg);
    }

    /* v1.2.0 处理专用回调 */
    if (msg.type == JK_LLM_MSG_TYPE_TOOL_CALLBACK && llm->cbs.on_tool_callback) {
        llm->cbs.on_tool_callback(llm, msg.call_id, msg.tool_name, msg.arguments);
    }
    if (msg.type == JK_LLM_MSG_TYPE_TOOLS_REGISTERED && llm->cbs.on_tools_registered) {
        int count = msg.result ? atoi(msg.result) : 0;
        llm->cbs.on_tools_registered(llm, count);
    }

    if (msg.arguments) lisa_mem_free(msg.arguments);
    if (msg.result) lisa_mem_free(msg.result);
    cJSON_Delete(json);
}

static void llm_on_data(jk_websocket_t *ws, jk_ws_data_type_e type,
                        const void *data, uint32_t len, void *user) {
    jk_llm_t *llm = (jk_llm_t *)user;
    if (!llm || !data) {
        LISA_LOGW(TAG, "llm_on_data: llm=%p, data=%p", llm, data);
        return;
    }

    if (type != JK_WS_DATA_TEXT) {
        LISA_LOGD(TAG, "llm_on_data: non-TEXT type=%d, len=%u", type, len);
        return;
    }

    /* Log raw message for debugging */
    const char *data_str = (const char *)data;
    if (len > 0 && data_str[0] == '{') {
        LISA_LOGI(TAG, "llm_on_data: TEXT message received, len=%u: %.500s%s",
                  len, data_str, len > 500 ? "..." : "");
    } else {
        LISA_LOGD(TAG, "llm_on_data: TEXT message received, len=%u", len);
    }

    parse_llm_message(llm, data_str, len);
}

jk_llm_t *jk_llm_create(const char *host, const char *port, jk_llm_callbacks_t *cbs) {
    jk_llm_t *llm = lisa_mem_calloc(1, sizeof(jk_llm_t));
    if (!llm) {
        LISA_LOGE(TAG, "Failed to allocate");
        return NULL;
    }

    llm->host = host ? strdup(host) : strdup(JK_LLM_DEFAULT_HOST);
    llm->port = port ? strdup(port) : strdup(JK_LLM_DEFAULT_PORT);
    llm->state = JK_LLM_STATE_DISCONNECTED;

    if (cbs) {
        memcpy(&llm->cbs, cbs, sizeof(jk_llm_callbacks_t));
    }

    jk_ws_config_t config = {
        .host = llm->host,
        .port = llm->port,
        .path = "/",
        .timeout_ms = 60000,
        .user = llm,
        .on_event = llm_on_event,
        .on_data = llm_on_data,
    };

    llm->ws = jk_ws_create(&config);
    if (!llm->ws) {
        LISA_LOGE(TAG, "Failed to create WebSocket");
        lisa_mem_free(llm->host);
        lisa_mem_free(llm->port);
        lisa_mem_free(llm);
        return NULL;
    }

    return llm;
}

void jk_llm_destroy(jk_llm_t *llm) {
    if (!llm) return;

    if (llm->ws) {
        jk_ws_destroy(llm->ws);
    }

    if (llm->host) lisa_mem_free(llm->host);
    if (llm->port) lisa_mem_free(llm->port);
    lisa_mem_free(llm);
}

int jk_llm_connect(jk_llm_t *llm) {
    LISA_LOGI(TAG, "jk_llm_connect: called, llm=%p, ws=%p", llm, llm ? llm->ws : NULL);
    if (!llm || !llm->ws) {
        LISA_LOGE(TAG, "jk_llm_connect: llm or ws is NULL!");
        return -1;
    }

    if (llm->state == JK_LLM_STATE_CONNECTED) {
        LISA_LOGI(TAG, "jk_llm_connect: already connected");
        return 0;
    }

    LISA_LOGI(TAG, "jk_llm_connect: connecting to ws://%s:%s/ (current_state=%d)",
              llm->host, llm->port, llm->state);
    llm->state = JK_LLM_STATE_CONNECTING;

    int ret = jk_ws_connect(llm->ws);
    if (ret != 0) {
        LISA_LOGE(TAG, "jk_llm_connect: jk_ws_connect failed with ret=%d", ret);
        llm->state = JK_LLM_STATE_DISCONNECTED;
        return -1;
    }

    LISA_LOGI(TAG, "jk_llm_connect: jk_ws_connect returned successfully (ret=%d)", ret);
    return 0;
}

int jk_llm_disconnect(jk_llm_t *llm) {
    if (!llm || !llm->ws) return -1;

    jk_ws_disconnect(llm->ws);
    llm->state = JK_LLM_STATE_DISCONNECTED;
    return 0;
}

int jk_llm_send_text(jk_llm_t *llm, const char *text) {
    if (!llm || !llm->ws || !text) return -1;
    if (llm->state != JK_LLM_STATE_CONNECTED) return -1;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "text_input");
    cJSON_AddStringToObject(req, "text", text);
    if (strlen(llm->session_id) > 0) {
        cJSON_AddStringToObject(req, "session_id", llm->session_id);
    }

    char *json_str = cJSON_PrintUnformatted(req);
    int ret = jk_ws_send_text(llm->ws, json_str);
    lisa_mem_free(json_str);
    cJSON_Delete(req);

    return ret;
}

int jk_llm_send_ping(jk_llm_t *llm) {
    if (!llm || !llm->ws) return -1;
    if (llm->state != JK_LLM_STATE_CONNECTED) return -1;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "ping");

    char *json_str = cJSON_PrintUnformatted(req);
    int ret = jk_ws_send_text(llm->ws, json_str);
    lisa_mem_free(json_str);
    cJSON_Delete(req);

    return ret;
}

bool jk_llm_is_connected(jk_llm_t *llm) {
    return llm && llm->state == JK_LLM_STATE_CONNECTED;
}

const char *jk_llm_get_session_id(jk_llm_t *llm) {
    return llm ? llm->session_id : NULL;
}

/* ==================== v1.2.0 客户端工具注册 API ==================== */

int jk_llm_register_tools(jk_llm_t *llm, const jk_llm_tool_def_t *tools, int count) {
    if (!llm || !llm->ws || !tools || count <= 0) {
        LISA_LOGE(TAG, "jk_llm_register_tools: invalid params llm=%p, ws=%p, tools=%p, count=%d",
                  llm, llm ? llm->ws : NULL, tools, count);
        return -1;
    }
    if (llm->state != JK_LLM_STATE_CONNECTED) {
        LISA_LOGE(TAG, "jk_llm_register_tools: not connected, state=%d", llm->state);
        return -1;
    }

    LISA_LOGI(TAG, "jk_llm_register_tools: starting with %d tools", count);

    cJSON *req = cJSON_CreateObject();
    if (!req) {
        LISA_LOGE(TAG, "jk_llm_register_tools: failed to create JSON object");
        return -1;
    }

    cJSON_AddStringToObject(req, "type", "register_tools");

    cJSON *tools_array = cJSON_CreateArray();
    if (!tools_array) {
        LISA_LOGE(TAG, "jk_llm_register_tools: failed to create tools array");
        cJSON_Delete(req);
        return -1;
    }

    int added_count = 0;
    for (int i = 0; i < count; i++) {
        cJSON *tool = cJSON_CreateObject();
        if (tool) {
            cJSON_AddStringToObject(tool, "name", tools[i].name);
            cJSON_AddStringToObject(tool, "description", tools[i].description);

            /* parameters 是 JSON Schema 格式的字符串 */
            if (tools[i].parameters) {
                cJSON *params = cJSON_Parse(tools[i].parameters);
                if (params) {
                    cJSON_AddItemToObject(tool, "parameters", params);
                    LISA_LOGD(TAG, "Tool[%d] %s: added parameters (len=%zu)",
                              i, tools[i].name, strlen(tools[i].parameters));
                } else {
                    LISA_LOGW(TAG, "Tool[%d] %s: failed to parse parameters",
                              i, tools[i].name);
                }
            } else {
                LISA_LOGW(TAG, "Tool[%d] %s: parameters is NULL", i, tools[i].name);
            }
            cJSON_AddItemToArray(tools_array, tool);
            added_count++;
        } else {
            LISA_LOGE(TAG, "Tool[%d]: failed to create tool object", i);
        }
    }
    cJSON_AddItemToObject(req, "tools", tools_array);

    LISA_LOGI(TAG, "jk_llm_register_tools: added %d tools to array", added_count);

    char *json_str = cJSON_PrintUnformatted(req);
    size_t json_len = json_str ? strlen(json_str) : 0;
    LISA_LOGI(TAG, "Sending register_tools JSON (len=%zu bytes): %.2000s%s",
              json_len, json_str, json_len > 2000 ? "..." : "");
    int ret = jk_ws_send_text(llm->ws, json_str);
    LISA_LOGI(TAG, "Sent register_tools with %d tools, ret=%d, json_len=%zu",
              added_count, ret, json_len);
    lisa_mem_free(json_str);
    cJSON_Delete(req);

    return ret;
}

int jk_llm_send_tool_result(jk_llm_t *llm, const char *call_id,
                            const char *result, bool success, const char *error) {
    if (!llm || !llm->ws || !call_id) return -1;
    if (llm->state != JK_LLM_STATE_CONNECTED) return -1;

    cJSON *req = cJSON_CreateObject();
    if (!req) return -1;

    cJSON_AddStringToObject(req, "type", "tool_result");
    cJSON_AddStringToObject(req, "call_id", call_id);
    cJSON_AddBoolToObject(req, "success", success);

    if (success && result) {
        /* result 是 JSON 字符串，解析后添加 */
        cJSON *result_obj = cJSON_Parse(result);
        if (result_obj) {
            cJSON_AddItemToObject(req, "result", result_obj);
        } else {
            /* 如果不是有效 JSON，作为字符串添加 */
            cJSON_AddStringToObject(req, "result", result);
        }
    } else if (!success && error) {
        cJSON_AddStringToObject(req, "error", error);
    }

    char *json_str = cJSON_PrintUnformatted(req);
    int ret = jk_ws_send_text(llm->ws, json_str);
    lisa_mem_free(json_str);
    cJSON_Delete(req);

    LISA_LOGI(TAG, "Sent tool_result: call_id=%s, success=%d, ret=%d", call_id, success, ret);
    return ret;
}
