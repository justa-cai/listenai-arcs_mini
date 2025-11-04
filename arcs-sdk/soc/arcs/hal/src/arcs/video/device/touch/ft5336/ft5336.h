#ifndef __FT5336_H_
#define __FT5336_H_

#include "touch.h"


/*************************************************************************************************************
 * 文件名      :   FT5336.h
 * 功能           :   FT5336电容触摸屏驱动
 * 详细:          使用的IIC与FT5336通讯
*************************************************************************************************************/


#define FT5336_IIC_SLAVE_ADDRESS            ((uint8_t)0x70 >> 1)          //FT5336 IIC通讯地址  7bit

//触摸屏宽高定义
#define FT5336_MAX_WIDTH                    ((uint16_t)TOUCH_WIDTH)      //触摸屏最大宽度
#define FT5336_MAX_HEIGHT                   ((uint16_t)TOUCH_HEIGHT)      //触摸屏最大高度

#define FT5336_TOUCH_POS_XY_EXCHANGE        1               //是否将横坐标与纵坐标对调

//芯片支持的最大触摸点数
#define FT5336_MAX_DETECTABLE_TOUCH         (0x05)

//FT5336的当前工作模式寄存器，用于判断触摸屏是否在正常工作
#define FT5336_DEV_MODE_REG                 ((uint8_t)0x00)
#define FT5336_DEV_MODE_WORKING             ((uint8_t)0x00)              //默认工作模式
#define FT5336_DEV_MODE_FACTORY             ((uint8_t)0x04)              //工厂模式

#define FT5336_DEV_MODE_MASK                ((uint8_t)0x07)
#define FT5336_DEV_MODE_SHIFT               ((uint8_t)0x04)

//手势ID寄出去你
#define FT5336_GEST_ID_REG                  ((uint8_t)0x01)
//相关手势值
#define FT5336_GEST_ID_NO_GESTURE           ((uint8_t)0x00)
#define FT5336_GEST_ID_MOVE_UP              ((uint8_t)0x10)
#define FT5336_GEST_ID_MOVE_RIGHT           ((uint8_t)0x14)
#define FT5336_GEST_ID_MOVE_DOWN            ((uint8_t)0x18)
#define FT5336_GEST_ID_MOVE_LEFT            ((uint8_t)0x1C)
#define FT5336_GEST_ID_SINGLE_CLICK         ((uint8_t)0x20)
#define FT5336_GEST_ID_DOUBLE_CLICK         ((uint8_t)0x22)
#define FT5336_GEST_ID_ROTATE_CLOCKWISE     ((uint8_t)0x28)
#define FT5336_GEST_ID_ROTATE_C_CLOCKWISE   ((uint8_t)0x29)
#define FT5336_GEST_ID_ZOOM_IN              ((uint8_t)0x40)
#define FT5336_GEST_ID_ZOOM_OUT             ((uint8_t)0x49)

//触摸数据状态寄存器：读取将有效触摸点的数量 (0..5)
#define FT5336_TD_STAT_REG                  ((uint8_t)0x02)

//XH或YH的最高2位代表的触摸按钮状态信息
#define FT5336_TOUCH_EVT_FLAG_PRESS_DOWN    ((uint8_t)0x00)
#define FT5336_TOUCH_EVT_FLAG_LIFT_UP       ((uint8_t)0x01)
#define FT5336_TOUCH_EVT_FLAG_CONTACT       ((uint8_t)0x02)
#define FT5336_TOUCH_EVT_FLAG_NO_EVENT      ((uint8_t)0x03)

#define FT5336_TOUCH_EVT_FLAG_SHIFT         ((uint8_t)12)        //从X坐标的最高2位获取EVT
#define FT5336_TOUCH_EVT_FLAG_MASK          ((uint8_t)3)


//触摸区域坐标的掩码-12bit有效值
#define FT5336_TOUCH_POS_DATA_MASK           ((uint16_t)0x0FFF)  //掩码

