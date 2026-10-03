#include "lisa_ui.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"

/*
 * sim 精简版产品 UI 初始化:
 * PC 模拟器不编译 llm/ 产品代码, 这里只注册并直接进入 game 屏。
 * 设备版在 apps/llm/lisa_ui_app.c。
 */
extern const struct lisa_ui_nav_scr nes_game_nav_scr;

int lisa_ui_app_init(void)
{
    lisa_ui_nav_scr_add(&nes_game_nav_scr);
    lisa_ui_nav_scr_default_set(&nes_game_nav_scr);
    return 0;
}
