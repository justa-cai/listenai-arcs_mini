#include <assert.h>
#include <string.h>
#include "cJSON.h"

#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_mutex.h"

#include "cc/slist.h"
#include "voice_msg.h"
#include "mcp.h"

struct async_call_item {
    uint32_t timeout;
    char id[64];
};

static bool volatile is_init = false;
static SList *tool_dynamic_slist = NULL;
static SList *tool_call_list = NULL;
static lisa_mutex_t *tool_list_mutex = NULL;

static void *mem_dup(const void *src, size_t size)
{
    void *dest = lisa_mem_alloc(size);
    if (!dest) {
        return NULL;
    }

    memcpy(dest, src, size);

    return dest;
}

static void mem_free(void *ptr)
{
    lisa_mem_free(ptr);
}

static int mcp_tool_init(void)
{
    enum cc_stat stat;

    if (!is_init) {
        is_init = true;
    }

    SListConf conf = {
        .mem_alloc = (void *(*)(size_t))lisa_mem_alloc,
        .mem_free = lisa_mem_free,
        .mem_calloc = (void *(*)(size_t, size_t))lisa_mem_calloc,
    };

    stat = slist_new_conf(&conf, &tool_dynamic_slist);
    assert(stat == CC_OK);

    stat = slist_new_conf(&conf, &tool_call_list);
    assert(stat == CC_OK);

    return 0;
}

static const struct mcp_tool *mcp_tool_find(const char *name)
{
    lisa_mutex_lock(tool_list_mutex, LISA_OS_WAIT_FOREVER);

    SLIST_FOREACH(el, tool_dynamic_slist, {
        struct mcp_tool *tool = (struct mcp_tool *)el;
        if (tool && strcmp(tool->name, name) == 0) {
            lisa_mutex_unlock(tool_list_mutex);
            return tool;
        }
    });

    lisa_mutex_unlock(tool_list_mutex);

    extern const struct mcp_tool __mcp_tool_start[];
    extern const struct mcp_tool __mcp_tool_end[];

    const struct mcp_tool *tool = __mcp_tool_start;

    for (tool = (const struct mcp_tool *)__mcp_tool_start; tool < (const struct mcp_tool *)__mcp_tool_end; tool++) {
        if (strcmp(tool->name, name) == 0) {
            return tool;
        }
    }

    return NULL;
}

static const struct async_call_item *mcp_tool_async_call_item_find_by_id(const char *id)
{
    SLIST_FOREACH(el, tool_call_list, {
        struct async_call_item *call_item = (struct async_call_item *)el;
        if (call_item && strcmp(call_item->id, id) == 0) {
            return call_item;
        }
    });

    return NULL;
}

static int mcp_tool_async_call_item_add(const struct async_call_item *item)
{
    struct async_call_item *cpy = mem_dup(item, sizeof(struct async_call_item));
    if (cpy == NULL) {
        LOGE("mcp tool %s mem dup failed.", item->id);
        return -1;
    }

    lisa_mutex_lock(tool_list_mutex, LISA_OS_WAIT_FOREVER);
    slist_add(tool_call_list, cpy);
    lisa_mutex_unlock(tool_list_mutex);

    return 0;
}

static void mcp_tool_async_call_item_remove_by_id(const char *id)
{
    lisa_mutex_lock(tool_list_mutex, LISA_OS_WAIT_FOREVER);
    SLIST_FOREACH(el, tool_call_list, {
        struct async_call_item *call_item = (struct async_call_item *)el;
        if (call_item && strcmp(call_item->id, id) == 0) {
            slist_remove(tool_call_list, call_item, NULL);
            mem_free(call_item);
            lisa_mutex_unlock(tool_list_mutex);
            break;
        }
    });
    lisa_mutex_unlock(tool_list_mutex);
}

static int mcp_tool_count_total(void)
{
    int count = 0;

    lisa_mutex_lock(tool_list_mutex, LISA_OS_WAIT_FOREVER);
    SLIST_FOREACH(el, tool_dynamic_slist, { count++; });
    lisa_mutex_unlock(tool_list_mutex);

    extern const struct mcp_tool __mcp_tool_start[];
    extern const struct mcp_tool __mcp_tool_end[];

    for (const struct mcp_tool *tool = __mcp_tool_start; tool < __mcp_tool_end; tool++) {
        count++;
    }

    return count;
}