//触摸区域信息相关寄存器
#define FT5336_ONE_TOUCH_REG_CNT            6               //一个触摸点由6个寄存器组成
//#1触摸点信息
#define FT5336_P1_XH_REG                    ((uint8_t)0x03)      //X坐标信息
#define FT5336_P1_XL_REG                    ((uint8_t)0x04)
#define FT5336_P1_YH_REG                    ((uint8_t)0x05)      //Y坐标信息
#define FT5336_P1_YL_REG                    ((uint8_t)0x06)
#define FT5336_P1_WEIGHT_REG                ((uint8_t)0x07)      //触摸点压力
#define FT5336_P1_MISC_REG                  ((uint8_t)0x08)      //触摸区域
//#2触摸点信息
#define FT5336_P2_XH_REG                    ((uint8_t)0x09)
#define FT5336_P2_XL_REG                    ((uint8_t)0x0A)
#define FT5336_P2_YH_REG                    ((uint8_t)0x0B)
#define FT5336_P2_YL_REG                    ((uint8_t)0x0C)
#define FT5336_P2_WEIGHT_REG                ((uint8_t)0x0D)
#define FT5336_P2_MISC_REG                  ((uint8_t)0x0E)
//#3触摸点信息
#define FT5336_P3_XH_REG                    ((uint8_t)0x0F)
#define FT5336_P3_XL_REG                    ((uint8_t)0x10)
#define FT5336_P3_YH_REG                    ((uint8_t)0x11)
#define FT5336_P3_YL_REG                    ((uint8_t)0x12)
#define FT5336_P3_WEIGHT_REG                ((uint8_t)0x13)
#define FT5336_P3_MISC_REG                  ((uint8_t)0x14)
//#4触摸点信息
#define FT5336_P4_XH_REG                    ((uint8_t)0x15)
#define FT5336_P4_XL_REG                    ((uint8_t)0x16)
#define FT5336_P4_YH_REG                    ((uint8_t)0x17)
#define FT5336_P4_YL_REG                    ((uint8_t)0x18)
#define FT5336_P4_WEIGHT_REG                ((uint8_t)0x19)
#define FT5336_P4_MISC_REG                  ((uint8_t)0x1A)
//#5触摸点信息
#define FT5336_P5_XH_REG                    ((uint8_t)0x1B)
#define FT5336_P5_XL_REG                    ((uint8_t)0x1C)
#define FT5336_P5_YH_REG                    ((uint8_t)0x1D)
#define FT5336_P5_YL_REG                    ((uint8_t)0x1E)
#define FT5336_P5_WEIGHT_REG                ((uint8_t)0x1F)
#define FT5336_P5_MISC_REG                  ((uint8_t)0x20)


//控制寄存器-设置模式
#define FT5336_CTRL_REG                             ((uint8_t)0x86)
//Active模式 正常模式，没有触摸是依旧保持活动，功耗大
#define FT5336_CTRL_KEEP_ACTIVE_MODE                ((uint8_t)0x00)
//Monitor模式 省电模式，没有触摸时自动从活动模式切换到监控模式，降低功耗
#define FT5336_CTRL_KEEP_AUTO_SWITCH_MONITOR_MODE   ((uint8_t)0x01)
//Hibernate模式 休眠模式
#define FT5336_CTRL_HIBERNATE_MODE                  ((uint8_t)0x02)

//Monitor模式下：无触摸时从活动模式切换到监控模式的时间段寄存器
#define FT5336_TIMEENTERMONITOR_REG                 ((uint8_t)0x87)

//Active模式下更新速度寄存器
#define FT5336_PERIODACTIVE_REG                     ((uint8_t)0x88)

//Monitor模式下更新速度寄存器
#define FT5336_PERIODMONITOR_REG                    ((uint8_t)0x89)

//中断模式寄存器（在中断模式下使用）
#define FT5336_GMODE_REG                            ((uint8_t)0xA4)
#define FT5336_G_MODE_INTERRUPT_POLLING             ((uint8_t)0x00)  //中断模式-触摸过程中保持低电平自己轮训
#define FT5336_G_MODE_INTERRUPT_TRIGGER             ((uint8_t)0x01)  //脉冲触发模式-触摸过程中输出脉冲-默认模式

//FT5336 芯片ID寄存器
#define FT5336_CHIP_ID_REG                          ((uint8_t)0xA8)
#define FT5336_ID_VALUE                             ((uint8_t)0x51)  //FT5336 芯片ID


