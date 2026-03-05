#include "stdint.h"
#include <stdio.h>
#include <string.h>

#include "rcmd_router.h"
#include "voice_msg.h"
#include "sys_init.h"
#include "voice_cloud.h"
#include "app_datas.h"

#define TAG "recognize.cmd.msg"
#include "lisa_log.h"

static void recognize_cmd_msg_combine_pub(voice_msg_cloud_recognized_command_t *cmd, rcmd_router_cmd_type_e cmd_type)
{
    LISA_LOGI(TAG, "recognize_cmd_msg_combine_pub: %s, context id: %s, from type: %s",
        cmd->command, cmd->context.id,
        cmd_type == RCMD_ROUTER_CMD_TYPE_OFFLINE? "OFFLINE":"ONLINE");

    if(cmd_type == RCMD_ROUTER_CMD_TYPE_OFFLINE){
        voice_msg_pub(VOICE_MSG_RECOGNIZED_OFFLINE_COMMAND, cmd, sizeof(voice_msg_cloud_recognized_command_t));
    }
    else{
        voice_msg_pub(VOICE_MSG_RECOGNIZED_ONLINE_COMMAND, cmd, sizeof(voice_msg_cloud_recognized_command_t));
    }

}

static void recognize_status_msg_combine_pub(rcmd_router_status_e status, rcmd_router_cmd_type_e cmd_type)
{
    LISA_LOGI(TAG, "recognize_status_msg_combine_pub status: %d, from type: %s", status, 
        cmd_type == RCMD_ROUTER_CMD_TYPE_OFFLINE? "OFFLINE":"ONLINE");
        

    /*只有离线命令才需要终端处理finish,在线状态云端直接播报tts*/
    if((status == RCMD_ROUTER_STATUS_SESSION_FINISH)&&(cmd_type == RCMD_ROUTER_CMD_TYPE_OFFLINE)){
        voice_msg_pub(VOICE_MSG_RECOGNIZED_SESSION_FINISH, NULL, 0);
    }
    
}

/*离线命令处理*/
static void voice_offline_command_msg(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    struct app_datas *app_datas = get_app_datas();
    voice_msg_cloud_recognized_command_t cmd_data;
    memset(&cmd_data, 0, sizeof(cmd_data));
    snprintf(cmd_data.command, sizeof(cmd_data.command), "%s", (char *)data);
    // 离线命令没有 context.id，保持为空
    rcmd_router_offline_cmd_proc(&cmd_data, app_datas->voice_cloud_connected);
    LISA_LOGI(TAG, "voice_offline_command_msg: %s", (char *)data);
}

static void voice_offline_status_finish_msg(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    static int32_t prev_mode = -1;
    struct app_datas *app_datas = get_app_datas();
    if (app_datas == NULL) {
        LOGW("Invalid app_datas");
        return;
    }
    
    rcmd_router_offline_status_proc(RCMD_ROUTER_STATUS_SESSION_FINISH,app_datas->voice_cloud_connected);
}

static void voice_online_status_finish_msg(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    static int32_t prev_mode = -1;
    struct app_datas *app_datas = get_app_datas();
    if (app_datas == NULL) {
        LOGW("Invalid app_datas");
        return;
    }
    
    rcmd_router_online_status_proc(RCMD_ROUTER_STATUS_SESSION_FINISH);
}

/*在线命令处理*/
static void voice_online_command_msg(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    voice_msg_cloud_recognized_command_t *msg_cmd = (voice_msg_cloud_recognized_command_t *)data;
    struct app_datas *app_datas = get_app_datas();
    rcmd_router_online_cmd_proc(msg_cmd);
    LISA_LOGI(TAG, "voice_online_command_msg: %s, context id: %s", msg_cmd->command, msg_cmd->context.id);
}

/*初始化*/
int voice_rcmd_msg_process_init(void)
{
    int ret;
    rcmd_router_config_t config = {
        
#ifdef CONFIG_RCMD_ROUTER_STRATEGY_ONLINE_FIRST
        .strategy = CMD_ROUTER_STRATEGY_ONLINE_FIRST,
#elif defined(CONFIG_RCMD_ROUTER_STRATEGY_OFFLINE_ONLY)
        .strategy = CMD_ROUTER_STRATEGY_OFFLINE_ONLY,
#elif defined(CONFIG_RCMD_ROUTER_STRATEGY_TIMEOUT_FALLBACK)
        .strategy = CMD_ROUTER_STRATEGY_TIMEOUT_FALLBACK,
#else
        .strategy = CMD_ROUTER_STRATEGY_ONLINE_FIRST,
#endif


#ifdef CONFIG_RCMD_ROUTER_ONLINE_TIMEOUT_MS
        .online_timeout_ms = CONFIG_RCMD_ROUTER_ONLINE_TIMEOUT_MS,
#else
        .online_timeout_ms = 3000,
#endif
        .action = recognize_cmd_msg_combine_pub,
        .status_action = recognize_status_msg_combine_pub,
    };

    LOGI("recognize_cmd_msg_process_init");

    ret = rcmd_router_init(&config);
    if (ret != 0) {
        LOGE("rcmd_router_init failed:%d", ret);
        return ret;
    }
    voice_msg_sub(VOICE_MSG_WAKEUP_COMMAND, voice_offline_command_msg, NULL);
    voice_msg_sub(VOICE_MSG_WAKEUP_RECOGNIZED_FINISH, voice_offline_status_finish_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_RECOGNIZED_COMMAND, voice_online_command_msg, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SESSION_FINISHED, voice_online_status_finish_msg, NULL);
    
    

    return 0;
}

SYS_INIT(voice_rcmd_msg_process_init, SYS_INIT_LEVEL_PRE_APPLICATION, 90);
