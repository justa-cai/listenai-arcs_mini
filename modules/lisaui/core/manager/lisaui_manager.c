#include "dlist.h"

#include "lisaui_group.h"
#include "lisaui_group_stack.h"
#include "lisaui_manager.h"
#include "lisaui_type.h"
#include "lisaui_log.h"
#include "platform.h" /* 用于内存分配函数lisaui_malloc/lisaui_free */

#define TAG "lisaui.manager"
typedef struct{
    uint32_t event_bits;
    lisaui_manager_event_cb_t cb;
    void *user_data;      /* 用户数据指针，会在回调时传递给用户 */
    sys_dnode_t node;
}lisaui_manager_event_cb_list_t;

typedef struct  {
    bool initialized;
    lisaui_group_t *current_group;
    sys_dlist_t cb_list;
    lisaui_group_stack_t *group_stack;

}lisaui_manager_t;



static lisaui_manager_t s_group_mgr = {
    .initialized = false,
    .current_group = NULL,

};


/**
 * @brief Initialize the manager module
 * 
 * This function initializes the manager module by:
 * 1. Initializing the callback list for event notifications
 * 2. Calling groups_init() to initialize all registered groups
 * 3. Marking the manager as initialized
 *
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_init(void){

    sys_dlist_init(&s_group_mgr.cb_list);
    s_group_mgr.group_stack = lisaui_group_stack_create();

    s_group_mgr.initialized = true;
    
    LISAUI_LOGI(TAG, "Lisaui manager initialized");
    return LISAUI_ERR_OK;
}

/**
 * @brief Deinitialize the LISAUI manager module
 * 
 * This function deinitializes the manager module, releasing any resources
 * that were allocated during initialization.
 *
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_deinit(void){
    // TODO: Implement proper resource cleanup
    return LISAUI_ERR_OK;
}

/**
 * @brief Add a callback function for specified manager events
 * 
 * This function registers a callback that will be called when any of the specified
 * events occur. The callback will receive the event type, the related group (if any),
 * and the user_data pointer provided during registration.
 *
 * @param events Bitwise OR of LISAUI_MANAGER_EVENT_* values indicating which events to listen for
 * @param cb Callback function to be called when the specified events occur
 * @param user_data User-provided data that will be passed to the callback
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_add_callback(uint32_t events, lisaui_manager_event_cb_t cb, void* user_data){
    if (!cb || !s_group_mgr.initialized) {
        LISAUI_LOGE(TAG, "Invalid callback or manager not initialized");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 分配回调节点内存 */
    lisaui_manager_event_cb_list_t *cb_node = (lisaui_manager_event_cb_list_t *)lisaui_malloc(sizeof(lisaui_manager_event_cb_list_t));
    if (cb_node == NULL) {
        LISAUI_LOGE(TAG, "Failed to allocate memory for callback node");
        return LISAUI_ERR_NO_MEMORY;
    }
    
    /* 初始化回调节点 */
    cb_node->event_bits = events;
    cb_node->cb = cb;
    cb_node->user_data = user_data;
    
    /* 将节点添加到链表中 */
    sys_dlist_append(&s_group_mgr.cb_list, &cb_node->node);
    
    LISAUI_LOGI(TAG, "Callback registered for events: 0x%x", events);
    return LISAUI_ERR_OK;
}

/**
 * @brief Remove a previously registered callback function
 * 
 * This function removes a callback that was previously registered with
 * lisaui_manager_add_callback().
 *
 * @param cb The callback function to remove
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_remove_callback(lisaui_manager_event_cb_t cb){
    if (!cb || !s_group_mgr.initialized) {
        LISAUI_LOGE(TAG, "Invalid callback or manager not initialized");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    sys_dnode_t *node, *next_node;
    lisaui_manager_event_cb_list_t *cb_item;
    bool found = false;
    
    /* 遍历回调链表查找目标回调函数 */
    SYS_DLIST_FOR_EACH_NODE_SAFE(&s_group_mgr.cb_list, node, next_node) {
        cb_item = CONTAINER_OF(node, lisaui_manager_event_cb_list_t, node);
        if (cb_item->cb == cb) {
            /* 找到目标回调，从链表移除 */
            sys_dlist_remove(node);
            lisaui_free(cb_item);
            found = true;
            LISAUI_LOGI(TAG, "Callback removed successfully");
        }
    }
    
    if (!found) {
        LISAUI_LOGW(TAG, "Callback not found in the registered list");
        return LISAUI_ERR_FAIL;
    }
    
    return LISAUI_ERR_OK;
}

