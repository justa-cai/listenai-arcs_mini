#include "Driver_CALENDAR.h"

// Calibration Parameter
#define CALENDAR_CALIB_INIT_CAL          (0x2ee0)

#define CALENDAR_CALIB_TRIGGER_MODE      (0x1)
#define CALENDAR_CALIB_SWITCH_MODE       (0x0)
#define CALENDAR_CALIB_OFFSET_PLCK       (0x2)
#define CALENDAR_CALIB_SEL_PCLK          (0x3)

#define CALENDAR_CALIB_HOUR_PLCK         (0x0)
#define CALENDAR_CALIB_MIN_PLCK          (0x0)
#define CALENDAR_CALIB_SEC_PLCK          (0x5)

// PARAMETER MARGIN
#define CALENDAR_SET_TIME_YEAR_MARGIN    (127UL)
#define CALENDAR_SET_TIME_month_MARGIN   (12UL)
#define CALENDAR_SET_TIME_WEEKEND_MARGIN (7UL)
#define CALENDAR_SET_TIME_DAY_MARGIN     (31UL)
#define CALENDAR_SET_TIME_HOUR_MARGIN    (23UL)
#define CALENDAR_SET_TIME_MIN_MARGIN     (59UL)
#define CALENDAR_SET_TIME_SEC_MARGIN     (59UL)

#define CALENDAR_SET_ALARM_YEAR_MARGIN   (127UL)
#define CALENDAR_SET_ALARM_month_MARGIN  (12UL)
#define CALENDAR_SET_ALARM_DAY_MARGIN    (31UL)
#define CALENDAR_SET_ALARM_HOUR_MARGIN   (23UL)
#define CALENDAR_SET_ALARM_MIN_MARGIN    (59UL)
#define CALENDAR_SET_ALARM_SEC_MARGIN    (59UL)

#define CALENDAR_PER_DIS                 (0x0)
#define CALENDAR_PER_SEC_CAUSE           (0x1)
#define CALENDAR_PER_MIN_CAUSE           (0x2)
#define CALENDAR_PER_HOUR_CAUSE          (0x3)

#define CALENDAR_FLAG_INITIALIZED        (1UL << 0)
#define CALENDAR_FLAG_POWERED            (1UL << 1)

typedef struct {
    CSK_CALENDAR_SignalEvent_t cb_event;
    void* workspace;
    uint32_t flag;
} CALENDAR_INFO;

typedef struct
{
    CALENDAR_RegDef *reg;
    uint32_t irq_num;
    void (*irq_handler)(void);
    CALENDAR_INFO *info;
} const CALENDAR_RESOURCES;

static CALENDAR_INFO info = {0};

static void CALENDAR_Handler(void);
static CALENDAR_RESOURCES calendar_resources = {
   IP_CALENDAR,
   IRQ_CALENDAR_VECTOR,
   CALENDAR_Handler,
   &info,
};

#define CHECK_RESOURCES(res)  do{\
       if(res != &calendar_resources){\
           return CSK_DRIVER_ERROR_PARAMETER;\
       }\
}while(0)

void* CALENDAR(){
   return (void*)&calendar_resources;
}

int32_t
CALENDAR_Initialize(void *res, CSK_CALENDAR_SignalEvent_t cb_event, void* workspace)
{
   CHECK_RESOURCES(res);
   CALENDAR_RESOURCES *calendar = (CALENDAR_RESOURCES*)res;
   if(calendar->info->flag & CALENDAR_FLAG_INITIALIZED){
       return CSK_DRIVER_OK;
   }

   calendar->info->cb_event = cb_event;
   calendar->info->workspace = workspace;

   calendar->info->flag = CALENDAR_FLAG_INITIALIZED;

   return CSK_DRIVER_OK;
}

int32_t
CALENDAR_Uninitialize(void *res)
{
   CHECK_RESOURCES(res);
   CALENDAR_RESOURCES *calendar = (CALENDAR_RESOURCES*)res;

   calendar->info->cb_event = NULL;
   calendar->info->workspace = NULL;

   calendar->info->flag = 0;

   return CSK_DRIVER_OK;
}

