/**
 * @file lv_group_grid.c
 * @brief 应用组网格视图组件实现
 */

#include <stdint.h>
#include "lvgl.h"
#include "platform.h"
#include "lisaui_common.h"
#include "assets/assets_res.h"
#include "lisaui_log.h"
#include "lv_group_grid.h"

#define TAG "lv_group_grid"

/*********************
 * 类型定义
 *********************/

/**
 * 图标信息结构
 */
typedef struct {
    uint32_t group_id;           /**< 组ID */
    void *user_data;       /**< 用户数据 */
    lv_obj_t *icon_obj;    /**< 图标对象 */
} grid_icon_info_t;

/**
 * 网格组件数据结构
 */
typedef struct {
    lv_obj_t obj;          /**< 基类对象 */
    void *user_data;        /**< 用户数据 */

} lv_group_grid_t;

/*********************
 * 函数声明
 *********************/
static void grid_icon_event_handler(lv_event_t *e);
static void lv_group_grid_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lv_group_grid_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static grid_icon_info_t* create_grid_icon(lv_obj_t *grid, const lv_group_grid_item_t *item);

/*********************
 * 组件类定义
 *********************/
const lv_obj_class_t lv_group_grid_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = lv_group_grid_constructor,
    .destructor_cb = lv_group_grid_destructor,
    .instance_size = sizeof(lv_group_grid_t)
};

/*********************
 * 内部函数实现
 *********************/

/**
 * Create a grid icon widget
 * @param grid Parent grid object
 * @param item Item data
 * @return Pointer to the created icon info
 */
