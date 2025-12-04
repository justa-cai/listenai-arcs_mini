#include "board.h"


static void lisa_aon_iomux_config()
{
    for(uint32_t i = 0; i < sizeof(board_iomux_aon_config) / sizeof(board_iomux_aon_config[0]); i++){
        AON_IOMuxManager_PinConfigure(board_iomux_aon_config[i].pad, board_iomux_aon_config[i].pin_num, board_iomux_aon_config[i].pin_cfg);
    }
}

static void lisa_iomux_config()
{
    for(uint32_t i = 0; i < sizeof(board_iomux_config) / sizeof(board_iomux_config[0]); i++)
    {
        IOMuxManager_PinConfigure(board_iomux_config[i].pad, board_iomux_config[i].pin_num, board_iomux_config[i].pin_cfg);
    }
}
static void hw_board_iomux_config(void)
{
    lisa_iomux_config();
    lisa_aon_iomux_config();
}

void pre_main_hook(void)
{
    hw_board_iomux_config();

    GPIO_Initialize(GPIOA(), NULL, NULL);
    GPIO_Initialize(GPIOB(), NULL, NULL);
}

const char* board_get_name(void)
{
    return "arcs_wordcard";
}

