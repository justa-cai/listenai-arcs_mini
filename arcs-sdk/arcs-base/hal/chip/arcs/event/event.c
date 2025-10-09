
#include "ls_err.h"
#include "ls_list.h"
#include "rtos_al.h"
#include "ls_event.h"
#include <string.h>
#include "event.h"


static bool event_inited = false;
static rtos_queue event_queue;
static rtos_task_handle event_task_handle;
static struct list_head event_list;

static bool event_is_inited(void)
{
	return event_inited;
}

static bool event_is_invalid(event_module_t event_module, int event_id)
{
	if ((event_module >= EVENT_MAX) || (event_module < 0))
		return true;

	if ((event_id != EVENT_ID_ALL) && (event_id < 0))
		return true;

	return false;
}

static int register_cb_node(event_cb_node_t *cb_node, event_register_info_t *reg_info)
{

    EVENT_LOGD("register cb node<%d %d>\n", reg_info->event_module_id, reg_info->event_id);

    if (cb_node)
    {
        cb_node->event_cb = reg_info->event_cb;
        cb_node->event_cb_arg = reg_info->event_cb_arg;
    }

    return LS_OK;
}

static int register_cb_to_event_node_list(struct list_head *event_node_list,
                event_register_info_t *reg_info)
{
    struct list_head *pos, *next;
    event_node_t *event_node = NULL;
    event_node_t *new_event_node = NULL;
    int ret;

    EVENT_LOGD("register cb to event node(%p)\n", event_node_list);
    list_for_each_safe(pos, next, event_node_list)
    {
        event_node = list_entry(pos, event_node_t, next);
        if (event_node && (event_node->event_id == reg_info->event_id) && ((reg_info->event_cb) == event_node->cb_node.event_cb)) 
        {
            return LS_OK;
        }
    }

    new_event_node = (event_node_t *)rtos_calloc(sizeof(event_node_t), sizeof(uint8_t));
    if (!new_event_node)
    {
        EVENT_LOGE("failed to alloc event node\n");
        return LS_ERR_NO_MEM;
    }

    EVENT_LOGD("new event node(%p)\n", new_event_node);
    INIT_LIST_HEAD(&new_event_node->next);
    new_event_node->event_id = reg_info->event_id;

    ret = register_cb_node(&new_event_node->cb_node, reg_info);
    if (ret != LS_OK) {
        rtos_free(new_event_node);
        return ret;
    }

    list_add_tail(&new_event_node->next, event_node_list);
    return LS_OK;
}

static int register_cb_to_module_node(event_module_node_t *module_node,
                            event_register_info_t *reg_info)
{
    EVENT_LOGD("register cb to module node(%p)\n", module_node);
    if (reg_info->event_id == EVENT_ID_ALL)
    {
        return register_cb_node(&module_node->cb_node, reg_info);
    }
    else
    {
        return register_cb_to_event_node_list(&module_node->event_node_list, reg_info);
    }
}

static int register_cb(event_register_info_t *reg_info)
{
    event_module_node_t *new_module_node = NULL;
    event_module_node_t *module_node = NULL;
    struct list_head *pos, *next;
    int ret = 0;

    EVENT_LOGD("handle event register, event<%d %d %p %p>\n",
                reg_info->event_module_id, reg_info->event_id, reg_info->event_cb,
                reg_info->event_cb_arg);

    list_for_each_safe(pos, next, &event_list) {
        module_node = list_entry(pos, event_module_node_t, next);
        if (module_node && (reg_info->event_module_id == module_node->event_module_id))
            return register_cb_to_module_node(module_node, reg_info);
    }

    new_module_node = (event_module_node_t *)rtos_calloc(sizeof(event_module_node_t), sizeof(uint8_t));
    if (!new_module_node)
        return LS_ERR_NO_MEM;

    EVENT_LOGD("new event module=%p\n", new_module_node);
    new_module_node->event_module_id = reg_info->event_module_id;
    INIT_LIST_HEAD(&new_module_node->next);
    INIT_LIST_HEAD(&new_module_node->event_node_list);
    rtos_event_create(&new_module_node->pending);
    ret = register_cb_to_module_node(new_module_node, reg_info);
    if (ret != LS_OK) {
        rtos_free(new_module_node);
        return ret;
    }

    list_add_tail(&new_module_node->next, &event_list);
    return LS_OK;
}