//下面这些寄存器暂时用不上，也找不到资料
//旋转手势模式下的最小允许角度值寄存器
#define FT5336_RADIAN_VALUE_REG                     ((uint8_t)0x91)
//左移和右移手势时的最大偏移寄存器
#define FT5336_OFFSET_LEFT_RIGHT_REG                ((uint8_t)0x92)
//上移和下移手势时的最大偏移寄存器
#define FT5336_OFFSET_UP_DOWN_REG                   ((uint8_t)0x93)
//左右移动手势时的最小距离寄存器
#define FT5336_DISTANCE_LEFT_RIGHT_REG              ((uint8_t)0x94)
//向上和向下手势时的最小距离寄存器
#define FT5336_DISTANCE_UP_DOWN_REG                 ((uint8_t)0x95)
//放大和缩小手势时的最大距离寄存器
#define FT5336_DISTANCE_ZOOM_REG                    ((uint8_t)0x96)
//LIB版本信息的高8位寄存器
#define FT5336_LIB_VER_H_REG                        ((uint8_t)0xA1)
//LIB版本信息的低8位寄存器
#define FT5336_LIB_VER_L_REG                        ((uint8_t)0xA2)
//芯片选择寄存器
#define FT5336_CIPHER_REG                           ((uint8_t)0xA3)
//当前的电源模式-只读寄存器
#define FT5336_PWR_MODE_REG                         ((uint8_t)0xA5)
//FT5336固件版本寄存器
#define FT5336_FIRMID_REG                           ((uint8_t)0xA6)
//发布代码版本寄存器
#define FT5336_RELEASE_CODE_ID_REG                  ((uint8_t)0xAF)
//FT5336系统当前的运行模式寄存器-只读
#define FT5336_STATE_REG                            ((uint8_t)0xBC)
//滤波系数寄存器
#define FT5336_TH_DIFF_REG                          ((uint8_t)0x85)
//触摸检测阈值
#define FT5336_TH_GROUP_REG                         ((uint8_t)0x80)

#define FT5336_USER_TOUCHT_COUNT        5   //用户定义的触摸点数量支持1-FT5336_MAX_DETECTABLE_TOUCH
#if( FT5336_USER_TOUCHT_COUNT == 0 || FT5336_USER_TOUCHT_COUNT > FT5336_MAX_DETECTABLE_TOUCH)
#error("定义的无效的触摸点数量支持！");
#endif //FT5336_USER_TOUCHT_COUNT


//工作模式
typedef enum
{
    FT5336_ACTIVE_MODE          = FT5336_CTRL_KEEP_ACTIVE_MODE,                 //Active模式 正常模式，没有触摸是依旧保持活动，功耗大
    FT5336_MONITOR_MODE         = FT5336_CTRL_KEEP_AUTO_SWITCH_MONITOR_MODE,    //Monitor模式 省电模式，没有触摸时自动从活动模式切换到监控模式，降低功耗
    FT5336_HIBERNATE_MODE       = FT5336_CTRL_HIBERNATE_MODE,                   //Hibernate模式 休眠模式
}FT5336_CTRL_MODE;


/* addr: 7bit or 10bit */
#define FT5336_I2C_WRITE_BYTE(reg, value)   TOUCH_I2C_WRITE_BYTE(FT5336_IIC_SLAVE_ADDRESS, reg, value)
#define FT5336_I2C_READ_BYTE(reg)           TOUCH_I2C_READ_BYTE(FT5336_IIC_SLAVE_ADDRESS, reg)
#define FT5336_I2C_READ(reg, pData, num)    TOUCH_I2C_READ(FT5336_IIC_SLAVE_ADDRESS, reg, pData, num)

//FT5336初始化
int ft5336_init(void);
uint8_t ft5336_read_id(void);                               //FT5336读取芯片id
uint8_t FT5336_GetTouchPointCount(void);                    //FT5336芯片返回有效的触摸点数量
int FT5336_SetCtrlMode(touch_ctrl_t CtrlMode);              //设置FT5336芯片控制模式
uint8_t FT5336_GetTouchAreaInfo(touch_info_t *p_info);         //FT5336获取触摸区域信息

#endif //__FT5336_H_

