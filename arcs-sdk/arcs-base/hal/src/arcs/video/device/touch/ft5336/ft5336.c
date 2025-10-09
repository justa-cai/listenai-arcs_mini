#include "ft5336.h"


/*************************************************************************************************************
 * 文件名      :   FT5336.c
 * 功能           :   FT5336电容触摸屏驱动
 * 详细:          FT5336默认开启的触摸中断输出，并且空闲高电平，中断为低电平触发
 FT5336最大更新速度120Hz,支持5点触摸
*************************************************************************************************************/


static void FT5336_SetIntOutMode(uint8_t isPolling);  //FT5336芯片中断输出模式


/*************************************************************************************************************************
*函数         :   bool FT5336_Init(FT5336_HANDLE *pHandle, u8 SlaveAddr,
                        bool (*Func_ReadReg)(u16 SlaveAddr, u8 RegAddr, u8 *pDataBuff, u16 ByteNum),
                        bool (*Func_WriteReg)(u16 SlaveAddr, u8 RegAddr, u8 *pDataBuff, u16 ByteNum))
*功能         :   FT5336初始化
*参数         :   pHandle:句柄；SlaveAddr通讯地址；Func_ReadReg：读寄存器接口；Func_WriteReg：写寄存器接口
*返回         :   TRUE:初始化成功；FALSE:初始化失败
*依赖         :   底层宏定义
*说明         :   需要提前初始化IIC接口
*************************************************************************************************************************/
int ft5336_init(void)
{
    uint8_t id;
    uint8_t retry;
    uint8_t temp;

#if 0
    //读取芯片id
    for(retry = 0;retry < 5;retry ++)
    {
        id = ft5336_read_id();                        //读取芯片id
        if(FT5336_ID_VALUE != id)
        {
            SYS_DelayMS(10);
        }
    }

    if(FT5336_ID_VALUE != id)
    {
        printf("[FT5336读取失败]，无效的id:0x%02X\r\n",id);
        return -1;
    }
#endif
    //查看工作模式
    temp = FT5336_I2C_READ_BYTE(FT5336_DEV_MODE_REG);
    if(temp != FT5336_DEV_MODE_WORKING)
    {
        printf("[FT5336]:异常的工作模式%d\r\n", temp);
    }

    //检查固件版本
    temp = FT5336_I2C_READ_BYTE(FT5336_FIRMID_REG);
    if(temp == 0 || temp == 0xff)
    {
        printf("[FT5336]:异常的固件版本%d\r\n", temp);
    }
    else
    {
        printf("[FT5336固件版本]：0x%02X\r\n", temp);
    }

    //此处初始化成功了，进行后续操作
    FT5336_SetIntOutMode(1);                                //设置触摸中断为低电平保持模式
    FT5336_SetCtrlMode(FT5336_CTRL_KEEP_ACTIVE_MODE);          //设置FT5336芯片控制模式

    return 0;
}


/*************************************************************************************************************************
*函数         :   u8 FT5336_ReadID(FT5336_HANDLE *pHandle)
*功能         :   FT5336读取芯片id
*参数         :   pHandle:句柄
*返回         :   id；0：无效
*依赖         :   底层宏定义
*说明         :
*************************************************************************************************************************/
uint8_t ft5336_read_id(void)
{
    return FT5336_I2C_READ_BYTE(FT5336_CHIP_ID_REG);
}


/*************************************************************************************************************************
*函数         :   bool FT5336_SetIntOutMode(FT5336_HANDLE *pHandle, bool isPolling)
*功能         :   FT5336芯片中断输出模式
*参数         :   pHandle:句柄；isPolling：TRUE:触摸一直保持低电平，松开后恢复高电平；FALSE:触摸过程中定时输出低电平脉冲
*返回         :   TRUE:成功；FALSE:失败
*依赖         :   底层宏定义
*说明         :   用于设置触摸屏的触摸中断状态输出模式
*************************************************************************************************************************/
static void FT5336_SetIntOutMode(uint8_t isPolling)
{
    uint8_t regValue = 0;

    if(isPolling)   //触摸保持低电平
    {
        regValue = FT5336_G_MODE_INTERRUPT_POLLING;
    }
    else
    {
        regValue = FT5336_G_MODE_INTERRUPT_TRIGGER;
    }

    FT5336_I2C_WRITE_BYTE(FT5336_GMODE_REG, regValue);      //写入FT5336中断寄存器
}


/*************************************************************************************************************************
*函数         :   bool FT5336_SetCtrlMode(FT5336_HANDLE *pHandle, FT5336_CTRL_MODE CtrlMode)
*功能         :   设置FT5336芯片控制模式
*参数         :   pHandle:句柄；CtrlMode:控制模式；见：FT5336_CTRL_MODE
*返回         :   TRUE:成功；FALSE:失败
*依赖         :   底层宏定义
*说明         :   用于设置触摸屏的工作模式
*************************************************************************************************************************/
int FT5336_SetCtrlMode(touch_ctrl_t CtrlMode)
{
    uint8_t regValue = CtrlMode;

    FT5336_I2C_WRITE_BYTE(FT5336_CTRL_REG, regValue);       //写入FT5336寄存器

    return 0;
}


