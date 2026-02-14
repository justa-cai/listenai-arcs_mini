#define TAG "lisa_ui_assets"

#include <string.h>
#include <stdio.h>

#include "lv_img_utils.h"
#include "lisa_log.h"
#include "lisa_ui_assets.h"
#include "romfs/romfs.h"
#include "config/config_parser.h"

typedef struct {
    lv_img_dsc_t *img_dsc;
    uint32_t count;
    const char *path_pattern;
} lisa_ui_asset_item_t;

#define LISA_UI_ASSET_ITEM(_name, _path_pattern)                                                                       \
    {                                                                                                                  \
        .img_dsc = LISA_UI_ASSETS_IMG_DSC(_name),                                                                      \
        .count = LISA_UI_ASSETS_IMG_DSC_LIST_SIZE(_name),                                                              \
        .path_pattern = _path_pattern,                                                                                 \
    }

#define LISA_UI_ASSETS_PATH(_path) "/" _path

#define LISA_UI_ASSETS_ICON_PATH(path) LISA_UI_ASSETS_PATH("icon/" path)
#define LISA_UI_ASSETS_FONT_PATH(path) LISA_UI_ASSETS_PATH("fonts/" path)
#define LISA_UI_ASSETS_GIF_PATH(path)  LISA_UI_ASSETS_PATH("gif/" path)
#define LISA_UI_ASSETS_JPG_PATH(path)  LISA_UI_ASSETS_PATH("jpg/" path)
#define LISA_UI_ASSETS_PNG_PATH(path)  LISA_UI_ASSETS_PATH("png/" path)

LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_wifi);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_charging);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_power);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_angry);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_blink);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_eye);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_hug);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_love);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_puzzled);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_sad);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_sleepy);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_wait);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_wakeup);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_battery);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_happy);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_cute);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_interactive);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_alarm);
LISA_UI_ASSETS_IMG_DSC_LIST_DEFINE(img_png_music);

static lisa_ui_asset_item_t lisa_ui_asset_items[] = {
    LISA_UI_ASSET_ITEM(img_png_wifi, LISA_UI_ASSETS_PNG_PATH("wifi/ic_status_wifi%d.png")),
    LISA_UI_ASSET_ITEM(img_png_charging, LISA_UI_ASSETS_PNG_PATH("battery/ic_status2_charging%d.png")),
    LISA_UI_ASSET_ITEM(img_png_power, LISA_UI_ASSETS_PNG_PATH("battery/ic_status2_power%d.png")),
    LISA_UI_ASSET_ITEM(img_png_angry, LISA_UI_ASSETS_PNG_PATH("emoji/angry/angry_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_blink, LISA_UI_ASSETS_PNG_PATH("emoji/blink/blink_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_eye, LISA_UI_ASSETS_PNG_PATH("emoji/eye/eye_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_hug, LISA_UI_ASSETS_PNG_PATH("emoji/hug/hug_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_love, LISA_UI_ASSETS_PNG_PATH("emoji/love/love_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_puzzled, LISA_UI_ASSETS_PNG_PATH("emoji/puzzled/puzzled_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_sad, LISA_UI_ASSETS_PNG_PATH("emoji/sad/sad_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_sleepy, LISA_UI_ASSETS_PNG_PATH("emoji/sleepy/sleepy_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_wait, LISA_UI_ASSETS_PNG_PATH("emoji/wait/wait_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_wakeup, LISA_UI_ASSETS_PNG_PATH("emoji/wakeup/wakeup_%03d.png")),
    LISA_UI_ASSET_ITEM(img_png_battery, LISA_UI_ASSETS_PNG_PATH("emoji/battery/frame_%06d.png")),
    LISA_UI_ASSET_ITEM(img_png_happy, LISA_UI_ASSETS_PNG_PATH("emoji/happy/frame-%06d.png")),
    LISA_UI_ASSET_ITEM(img_png_cute, LISA_UI_ASSETS_PNG_PATH("emoji/cute/frame-%06d.png")),
    LISA_UI_ASSET_ITEM(img_png_interactive, LISA_UI_ASSETS_PNG_PATH("interactive/interactive_mode_flag%d.png")),
    LISA_UI_ASSET_ITEM(img_png_alarm, LISA_UI_ASSETS_PNG_PATH("alarm/ic_status_alarm%d.png")),
    LISA_UI_ASSET_ITEM(img_png_music, LISA_UI_ASSETS_PNG_PATH("music/ic_status_music%d.png")),
};

void lisa_ui_assets_init(void)
{
    const Config *config = config_get();
    if (!config) {
        LISA_LOGE(TAG, "Failed to get config");
        return;
    }

    const ResourceConfig *respak = config_get_resource_by_name("respak");
    if (!respak || respak->address == 0 || respak->size == 0) {
        LISA_LOGE(TAG, "Failed to get respak resource");
        return;
    }

    struct romfs *respak_fs = NULL;
    if (romfs_init(&respak_fs, (const void *)respak->address, respak->size) != 0) {
        LISA_LOGE(TAG, "Failed to init respak romfs");
        return;
    }

    char path[ROMFS_PATH_MAX];
    uint8_t *data = NULL;
    uint32_t size = 0;
    for (size_t i = 0; i < LISA_UI_ARRAY_SIZE(lisa_ui_asset_items); i++) {
        const lisa_ui_asset_item_t *item = &lisa_ui_asset_items[i];
        for (size_t j = 0; j < item->count; j++) {
            snprintf(path, sizeof(path), item->path_pattern, j);
            if (romfs_info_get(respak_fs, path, &data, &size) != 0) {
                LISA_LOGE(TAG, "Failed to get asset: %s", path);
                goto deinit;
            }
            lv_img_png_src_init(&item->img_dsc[j], (const void *)data, size);
        }
        LISA_LOGI(TAG, "Loaded asset: %s (count=%d)", item->path_pattern, item->count);
    }

deinit:
    romfs_deinit(&respak_fs);
}
