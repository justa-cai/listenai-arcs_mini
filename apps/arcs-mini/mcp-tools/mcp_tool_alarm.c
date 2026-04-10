#define TAG "mcp_tool_alarm"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "cJSON.h"
#include "lisa_log.h"

#include "mcp.h"
#include "service_alarm.h"

#define ALARM_CTRL_FAIL(_msg)                            \
    do {                                                 \
        snprintf(result_text, text_len, "%s", (_msg));   \
        return -1;                                       \
    } while (0)

/* ==================== 参数解析工具 ==================== */

static void alarm_control_params_parse(cJSON *args, alarm_construct_params_t *params)
{
    if (!args || !params) {
        return;
    }

    cJSON *pval = NULL;

    pval = mcp_tool_call_args_get(args, "id");
    if (pval && cJSON_IsNumber(pval)) {
        params->cloud_id = (uint64_t)pval->valuedouble;
    }

    pval = mcp_tool_call_args_get(args, "action");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->action = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "alarm_type");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->alarm_type = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "holiday_name");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->holiday_name = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "calendar_type");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->calendar_type = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "time");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->time_str = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "date");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->date_str = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "lunar_date");
    if (pval && cJSON_IsString(pval) && pval->valuestring[0] != '\0') {
        params->lunar_date_str = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "day_of_week");
    if (pval && cJSON_IsNumber(pval)) {
        params->day_of_week = pval->valueint;
    }

    pval = mcp_tool_call_args_get(args, "day_of_month");
    if (pval && cJSON_IsNumber(pval)) {
        params->day_of_month = pval->valueint;
    }

    pval = mcp_tool_call_args_get(args, "month_of_year");
    if (pval && cJSON_IsNumber(pval)) {
        params->month_of_year = pval->valueint;
    }

    pval = mcp_tool_call_args_get(args, "text");
    if (pval && cJSON_IsString(pval)) {
        params->text = pval->valuestring;
    }

    pval = mcp_tool_call_args_get(args, "snooze_enabled");
    if (pval && cJSON_IsBool(pval)) {
        params->snooze_enabled = cJSON_IsTrue(pval) ? 1 : 0;
    }

    pval = mcp_tool_call_args_get(args, "snooze_interval");
    if (pval && cJSON_IsNumber(pval)) {
        params->snooze_interval = pval->valueint;
    }

    pval = mcp_tool_call_args_get(args, "snooze_count");
    if (pval && cJSON_IsNumber(pval)) {
        params->snooze_count = pval->valueint;
    }
}

/* ==================== 执行层 ==================== */

