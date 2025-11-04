#ifndef _TOUCH_H_
#define _TOUCH_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "log_print.h"
#include "csk_driver.h"


#define USER_TOUCH_COUNT        5


typedef enum {
    TOUCH_FT5336 = 0,
    TOUCH_TYPE_BUTT,
} touch_type_e;

typedef enum
{
    TOUCH_ACTIVE_MODE    = 0x00,       //Active模式 正常模式，没有触摸是依旧保持活动，功耗大
    TOUCH_MONITOR_MODE   = 0x01,       //Monitor模式 省电模式，没有触摸时自动从活动模式切换到监控模式，降低功耗
    TOUCH_HIBERNATE_MODE = 0x02,       //Hibernate模式 休眠模式
}touch_ctrl_t;

typedef struct
{
    uint16_t Xpos[USER_TOUCH_COUNT];       //触摸区域中心X坐标
    uint16_t Ypos[USER_TOUCH_COUNT];       //触摸区域中心Y坐标
    uint8_t Weight[USER_TOUCH_COUNT];      //触摸的压力
    uint8_t Area[USER_TOUCH_COUNT];        //触摸区域
    uint8_t Event[USER_TOUCH_COUNT];       //触摸状态
}touch_info_t;

#define TOUCH_I2C_INDEX        1

 /* addr: 7bit or 10bit */
#define TOUCH_I2C_WRITE_BYTE(addr, reg, value)   CSK_I2C_WRITE_REG8(TOUCH_I2C_INDEX, addr, reg, value)
#define TOUCH_I2C_READ_BYTE(addr, reg)           CSK_I2C_READ_REG8(TOUCH_I2C_INDEX, addr, reg)
#define TOUCH_I2C_READ(addr, reg, pData, num)    CSK_I2C_READ(TOUCH_I2C_INDEX, addr, reg, pData, num)

#define TOUCH_DEV_TYPE          TOUCH_FT5336
#define TOUCH_WIDTH             320
#define TOUCH_HEIGHT            240

int touch_init(void);
int touch_deinit(void);
int touch_control(touch_ctrl_t cmd);
uint8_t touch_read(touch_info_t *p_info);


#endif
