#include <stdint.h>
#include "lvgl.h"
#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "../launcher.h"
#include "launcher_pages.h"
#include "../widgets/lv_group_grid.h"
#include "user_groups.h"
#include "lisaui_log.h"

#define TAG "launcher.page.main"

/**
 * 页面视图结构体 
 * 遵循MVC架构，视图仅负责UI渲染
 */
typedef struct {
    lv_obj_t *display;      /**< 显示容器 */
    lv_obj_t *group_grid;    /**< 组网格视图组件 */
} page_view_t;

/**
 * @brief 组网格事件处理函数
 * @param e 事件对象
 */
static void group_grid_event_handler(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    
    if(event_code == LV_GROUP_GRID_EVENT_ITEM_CLICK){
        // 获取点击的组信息
        lv_group_grid_event_data_t *data = lv_event_get_param(e);
        LISAUI_LOGI(TAG, "点击组: ID=%d, 索引=%d", data->group_id, data->item_index);
        
        // 进入目标组
        lisaui_manager_group_enter(data->group_id, GROUP_ENTER_PAGE_METHOD_POP_STACK_TOP, 0);
    }

}

/**
 * @brief 显示面板事件处理函数
 * @param e 事件对象
 */
static void display_panel_event_handler(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    
    if (event_code == LV_EVENT_GESTURE) {
        LISAUI_LOGI(TAG, "手势识别");
    }
}

/**
 * @brief 创建页面
 * @param page 页面对象
 * @return 创建的页面对象
 */
static lisaui_page_t *create(lisaui_page_t *page) {
    page_view_t *view = NULL;

    // 分配视图内存
    view = lisaui_malloc(sizeof(page_view_t));
    if (view == NULL) {
        LISAUI_LOGE(TAG, "[%s %d]no memory!", __FUNCTION__, __LINE__);
        goto _ERR;
    }
    
    // 创建显示容器（根对象）
    view->display = lv_obj_create(NULL); // launcher 页面根容器
    lisaui_common_set_style_container(view->display, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    lv_obj_set_size(view->display, LV_PCT(100), LV_PCT(100));
    lv_obj_add_event_cb(view->display, display_panel_event_handler, LV_EVENT_GESTURE, NULL);

    // 创建组网格视图组件（MVC架构中的View）
    view->group_grid = lv_group_grid_create(view->display);
    lv_group_grid_set_bg_color(view->group_grid, lv_color_hex(0x000000));
    lv_group_grid_set_event_cb(view->group_grid, group_grid_event_handler, NULL);
    
    // 在创建时不添加任何图标
    // 图标将在update_data函数调用时通过LAUNCHER_PAGE_MAIN_UPDATE_ADD_ICON事件添加
    // 这符合原来的设计逻辑

    // 保存视图
    page->view = view;

    LISAUI_LOGI(TAG, "Create page:%p", page);
    return page;

_ERR:
    if (NULL != view) {
        lisaui_free(view);
    }
    return NULL;
}

/**
 * @brief 销毁页面
 * @param page 页面对象
 * @return 错误码
 */
static lisaui_err_t destroy(lisaui_page_t *page) {
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for destroy");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 释放视图资源 */
    if (page->view) {
        page_view_t *view = (page_view_t *)page->view;
        
        /* 销毁显示容器（根对象）*/
        /* LVGL会自动递归删除所有子对象，包括group_grid */
        if (view->display) {
            lv_obj_del(view->display);
            view->display = NULL;
            view->group_grid = NULL; // 已经被自动删除
        }
        
        /* 释放视图结构体 */
        lisaui_free(view);
        page->view = NULL;
    }
    
    /* 释放页面私有数据（如果有的话） */
    if (page->page_data) {
        lisaui_free(page->page_data);
        page->page_data = NULL;
    }
    
    LISAUI_LOGI(TAG, "Page main destroyed");
    return LISAUI_ERR_OK;
}

static lisaui_err_t show(lisaui_page_t *page){

    LISAUI_LOGI(TAG,"Show page:%p",page);
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 显示页面主画面 */
    if (view->display) {
        lv_disp_load_scr(view->display);
    }
    
    LISAUI_LOGI(TAG, "Page main showed");
    return LISAUI_ERR_OK;
}

/**
 * @brief 关闭页面
 * @param page 页面对象
 * @return 错误码
 */
static lisaui_err_t close(lisaui_page_t *page) {
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    
    LISAUI_LOGI(TAG, "Page main closed");
    return LISAUI_ERR_OK;
}

/**
 * @brief 更新页面数据
 * @param page 页面对象
 * @param data 更新参数
 * @return 错误码
 */
static lisaui_err_t update_data(lisaui_page_t *page, void *data) {
    launcher_page_update_parameter_t *parameter = (launcher_page_update_parameter_t*)data;
    if (!page || !data) {
        LISAUI_LOGE(TAG, "Invalid parameters for update");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view || !view->group_grid) {
        LISAUI_LOGE(TAG, "Page view not created or group_grid is NULL");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 在这里实现页面内容的更新逻辑 */
    LISAUI_LOGI(TAG, "Updating launcher page");
    
    if (parameter->event == LAUNCHER_PAGE_MAIN_UPDATE_ADD_ICON) {
        lisaui_group_t *group = parameter->update_icon.group;
        if (group && !group->icon->hidden_icon) {
            // 将业务对象转换为通用视图数据结构
            lv_group_grid_item_t item = {
                .img_src = group->icon->res ? group->icon->res : &icon_img_app_default_png,
                .title = group->icon->title,
                .group_id = group->info.id,
                .zoom = group->icon->zoom,
                .user_data = group
            };
            
            // 添加组图标到网格
            lv_group_grid_add_item(view->group_grid, &item);
            LISAUI_LOGI(TAG, "添加组图标: %s", group->icon->title);
        }
    }

    LISAUI_LOGI(TAG, "Page main updated");
    return LISAUI_ERR_OK;
}

static const lisaui_page_t luancher_main_page = {
    .group_index = LISAUI_GROUP_INDEX_LAUNCHER,
    .page_index = LISAUI_GROUP_LUNCHER_PAGE_INDEX_MAIN,
    .cname = "launcher.main",
    .view = NULL,
    .page_data = NULL,
    .attribute = 0,
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

LISAUI_PAGE_EXPORT(luancher_main_page);
