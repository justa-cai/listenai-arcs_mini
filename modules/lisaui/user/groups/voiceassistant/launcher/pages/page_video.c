#include <stdint.h>

#include "lvgl.h"
#include "ebus/ebus.h"

#include "platform.h"
#include "lisaui_common.h"
#include "lisaui_stack_page.h"
#include "lisaui_group.h"
#include "lisaui_manager.h"

#include "assets/assets_res.h"
#include "launcher_pages.h"
#include "lisa_ui_src_base.h"
#include "lisa_ui_scr_camera.h"
#include "lisa_ui_assets.h"
#include "user_groups.h"
#include "lisaui_log.h"
#include "lisaui_user_data.h"

#include "../group_launcher.h"

#define TAG "launcher.page.video"
#define VIDEO_FLUSH_INTERVAL    (33)
#define VIDEO_USE_MOCK_DATA_ENABLE (0)
typedef struct {
    lv_obj_t *inter;

} page_view_t;

typedef struct {
    lisaui_videoqueue_t *videoqueue;
    uint32_t frame_index;
    uint32_t frame_count;
    lisaui_video_frame_desc_t frame_desc[2];
    ebus_chn_t *ebus_ch_base_event;

    lv_img_dsc_t img_dsc;
    lv_timer_t *video_timer;
} page_data_t;

static void btn_shut_click_event_cb(void *user_data)
{
    lisaui_page_t *page = (lisaui_page_t *)user_data;
    page_data_t *page_data = NULL;
    lisaui_video_frame_desc_t frame_desc;

    if(page != NULL){
        page_data = (page_data_t *)page->page_data;
    }
    if((page == NULL) || (page_data == NULL)){
        LISAUI_LOGE(TAG, "btn_shut_click_event_cb,page or page_data is NULL\n");
        return;
    }
    
    LISAUI_LOGI(TAG, "btn_shut_click_event_cb,page:%s\n", page->cname);
    if (0 != lisaui_videoqueue_pop(page_data->videoqueue, &frame_desc, 1000)) {

        LISAUI_LOGE(TAG, "Take photo failed because videoqueue_pop failed");
        return;
    }

    LISAUI_USERDATA_WITH_LOCK(_userdata){
    
        if(_userdata->photo != NULL){
            lisaui_free(_userdata->photo);
        }
        _userdata->photo = lisaui_malloc(sizeof(lisaui_userdata_photo_t) + frame_desc.size);
        _userdata->photo->width = frame_desc.width;
        _userdata->photo->height = frame_desc.height;
        _userdata->photo->size = frame_desc.size;
        memcpy(_userdata->photo->data, frame_desc.data, frame_desc.size);
    }

    lisaui_videoqueue_release(page_data->videoqueue, &frame_desc);
    page->flags &= ~LISAUI_PAGE_FLAG_NAV_LOCKED;
    LISAUI_LOGI(TAG, "btn_shut_click_event_cb,page:%s, channel:%p\n", page->cname, page_data->ebus_ch_base_event);
    ebus_message_pub(page_data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_TAKE_PHOTO, NULL, 0);
    lisaui_manager_nav_back();
}

static void btn_back_click_event_cb(void *user_data)
{
    lisaui_page_t *page;
    page_data_t *page_data = NULL;

    page = (lisaui_page_t *)user_data;
    page_data = (page_data_t *)page->page_data;
    LISAUI_LOGI(TAG, "btn_back_click_event_cb, user_data:%p\n", user_data);
    page->flags &= ~LISAUI_PAGE_FLAG_NAV_LOCKED;

    ebus_message_pub(page_data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_VIDEO_EXIT, NULL, 0);

    lisaui_manager_nav_back();
}

static void video_frame_timer_cb(lv_timer_t *timer)
{
    lisaui_page_t *page = (lisaui_page_t *)timer->user_data;
    page_view_t *view = (page_view_t *)page->view;

#if (VIDEO_USE_MOCK_DATA_ENABLE)
    uint8_t *mock_red_image = NULL;
    if(mock_red_image == NULL){
        mock_red_image = lisaui_malloc(320*240*2);
        for(int i=0;i<320*240*2;i++){
            if(i%2==0){
                mock_red_image[i]=0x00;
            }else{
                mock_red_image[i]=0xF8;
            }
        }
    }
#endif

    if ((page != NULL) && (page->page_data != NULL)) 
    {
    
        page_data_t *page_data = (page_data_t *)page->page_data;
    
        if (0 == lisaui_videoqueue_pop(page_data->videoqueue, &page_data->frame_desc[page_data->frame_index], 0)) 
        {

            page_data->img_dsc.header.always_zero = 0;
            
#if VIDEO_USE_MOCK_DATA_ENABLE
            page_data->img_dsc.data = mock_red_image;
#else
            page_data->img_dsc.data = page_data->frame_desc[page_data->frame_index].data;
#endif
            page_data->img_dsc.data_size = page_data->frame_desc[page_data->frame_index].size;
            page_data->img_dsc.header.w = page_data->frame_desc[page_data->frame_index].width;
            page_data->img_dsc.header.h = page_data->frame_desc[page_data->frame_index].height;
            page_data->img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
            lisa_ui_scr_camera_img_set(view->inter, &page_data->img_dsc);
            lisa_ui_scr_camera_img_rotate(view->inter, 900);
            page_data->frame_index = !page_data->frame_index;

            lisaui_videoqueue_release(page_data->videoqueue, &page_data->frame_desc[page_data->frame_index]);
            page_data->frame_desc[page_data->frame_index].data = NULL;
        }
    }
}

