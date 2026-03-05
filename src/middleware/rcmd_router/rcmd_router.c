
#include <stdint.h>
#include <string.h>

#include "utils/dlist.h"
#include "sys_init.h"
#include "workqueue.h"
#include "rcmd_router.h"

#define TAG "cmd_router"
#include "lisa_log.h"


#define RECOGNIZE_CMD_WORD_MAX_LENGTH 64

typedef struct {
    voice_msg_cloud_recognized_command_t cmd_data;
    bool is_active;
    sys_dnode_t node;

} cache_cmd_list_item_t;



typedef struct{
    
    rcmd_router_config_t config;
    workqueue_t *workqueue;           /* Workqueue for delayed tasks */
    sys_dlist_t cache_cmd_list;

}rcmd_router_t;

rcmd_router_t *s_cmd_router = NULL;

static void cmd_router_timer_callback(void *param)
{
    cache_cmd_list_item_t *cache_item = (cache_cmd_list_item_t *)param;

    if(!s_cmd_router || !cache_item){
        return;
    }

    /*超时未收到在线命令，执行离线命令*/
    if(cache_item->is_active){
        LOGI("cmd_router timeout, process offline command: %s, context id: %s",
             cache_item->cmd_data.command, cache_item->cmd_data.context.id);
        if(s_cmd_router->config.action){
            s_cmd_router->config.action(&cache_item->cmd_data, RCMD_ROUTER_CMD_TYPE_OFFLINE);
        }
        cache_item->is_active = false;
    }

    /*从缓存列表中移除该命令*/
    sys_dlist_remove(&cache_item->node);
    psram_free(cache_item);

}

void rcmd_router_offline_cmd_proc(voice_msg_cloud_recognized_command_t *data, bool is_cloud_connected)
{
    struct app_datas *app_datas = get_app_datas();

    if(!s_cmd_router){
        return;
    }

    if((s_cmd_router->config.strategy == CMD_ROUTER_STRATEGY_OFFLINE_ONLY)
        ||(is_cloud_connected != true)){
        if(s_cmd_router->config.action){
            s_cmd_router->config.action(data, RCMD_ROUTER_CMD_TYPE_OFFLINE);
        }
        return;
    }

    if(s_cmd_router->config.strategy == CMD_ROUTER_STRATEGY_TIMEOUT_FALLBACK){
        /*缓存离线命令，等待在线命令到来*/
        cache_cmd_list_item_t *cache_item = (cache_cmd_list_item_t *)psram_malloc(sizeof(cache_cmd_list_item_t));
        if(!cache_item){
            LOGE("cmd_router cache_item malloc failed");
            return;
        }
        memset(cache_item, 0, sizeof(cache_cmd_list_item_t));
        memcpy(&cache_item->cmd_data, data, sizeof(voice_msg_cloud_recognized_command_t));
        cache_item->is_active = true;
        sys_dlist_append(&s_cmd_router->cache_cmd_list, &cache_item->node);
        /*启动定时器，等待在线命令到来*/
        workqueue_submit(s_cmd_router->workqueue, cmd_router_timer_callback, cache_item, pdMS_TO_TICKS(s_cmd_router->config.online_timeout_ms));
    }
}


void rcmd_router_online_cmd_proc(voice_msg_cloud_recognized_command_t *data)
{
    struct app_datas *app_datas = get_app_datas();
    sys_dnode_t *node;

    if(!s_cmd_router){
        return;
    }

    if(s_cmd_router->config.strategy == CMD_ROUTER_STRATEGY_OFFLINE_ONLY){
        return;
    }

    /*遍历缓存命令表,失效已经缓存的命令*/
    SYS_DLIST_FOR_EACH_NODE(&s_cmd_router->cache_cmd_list, node)
    {
        cache_cmd_list_item_t *cache_cmd = CONTAINER_OF(node, cache_cmd_list_item_t, node);
        if (strcmp(cache_cmd->cmd_data.command, data->command) == 0) {
            /*找到匹配的离线命令，失效该命令*/
            cache_cmd->is_active = false;
            LOGI("cmd_router online command arrived, invalidate offline command: %s, context id: %s",
                 cache_cmd->cmd_data.command, cache_cmd->cmd_data.context.id);
            break;
        }
    }

    if(s_cmd_router->config.action){
        s_cmd_router->config.action(data, RCMD_ROUTER_CMD_TYPE_ONLINE);
    }

    return;
}

void rcmd_router_offline_status_proc(rcmd_router_status_e status, bool is_cloud_connected)
{
    struct app_datas *app_datas = get_app_datas();

    if(!s_cmd_router){
        return;
    }

    if((s_cmd_router->config.strategy == CMD_ROUTER_STRATEGY_OFFLINE_ONLY)
        ||(is_cloud_connected != true)){
        if(s_cmd_router->config.status_action){
            s_cmd_router->config.status_action(status, RCMD_ROUTER_CMD_TYPE_OFFLINE);
        }
        return;
    }

}

void rcmd_router_online_status_proc(rcmd_router_status_e status)
{
    struct app_datas *app_datas = get_app_datas();
    sys_dnode_t *node;

    if(!s_cmd_router){
        return;
    }

    if(s_cmd_router->config.strategy == CMD_ROUTER_STRATEGY_OFFLINE_ONLY){
        return;
    }

    if(s_cmd_router->config.status_action){
        s_cmd_router->config.status_action(RCMD_ROUTER_STATUS_SESSION_FINISH, RCMD_ROUTER_CMD_TYPE_ONLINE);
    }

    return;
}

int rcmd_router_init(rcmd_router_config_t *param)
{
    s_cmd_router = (rcmd_router_t *)psram_malloc(sizeof(rcmd_router_t));

    if (s_cmd_router == NULL) {
        LOGE("Failed to create s_cmd_router ");
        return -1;
    }
    memset(s_cmd_router,0,sizeof(rcmd_router_t));

    sys_dlist_init(&s_cmd_router->cache_cmd_list);
    

    s_cmd_router->workqueue = workqueue_create("cmd_router_wq", 5, 10, 2048);
    if (!s_cmd_router->workqueue) {
        LISA_LOGE(TAG, "Failed to create workqueue");
        return -1;
    }
    LISA_LOGI(TAG, "Workqueue created for timeout handling");
    memcpy(&s_cmd_router->config, param, sizeof(rcmd_router_config_t));

    LOGI("cmd_router initialized with timer");
    return 0; // Return 0 on success
}