/**
 * @brief Register a group with the LISAUI manager
 * 
 * This function registers a group with the manager, making it available for access
 * via the manager API. After registration, event callbacks will be notified with a
 * LISAUI_MANAGER_EVENT_GROUP_REGISTER event.
 *
 * @param group Pointer to the group structure to register
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_group_register(lisaui_group_t *group){
    if (!group || !s_group_mgr.initialized) {
        LISAUI_LOGE(TAG, "Invalid group pointer or manager not initialized");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (!group->setup || !group->cleanup || !group->enter || !group->exit) {
        LISAUI_LOGE(TAG, "Invalid group: missing required function pointers");
        return LISAUI_ERR_INVALID_PARAM;
    }

    /* 当前暂时不检查组ID是否重复，由调用者确保全局唯一性 */
    group->setup(group);
    LISAUI_LOGI(TAG, "Group [%s] (ID:%d) registered", group->info.name, group->info.id);
    
    /* 触发组注册事件通知 */
    sys_dnode_t *node;
    lisaui_manager_event_cb_list_t *cb_item;
    
    SYS_DLIST_FOR_EACH_NODE(&s_group_mgr.cb_list, node) {
        cb_item = CONTAINER_OF(node, lisaui_manager_event_cb_list_t, node);
        
        /* 检查该回调是否关注组注册事件 */
        if (cb_item->event_bits & LISAUI_MANAGER_EVENT_GROUP_REGISTER) {
            cb_item->cb(LISAUI_MANAGER_EVENT_GROUP_REGISTER, group, cb_item->user_data);
        }
    }
    
    return LISAUI_ERR_OK;
}

/**
 * @brief Unregister a group from the LISAUI manager
 * 
 * This function unregisters a previously registered group from the manager.
 * After unregistration, event callbacks will be notified with a
 * LISAUI_MANAGER_EVENT_GROUP_UNREGISTER event.
 *
 * @param group Pointer to the group structure to unregister
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_group_unregister(lisaui_group_t *group){
    if (!group || !s_group_mgr.initialized) {
        LISAUI_LOGE(TAG, "Invalid group pointer or manager not initialized");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    group->cleanup(group);
    /* 触发组注销事件通知 */
    sys_dnode_t *node;
    lisaui_manager_event_cb_list_t *cb_item;
    
    SYS_DLIST_FOR_EACH_NODE(&s_group_mgr.cb_list, node) {
        cb_item = CONTAINER_OF(node, lisaui_manager_event_cb_list_t, node);
        
        /* 检查该回调是否关注组注销事件 */
        if (cb_item->event_bits & LISAUI_MANAGER_EVENT_GROUP_UNREGISTER) {
            cb_item->cb(LISAUI_MANAGER_EVENT_GROUP_UNREGISTER, group, cb_item->user_data);
        }
    }
    
    LISAUI_LOGI(TAG, "Group [%s] (ID:%d) unregistered", group->info.name, group->info.id);
    return LISAUI_ERR_OK;
}