static cJSON *mcp_server_capabilities_get(void)
{
    cJSON *capabilities = cJSON_CreateObject();
    if (!capabilities) {
        LOGE("mcp server capabilities get, cJSON_CreateObject failed");
        return NULL;
    }

    cJSON *tools = cJSON_CreateObject();
    if (tools) {
        cJSON_AddItemToObject(capabilities, "tools", tools);
        cJSON_AddNumberToObject(tools, "count", mcp_tool_count_total());

        cJSON *categories = cJSON_CreateArray();
        if (categories) {
            cJSON_AddItemToArray(categories, cJSON_CreateString("system"));
            cJSON_AddItemToArray(categories, cJSON_CreateString("device"));
            cJSON_AddItemToArray(categories, cJSON_CreateString("utility"));
            cJSON_AddItemToObject(tools, "categories", categories);
        }
    }

    return capabilities;
}

static cJSON *mcp_server_info_get(void)
{
    cJSON *server_info = cJSON_CreateObject();
    if (!server_info) {
        LOGE("mcp server info get, cJSON_CreateObject failed");
        return NULL;
    }

    cJSON_AddStringToObject(server_info, "name", "arcs-mini");
    cJSON_AddStringToObject(server_info, "version", "1.0.0");

    return server_info;
}

static const char *mcp_instructions_get(void)
{
    return "设备端MCP服务，提供系统信息查询、设备控制等工具调用功能";
}

static cJSON *mcp_do_initialize(cJSON *msg)
{

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        LOGE("mcp do initialize, cJSON_CreateObject failed");
        return NULL;
    }

    cJSON_AddItemToObject(result, "capabilities", mcp_server_capabilities_get());
    cJSON_AddItemToObject(result, "serverInfo", mcp_server_info_get());
    cJSON_AddStringToObject(result, "instructions", mcp_instructions_get());

    return result;
}

static cJSON *mcp_do_tools_list(cJSON *msg)
{
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        LOGE("mcp do tools list, cJSON_CreateObject failed");
        return NULL;
    }

    cJSON *tools = cJSON_CreateArray();
    if (!tools) {
        LOGE("mcp do tools list, cJSON_CreateArray failed");
        cJSON_Delete(result);
        return NULL;
    }

    lisa_mutex_lock(tool_list_mutex, LISA_OS_WAIT_FOREVER);
    SLIST_FOREACH(el, tool_dynamic_slist, {
        struct mcp_tool *tool = (struct mcp_tool *)el;
        if (tool && tool->list) {
            cJSON *tool_info = tool->list(tool->name);
            if (tool_info) {
                cJSON_AddItemToArray(tools, tool_info);
                LOGI("mcp tool: %s", tool->name);
            }
        }
    });
    lisa_mutex_unlock(tool_list_mutex);

    extern const struct mcp_tool __mcp_tool_start[];
    extern const struct mcp_tool __mcp_tool_end[];

    const struct mcp_tool *tool = __mcp_tool_start;

    for (tool = (const struct mcp_tool *)__mcp_tool_start; tool < (const struct mcp_tool *)__mcp_tool_end; tool++) {
        if (tool && tool->list) {
            cJSON *tool_info = tool->list(tool->name);
            if (tool_info) {
                cJSON_AddItemToArray(tools, tool_info);
                LOGI("mcp tool: %s", tool->name);
            }
        }
    }

    cJSON_AddItemToObject(result, "tools", tools);

    return result;
}

int mcp_tool_call_result_response(const char *id, cJSON *result)
{
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        LOGE("mcp tool call result response, cJSON_CreateObject failed");
        return -1;
    }

    cJSON_AddStringToObject(response, "method", "tools/call");
    cJSON_AddStringToObject(response, "id", id);
    cJSON_AddItemToObject(response, "result", cJSON_Duplicate(result, 1));
    cJSON_AddStringToObject(response, "action", "mcp");
    const char *response_str = cJSON_PrintUnformatted(response);

    if (response_str) {
        voice_msg_pub(VOICE_MSG_CLOUD_MCP_CALL_RESP, (void *)response_str, strlen(response_str) + 1);
        cJSON_free((void *)response_str);
    }

    cJSON_Delete(response);

    return 0;
}

