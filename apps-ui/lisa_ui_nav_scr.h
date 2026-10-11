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

/* 粘性前台：scr_id 页面在栈顶时，nav_to(主页) 被拒绝（返回 -1）。
 * 传 -1 清除。独占式前台（如小应用）用它挡住"语音结束/单击回主页"
 * 等路径；页面自身退出不走 nav_to，不受影响。 */
void lisa_ui_nav_set_sticky_home_block(int scr_id);

#endif
