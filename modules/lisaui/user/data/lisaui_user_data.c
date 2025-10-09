#include <string.h>
#include "platform.h"
#include "lisaui_user_data.h"
#include "lisa_ui_assets.h"
#include "config_parser.h"

static lisaui_userdata_t _userdata;

#define _user_data_lock_init() lisaui_mutex_create(&_mutex)
#define _user_data_lock()      lisaui_mutex_lock(&_mutex, LISAUI_ENV_MAX_DELAY)
#define _user_data_unlock()    lisaui_mutex_unlock(&_mutex)

extern const lv_img_dsc_t boli;
extern const lv_img_dsc_t neza;
extern const lv_img_dsc_t niudun;
extern const lv_img_dsc_t peiji;
extern const lv_img_dsc_t teeni;
extern const lv_img_dsc_t wukong;
extern const lv_img_dsc_t manman;

static role_info_t role_imgs[] = {
    {
        .name = "Teeni",
        .model = "",
        .img_main = (void *)&teeni,
        // .img_commu = &LISA_UI_ASSETS_IMG_DSC(img_png_teeni_commu),
        .is_emoji = false,
        .emoji = ROLE_EMOJI_BLINK,
    },
    {
        .name = "小仙",
        .model = "",
        // .img_main = (void *)&xiaoxian,
        // .img_commu = &LISA_UI_ASSETS_IMG_DSC(img_png_teeni_commu),
        .is_emoji = true,
        .emoji = ROLE_EMOJI_BLINK,
    },
    {
        .name = "小鹦鹉波力",
        .model = "",
        .img_main = (void *)&boli,
        // .img_commu = &LISA_UI_ASSETS_IMG_DSC(img_png_boli_commu),
        .is_emoji = false,
        .emoji = ROLE_EMOJI_BLINK,
    },
    {
        .name = "科学家牛顿",
        .model = "",
        .img_main = (void *)&niudun,
        // .img_commu = &LISA_UI_ASSETS_IMG_DSC(img_png_niudun_commu),
        .is_emoji = false,
        .emoji = ROLE_EMOJI_BLINK,
    },
    {
        .name = "哪吒",
        .model = "",
        .img_main = (void *)&neza,
        // .img_commu = &LISA_UI_ASSETS_IMG_DSC(img_png_neza_commu),
        .is_emoji = false,
        .emoji = ROLE_EMOJI_BLINK,
    },
    {
        .name = "孙悟空",
        .model = "",
        .img_main = (void *)&wukong,
        // .img_commu = &LISA_UI_ASSETS_IMG_DSC(img_png_wukong_commu),
        .is_emoji = false,
        .emoji = ROLE_EMOJI_BLINK,
    },
    {
        .name = "绘本企鹅佩吉",
        .model = "",
        .img_main = (void *)&peiji,
        // .img_commu = &LISA_UI_ASSETS_IMG_DSC(img_png_peiji_commu),
        .is_emoji = false,
        .emoji = ROLE_EMOJI_BLINK,
    },
};

static lisaui_mutex_handle_t _mutex;

int lisaui_data_init(void)
{
    _user_data_lock_init();
    memset(&_userdata, 0, sizeof(_userdata));
    role_info_t *roles = (role_info_t *)malloc(sizeof(role_info_t) * 1);
    if (roles) {
        const Config *config = config_get();
        roles[0].name = config->role.name;
        roles[0].model = "";
        roles[0].is_emoji = true;
        roles[0].emoji = ROLE_EMOJI_BLINK;
        roles[0].prompt = config->role.prompt;

        _userdata.roles.roles = roles;
        _userdata.roles.roles_count = 1;
        _userdata.roles.role_idx = 0;
    } else {
        _userdata.roles.roles = role_imgs;
        _userdata.roles.roles_count = sizeof(role_imgs) / sizeof(role_imgs[0]);
        _userdata.roles.role_idx = 0;
    }
    _userdata.camera_video =
        lisaui_videoqueue_create(LISAUI_USERDATA_DVP_CAMERA_VIDEO_SIZE, LISAUI_USERDATA_DVP_CAMERA_VIDEO_BUFFER_COUNT,
                                 LISAUI_USERDATA_DVP_CAMERA_VIDEO_ATTRIBUTES);
    return 0;
}

lisaui_userdata_t *lisaui_userdata_acquire(void)
{
    lisaui_userdata_t *ptr;
    _user_data_lock();
    ptr = &_userdata;

    return ptr;
}

void lisaui_userdata_release(void)
{
    _user_data_unlock();
}

lisaui_videoqueue_t *lisaui_userdata_get_camera_videoqueue(void)
{
    return _userdata.camera_video;
}
