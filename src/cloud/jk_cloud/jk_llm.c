#define TAG "jk_llm"

#include "jk_llm.h"
#include "jk_websocket.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>

static void llm_on_event(jk_websocket_t *ws, jk_ws_event_type_e event, void *user) {
    jk_llm_t *llm = (jk_llm_t *)user;
    if (!llm) return;

    switch (event) {
    case JK_WS_EVENT_CONNECTED:
        LISA_LOGI(TAG, "LLM CONNECTED");
        llm->state = JK_LLM_STATE_CONNECTED;
        if (llm->cbs.on_connected) {
            llm->cbs.on_connected(llm);
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
        LISA_LOGE(TAG, "LLM ERROR");
        llm->state = JK_LLM_STATE_DISCONNECTED;
        if (llm->cbs.on_error) {
            llm->cbs.on_error(llm, "WebSocket error");
        }
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

    if (msg.arguments) lisa_mem_free(msg.arguments);
    if (msg.result) lisa_mem_free(msg.result);
    cJSON_Delete(json);
}

static void llm_on_data(jk_websocket_t *ws, jk_ws_data_type_e type,
                        const void *data, uint32_t len, void *user) {
    jk_llm_t *llm = (jk_llm_t *)user;
    if (!llm || !data) return;

    if (type != JK_WS_DATA_TEXT) {
        return;
    }

    parse_llm_message(llm, (const char *)data, len);
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
    if (!llm || !llm->ws) return -1;

    if (llm->state == JK_LLM_STATE_CONNECTED) {
        return 0;
    }

    LISA_LOGI(TAG, "Connecting to ws://%s:%s/", llm->host, llm->port);
    llm->state = JK_LLM_STATE_CONNECTING;

    int ret = jk_ws_connect(llm->ws);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to connect: %d", ret);
        llm->state = JK_LLM_STATE_DISCONNECTED;
        return -1;
    }

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
