#include <stdint.h>
#include "lvgl.h"
#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"

#include "assets/assets_res.h"
#include "../group_ocr.h"
#include "../widgets/lv_scanner.h"
#include "ocr_pages.h"
#include "user_groups.h"

#include "lisaui_log.h"

#define TAG "ocr.page.scanner"

typedef struct{
    lv_obj_t *display;
    lv_obj_t *scanner;    /**< 组网格视图组件 */
}page_view_t;


static lisaui_page_t *create(lisaui_page_t *page){

    page_view_t *view = NULL;

    view = lisaui_malloc(sizeof(page_view_t));
    if(view == NULL){
        LISAUI_LOGE(TAG,"[%s %d]no memory!",__FUNCTION__,__LINE__);
        goto _ERR;
    }
    
    view->display = lv_obj_create(NULL); // launcher 页面根容器
    lisaui_common_set_style_container(view->display, lv_color_hex(0x000000), 255, lv_color_hex(0x000000), 0, 0);
    lv_obj_set_size(view->display, LV_PCT(100), LV_PCT(100));

    view->scanner = lv_scanner_create(view->display);
    page->view = view;

    LISAUI_LOGI(TAG,"Create page:%p,view->scanner:%p",page,view->scanner);
    return page;

_ERR:

    if(NULL != view){
        lisaui_free(view);
    }

    return NULL;
}

static lisaui_err_t destroy(lisaui_page_t *page){
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for destroy");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 释放视图资源 */
    if (page->view) {
        page_view_t *view = (page_view_t *)page->view;
        
        /* 销毁所有UI对象 */
        if (view->display) {
            lv_obj_del(view->display);
            view->display = NULL;
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

static lisaui_err_t close(lisaui_page_t *page){
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }
    

    LISAUI_LOGI(TAG, "Page main closed");
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page,void *data){
    
    launcher_page_update_parameter_t *parameter = (launcher_page_update_parameter_t*)data;
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for update");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }
    
    /* 在这里实现页面内容的更新逻辑 */
    /* 比如更新图标、刷新数据等 */


    LISAUI_LOGI(TAG, "Page main updated");
    return LISAUI_ERR_OK;
}

static const lisaui_page_t ocr_scanner_page = {
    .group_index = LISAUI_GROUP_INDEX_OCR,
    .page_index = LISAUI_GROUP_OCR_PAGE_INDEX_MAIN,
    .cname = "ocr.scanner",
    .view = NULL,
    .page_data = NULL,
    .attribute = 0,
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

LISAUI_PAGE_EXPORT(ocr_scanner_page);