static inline lisaui_err_t _manager_group_exit(lisaui_group_t *group){
    
    if(group == NULL){
        return LISAUI_ERR_INVALID_PARAM;
    }

    /* 调用组的exit函数 */
    if(group->info.keep_in_stack){
        lisaui_group_stack_push(s_group_mgr.group_stack, group);
    }
    
    lisaui_err_t err = group->exit(group);
    if (err != LISAUI_ERR_OK) {
        LISAUI_LOGE(TAG, "Failed to exit group [%s] (ID:%d), error: %d", 
                   group->info.name, group->info.id, err);
        return err;
    }
    
    /* 触发组退出事件通知 */
    sys_dnode_t *node;
    lisaui_manager_event_cb_list_t *cb_item;
    
    SYS_DLIST_FOR_EACH_NODE(&s_group_mgr.cb_list, node) {
        cb_item = CONTAINER_OF(node, lisaui_manager_event_cb_list_t, node);
        
        /* 检查该回调是否关注组退出事件 */
        if (cb_item->event_bits & LISAUI_MANAGER_EVENT_GROUP_EXIT) {
            cb_item->cb(LISAUI_MANAGER_EVENT_GROUP_EXIT, group, cb_item->user_data);
        }
    }
    
    LISAUI_LOGI(TAG, "Exited group [%s] (ID:%d)", group->info.name, group->info.id);

    return LISAUI_ERR_OK;
}
static inline lisaui_err_t _manager_group_enter(lisaui_group_t *group,lisaui_group_enter_page_method_t method,int page_index,uint32_t flags)
{
    lisaui_err_t err;
    bool is_in_group = false;
    
    if((s_group_mgr.current_group != NULL)  && (s_group_mgr.current_group->info.id == group->info.id)){
        is_in_group = true;
    }

     /* 调用组的enter函数 */
    err = group->enter(group, method, page_index, flags);
    if (err != LISAUI_ERR_OK) {
        LISAUI_LOGE(TAG, "Failed to enter group [%s] (ID:%d), error: %d", 
                    group->info.name, group->info.id, err);
        return err;
    }
    
    /* 触发组进入事件通知 */
    sys_dnode_t *node;
    lisaui_manager_event_cb_list_t *cb_item;
    
    SYS_DLIST_FOR_EACH_NODE(&s_group_mgr.cb_list, node) {
        cb_item = CONTAINER_OF(node, lisaui_manager_event_cb_list_t, node);
        
        /* 检查该回调是否关注组进入事件 */
        if ((!is_in_group) && (cb_item->event_bits & LISAUI_MANAGER_EVENT_GROUP_ENTER)) {
            cb_item->cb(LISAUI_MANAGER_EVENT_GROUP_ENTER, group, cb_item->user_data);
        }
        if (cb_item->event_bits & LISAUI_MANAGER_EVENT_GROUP_SWITCH_PAGE) {
            cb_item->cb(LISAUI_MANAGER_EVENT_GROUP_SWITCH_PAGE, group, cb_item->user_data);
        }
    }
    s_group_mgr.current_group = group;
    LISAUI_LOGI(TAG, "Entered group [%s] (ID:%d), method:%d, page_index:%d", 
                group->info.name, group->info.id, method, page_index);
    return LISAUI_ERR_OK;   
}
/**
 * @brief Enter a specific group with the specified method and page index
 * 
 * This function activates the specified group and displays one of its pages.
 * The page to display is determined by the 'method' parameter:
 * - GROUP_ENTER_PAGE_METHOD_POP_STACK_TOP: Shows the page at the top of the group's page stack
 * - GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX: Shows the page specified by 'page_index'
 *
 * After entering the group, event callbacks will be notified with a
 * LISAUI_MANAGER_EVENT_GROUP_ENTER event.
 *
 * @param group_id ID of the group to enter
 * @param method Method to use when entering the group
 * @param page_index Index of the page to show (used when method is GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX)
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_group_enter(int group_id, lisaui_group_enter_page_method_t method, int page_index,uint32_t flags){
    
    lisaui_err_t ret;

    if (!s_group_mgr.initialized) {
        LISAUI_LOGE(TAG, "Manager not initialized");
        return LISAUI_ERR_INVALID_PARAM;
    }

    /* 找到指定的组 - 这里我们需要在实际实现中添加组的存储结构 */
    /* 注意：当前仅模拟实现，没有实际的组管理存储结构 */
    lisaui_group_t *group = NULL;
    
    /* 这里应该有代码查找对应ID的组 */
    group = lisaui_group_find_by_id(group_id);
    
    if (!group) {
        LISAUI_LOGE(TAG, "Cannot find group with ID: %d", group_id);
        return LISAUI_ERR_INVALID_PARAM;
    }

    if((group->current_page != NULL) && (group->current_page->flags & LISAUI_PAGE_FLAG_NAV_LOCKED)){
        LISAUI_LOGE(TAG, "Group [%s] (ID:%d) is locked", group->info.name, group->info.id);
        return LISAUI_ERR_PAGE_LOCKED;
    }

    /* 退出当前的GROUP*/
    if((s_group_mgr.current_group != NULL)  && (s_group_mgr.current_group->info.id != group_id)){
        _manager_group_exit(s_group_mgr.current_group);
    }

    /* 进入新的GROUP*/
    return _manager_group_enter(group,method,page_index,flags);
}