static lisaui_page_t *create(lisaui_page_t *page)
{
    page_view_t *view = NULL;
    page_data_t *page_data = NULL;

    view = lisaui_malloc(sizeof(page_view_t));
    if (view == NULL) {
        LISAUI_LOGE(TAG, "Create %s page view failed,no memory!", page->cname);
        goto _ERR;
    }
    page_data = lisaui_malloc(sizeof(page_data_t));

    if (page_data == NULL) {

        LISAUI_LOGE(TAG, "Create %s page page data failed,no memory!",page->cname);
        goto _ERR;
    }

    memset(page_data, 0, sizeof(page_data_t));
    page_data->ebus_ch_base_event = ebus_chn_bind(LISAUI_EBUS_NAME, LISAUI_EBUS_CH_BASE_EVENT_NAME);
    page_data->videoqueue = lisaui_userdata_get_camera_videoqueue();
    page->page_data = page_data;

    view->inter = lisa_ui_scr_camera_create(NULL);
    lisa_ui_scr_camera_rec_img_set(view->inter, &LISA_UI_ASSETS_IMG_DSC(img_png_rec));
    lisa_ui_scr_camera_btn_shut_click_event_cb_set(view->inter, btn_shut_click_event_cb, page);
    lisa_ui_scr_camera_btn_back_click_event_cb_set(view->inter, btn_back_click_event_cb, page);
    // lv_obj_add_event_cb(view->inter, display_screen_event_cb, LV_EVENT_REFRESH_DONE, page);
    page_data->video_timer = lv_timer_create(video_frame_timer_cb, VIDEO_FLUSH_INTERVAL, page); // 约30fps
    lv_timer_pause(page_data->video_timer);
    page->view = view;

    LISAUI_LOGI(TAG, "Create %s page success", page->cname);
    return page;

_ERR:
    if (NULL != view) {

        lisaui_free(view);
    }

    if(NULL != page_data){
        lisaui_free(page_data);
    }
    return NULL;
}

static lisaui_err_t destroy(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for destroy");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (page->view) {

        page_view_t *view = (page_view_t *)page->view;
        if (view->inter) {

            lisa_ui_scr_base_del(view->inter);
            view->inter = NULL;
        }
        lisaui_free(view);
        page->view = NULL;
    }

    if (page->page_data) {

        page_data_t *page_data = (page_data_t *)page->page_data;
        if (page_data->video_timer) {

            lv_timer_del(page_data->video_timer);
            page_data->video_timer = NULL;
        }
        lisaui_free(page->page_data);
        page->page_data = NULL;
    }

    LISAUI_LOGI(TAG, "Destroy %s page",page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t show(lisaui_page_t *page)
{

    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for show");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    if (view->inter) {
        lisa_ui_scr_base_show(view->inter);
    }
    lisa_ui_scr_base_bar_hide(view->inter);

    page_data_t *page_data = (page_data_t *)page->page_data;
    ebus_message_pub    (page_data->ebus_ch_base_event, 
                        LISAUI_EBUS_CH_EVENT_U2M_SETTING_CTR_VIDEO_START, 
                        NULL, 
                        0);
    lv_timer_resume(page_data->video_timer);
    page->flags |= LISAUI_PAGE_FLAG_NAV_LOCKED;
    LISAUI_LOGI(TAG, "Show %s page",page->cname);

    return LISAUI_ERR_OK;
}

static lisaui_err_t close(lisaui_page_t *page)
{
    if (!page) {
        LISAUI_LOGE(TAG, "Invalid page pointer for close");
        return LISAUI_ERR_INVALID_PARAM;
    }
    page_view_t *view = (page_view_t *)page->view;
    if (!view) {
        LISAUI_LOGE(TAG, "Page view not created");
        return LISAUI_ERR_INVALID_PARAM;
    }

    lisa_ui_scr_base_hide(view->inter);
    page_data_t *page_data = (page_data_t *)page->page_data;

    lv_timer_pause(page_data->video_timer);
    for(int i=0;i<sizeof(page_data->frame_desc)/sizeof(page_data->frame_desc[0]);i++){
        lisaui_videoqueue_release(page_data->videoqueue, &page_data->frame_desc[i]);
        page_data->frame_desc[i].data = NULL;
    }
    ebus_message_pub(page_data->ebus_ch_base_event, LISAUI_EBUS_CH_EVENT_U2M_SETTING_CTR_VIDEO_STOP, NULL, 0);
    

    LISAUI_LOGI(TAG, "Close %s page",page->cname);
    return LISAUI_ERR_OK;
}

static lisaui_err_t update_data(lisaui_page_t *page, void *data)
{
    launcher_page_update_parameter_t *parameter = (launcher_page_update_parameter_t *)data;
    if (!page || !data) {
        LISAUI_LOGE(TAG, "Invalid parameters for update");
        return LISAUI_ERR_INVALID_PARAM;
    }

    page_view_t *view = (page_view_t *)page->view;
    if (!view || !view->inter) {
        LISAUI_LOGE(TAG, "Page view not created or inter is NULL");
        return LISAUI_ERR_INVALID_PARAM;
    }

    LISAUI_LOGI(TAG, "launcher video page updated");
    return LISAUI_ERR_OK;
}

static const lisaui_page_t luancher_video_page = {
    .group_index = LISAUI_GROUP_INDEX_LAUNCHER,
    .page_index = LISAUI_GROUP_LAUNCHER_PAGE_INDEX_VIDEO,
    .cname = "launcher.video",
    .view = NULL,
    .page_data = NULL,
    .attribute =
        {
            .type = LISAUI_PAGE_TYPE_DATA_PAGE,
        },
    .create = create,
    .destroy = destroy,
    .show = show,
    .close = close,
    .update_data = update_data,
};

LISAUI_PAGE_EXPORT(luancher_video_page);
