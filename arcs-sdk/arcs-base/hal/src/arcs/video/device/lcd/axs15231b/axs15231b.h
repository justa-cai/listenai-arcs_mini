#ifndef __AXS15231B_H
#define __AXS15231B_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "lcd.h"


void axs15231b_init(lcd_format_e format);
void axs15231b_datalane_set(uint8_t lane_num);
void axs15231b_window_set(uint16_t start_x, uint16_t start_y, uint16_t image_w, uint16_t image_h);


#ifdef __cplusplus
}
#endif

#endif  // __AXS15231B_H