static int unregister_cb_node(event_cb_node_t *cb_node,
                event_register_info_t *unreg_info)
{
    EVENT_LOGD("unregister module event <%d %d %p> \n", unreg_info->event_module_id,
              unreg_info->event_id, unreg_info->event_cb);

    if(cb_node)
    {
        cb_node->event_cb = NULL;
        cb_node->event_cb_arg = NULL;
        return LS_OK;
    }

    EVENT_LOGD("event <%d %d %p> doesn't exist\n", unreg_info->event_module_id,
                unreg_info->event_id, unreg_info->event_cb);
    return LS_ERR_EVENT_NO_CB;
}

static int unregister_cb_from_event_node_list(struct list_head *event_node_list,
                                            event_register_info_t *unreg_info)
{
    struct list_head *pos, *next;
    event_node_t *event_node = NULL;
    int ret;

    EVENT_LOGD("unregister cb from event node(%p)\n", event_node_list);
    list_for_each_safe(pos, next, event_node_list)
    {
        event_node = list_entry(pos, event_node_t, next);
        if (event_node->event_id == unreg_info->event_id)
        {
            EVENT_LOGD("free event node(%p)\n", event_node);
            list_del(pos);
            rtos_free(event_node);
            return LS_OK;
        }
    }

    EVENT_LOGD("event <%d %d %p> doesn't exist\n", unreg_info->event_module_id,
                unreg_info->event_id, unreg_info->event_cb);
    return LS_ERR_EVENT_NO_CB;
}

static int unregister_cb_from_module_node(event_module_node_t *module_node,
                                event_register_info_t *unreg_info)
{
    EVENT_LOGD("unregister cb from module node(%p)\n", module_node);
    if (unreg_info->event_id == EVENT_ID_ALL)
    {
        return unregister_cb_node(&module_node->cb_node, unreg_info);
    }
    else
    {
        return unregister_cb_from_event_node_list(&module_node->event_node_list, unreg_info);
    }

    return LS_ERR_EVENT_NO_CB;
}

static int unregister_cb(event_register_info_t *unreg_info)
{
    event_module_node_t *module_node = NULL;
    struct list_head *pos, *next;
    int ret;

    EVENT_LOGD("handle event unregister, event<%d %d %p>\n",
                 unreg_info->event_module_id, unreg_info->event_id, unreg_info->event_cb);

    list_for_each_safe(pos, next, &event_list)
    {
        module_node = list_entry(pos, event_module_node_t, next);
        if (module_node && (unreg_info->event_module_id == module_node->event_module_id))
        {
            ret = unregister_cb_from_module_node(module_node, unreg_info);
            if (!module_node->cb_node.event_cb &&
                    list_empty(&module_node->event_node_list))
            {
                EVENT_LOGD("free module node(%p)\n", module_node);
                list_del(pos);
                rtos_event_delete(module_node->pending);
                rtos_free(module_node);
                return LS_OK;
            }

            return ret;
        }
    }

    EVENT_LOGD("event <%d %d %p> doesn't exist\n", unreg_info->event_module_id,
                unreg_info->event_id, unreg_info->event_cb);
    return LS_ERR_EVENT_NO_CB;
}

static void event_task_init(void)
{
    INIT_LIST_HEAD(&event_list);
}