int32_t
CALENDAR_PowerControl(void* res, CSK_POWER_STATE state)
{
   CHECK_RESOURCES(res);
   CALENDAR_RESOURCES *calendar = (CALENDAR_RESOURCES*)res;

   switch (state)
   {
   case CSK_POWER_OFF:
       if((calendar->info->flag & CALENDAR_FLAG_INITIALIZED) == 0U){
           return CSK_DRIVER_ERROR;
       }

       disable_IRQ(calendar->irq_num);

       register_ISR(calendar->irq_num, NULL, NULL);

       // Disable CALENDAR interrupt
       calendar->reg->REG_CMD.bit.ALARM_ENABLE_CLR = 0x1;
       while(calendar->reg->REG_CMD.bit.ALARM_ENABLE_CLR);

       calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_CLR = 0x1;
       while(calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_CLR);

       // Clear IRQ pending status
       calendar->reg->REG_CMD.bit.ITV_IRQ_CLR = 0x1;
       calendar->reg->REG_CMD.bit.ALARM_CLR = 0x1;

       // Disable force wakeup
       IP_SYSCTRL->REG_RTC_FORCE_WAKEUP.bit.FORCE_WAKEUP = 0x0;

       calendar->info->flag &= (~CALENDAR_FLAG_POWERED);

       break;

   case CSK_POWER_LOW:
       return CSK_DRIVER_ERROR_UNSUPPORTED;

   case CSK_POWER_FULL:
      if(!(calendar->info->flag & CALENDAR_FLAG_INITIALIZED)){
          return CSK_DRIVER_ERROR;
      }
      if(calendar->info->flag & CALENDAR_FLAG_POWERED){
          return CSK_DRIVER_OK;
      }

       // TODO reset pheripheral (importance)

       // Clear IRQ pending status
       calendar->reg->REG_CMD.bit.ITV_IRQ_CLR = 0x1;
       calendar->reg->REG_CMD.bit.ALARM_CLR = 0x1;

       register_ISR(calendar->irq_num, calendar->irq_handler, NULL);
       enable_IRQ(calendar->irq_num);

       // Enable force wakeup
       IP_SYSCTRL->REG_RTC_FORCE_WAKEUP.bit.FORCE_WAKEUP = 0x1;
       while(!calendar->reg->REG_STATUS.bit.FORCE_WAKEUP);

       // Disable CALENDAR interrupt
       calendar->reg->REG_CMD.bit.ALARM_ENABLE_CLR = 0x1;
       while(calendar->reg->REG_CMD.bit.ALARM_ENABLE_CLR);

       calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_CLR = 0x1;
       while(calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_CLR);

       // Clear IRQ pending status
       calendar->reg->REG_CMD.bit.ITV_IRQ_CLR = 0x1;
       calendar->reg->REG_CMD.bit.ALARM_CLR = 0x1;

       calendar->info->flag |= CALENDAR_FLAG_POWERED;

       break;

   default:
       return CSK_DRIVER_ERROR_UNSUPPORTED;
   }

   return CSK_DRIVER_OK;
}

int32_t
CALENDAR_SetTime(void* res, CSK_CALENDAR_TIME* stime)
{
   CHECK_RESOURCES(res);
   CALENDAR_RESOURCES *calendar = (CALENDAR_RESOURCES*)res;

   if((calendar->info->flag & CALENDAR_FLAG_POWERED) == 0){
       return CSK_DRIVER_ERROR;
   }

   // check the parameter
   if((stime->year > CALENDAR_SET_TIME_YEAR_MARGIN) ||\
           (stime->month > CALENDAR_SET_TIME_month_MARGIN) ||\
           (stime->weekend > CALENDAR_SET_TIME_WEEKEND_MARGIN) ||\
           (stime->day > CALENDAR_SET_TIME_DAY_MARGIN) ||\
           (stime->hour > CALENDAR_SET_TIME_HOUR_MARGIN) ||\
           (stime->min > CALENDAR_SET_TIME_MIN_MARGIN) ||\
           (stime->sec > CALENDAR_SET_TIME_SEC_MARGIN)){
       return CSK_DRIVER_ERROR_PARAMETER;
   }

   calendar->reg->REG_LOAD_VAL_I.bit.SEC_SET = stime->sec;
   calendar->reg->REG_LOAD_VAL_I.bit.MIN_SET = stime->min;
   calendar->reg->REG_LOAD_VAL_I.bit.HOUR_SET = stime->hour;
   calendar->reg->REG_LOAD_VAL_H.bit.DAY_SET = stime->day;
   calendar->reg->REG_LOAD_VAL_H.bit.WEEKDAY_SET = stime->weekend;
   calendar->reg->REG_LOAD_VAL_H.bit.MON_SET = stime->month;
   calendar->reg->REG_LOAD_VAL_H.bit.YEAR_SET = stime->year;

   // Load and wait finished
   calendar->reg->REG_CMD.bit.CALENDAR_LOAD = 0x1;
   while(calendar->reg->REG_CMD.bit.CALENDAR_LOAD);

   return CSK_DRIVER_OK;
}

