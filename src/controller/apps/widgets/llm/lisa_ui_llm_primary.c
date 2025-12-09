#include "lisa_ui_res.h"
#include "lisa_ui_llm_base.h"
#include "lisa_ui_llm_primary.h"
#include "lisa_log.h"
#include <string.h>

static void emoji_timer_cb(lv_timer_t *timer);
static void lisa_ui_llm_primary_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
// static void lisa_ui_llm_primary_class_destructor(lv_obj_t *obj);
static void emoji_timer_cb(lv_timer_t *timer);

// 定义继承的类结构
const lv_obj_class_t lisa_ui_llm_primary_class = {
    .base_class = &lisa_ui_llm_base_class,
    .instance_size = sizeof(lisa_ui_llm_primary_t),
    .constructor_cb = lisa_ui_llm_primary_class_constructor,
};

// emoji动画定时器回调函数
static void emoji_timer_cb(lv_timer_t *timer)
{
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)timer->user_data;
    
    if (!llm_primary || !llm_primary->emoji_img || !llm_primary->emoji_images) {
        return;
    }
    
    // 如果当前在第一帧延迟状态，结束延迟状态，但不切换帧
    if (llm_primary->is_first_frame_delayed) {
        llm_primary->is_first_frame_delayed = false;
        lv_timer_set_period(timer, llm_primary->frame_duration);
        return;
    }
    
    // 更新到下一帧
    llm_primary->current_emoji_frame = (llm_primary->current_emoji_frame + 1) % llm_primary->emoji_images_count;
    
    // 设置当前帧图片
    lv_img_set_src(llm_primary->emoji_img, llm_primary->emoji_images[llm_primary->current_emoji_frame]);
    
    // 检查是否完成了一个完整的循环
    if (llm_primary->current_emoji_frame == 0) {
        // 如果启用了循环模式且有目标循环次数限制
        if (llm_primary->loop_mode_enabled && llm_primary->target_loops > 0) {
            llm_primary->loop_count++;
            
            // 如果达到目标循环次数，停止动画
            if (llm_primary->loop_count >= llm_primary->target_loops) {
                lv_timer_del(timer);
                llm_primary->emoji_timer = NULL;
                return;
            }
        }
        
        // 如果设置了第一帧延迟，则进入延迟状态
        if (llm_primary->first_frame_delay > 0) {
            llm_primary->is_first_frame_delayed = true;
            lv_timer_set_period(timer, llm_primary->first_frame_delay);
        }
    }
}

