#include "touch.h"
#include "ft5336.h"


/********************************************************************************
function:    TOUCH DEV function
note:

********************************************************************************/


int touch_control(touch_ctrl_t cmd)
{
    switch(TOUCH_DEV_TYPE)
    {
        case TOUCH_FT5336:
            FT5336_SetCtrlMode(cmd);
            break;

        default:
            VIDEO_LOG("[%s:%d] touch type error", __func__, __LINE__);
            return -1;
    }

    return 0;
}


uint8_t touch_read(touch_info_t *p_info)
{
    uint8_t touch_cnt = 0;

    if (NULL == p_info) {
        ("[%s:%d] Error input: pp_info is NULL", __func__, __LINE__);
        return 0;
    }

    switch(TOUCH_DEV_TYPE)
    {
        case TOUCH_FT5336:
            touch_cnt = FT5336_GetTouchAreaInfo(p_info);
            break;

        default:
            VIDEO_LOG("[%s:%d] touch type error", __func__, __LINE__);
            return 0;
    }

    return touch_cnt;
}


int touch_init(void)
{
    CSK_I2C_INIT(TOUCH_I2C_INDEX);

    switch(TOUCH_DEV_TYPE)
    {
        case TOUCH_FT5336:
            VIDEO_LOG("[%s:%d] FT5336 begin", __func__, __LINE__);
            ft5336_init();
            VIDEO_LOG("[%s:%d] FT5336 end", __func__, __LINE__);
            break;

        default:
            VIDEO_LOG("[%s:%d] touch type error", __func__, __LINE__);
            return -1;
    }

    return 0;
}


int touch_deinit(void)
{
    return 0;
}