int32_t
CALENDAR_GetTime(void *res, CSK_CALENDAR_TIME* stime)
{
   CHECK_RESOURCES(res);
   CALENDAR_RESOURCES *calendar = (CALENDAR_RESOURCES*)res;

   stime->sec = calendar->reg->REG_CUR_VAL_L.bit.SEC;
   stime->min = calendar->reg->REG_CUR_VAL_L.bit.MIN;
   stime->hour = calendar->reg->REG_CUR_VAL_L.bit.HOUR;
   stime->day = calendar->reg->REG_CUR_VAL_H.bit.DAY;
   stime->weekend = calendar->reg->REG_CUR_VAL_H.bit.WEEKDAY;
   stime->month = calendar->reg->REG_CUR_VAL_H.bit.MON;
   stime->year = calendar->reg->REG_CUR_VAL_H.bit.YEAR;

   return CSK_DRIVER_OK;
}

int32_t
CALENDAR_SetAlarm(void* res, CSK_CALENDAR_ALARM* salarm)
{
   CHECK_RESOURCES(res);
   CALENDAR_RESOURCES *calendar = (CALENDAR_RESOURCES*)res;

   // check the parameter
   if((salarm->year > CALENDAR_SET_ALARM_YEAR_MARGIN) ||\
           (salarm->month > CALENDAR_SET_ALARM_month_MARGIN) ||\
           (salarm->day > CALENDAR_SET_ALARM_DAY_MARGIN) ||\
           (salarm->hour > CALENDAR_SET_ALARM_HOUR_MARGIN) ||\
           (salarm->min > CALENDAR_SET_ALARM_MIN_MARGIN) ||\
           (salarm->sec > CALENDAR_SET_ALARM_SEC_MARGIN)){
       return CSK_DRIVER_ERROR_PARAMETER;
   }

   calendar->reg->REG_ALARMVAL_L.bit.SEC_ALARM = salarm->sec;
   calendar->reg->REG_ALARMVAL_L.bit.MIN_ALARM = salarm->min;
   calendar->reg->REG_ALARMVAL_L.bit.HOUR_ALARM = salarm->hour;
   calendar->reg->REG_ALARMVAL_H.bit.DAY_ALARM = salarm->day;
   calendar->reg->REG_ALARMVAL_H.bit.MON_ALARM = salarm->month;
   calendar->reg->REG_ALARMVAL_H.bit.YEAR_ALARM = salarm->year;

   calendar->reg->REG_CMD.bit.ALARM_LOAD = 0x1;
   while(calendar->reg->REG_CMD.bit.ALARM_LOAD);

   return CSK_DRIVER_OK;
}

int32_t
CALENDAR_GetAlarm(void* res, CSK_CALENDAR_ALARM* salarm)
{
   CHECK_RESOURCES(res);
   CALENDAR_RESOURCES *calendar = (CALENDAR_RESOURCES*)res;

   salarm->sec = calendar->reg->REG_ALARMVAL_L.bit.SEC_ALARM;
   salarm->min = calendar->reg->REG_ALARMVAL_L.bit.MIN_ALARM;
   salarm->hour = calendar->reg->REG_ALARMVAL_L.bit.HOUR_ALARM;
   salarm->day = calendar->reg->REG_ALARMVAL_H.bit.DAY_ALARM;
   salarm->month = calendar->reg->REG_ALARMVAL_H.bit.MON_ALARM;
   salarm->year = calendar->reg->REG_ALARMVAL_H.bit.YEAR_ALARM;

   return CSK_DRIVER_OK;
}