// 构造函数实现
static void lisa_ui_llm_primary_class_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    (void)class_p;
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    // 获取基础任务栏容器
    lv_obj_t *bar = lisa_ui_llm_base_bar_get(obj);
    if (NULL == bar) {
        LOGE("Failed to get base bar");
        return;
    }

    lv_obj_t *container = lisa_ui_llm_base_container_get(obj);
    if (NULL == container) {
        LOGE("Failed to get base container");
        return;
    }
    
    // 设置任务栏布局
    lv_obj_set_style_pad_left(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_min_height(bar, 36, LV_PART_MAIN);
    lv_obj_set_style_base_dir(bar, LV_BASE_DIR_LTR, LV_PART_MAIN);

    // 创建WiFi图标（左边）
    llm_primary->wifi_icon = lv_img_create(bar);

    // 创建状态文本标签（中间）
    llm_primary->status_label = lv_label_create(bar);

    // 创建电量图标（右边）
    llm_primary->battery_icon = lv_img_create(bar);

    // 设置主容器为垂直布局
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(container, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 创建emoji动画容器（上半部分）
    llm_primary->emoji_container = lv_obj_create(container);

    // 创建emoji动画图片
    llm_primary->emoji_img = lv_img_create(llm_primary->emoji_container);
    
    // 创建内容文本容器（下半部分）
    llm_primary->content_container = lv_obj_create(container);

    // 创建内容文本标签
    llm_primary->content_label = lv_textarea_create(llm_primary->content_container);
}

// ===================== 公共API实现 =====================

lv_obj_t *lisa_ui_llm_primary_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&lisa_ui_llm_primary_class, parent);
    lv_obj_class_init_obj(obj);

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    // 设置WiFi图标
    lv_obj_align(llm_primary->wifi_icon, LV_ALIGN_LEFT_MID, 0, 0);

    // 设置状态文本标签
    lv_label_set_text(llm_primary->status_label, "Ready");
    lv_obj_set_style_text_letter_space(llm_primary->status_label, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(llm_primary->status_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->status_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(llm_primary->status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(llm_primary->status_label, 0, LV_PART_MAIN);
    lv_obj_align(llm_primary->status_label, LV_ALIGN_CENTER, 0, 0);

    // 设置电量图标
    lv_obj_align(llm_primary->battery_icon, LV_ALIGN_RIGHT_MID, 0, 0);

    // 设置emoji动画容器
    lv_obj_set_style_bg_opa(llm_primary->emoji_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_primary->emoji_container, 0, LV_PART_MAIN);
    lv_obj_set_size(llm_primary->emoji_container, LV_PCT(100), 104);
    lv_obj_set_style_pad_all(llm_primary->emoji_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(llm_primary->emoji_container, 20, LV_PART_MAIN);
    lv_obj_update_layout(llm_primary->emoji_container);

    // 设置emoji容器为居中对齐
    lv_obj_set_layout(llm_primary->emoji_container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(llm_primary->emoji_container, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(llm_primary->emoji_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(llm_primary->emoji_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 设置emoji动画图片
    lv_obj_set_size(llm_primary->emoji_img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    // 创建拍照图片（覆盖在emoji上层，默认隐藏）
    llm_primary->camera_img = lv_img_create(obj);
    lv_obj_add_flag(llm_primary->camera_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(llm_primary->camera_img, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(llm_primary->camera_img, 160, 120);
    lv_obj_center(llm_primary->camera_img);
    lv_obj_move_foreground(llm_primary->camera_img);
    
    // 设置内容文本容器
    lv_obj_set_style_bg_opa(llm_primary->content_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(llm_primary->content_container, 0, LV_PART_MAIN);
    lv_obj_set_size(llm_primary->content_container, LV_PCT(100), 88);
    lv_obj_set_style_pad_all(llm_primary->content_container, 10, LV_PART_MAIN);
    
    // 设置内容容器为居中对齐
    lv_obj_set_layout(llm_primary->content_container, LV_LAYOUT_FLEX);
    lv_obj_set_style_flex_flow(llm_primary->content_container, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(llm_primary->content_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(llm_primary->content_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 设置内容文本区域
    lv_textarea_set_text(llm_primary->content_label, "请唤醒我");
    lv_obj_set_style_text_color(llm_primary->content_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(llm_primary->content_label, &lv_font_chinese_18, LV_PART_MAIN);
    lv_obj_set_style_text_align(llm_primary->content_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    
    // 隐藏边框和背景
    lv_obj_set_style_border_width(llm_primary->content_label, 0, LV_PART_MAIN);           // 隐藏边框
    lv_obj_set_style_bg_opa(llm_primary->content_label, LV_OPA_TRANSP, LV_PART_MAIN);     // 背景透明
    lv_obj_set_style_outline_width(llm_primary->content_label, 0, LV_PART_MAIN);          // 隐藏轮廓
    
    // 设置滚动和交互
    lv_textarea_set_cursor_click_pos(llm_primary->content_label, false);  // 禁用光标点击
    lv_obj_add_flag(llm_primary->content_label, LV_OBJ_FLAG_SCROLLABLE);  // 启用滚动
    lv_obj_set_scrollbar_mode(llm_primary->content_label, LV_SCROLLBAR_MODE_AUTO);  // 自动显示滚动条
    lv_obj_clear_flag(llm_primary->content_label, LV_OBJ_FLAG_CLICKABLE);  // 禁用点击
    
    // 设置尺寸和位置 - 使用更大的高度以便滚动
    lv_obj_set_width(llm_primary->content_label, LV_PCT(100));
    lv_obj_set_height(llm_primary->content_label, LV_PCT(100));
    
    // 调整文本样式
    lv_obj_set_style_text_line_space(llm_primary->content_label, 6, LV_PART_MAIN);    // 行间距（行与行之间）
    lv_obj_set_style_text_letter_space(llm_primary->content_label, 1, LV_PART_MAIN);  // 字符间距（字符与字符之间）
    lv_obj_set_style_pad_top(llm_primary->content_label, 20, LV_PART_MAIN);     // 上边距
    lv_obj_set_style_pad_bottom(llm_primary->content_label, 20, LV_PART_MAIN);  // 下边距
    lv_obj_set_style_pad_left(llm_primary->content_label, 12, LV_PART_MAIN);   // 左边距  
    lv_obj_set_style_pad_right(llm_primary->content_label, 12, LV_PART_MAIN);  // 右边距
    
    // 初始化动画相关变量
    llm_primary->emoji_images = NULL;
    llm_primary->emoji_images_count = 0;
    llm_primary->current_emoji_frame = 0;
    llm_primary->first_frame_delay = 0;
    llm_primary->frame_duration = 100;  // 默认每帧100ms
    llm_primary->emoji_timer = NULL;
    llm_primary->is_first_frame_delayed = false;
    
    // 初始化循环控制变量
    llm_primary->loop_count = 0;
    llm_primary->target_loops = 0;  // 默认无限循环
    llm_primary->loop_mode_enabled = false;

    return obj;
}

lv_obj_t *lisa_ui_llm_primary_wifi_icon_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->wifi_icon;
}

lv_obj_t *lisa_ui_llm_primary_status_label_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->status_label;
}

lv_obj_t *lisa_ui_llm_primary_battery_icon_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->battery_icon;
}

lv_obj_t *lisa_ui_llm_primary_emoji_img_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->emoji_img;
}

lv_obj_t *lisa_ui_llm_primary_content_label_get(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return NULL;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    return llm_primary->content_label;
}

void lisa_ui_llm_primary_set_status_text(lv_obj_t *obj, const char *status)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    if (llm_primary->status_label) {
        lv_label_set_text(llm_primary->status_label, status);
    }
}

void lisa_ui_llm_primary_set_content_text(lv_obj_t *obj, const char *content)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    if (llm_primary->content_label) {
        lv_textarea_set_text(llm_primary->content_label, content);
    }
}

void lisa_ui_llm_primary_add_content_text(lv_obj_t *obj, const char *content)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    if (llm_primary->content_label) {
        lv_textarea_add_text(llm_primary->content_label, content);
    }
}

void lisa_ui_llm_primary_set_wifi_img(lv_obj_t *obj, const void *img_path)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    if (llm_primary->wifi_icon && img_path) {
        lv_img_set_src(llm_primary->wifi_icon, img_path);
    }
}

void lisa_ui_llm_primary_set_battery_img(lv_obj_t *obj, const void *img_path)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    if (!llm_primary->battery_icon || !img_path) {
        return;
    }

    // 显示图片（移除隐藏标志）
    lv_obj_clear_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);
    lv_img_set_src(llm_primary->battery_icon, img_path);
    LOGI("battery image show");
}

void lisa_ui_llm_primary_battery_icon_hide(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    if (!llm_primary->battery_icon) {
        return;
    }
    
    // 隐藏图片
    lv_obj_add_flag(llm_primary->battery_icon, LV_OBJ_FLAG_HIDDEN);
    LOGI("battery image hidden");
}

void lisa_ui_llm_primary_start_emoji_animation(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    if (llm_primary->emoji_images && llm_primary->emoji_images_count > 0 && !llm_primary->emoji_timer) {

        // 重置到第一帧
        llm_primary->current_emoji_frame = 0;
        llm_primary->is_first_frame_delayed = false;
        llm_primary->loop_count = 0;  // 重置循环计数
        
        // 设置第一帧图片
        if (llm_primary->emoji_img) {
            lv_img_set_src(llm_primary->emoji_img, llm_primary->emoji_images[0]);
        }
        
        // 创建定时器，根据是否有第一帧延迟决定初始周期
        uint32_t initial_period = llm_primary->first_frame_delay > 0 ? llm_primary->first_frame_delay : llm_primary->frame_duration;
        llm_primary->emoji_timer = lv_timer_create(emoji_timer_cb, initial_period, llm_primary);
        
        if (llm_primary->first_frame_delay > 0) {
            llm_primary->is_first_frame_delayed = true;
        }
    }
}

void lisa_ui_llm_primary_stop_emoji_animation(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    // 清理定时器
    if (llm_primary->emoji_timer) {
        lv_timer_del(llm_primary->emoji_timer);
        llm_primary->emoji_timer = NULL;
    }
    
    llm_primary->is_first_frame_delayed = false;
}

void lisa_ui_llm_primary_set_custom_emoji_animation(lv_obj_t *obj, const void **images, uint32_t images_count, uint32_t duration, uint32_t first_frame_delay)
{
    if (!lisa_ui_llm_primary_is_valid(obj) || !images || images_count == 0) {
        return;
    }

    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;

    // 停止当前动画
    lisa_ui_llm_primary_stop_emoji_animation(obj);
    
    // 保存新的动画配置
    llm_primary->emoji_images = images;
    llm_primary->emoji_images_count = images_count;
    llm_primary->current_emoji_frame = 0;
    llm_primary->first_frame_delay = first_frame_delay;
    
    // 计算每帧时长：总时长除以帧数
    uint32_t total_duration = duration > 0 ? duration : 1000;  // 默认1秒
    llm_primary->frame_duration = total_duration / images_count;
    if (llm_primary->frame_duration < 50) {  // 最小50ms，避免太快
        llm_primary->frame_duration = 50;
    }
    
    // 设置第一帧图片
    if (llm_primary->emoji_img) {
        lv_img_set_src(llm_primary->emoji_img, images[0]);
    }
    
    // 自动启动动画
    lisa_ui_llm_primary_start_emoji_animation(obj);
}

void lisa_ui_llm_primary_set_loop_count(lv_obj_t *obj, uint32_t loop_count)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    
    llm_primary->target_loops = loop_count;
    llm_primary->loop_mode_enabled = (loop_count > 0);
    llm_primary->loop_count = 0;  // 重置当前循环计数
}

void lisa_ui_llm_primary_show_camera_image(lv_obj_t *obj, const uint16_t *rgb565_data, uint32_t width, uint32_t height)
{
    if (!lisa_ui_llm_primary_is_valid(obj) || !rgb565_data) {
        LOGE("Invalid parameters for show_camera_image");
        return;
    }
    
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->camera_img) {
        LOGE("camera_img is NULL");
        return;
    }
    

    // 创建LVGL图片描述符（static确保地址稳定，LVGL可以持续访问）
    static lv_img_dsc_t img_dsc;
    img_dsc.header.always_zero = 0;
    img_dsc.header.w = width;
    img_dsc.header.h = height;
    img_dsc.data_size = width * height * 2;  // RGB565 = 2 bytes per pixel
    img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565格式
    
    // 直接指向全局PSRAM缓冲区，不复制数据
    img_dsc.data = (const uint8_t *)rgb565_data;
    
    LOGI("Setting camera image source: buffer=%p, size=%dx%d (%u bytes)", 
         rgb565_data, width, height, img_dsc.data_size);
    
    // 设置图片源（LVGL会直接从rgb565_data地址读取数据）
    lv_img_set_src(llm_primary->camera_img, &img_dsc);
    // lv_img_set_zoom(llm_primary->camera_img, 100);//比例*256

    // 设置图片位置：水平居中，垂直居中并向上偏移 15 像素
    lv_obj_align(llm_primary->camera_img, LV_ALIGN_CENTER, 0, -15);
    lv_obj_move_foreground(llm_primary->camera_img);
    
    // 显示图片（移除隐藏标志）
    lv_obj_clear_flag(llm_primary->camera_img, LV_OBJ_FLAG_HIDDEN);
    
    // 强制刷新显示
    lv_obj_invalidate(llm_primary->camera_img);
    
    LOGI("Camera image displayed: %dx%d from global buffer, obj=%p", 
         width, height, llm_primary->camera_img);

}

void lisa_ui_llm_primary_hide_camera_image(lv_obj_t *obj)
{
    if (!lisa_ui_llm_primary_is_valid(obj)) {
        return;
    }
    
    lisa_ui_llm_primary_t *llm_primary = (lisa_ui_llm_primary_t *)obj;
    if (!llm_primary->camera_img) {
        return;
    }
    
    // 隐藏图片
    lv_obj_add_flag(llm_primary->camera_img, LV_OBJ_FLAG_HIDDEN);
    
    LOGI("Camera image hidden");
}