static int alarm_control_execute(const alarm_construct_params_t *params, char *result_text, size_t text_len)
{
    if (!params || !result_text || text_len == 0) {
        ALARM_CTRL_FAIL("缺少参数");
    }
    if (!params->action || params->action[0] == '\0') {
        ALARM_CTRL_FAIL("缺少操作类型");
    }
    if (params->cloud_id == 0) {
        ALARM_CTRL_FAIL("缺少闹钟id");
    }

    result_text[0] = '\0';

    // 执行闹钟指令
    if (params->action && strcmp(params->action, "CREATE") == 0) {
        // 调用 Service 层的便捷接口
        if (service_alarm_create_from_params(params, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }
    } else if (params->action && strcmp(params->action, "DELETE") == 0) {
        if (service_alarm_delete_by_cloud_id(params->cloud_id, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }
    } else if (params->action && strcmp(params->action, "UPDATE") == 0) {
        // 先删除再创建
        if (service_alarm_delete_by_cloud_id(params->cloud_id, result_text, text_len) != 0) {
            // 删除失败但不影响后续创建
        }

        if (service_alarm_create_from_params(params, result_text, text_len) != 0) {
            ALARM_CTRL_FAIL(result_text);
        }
    } else if (params->action && strcmp(params->action, "QUERY") == 0) {
        const alarm_object_t *target = service_alarm_find_by_cloud_id(params->cloud_id);
        if (!target) {
            ALARM_CTRL_FAIL("未找到对应闹钟");
        }

        snprintf(result_text, text_len, "查询成功, id=%llu, timestamp=%llu, text=%s",
                 (unsigned long long)target->cloud_id,
                 (unsigned long long)target->alarm_id,
                 target->text);
    } else {
        ALARM_CTRL_FAIL("不支持的操作类型");
    }

    return 0;
}

/* ==================== MCP 接口层 ==================== */

static cJSON *alarm_control_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    alarm_construct_params_t params;
    memset(&params, 0, sizeof(params));
    params.day_of_week = -1;
    params.day_of_month = -1;
    params.month_of_year = -1;
    params.snooze_enabled = -1;
    params.snooze_interval = -1;
    params.snooze_count = -1;

    alarm_control_params_parse(args, &params);

    char result_text[256] = {0};
    int exec_ret = alarm_control_execute(&params, result_text, sizeof(result_text));

    if (result_text[0] == '\0') {
        snprintf(result_text, sizeof(result_text), "%s", (exec_ret == 0) ? "闹钟操作成功" : "闹钟操作失败");
    }

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON_AddStringToObject(result, "content", result_text);
    cJSON_AddBoolToObject(result, "isError", exec_ret != 0);

    return result;
}

static cJSON *alarm_control_list(const char *name)
{
    cJSON *tool = cJSON_CreateObject();
    if (!tool) {
        return NULL;
    }

    cJSON_AddStringToObject(tool, "name", name);
    cJSON_AddStringToObject(tool, "description",
                            "智能闹钟与提醒管理系统。支持公历/农历、固定周期及动态节假日。核心准则：1.【禁止猜测时间】若用户仅提供'下午'、'早晨'、'待会'、'以后'等模糊时段，或未提及具体时间，严禁私自编造时间点，此时必须放弃调用函数，转而向用户询问具体几点几分。2. 识别到特定节日必须填充 holiday_name；3. 必须提供 cloud_id 用于闹钟的唯一标识和后续操作。");

    cJSON *schema = cJSON_CreateObject();
    if (!schema) {
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON_AddStringToObject(schema, "type", "object");

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        cJSON_Delete(schema);
        cJSON_Delete(tool);
        return NULL;
    }

    cJSON *action_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(action_prop, "type", "string");
    cJSON *action_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(action_enum, cJSON_CreateString("CREATE"));
    cJSON_AddItemToArray(action_enum, cJSON_CreateString("DELETE"));
    cJSON_AddItemToArray(action_enum, cJSON_CreateString("UPDATE"));
    cJSON_AddItemToArray(action_enum, cJSON_CreateString("QUERY"));
    cJSON_AddItemToObject(action_prop, "enum", action_enum);
    cJSON_AddStringToObject(action_prop, "description", "操作类型：CREATE(新建), DELETE(删除), UPDATE(修改), QUERY(查询)。");
    cJSON_AddItemToObject(properties, "action", action_prop);

    cJSON *alarm_type_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(alarm_type_prop, "type", "string");
    cJSON *alarm_type_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("ONCE"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("DAILY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("WEEKLY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("WORKDAY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("WEEKEND"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("MONTHLY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("YEARLY"));
    cJSON_AddItemToArray(alarm_type_enum, cJSON_CreateString("CUSTOM"));
    cJSON_AddItemToObject(alarm_type_prop, "enum", alarm_type_enum);
    cJSON_AddStringToObject(alarm_type_prop, "description", "循环策略。节日提醒固定使用 YEARLY。CUSTOM 类型必须提供 date 参数，触发后通过API查询下次时间。");
    cJSON_AddItemToObject(properties, "alarm_type", alarm_type_prop);

    cJSON *holiday_name_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(holiday_name_prop, "type", "string");
    cJSON_AddStringToObject(holiday_name_prop, "description", "节日名称（如'中秋节'、'母亲节'）。若用户提及节日，此项必填。");
    cJSON_AddItemToObject(properties, "holiday_name", holiday_name_prop);

    cJSON *calendar_type_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(calendar_type_prop, "type", "string");
    cJSON *calendar_type_enum = cJSON_CreateArray();
    cJSON_AddItemToArray(calendar_type_enum, cJSON_CreateString("GREGORIAN"));
    cJSON_AddItemToArray(calendar_type_enum, cJSON_CreateString("LUNAR"));
    cJSON_AddItemToObject(calendar_type_prop, "enum", calendar_type_enum);
    cJSON_AddStringToObject(calendar_type_prop, "default", "GREGORIAN");
    cJSON_AddStringToObject(calendar_type_prop, "description", "历法：GREGORIAN(公历), LUNAR(农历)。");
    cJSON_AddItemToObject(properties, "calendar_type", calendar_type_prop);

    cJSON *time_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(time_prop, "type", "string");
    cJSON_AddStringToObject(time_prop, "description", "24小时制时间 (HH:mm:ss)。严禁推测，未提及则反问用户。");
    cJSON_AddItemToObject(properties, "time", time_prop);

    cJSON *date_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(date_prop, "type", "string");
    cJSON_AddStringToObject(date_prop, "description", "具体日期 (YYYY-MM-DD)。仅在 ONCE 模式下使用。");
    cJSON_AddItemToObject(properties, "date", date_prop);

    cJSON *lunar_date_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(lunar_date_prop, "type", "string");
    cJSON_AddStringToObject(lunar_date_prop, "description",
                            "具体日期 (MM-DD)。仅在 calendar_type 为 LUNAR 模式下使用。需要输出农历对应的数字月日格式，如'正月初三'输出'01-03'。");
    cJSON_AddItemToObject(properties, "lunar_date", lunar_date_prop);

    cJSON *day_of_week_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(day_of_week_prop, "type", "integer");
    cJSON_AddNumberToObject(day_of_week_prop, "minimum", 1);
    cJSON_AddNumberToObject(day_of_week_prop, "maximum", 7);
    cJSON_AddStringToObject(day_of_week_prop, "description", "周几（1-7，1代表周一）。仅在 WEEKLY 模式有效。");
    cJSON_AddItemToObject(properties, "day_of_week", day_of_week_prop);

    cJSON *day_of_month_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(day_of_month_prop, "type", "integer");
    cJSON_AddNumberToObject(day_of_month_prop, "minimum", 1);
    cJSON_AddNumberToObject(day_of_month_prop, "maximum", 31);
    cJSON_AddStringToObject(day_of_month_prop, "description", "每月几号。仅在 MONTHLY 或无节日的 YEARLY 模式有效。");
    cJSON_AddItemToObject(properties, "day_of_month", day_of_month_prop);

    cJSON *month_of_year_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(month_of_year_prop, "type", "integer");
    cJSON_AddNumberToObject(month_of_year_prop, "minimum", 1);
    cJSON_AddNumberToObject(month_of_year_prop, "maximum", 12);
    cJSON_AddStringToObject(month_of_year_prop, "description", "每年几月。仅在无节日的 YEARLY 模式有效。");
    cJSON_AddItemToObject(properties, "month_of_year", month_of_year_prop);

    cJSON *text_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(text_prop, "type", "string");
    cJSON_AddStringToObject(text_prop, "description", "提醒的完整文本内容。例如：'给妈妈打电话庆祝生日'。");
    cJSON_AddItemToObject(properties, "text", text_prop);

    cJSON *snooze_enabled_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(snooze_enabled_prop, "type", "boolean");
    cJSON_AddBoolToObject(snooze_enabled_prop, "default", true);
    cJSON_AddStringToObject(snooze_enabled_prop, "description", "是否开启稍后提醒。默认开启。");
    cJSON_AddItemToObject(properties, "snooze_enabled", snooze_enabled_prop);

    cJSON *snooze_interval_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(snooze_interval_prop, "type", "integer");
    cJSON_AddNumberToObject(snooze_interval_prop, "default", 5);
    cJSON_AddNumberToObject(snooze_interval_prop, "minimum", 1);
    cJSON_AddNumberToObject(snooze_interval_prop, "maximum", 30);
    cJSON_AddStringToObject(snooze_interval_prop, "description", "稍后提醒的间隔时间（分钟）。默认 5 分钟，范围 1-30。");
    cJSON_AddItemToObject(properties, "snooze_interval", snooze_interval_prop);

    cJSON *snooze_count_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(snooze_count_prop, "type", "integer");
    cJSON_AddNumberToObject(snooze_count_prop, "default", 3);
    cJSON_AddNumberToObject(snooze_count_prop, "minimum", 1);
    cJSON_AddNumberToObject(snooze_count_prop, "maximum", 10);
    cJSON_AddStringToObject(snooze_count_prop, "description", "稍后提醒的最大次数。默认 3 次，范围 1-10。");
    cJSON_AddItemToObject(properties, "snooze_count", snooze_count_prop);


    cJSON_AddItemToObject(schema, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("action"));
    cJSON_AddItemToObject(schema, "required", required);

    cJSON_AddItemToObject(tool, "inputSchema", schema);

    return tool;
}

MCP_TOOL_DEFINE(ls.built_in.alarm_clock, alarm_control_list, alarm_control_call);