int32_t
CALENDAR_Control(void* res, uint32_t control, uint32_t arg)
{
   CHECK_RESOURCES(res);
   CALENDAR_RESOURCES *calendar = (CALENDAR_RESOURCES*)res;

   switch (control)
   {
   case CSK_CALENDAR_CTRL_ALARM_EN:
       // Enable alarm
       if (arg){
           calendar->reg->REG_CMD.bit.ALARM_ENABLE_SET = 0x1;
           while(calendar->reg->REG_CMD.bit.ALARM_ENABLE_SET);
       } else {
           calendar->reg->REG_CMD.bit.ALARM_ENABLE_CLR = 0x1;
           while(calendar->reg->REG_CMD.bit.ALARM_ENABLE_CLR);
       }
       break;

   case CSK_CALENDAR_CTRL_CALIBRATION_EN:
       if (arg) {
           IP_AON_CTRL->REG_RTC_CALIB_CTRL0.bit.DIV_CNT_DISABLE_PCLK = 0x0;

           calendar->reg->REG_CAL.bit.INIT_CAL = CALENDAR_CALIB_INIT_CAL;

           IP_AON_CTRL->REG_RTC_CALIB_CTRL0.bit.CALIB_TIME_SEL_PCLK = CALENDAR_CALIB_SEL_PCLK;
           IP_AON_CTRL->REG_RTC_CALIB_CTRL0.bit.CALIB_OFFSET_PCLK = CALENDAR_CALIB_OFFSET_PLCK;
           IP_AON_CTRL->REG_RTC_CALIB_CTRL0.bit.CALIB_TRIG_MODE_PCLK = CALENDAR_CALIB_TRIGGER_MODE;
           IP_AON_CTRL->REG_RTC_CALIB_CTRL0.bit.CALIB_SWITCH_MODE_PCLK = CALENDAR_CALIB_SWITCH_MODE;

           IP_AON_CTRL->REG_RTC_CALIB_CTRL1.bit.CALIB_INTERVAL_SECOND_PCLK = CALENDAR_CALIB_SEC_PLCK;
           IP_AON_CTRL->REG_RTC_CALIB_CTRL1.bit.CALIB_INTERVAL_MINUTE_PCLK = CALENDAR_CALIB_MIN_PLCK;
           IP_AON_CTRL->REG_RTC_CALIB_CTRL1.bit.CALIB_INTERVAL_HOUR_PCLK = CALENDAR_CALIB_HOUR_PLCK;

           IP_AON_CTRL->REG_RTC_CALIB_CTRL0.bit.CALIB_TRIG_ENABLE_PCLK = 0x1;

           calendar->reg->REG_CAL.bit.INIT_CAL_LOAD = 0x1;
       } else {
           IP_AON_CTRL->REG_RTC_CALIB_CTRL0.bit.DIV_CNT_DISABLE_PCLK = 0x1;
           IP_AON_CTRL->REG_RTC_CALIB_CTRL0.bit.CALIB_TRIG_ENABLE_PCLK = 0x0;
       }
       break;

   case CSK_CALENDAR_CTRL_SEC_INT:
       // Enable second interrupt
       if (arg){
           calendar->reg->REG_CTRL.bit.INTERVAL = CALENDAR_PER_SEC_CAUSE;
           calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_SET = 0x1;
       } else {
           calendar->reg->REG_CTRL.bit.INTERVAL = CALENDAR_PER_DIS;
           calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_CLR = 0x1;
       }
       break;
   case CSK_CALENDAR_CTRL_MIN_INT:
       // Enable minute interrupt
       if (arg){
           calendar->reg->REG_CTRL.bit.INTERVAL = CALENDAR_PER_MIN_CAUSE;
           calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_SET = 0x1;
       } else {
           calendar->reg->REG_CTRL.bit.INTERVAL = CALENDAR_PER_DIS;
           calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_CLR = 0x1;
       }
       break;
   case CSK_CALENDAR_CTRL_HOUR_INT:
       // Enable hour interrupt
       if (arg){
           calendar->reg->REG_CTRL.bit.INTERVAL = CALENDAR_PER_HOUR_CAUSE;
           calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_SET = 0x1;
       } else {
           calendar->reg->REG_CTRL.bit.INTERVAL = CALENDAR_PER_DIS;
           calendar->reg->REG_CMD.bit.ITV_IRQ_MASK_CLR = 0x1;
       }
       break;

   default:
       return CSK_DRIVER_ERROR_UNSUPPORTED;
   }

   return CSK_DRIVER_OK;
}

static void
__CALENDAR_Handler__(CALENDAR_RESOURCES *calendar){
   uint32_t status, event = 0;
   status = calendar->reg->REG_STATUS.all;

   if (status & CALENDAR_STATUS_ITV_IRQ_CAUSE_Msk){
       // Clear interrupt status
       calendar->reg->REG_CMD.bit.ITV_IRQ_CLR = 0x1;
       switch (calendar->reg->REG_CTRL.bit.INTERVAL){
       case CALENDAR_PER_SEC_CAUSE:
           event |= CSK_CALENDAR_EVENT_SEC_INT;
           break;
       case CALENDAR_PER_MIN_CAUSE:
           event |= CSK_CALENDAR_EVENT_MIN_INT;
           break;
       case CALENDAR_PER_HOUR_CAUSE:
           event |= CSK_CALENDAR_EVENT_HOUR_INT;
           break;
       }
   }

   if (status & CALENDAR_STATUS_ALARM_IRQ_CAUSE_Msk){
       calendar->reg->REG_CMD.bit.ALARM_CLR = 0x1;
       while(calendar->reg->REG_CMD.bit.ALARM_CLR);

       event |= CSK_CALENDAR_EVENT_ALARM_INT;
   }

   if (calendar->info->cb_event && event){
       calendar->info->cb_event(event, calendar->info->workspace);
   }
}

static void
CALENDAR_Handler(void){
   __CALENDAR_Handler__(&calendar_resources);
}