/**
 * @brief Exit a specific group
 * 
 * This function exits the specified group, hiding its UI elements.
 * After exiting the group, event callbacks will be notified with a
 * LISAUI_MANAGER_EVENT_GROUP_EXIT event.
 *
 * @param group_id ID of the group to exit
 * @return lisaui_err_t LISAUI_ERR_OK on success, otherwise an error code
 */
lisaui_err_t lisaui_manager_group_exit(int group_id){
    
    lisaui_group_t *group = NULL;
    lisaui_group_t *new_group = NULL;
    int ret;

    if (!s_group_mgr.initialized) {
        LISAUI_LOGE(TAG, "Manager not initialized");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 这里应该有代码查找对应ID的组 */
    group = lisaui_group_find_by_id(group_id); 
    if (!group) {
        LISAUI_LOGE(TAG, "Cannot find group with ID: %d", group_id);
        return LISAUI_ERR_INVALID_PARAM;
    }

    if((group->current_page != NULL) && (group->current_page->flags & LISAUI_PAGE_FLAG_NAV_LOCKED)){
        LISAUI_LOGE(TAG, "Group [%s] (ID:%d) is locked", group->info.name, group->info.id);
        return LISAUI_ERR_PAGE_LOCKED;
    }

    ret = _manager_group_exit(group);
    if(ret != LISAUI_ERR_OK){
        LISAUI_LOGE(TAG, "Failed to exit group [%s] (ID:%d), error: %d", 
                   group->info.name, group->info.id, ret);
        return ret;
    }
    new_group = lisaui_group_stack_pop(s_group_mgr.group_stack);
    if(new_group){
        _manager_group_enter(new_group, GROUP_ENTER_PAGE_METHOD_POP_STACK_TOP, 0,true);
    }
    
    return LISAUI_ERR_OK;
}


lisaui_group_t *lisaui_manager_get_current_group(void){
    return s_group_mgr.current_group;
}

lisaui_err_t lisaui_manager_send_event(lisaui_group_t *sender,uint32_t events){

    if (!s_group_mgr.initialized) {
        LISAUI_LOGE(TAG, "Manager not initialized");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 触发组进入事件通知 */
    sys_dnode_t *node;
    lisaui_manager_event_cb_list_t *cb_item;
    
    SYS_DLIST_FOR_EACH_NODE(&s_group_mgr.cb_list, node) {
        cb_item = CONTAINER_OF(node, lisaui_manager_event_cb_list_t, node);
        
        /* 检查该回调是否关注组进入事件 */
        if (cb_item->event_bits & events) {
            cb_item->cb(cb_item->event_bits & events, sender, cb_item->user_data);
        }
    }
    return LISAUI_ERR_OK;   
}


lisaui_err_t lisaui_manager_nav_back(void)
{
    lisaui_group_t *group = lisaui_manager_get_current_group();
    if(group == NULL){
        return LISAUI_ERR_GROUP_ID_INVALID;
    }

    lisaui_page_t *page = lisaui_page_stack_peek(group->page_stack);
    if(NULL == page){
        LISAUI_LOGI(TAG,"The page stack is empty,exit group:%s!",group->info.name);
        lisaui_manager_group_exit(group->info.id);
    }
    else{
        lisaui_manager_group_enter(group->info.id, GROUP_ENTER_PAGE_METHOD_POP_STACK_TOP, 0,0);
    }
    return LISAUI_ERR_OK;
}