/*************************************************************************************************************************
*函数         :   u8 FT5336_GetTouchPointCount(FT5336_HANDLE *pHandle)
*功能         :   FT5336芯片返回有效的触摸点数量
*参数         :   pHandle:句柄
*返回         :   0-5：触摸点数量
*依赖         :   底层宏定义
*说明         :
*************************************************************************************************************************/
uint8_t FT5336_GetTouchPointCount(void)
{
    uint8_t nbTouch = 0;

    //读取寄存器 FT5336_TD_STAT_REG 获取有效的触摸点数量
    nbTouch = FT5336_I2C_READ_BYTE(FT5336_TD_STAT_REG);
    nbTouch &= 0x07;
    if(nbTouch > FT5336_MAX_DETECTABLE_TOUCH)   //点数超过了最大值，读取的数据可能是无效的，直接清零
    {
        nbTouch = 0;
    }

    return nbTouch;
}


/*************************************************************************************************************************
*函数         :   u8 FT5336_GetTouchAreaInfo(FT5336_HANDLE *pHandle, FT5336_TOUCH_TYPE pTouchArea[FT5336_USER_TOUCHT_COUNT])
*功能         :   FT5336获取触摸区域信息
*参数         :   pHandle:句柄；pTouchArea：触摸区域信息，见FT5336_TOUCH_TYPE定义
*返回         :   0-5：触摸点数量
*依赖         :   底层宏定义
*说明         :   x,y坐标只有12bit有效位；x坐标的最高2位为触摸状态，具体值定义见：
*************************************************************************************************************************/
uint8_t FT5336_GetTouchAreaInfo(touch_info_t *p_info)  //(FT5336_TOUCH_TYPE pTouchArea[FT5336_USER_TOUCHT_COUNT])
{
    uint8_t num;
    uint8_t buff[FT5336_USER_TOUCHT_COUNT * FT5336_ONE_TOUCH_REG_CNT];       //读取缓冲区
    uint8_t i;
    uint8_t *p = buff;

    num = FT5336_GetTouchPointCount();                           //获取触摸点数量
    if(num > FT5336_USER_TOUCHT_COUNT)
        num = FT5336_USER_TOUCHT_COUNT;  //限制触摸点的数量-由用户决定支持的数量

    if(num > 0)                                                         //有触摸，读取触摸点的信息
    {
        if(FT5336_I2C_READ(FT5336_P1_XH_REG, buff, num * FT5336_ONE_TOUCH_REG_CNT) == 0)
        {
            printf("读取触摸区域信息失败\r\n");
            num = 0;
        }
        else //读取成功了，填充信息
        {
            for(i = 0;i < num;i ++)
            {
#if(FT5336_TOUCH_POS_XY_EXCHANGE)   //需要对调XY坐标
                p_info->Ypos[i] = *p ++;     //X高位
                p_info->Ypos[i] <<= 8;
                p_info->Ypos[i] |= *p ++;    //X低位
                p_info->Event[i] = (p_info->Ypos[i] >> FT5336_TOUCH_EVT_FLAG_SHIFT) & FT5336_TOUCH_EVT_FLAG_MASK;   //Event
                p_info->Ypos[i] &= FT5336_TOUCH_POS_DATA_MASK;
                p_info->Xpos[i] = *p ++;     //Y高位
                p_info->Xpos[i] <<= 8;
                p_info->Xpos[i] |= *p ++;    //Y低位
                p_info->Xpos[i] &= FT5336_TOUCH_POS_DATA_MASK;
#else //不需要对调
                p_info->Xpos[i] = *p ++;     //X高位
                p_info->Xpos[i] <<= 8;
                p_info->Xpos[i] |= *p ++;    //X低位
                p_info->Event[i] = (p_info->Xpos[i] >> FT5336_TOUCH_EVT_FLAG_SHIFT) & FT5336_TOUCH_EVT_FLAG_MASK;   //Event
                p_info->Xpos[i] &= FT5336_TOUCH_POS_DATA_MASK;
                p_info->Ypos[i] = *p ++;     //Y高位
                p_info->Ypos[i] <<= 8;
                p_info->Ypos[i] |= *p ++;    //Y低位
                p_info->Ypos[i] &= FT5336_TOUCH_POS_DATA_MASK;
#endif //FT5336_TOUCH_POS_XY_EXCHANGE


                p_info->Weight[i] = *p ++;   //触摸压力
                p_info->Area[i] = *p ++;     //触摸区域
            }
        }
    }

    return num;
}