static void event_deinit_module_node(event_module_node_t *event_module)
{
    struct list_head *pos, *next;
    event_node_t *event_node = NULL;

    list_for_each_safe(pos, next, &event_module->event_node_list)
    {
        event_node = list_entry(pos, event_node_t, next);
        list_del(pos);
        rtos_free(event_node);
        EVENT_LOGD("free event node(%p)\n", event_node);
    }
    rtos_event_delete(event_module->pending);
    rtos_free(event_module);
    EVENT_LOGD("free module(%p)\n", event_module);
}

static void event_deinit(void)
{
    event_module_node_t *module_node = NULL;
    struct list_head *pos, *next;

    EVENT_LOGD("event deinit\n");
    list_for_each_safe(pos, next, &event_list)
    {
        module_node = list_entry(pos, event_module_node_t, next);
        list_del(pos);
        event_deinit_module_node(module_node);
    }

    INIT_LIST_HEAD(&event_list);
}

static void event_task_deinit(void)
{
    EVENT_LOGI("event task deinit\n");

    event_deinit();
    if (event_queue)
    {
        rtos_queue_delete(event_queue);
        event_queue = NULL;
    }

    if (event_task_handle)
    {
        rtos_task_delete(event_task_handle);
        event_task_handle = NULL;
    }
}

static int event_post_to_cb_node(event_cb_node_t *cb_node, event_info_t *event_info)
{
    if (cb_node && cb_node->event_cb)
    {
        cb_node->event_cb(cb_node->event_cb_arg, event_info->event_module_id,
                      event_info->event_id, event_info->event_data);
        return LS_OK;
    }

    return LS_ERR_EVENT_NO_CB;
}

static int event_post_to_module(event_module_node_t *module_node,
								event_info_t *event_info)
{
    int ret = LS_OK;
    struct list_head *pos, *next;
    event_node_t *event_node;


    list_for_each_safe(pos, next, &module_node->event_node_list)
    {
        event_node = list_entry(pos, event_node_t, next);
        if (event_node && (event_node->event_id == event_info->event_id))
        {
            event_post_to_cb_node(&event_node->cb_node, event_info);
        }
    }

    event_post_to_cb_node(&module_node->cb_node, event_info); // if module has callback,means event id is EVENT_ID_ALL

    return ret;
}

static int event_post(event_info_t *event_info)
{
    event_module_node_t *module_node;
    struct list_head *pos, *next;
    int ret;

    if (event_is_invalid(event_info->event_module_id, event_info->event_id))
        return LS_ERR_EVENT_MOD_OR_ID;

    list_for_each_safe(pos, next, &event_list)
    {
        module_node = list_entry(pos, event_module_node_t, next);
        if (module_node && (module_node->event_module_id == event_info->event_module_id))
        {
            ret = event_post_to_module(module_node, event_info);
            if (event_info->event_data)
                rtos_free(event_info->event_data);
            return ret;
        }
    }

    if (event_info->event_data)
        rtos_free(event_info->event_data);

    EVENT_LOGW("event <%d %d> has no cb\n", event_info->event_module_id, event_info->event_id);
    return LS_ERR_EVENT_NO_CB;
}


static int event_task_handle_msg(event_msg_t *msg)
{
    if (!msg)
    {
        EVENT_LOGE("null event msg\n");
        return LS_ERR_EVENT_NULL_MSG;
    }

    switch (msg->msg_type)
    {
    case EVENT_MSG_REGISTER:
        return register_cb(&msg->msg.register_info);
        break;
    case EVENT_MSG_UNREGISTER:
        return unregister_cb(&msg->msg.register_info);
    case EVENT_MSG_POST:
        return event_post(&msg->msg.event_info);
    default:
        EVENT_LOGE("invalid event msg\n");
        return LS_ERR_EVENT_UNKNOWN_MSG;
    }

    return LS_OK;
}

