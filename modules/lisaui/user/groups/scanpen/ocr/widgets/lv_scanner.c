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
 #include "lv_scanner.h"
 
 #define TAG "lv_scanner"
 
 /*********************
  * 类型定义
  *********************/
 
 
 /**
  * 网格组件数据结构
  */
 typedef struct {
     lv_obj_t obj;          /**< 基类对象 */
     void *user_data;        /**< 用户数据 */
 
 } lv_scanner_t;
 
 /*********************
  * 函数声明
  *********************/
 static void scanner_event_handler(const struct _lv_obj_class_t * class_p,struct _lv_event_t * e);
 static void lv_scanner_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
 static void lv_scanner_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
 
 /*********************
  * 组件类定义
  *********************/
 const lv_obj_class_t lv_scanner_class = {
     .base_class = &lv_obj_class,
     .constructor_cb = lv_scanner_constructor,
     .destructor_cb = lv_scanner_destructor,
     .event_cb = scanner_event_handler,
     .instance_size = sizeof(lv_scanner_t)
 };
 
 /**
  * Handle grid icon events
  * @param e Event descriptor
  */
 static void scanner_event_handler(const struct _lv_obj_class_t * class_p,struct _lv_event_t * e)
 {
     /*TODO*/
 }
 
 /**
  * Constructor for the scanner widget
  * @param class_p Class pointer
  * @param obj Object to construct
  */
 static void lv_scanner_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
 {
     LV_UNUSED(class_p);
     lv_scanner_t *scanner = (lv_scanner_t *)obj;
     
     // 初始化内部数据
     scanner->user_data = NULL;
    LISAUI_LOGI(TAG,"Create scanner:%p",scanner);
     
     // 配置基类对象
     lv_obj_set_size(obj, LV_PCT(100), LV_PCT(70));
     lv_obj_set_y(obj, LV_DPX(LISAUI_STATUS_BAR_HEIGHT + 10));
     lisaui_common_set_style_container(obj, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
     
     // 设置布局为Flex布局
     lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
     lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
     lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
     
     // 创建并配置文本框
     lv_obj_t *text_label = lv_label_create(obj);
     lv_obj_set_style_text_color(text_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    //  lv_obj_set_style_text_font(text_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
     lv_label_set_text(text_label, "SCAN PAGE");
     lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
     lv_obj_center(text_label);
 }
 
 /**
  * Destructor for the scanner widget
  * @param class_p Class pointer
  * @param obj Object to destruct
  */
 static void lv_scanner_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
 {
     LV_UNUSED(class_p);
     lv_scanner_t *scanner = (lv_scanner_t *)obj;
     
 }
 

 /**
  * Create a scanner widget
  * @param parent Parent object
  * @return Pointer to the created scanner
  */
 lv_obj_t *lv_scanner_create(lv_obj_t *parent)
 {
     lv_obj_t *obj = lv_obj_class_create_obj(&lv_scanner_class, parent);
     lv_obj_class_init_obj(obj);
     return obj;
 }
 
 
 /**
  * Set the background color of the scanner
  * @param obj Scanner object
  * @param color Background color
  */
 void lv_scanner_set_bg_color(lv_obj_t *obj, lv_color_t color)
 {
     if (obj == NULL) {
         LISAUI_LOGE(TAG, "Invalid scanner object");
         return;
     }
     
     lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
 }
 