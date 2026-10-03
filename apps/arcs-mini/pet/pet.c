/*
 * pet.c - virtual pet module facade
 */
#include "pet.h"

#include "lisa_log.h"

#include "pet_buzzer.h"
#include "pet_core.h"
#include "pet_ui.h"

#define TAG "pet"

int pet_init(void)
{
    if (pet_core_init() != 0) {
        LISA_LOGE(TAG, "init: core failed");
        return -1;
    }
    if (pet_ui_init() != 0) {
        LISA_LOGE(TAG, "init: ui register failed");
        return -1;
    }
    if (pet_ui_become_default() != 0) {
        LISA_LOGW(TAG, "init: could not become default screen (home stays default)");
    }
    if (pet_buzzer_init() != 0) {
        LISA_LOGW(TAG, "init: buzzer disabled");
    }
    LISA_LOGI(TAG, "init: pet is up");
    return 0;
}