static RTOS_TASK_FCT(event_task)
{
    event_msg_t *msg = NULL;
    int event_ret;
    int msg_type;

    while (1)
    {
        rtos_queue_read(event_queue, &msg, -1, false);

        event_ret = event_task_handle_msg(msg);

        if (msg->is_sync_msg)
        {
            msg->sync_msg_ret = event_ret;
            rtos_semaphore_signal(msg->sync_msg_sem, 0);
        }
        else
        {
            msg_type = msg->msg_type;

            rtos_free(msg);

            if (msg_type == EVENT_MSG_DEINIT)
                break;
        }
    }

    event_task_deinit();
}

static int event_send_msg_to_event_task(event_msg_t *pmsg, uint32_t timeout)
{
	int ret;
	bool is_snyc_msg = pmsg->is_sync_msg;

	if (pmsg->is_sync_msg) {
		ret = rtos_semaphore_create(&pmsg->sync_msg_sem, 1, 0);
		if (ret)
			return LS_ERR_EVENT_INIT_SEM;
	}

	ret = rtos_queue_write(event_queue, &pmsg, timeout, 0);
	if (ret) {
		if (is_snyc_msg)
			rtos_semaphore_delete(pmsg->sync_msg_sem);

		return LS_ERR_EVENT_POST_QUEUE;
	}

	if (is_snyc_msg) {
		rtos_semaphore_wait(pmsg->sync_msg_sem, LS_NEVER_TIMEOUT);
		rtos_semaphore_delete(pmsg->sync_msg_sem);
		return pmsg->sync_msg_ret;
	} else
		return LS_OK;
}

ls_err_t ls_event_init(void)
{
	int ret;

	if (event_is_inited()) {
		EVENT_LOGD("event already init, ignore request");
		return LS_OK;
	}
	event_task_init();
	if (rtos_queue_create(sizeof(event_msg_t *), EVENT_QUEUE_SIZE, &event_queue)) {
		EVENT_LOGE("failed to create event queue\n");
		return LS_ERR_EVENT_CREATE_QUEUE;
	}

	if (rtos_task_create(event_task, "event task", EVENT_TASK,
				EVENT_TASK_STACK_SIZE, NULL, EVENT_TASK_PRIORITY, &event_task_handle)) {
		rtos_queue_delete(event_queue);
		EVENT_LOGE("failed to create event task\n");
		return LS_ERR_EVENT_CREATE_TASK;
	}

	event_inited = true;
	CLOGD(EVENT_TAG,"inited\n");
	return LS_OK;
}


ls_err_t ls_event_deinit(void)
{
	event_msg_t *pmsg;
	ls_err_t ret;

	if (!event_is_inited())
		return LS_OK;

	event_inited = false;

	pmsg = rtos_calloc(sizeof(event_msg_t), sizeof(uint8_t));
	if (!pmsg)
		return LS_ERR_NO_MEM;

	pmsg->msg_type = EVENT_MSG_DEINIT;
	pmsg->is_sync_msg = false;

	ret = event_send_msg_to_event_task(pmsg, LS_WAIT_FOREVER);

	if (ret != LS_OK) {
		rtos_free(pmsg);
	}

	return ret;
}

ls_err_t ls_event_register_cb(event_module_t event_module_id, int event_id,
							  event_cb_t event_cb, void *event_cb_arg)
{
	event_register_info_t register_info = {0};

	if (!event_is_inited()) {
		EVENT_LOGE("event not init\n");
		return LS_ERR_EVENT_NOT_INIT;
	}

	if (event_is_invalid(event_module_id, event_id))
		return LS_ERR_EVENT_MOD_OR_ID;

	EVENT_LOGD("register event <%d %d %p %p>\n", event_module_id, event_id,
			event_cb, event_cb_arg);

	register_info.event_module_id = event_module_id;
	register_info.event_id = event_id;
	register_info.event_cb = event_cb;
	register_info.event_cb_arg = event_cb_arg;

	return register_cb(&register_info);
}