static cJSON *mcp_do_tools_call(const char *id, cJSON *msg)
{
    const cJSON *params = cJSON_GetObjectItem(msg, "params");
    if (!cJSON_IsObject(params)) {
        LOGE("mcp do tools call, params is not object");
        return NULL;
    }

    const cJSON *name = cJSON_GetObjectItem(params, "name");
    if (!cJSON_IsString(name)) {
        LOGE("mcp do tools call, name is not string");
        return NULL;
    }

    const char *name_str = name->valuestring;

    const cJSON *args = cJSON_GetObjectItem(params, "arguments");
    const struct mcp_tool *tool = mcp_tool_find(name_str);
    if (!tool) {
        LOGE("mcp do tools call, tool %s not found", name_str);
        return NULL;
    }

    if (!tool->call) {
        LOGE("mcp do tools call, tool %s call is null", name_str);
        return NULL;
    }

    cJSON *result = tool->call(id, name_str, (cJSON *)args);
    if (!result) {
        LOGI("mcp do tools call, tool %s call no result, waiting for async call", name_str);
        // struct async_call_item item = {0};
        // memcpy(item.id, id, strlen(id) + 1);
        // item.timeout = 3000 + pdTICKS_TO_MS(xTaskGetTickCount());
        // mcp_tool_async_call_item_add(&item);

        return NULL;
    }

    return result;
}

int mcp_init(void)
{
    tool_list_mutex = lisa_mutex_create();

    assert(tool_list_mutex != NULL);

    mcp_tool_init();

    return 0;
}

cJSON *mcp_process(cJSON *msg)
{
    const cJSON *method = cJSON_GetObjectItem(msg, "method");

    if (!cJSON_IsString(method)) {
        LOGE("mcp process, method is not string");
        return NULL;
    }

    const char *method_str = method->valuestring;
    const cJSON *id = cJSON_GetObjectItem(msg, "id");

    if (!cJSON_IsString(id)) {
        LOGE("mcp process, id is not string");
        return NULL;
    }

    if (strcmp(method_str, "initialize") == 0) {
        cJSON *result = mcp_do_initialize(msg);
        cJSON *response = cJSON_CreateObject();
        if (!response) {
            LOGE("mcp process, cJSON_CreateObject failed");
            return NULL;
        }
        cJSON_AddStringToObject(response, "method", method_str);
        cJSON_AddStringToObject(response, "id", id->valuestring);
        cJSON_AddItemToObject(response, "result", result);
        return response;
    } else if (strcmp(method_str, "tools/list") == 0) {
        cJSON *result = mcp_do_tools_list(msg);
        cJSON *response = cJSON_CreateObject();
        if (!response) {
            LOGE("mcp process, cJSON_CreateObject failed");
            return NULL;
        }
        cJSON_AddStringToObject(response, "method", method_str);
        cJSON_AddStringToObject(response, "id", id->valuestring);
        cJSON_AddItemToObject(response, "result", result);
        return response;
    } else if (strcmp(method_str, "tools/call") == 0) {
        /* 这里比较特殊, 如果result不为空, 则直接进行响应, 否则是异步的通过mcp_tool_resp进行响应 */
        cJSON *result = mcp_do_tools_call(id->valuestring, msg);
        if (result) {
            cJSON *response = cJSON_CreateObject();
            if (!response) {
                LOGE("mcp process, cJSON_CreateObject failed");
                return NULL;
            }
            cJSON_AddStringToObject(response, "method", method_str);
            cJSON_AddStringToObject(response, "id", id->valuestring);
            cJSON_AddItemToObject(response, "result", result);
            return response;
        }
        return NULL;
    } else {
        LOGE("mcp process, unknown method: %s", method_str);
        return NULL;
    }
}

int mcp_tool_add(const struct mcp_tool *tool)
{
    if (mcp_tool_find(tool->name) != NULL) {
        LOGE("mcp tool %s already exists.", tool->name);
        return -1;
    }

    struct mcp_tool *cpy = mem_dup(tool, sizeof(struct mcp_tool));
    if (cpy == NULL) {
        LOGE("mcp tool %s mem dup failed.", tool->name);
        return -1;
    }

    cpy->name = mem_dup(tool->name, strlen(tool->name) + 1);
    if (cpy->name == NULL) {
        LOGE("mcp tool %s mem dup failed.", tool->name);
        mem_free(cpy);
        return -1;
    }

    lisa_mutex_lock(tool_list_mutex, LISA_OS_WAIT_FOREVER);
    slist_add(tool_dynamic_slist, cpy);
    lisa_mutex_unlock(tool_list_mutex);

    return 0;
}

