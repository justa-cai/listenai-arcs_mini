#ifndef __LISA_UI_SCR_H__
#define __LISA_UI_SCR_H__

struct lisa_ui_nav_scr {
    int unique_id;
    int (*open)(const struct lisa_ui_nav_scr *scr, void **data);
    int (*show)(const struct lisa_ui_nav_scr *scr, void *data);
    int (*pause)(const struct lisa_ui_nav_scr *scr, void *data);
    int (*resume)(const struct lisa_ui_nav_scr *src, void *data);
    int (*close)(const struct lisa_ui_nav_scr *scr, void *data);
};

int lisa_ui_nav_scr_add(const struct lisa_ui_nav_scr *scr);
int lisa_ui_nav_scr_default_set(const struct lisa_ui_nav_scr *scr);
int lisa_ui_nav_scr_default_set_by_id(int id);
int lisa_ui_nav_scr_nav_to(int unique_id);
int lisa_ui_nav_scr_nav_back();
int lisa_ui_nav_scr_nav_default(void);
int lisa_ui_nav_scr_get_top_id(void);

#endif
