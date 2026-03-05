#include <stdint.h>
#include <stddef.h>

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"

int lisa_ui_init(void)
{
    extern int lisa_ui_lvgl_init(void);
    extern int lisa_ui_app_init(void);
    extern int lisa_ui_lvgl_run(void);

    lv_i18n_init(lv_i18n_language_pack);

    lisa_ui_invoke_init();

    lisa_ui_lvgl_init();

    lisa_ui_app_init();

    lisa_ui_lvgl_run();

    return 0;
}