ls_err_t ls_event_unregister_cb(event_module_t event_module_id, int event_id,
								event_cb_t event_cb)
{
	event_msg_t msg = {0};
	event_msg_t *pmsg = &msg;

	if (!event_is_inited()) {
		EVENT_LOGE("unregister fail, event not init\n");
		return LS_ERR_EVENT_NOT_INIT;
	}

	if (event_is_invalid(event_module_id, event_id))
		return LS_ERR_EVENT_MOD_OR_ID;

	pmsg->msg_type = EVENT_MSG_UNREGISTER;
	pmsg->msg.register_info.event_module_id = event_module_id;
	pmsg->msg.register_info.event_id = event_id;
	pmsg->msg.register_info.event_cb = event_cb;
	pmsg->is_sync_msg = true;
	pmsg->sync_msg_ret = LS_OK;

	EVENT_LOGD("unregister event <%d, %d, %p>\n", event_module_id, event_id, event_cb);
	return event_send_msg_to_event_task(pmsg, LS_WAIT_FOREVER);
}

ls_err_t ls_event_clear(event_module_t event_module_id, int event_id)
{
    event_module_node_t *module_node = NULL;
    struct list_head *pos, *next;

    list_for_each(pos, &event_list) {
        module_node = list_entry(pos, event_module_node_t, next);
        if (module_node && (event_module_id == module_node->event_module_id)) {
            rtos_event_clear(module_node->pending, 1 << event_id);
            break;
        }
    }

    return LS_OK;
}

ls_err_t ls_event_wait(event_module_t event_module_id, int event_id, uint32_t timeout_ms)
{
    event_module_node_t *module_node = NULL;
    struct list_head *pos, *next;
    rtos_event_bit event_bit = 0;

    list_for_each(pos, &event_list) {
        module_node = list_entry(pos, event_module_node_t, next);
        if (module_node && (event_module_id == module_node->event_module_id)) {
            event_bit = rtos_event_wait(module_node->pending, 1 << event_id, timeout_ms);
            if((event_bit & (1 << event_id)) == 0) {
                return LS_FAIL;
            }
            break;
        }
    }

    return LS_OK;
}

ls_err_t ls_event_post(event_module_t event_module_id, int event_id,
						void *event_data, size_t event_data_size, uint32_t timeout, bool sync)
{
	void *event_data_copy = NULL;
	event_msg_t *pmsg;
	ls_err_t ret;
	bool is_sync_msg;
	event_module_node_t *module_node = NULL;
	struct list_head *pos, *next;

	if (!event_is_inited())
		return LS_ERR_EVENT_NOT_INIT;

	if (event_is_invalid(event_module_id, event_id))
		return LS_ERR_EVENT_MOD_OR_ID;

	pmsg = rtos_calloc(sizeof(event_msg_t), sizeof(uint8_t));
	if (!pmsg)
		return LS_ERR_NO_MEM;

	if (event_data_size > 0) {
		event_data_copy = rtos_calloc(event_data_size, sizeof(uint8_t));
		if (!event_data_copy) {
			rtos_free(pmsg);
			return LS_ERR_NO_MEM;
		}

		memcpy(event_data_copy, event_data, event_data_size);
	}

	pmsg->msg_type = EVENT_MSG_POST;
	pmsg->msg.event_info.event_module_id = event_module_id;
	pmsg->msg.event_info.event_id = event_id;
	pmsg->msg.event_info.event_data = event_data_copy;
	pmsg->is_sync_msg = is_sync_msg = sync;

	EVENT_LOGD("post event <%d %d %p %u>\n", event_module_id, event_id, event_data, timeout);
	ret = event_send_msg_to_event_task(pmsg, timeout);

	if (is_sync_msg || ret != LS_OK) {
		rtos_free(pmsg);
	}

	list_for_each(pos, &event_list) {
		module_node = list_entry(pos, event_module_node_t, next);
		if (module_node && (event_module_id == module_node->event_module_id)) {
			rtos_event_set(module_node->pending, 1 << event_id, false);
			break;
		}
	}

	return ret;
}

