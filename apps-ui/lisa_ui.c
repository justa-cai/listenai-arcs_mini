#include <stdint.h>
#include <stddef.h>

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"

int lisa_ui_init(void)
{
    extern int lisa_ui_lvgl_init(void);
    extern int lisa_ui_app_init(void);
    extern int lisa_ui_lvgl_run(void);
#if CONFIG_LLM_TERMINAL_UI
    extern int llmterm_ui_init(void);
#endif

    lv_i18n_init(lv_i18n_language_pack);

    lisa_ui_invoke_init();

    lisa_ui_lvgl_init();

#if CONFIG_LLM_TERMINAL_UI
    /* LLM 终端桌面：跳过常规 app UI（models/页面注册/默认 home 页） */
    llmterm_ui_init();
#else
    lisa_ui_app_init();
#endif

    lisa_ui_lvgl_run();

    return 0;
}