static grid_icon_info_t* create_grid_icon(lv_obj_t *grid, const lv_group_grid_item_t *item)
{
    // 分配图标信息结构体
    grid_icon_info_t *icon_info = (grid_icon_info_t *)lv_mem_alloc(sizeof(grid_icon_info_t));
    if (icon_info == NULL) {
        LISAUI_LOGE(TAG, "Failed to allocate icon_info");
        return NULL;
    }
    
    // 初始化图标信息
    icon_info->group_id = item->group_id;
    icon_info->user_data = item->user_data;
    icon_info->icon_obj = NULL;
    
    // 创建图标面板
    lv_obj_t *icon_panel = lv_obj_create(grid);
    lv_obj_set_height(icon_panel, LV_PCT(100));
    lisaui_common_set_style_container(icon_panel, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    lv_obj_clear_flag(icon_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    // 使整个面板可点击
    // lv_obj_add_flag(icon_panel, LV_OBJ_FLAG_CLICKABLE);
    
    // 创建图标图像
    lv_obj_t *app_icon = lv_img_create(icon_panel);
    
    // 调整图标位置
    lv_obj_align(app_icon, LV_ALIGN_CENTER, 0, -LV_DPX(20));
    lv_obj_add_flag(app_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(app_icon, grid_icon_event_handler, LV_EVENT_ALL, icon_info);
    lv_img_set_src(app_icon, item->img_src);
    if (item->zoom) {
        lv_img_set_zoom(app_icon, item->zoom);
    }
    icon_info->icon_obj = app_icon;
    
    // 创建图标标题
    lv_obj_t *app_title = lv_label_create(icon_panel);
    lv_label_set_text(app_title, item->title);
    lv_obj_align(app_title, LV_ALIGN_CENTER, 0, LV_DPX(50));
    
    // 设置文本样式
    lv_obj_set_style_text_color(app_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(app_title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(app_title, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    return icon_info;
}

/**
 * Handle grid icon events
 * @param e Event descriptor
 */
static void grid_icon_event_handler(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    grid_icon_info_t *icon_info = (grid_icon_info_t *)lv_event_get_user_data(e);
    lv_obj_t *grid_obj = lv_obj_get_parent(lv_obj_get_parent(target)); // 获取网格对象
    
    if (icon_info == NULL) {
        LISAUI_LOGW(TAG, "icon_info is NULL");
        return;
    }
    
    // 获取组件实例数据
    lv_group_grid_t *grid = (lv_group_grid_t *)grid_obj;
    if (grid == NULL) {
        LISAUI_LOGW(TAG, "grid is NULL");
        return;
    }
    
    // 准备事件数据
    lv_group_grid_event_data_t event_data = {
        .group_id = icon_info->group_id,
        .user_data = icon_info->user_data
    };
    
    if (event_code == LV_EVENT_CLICKED) {
        LISAUI_LOGI(TAG, "icon clicked: ID=%d, grid=%p", 
                icon_info->group_id, grid);
        
        // 触发点击事件
        lv_event_send(grid_obj, LV_GROUP_GRID_EVENT_ITEM_CLICK, &event_data);
        
    } 
    else if (event_code == LV_EVENT_PRESSED) {
        LISAUI_LOGI(TAG, "icon pressed: ID=%d,icon_info->icon_obj=%p", icon_info->group_id, icon_info->icon_obj);
        
        // 改变图标透明度
        if (icon_info->icon_obj) {
            LISAUI_LOGI(TAG,"SET OPACITY");
            lv_obj_set_style_img_opa(icon_info->icon_obj, LV_OPA_40, LV_PART_MAIN | LV_STATE_PRESSED);
        }
        
        // 触发按下事件
        lv_event_send(grid_obj, LV_GROUP_GRID_EVENT_ITEM_PRESS, &event_data);
        
    }
    else if (event_code == LV_EVENT_RELEASED || event_code == LV_EVENT_PRESS_LOST) {
        LISAUI_LOGI(TAG, "icon released: ID=%d", icon_info->group_id);
        
        // 恢复图标透明度
        if (icon_info->icon_obj) {
            lv_obj_set_style_img_opa(icon_info->icon_obj, LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        
        // 触发释放事件
        lv_event_send(grid_obj, LV_GROUP_GRID_EVENT_ITEM_RELEASE, &event_data);
    }
}

/**
 * Constructor for the grid widget
 * @param class_p Class pointer
 * @param obj Object to construct
 */
static void lv_group_grid_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    lv_group_grid_t *grid = (lv_group_grid_t *)obj;
    
    // 初始化内部数据
    grid->user_data = NULL;

    
    // 配置基类对象
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(70));
    lv_obj_set_y(obj, LV_DPX(LISAUI_STATUS_BAR_HEIGHT + 10));
    lisaui_common_set_style_container(obj, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    
    
    // 设置布局为Flex布局
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

/**
 * Destructor for the grid widget
 * @param class_p Class pointer
 * @param obj Object to destruct
 */
static void lv_group_grid_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    lv_group_grid_t *grid = (lv_group_grid_t *)obj;
    
}

/*********************
 * 公共API实现
 *********************/

/**
 * Get the grid class
 * @return Pointer to the grid class
 */
const lv_obj_class_t *lv_group_grid_class_get(void)
{
    return &lv_group_grid_class;
}

/**
 * Create a group grid widget
 * @param parent Parent object
 * @return Pointer to the created grid
 */
lv_obj_t *lv_group_grid_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lv_group_grid_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/**
 * Add an item to the grid
 * @param obj Grid object
 * @param item_data Item data
 * @return true if successful, false otherwise
 */
bool lv_group_grid_add_item(lv_obj_t *obj, const lv_group_grid_item_t *item_data)
{
    if (obj == NULL || item_data == NULL) {
        LISAUI_LOGE(TAG, "Invalid parameters");
        return false;
    }
    
    lv_group_grid_t *grid = (lv_group_grid_t *)obj;
    

    // 创建图标并保存信息
    grid_icon_info_t *icon_info = create_grid_icon(obj, item_data);
    if (icon_info == NULL) {
        LISAUI_LOGE(TAG, "Failed to create grid icon");
        return false;
    }
    
    
    return true;
}

/**
 * Clear all items from the grid
 * @param obj Grid object
 */
void lv_group_grid_clear_all(lv_obj_t *obj)
{
    if (obj == NULL) {
        LISAUI_LOGE(TAG, "Invalid grid object");
        return;
    }
    
    lv_group_grid_t *grid = (lv_group_grid_t *)obj;
    
    // 删除所有子对象
    lv_obj_clean(obj);
    

}

/**
 * Set the event callback for the grid
 * @param obj Grid object
 * @param event_cb Event callback function
 * @param user_data User data
 */
void lv_group_grid_set_event_cb(lv_obj_t *obj, lv_event_cb_t event_cb, void *user_data)
{
    if (obj == NULL) {
        LISAUI_LOGE(TAG, "Invalid grid object");
        return;
    }
    
    lv_group_grid_t *grid = (lv_group_grid_t *)obj;
    lv_obj_add_event_cb(obj, event_cb, LV_GROUP_GRID_EVENT_ITEM_CLICK, user_data);
    lv_obj_add_event_cb(obj, event_cb, LV_GROUP_GRID_EVENT_ITEM_PRESS, user_data);
    lv_obj_add_event_cb(obj, event_cb, LV_GROUP_GRID_EVENT_ITEM_RELEASE, user_data);
}

/**
 * Set the background color of the grid
 * @param obj Grid object
 * @param color Background color
 */
void lv_group_grid_set_bg_color(lv_obj_t *obj, lv_color_t color)
{
    if (obj == NULL) {
        LISAUI_LOGE(TAG, "Invalid grid object");
        return;
    }
    
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
}