int mcp_tool_remove(const char *name)
{
    const struct mcp_tool *tool = mcp_tool_find(name);
    if (tool == NULL) {
        LOGE("mcp tool %s not found.", name);
        return -1;
    }

    lisa_mutex_lock(tool_list_mutex, LISA_OS_WAIT_FOREVER);
    slist_remove(tool_dynamic_slist, (void *)tool, NULL);
    lisa_mutex_unlock(tool_list_mutex);

    mem_free(tool->name);
    mem_free((void *)tool);

    return 0;
}

cJSON *mcp_tool_list_info_create_default(const char *name, const char *desc)
{
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }

    cJSON_AddStringToObject(result, "name", name);
    cJSON_AddStringToObject(result, "description", desc);

    cJSON *input_schema = cJSON_CreateObject();
    if (!input_schema) {
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddItemToObject(result, "inputSchema", input_schema);
    cJSON_AddStringToObject(input_schema, "type", "object");

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddItemToObject(input_schema, "properties", properties);
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddItemToObject(input_schema, "required", required);
    cJSON_AddStringToObject(input_schema, "additionalProperties", "false");

    return result;
}

cJSON *mcp_tool_info_properties_get(cJSON *tool)
{
    cJSON *input_schema = cJSON_GetObjectItem(tool, "inputSchema");
    if (!cJSON_IsObject(input_schema)) {
        input_schema = cJSON_CreateObject();
        if (!input_schema) {
            return NULL;
        }
        cJSON_AddItemToObject(tool, "inputSchema", input_schema);
    }

    cJSON *properties = cJSON_GetObjectItem(input_schema, "properties");
    if (!cJSON_IsObject(properties)) {
        properties = cJSON_CreateObject();
        if (!properties) {
            return NULL;
        }
        cJSON_AddItemToObject(input_schema, "properties", properties);
    }

    return properties;
}

cJSON *mcp_tool_info_required_get(cJSON *tool)
{
    cJSON *input_schema = cJSON_GetObjectItem(tool, "inputSchema");
    if (!cJSON_IsObject(input_schema)) {
        return NULL;
    }

    cJSON *required = cJSON_GetObjectItem(input_schema, "required");
    if (!cJSON_IsArray(required)) {
        required = cJSON_CreateArray();
        if (!required) {
            return NULL;
        }
        cJSON_AddItemToObject(input_schema, "required", required);
    }

    return required;
}

void mcp_tool_info_add_property(cJSON *tool, const char *name, const char *desc, const char *type, uint8_t required)
{
    cJSON *properties = mcp_tool_info_properties_get(tool);
    if (!properties) {
        return;
    }

    cJSON *property = cJSON_CreateObject();
    if (!property) {
        return;
    }

    cJSON_AddStringToObject(property, "description", desc);
    cJSON_AddStringToObject(property, "type", type);
    cJSON_AddItemToObject(properties, name, property);

    if (required) {
        cJSON *required = mcp_tool_info_required_get(tool);
        if (!required) {
            return;
        }
        cJSON_AddItemToArray(required, cJSON_CreateString(name));
    }
}

void mcp_tool_info_add_json_property(cJSON *tool, const char *name, cJSON *property, uint8_t required)
{
    cJSON *properties = mcp_tool_info_properties_get(tool);
    if (!properties) {
        return;
    }

    cJSON_AddItemToObject(properties, name, property);

    if (required) {
        cJSON *required = mcp_tool_info_required_get(tool);
        if (!required) {
            return;
        }
        cJSON_AddItemToArray(required, cJSON_CreateString(name));
    }
}

cJSON *mcp_tool_call_args_get(cJSON *args, const char *name)
{
    cJSON *arg = cJSON_GetObjectItem(args, name);

    return arg;
}

cJSON *mcp_tool_call_result_create(const char *name)
{
    cJSON *call_result = cJSON_CreateObject();
    if (!call_result) {
        return NULL;
    }

    cJSON_AddStringToObject(call_result, "tool", name);

    return call_result;
}
