#include "led_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "../../led/led.h"

#define TAG "led_control"

// LED状态管理
static int led_state = 0;  // 0=关闭，1=开启
static char led_blink_mode[16] = "off";  // "off", "normal", "fast", "slow"

// 初始化LED控制模块
__attribute__((constructor)) static void led_control_init(void)
{
    app_led_init();
}

// LED开关控制处理函数
static mcp_result_t led_switch_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *action = NULL;
    
    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "action") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                action = ctx->params[i].value->valuestring;
            }
        }
    }

    if (!action) {
        response->content = cJSON_CreateString("错误：缺少必需的 action 参数");
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    int new_state = -1;
    if (strcmp(action, "on") == 0 || strcmp(action, "turn_on") == 0) {
        new_state = 1;
    } else if (strcmp(action, "off") == 0 || strcmp(action, "turn_off") == 0) {
        new_state = 0;
    } else {
        response->content = cJSON_CreateString("错误：action 只能是 'on'、'turn_on'、'off' 或 'turn_off'");
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    int old_state = led_state;
    led_state = new_state;

    // 如果LED关闭，同时停止闪烁
    if (led_state == 0) {
        strcpy(led_blink_mode, "off");
    }

    // 调用LED控制API
    if (led_state) {
        app_led_on();
    } else {
        app_led_off();
    }
    
    LISA_LOGI(TAG, "LED state changed from %s to %s", 
              old_state ? "ON" : "OFF", 
              led_state ? "ON" : "OFF");
    
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), 
            "LED已%s", led_state ? "开启" : "关闭");
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// LED闪烁控制处理函数
static mcp_result_t led_blink_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *mode = NULL;
    
    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "mode") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                mode = ctx->params[i].value->valuestring;
            }
        }
    }

    if (!mode) {
        response->content = cJSON_CreateString("错误：缺少必需的 mode 参数");
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    // 验证闪烁模式
    if (strcmp(mode, "off") != 0 && 
        strcmp(mode, "normal") != 0 && 
        strcmp(mode, "fast") != 0 && 
        strcmp(mode, "slow") != 0) {
        response->content = cJSON_CreateString("错误：mode 只能是 'off'、'normal'、'fast' 或 'slow'");
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    char old_mode[16];
    strcpy(old_mode, led_blink_mode);
    strcpy(led_blink_mode, mode);

    // 如果设置闪烁模式，自动开启LED
    if (strcmp(mode, "off") != 0) {
        led_state = 1;
    }

    // 根据闪烁模式设置LED
    if (strcmp(mode, "normal") == 0) {
        app_led_blink(500, 500);
    } else if (strcmp(mode, "fast") == 0) {
        app_led_blink(200, 200);
    } else if (strcmp(mode, "slow") == 0) {
        app_led_blink(1000, 1000);
    } else {  // off
        // 停止闪烁，保持当前状态
        app_led_stop();
        if (led_state) {
            app_led_on();
        } else {
            app_led_off();
        }
        response->content = cJSON_CreateString("LED闪烁已停止");
        response->result = MCP_RESULT_SUCCESS;
        return MCP_RESULT_SUCCESS;
    }

    LISA_LOGI(TAG, "LED blink mode changed from %s to %s", old_mode, led_blink_mode);
    
    char result_msg[256];
    if (strcmp(mode, "off") == 0) {
        snprintf(result_msg, sizeof(result_msg), "LED闪烁已关闭");
    } else {
        const char *mode_desc = "普通";
        if (strcmp(mode, "fast") == 0) {
            mode_desc = "快速";
        } else if (strcmp(mode, "slow") == 0) {
            mode_desc = "慢速";
        }
        snprintf(result_msg, sizeof(result_msg), "LED已设置为%s闪烁", mode_desc);
    }
    
    response->content = cJSON_CreateString(result_msg);
    response->result = MCP_RESULT_SUCCESS;
    
    return MCP_RESULT_SUCCESS;
}

// LED开关工具参数定义
static mcp_param_def_t led_switch_params[] = {
    MCP_PARAM_DEF("action", MCP_PARAM_STRING, true, 
                  "LED开关操作，可以是 'on'、'turn_on'、'off' 或 'turn_off'", 
                  NULL),
    MCP_PARAM_DEF_END
};

// LED闪烁工具参数定义
static mcp_param_def_t led_blink_params[] = {
    MCP_PARAM_DEF("mode", MCP_PARAM_STRING, true, 
                  "LED闪烁模式，可以是 'off'、'normal'、'fast' 或 'slow'", 
                  NULL),
    MCP_PARAM_DEF_END
};

// 注册LED开关控制工具
MCP_REGISTER_TOOL_STATIC(led_switch, 
                         "控制LED开关状态，可以是开启或关闭", 
                         "1.0", 
                         led_switch_params, 
                         1, 
                         led_switch_handler, 
                         false, 
                         NULL);

// 注册LED闪烁控制工具
MCP_REGISTER_TOOL_STATIC(led_blink, 
                         "控制LED闪烁模式，可以是关闭、普通、快速或慢速", 
                         "1.0", 
                         led_blink_params, 
                         1, 
                         led_blink_handler, 
                         false, 
                         NULL);

int get_led_state(void)
{
    // 返回LED当前状态
    // 注意：这里返回的是软件状态，不是实际硬件状态
    return led_state;
}

const char* get_led_blink_mode(void)
{
    // 返回LED当前闪烁模式
    return led_blink_mode;
}
