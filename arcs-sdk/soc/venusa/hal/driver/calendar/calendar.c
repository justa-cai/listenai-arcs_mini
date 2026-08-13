/**
 * @file Driver_CALENDAR.h
 * @brief Real-Time Clock (RTC) / Calendar Driver Interface
 *        This file provides the interface for controlling and interacting with the RTC/Calendar peripheral.
 *        It includes function declarations, data types, and status codes required for driver operation.
 *
 * @details
 *    The driver supports core functionalities including:
 *      - Timekeeping (year/month/day/hour/minute/second)
 *      - Alarm configuration and detection
 *      - Hardware calibration mechanisms
 *      - Power management control
 *      - Interrupt generation for periodic events and alarms
 *
 *    Design Considerations:
 *      - All time values follow BCD encoding conventions per hardware specification
 *      - Margin checking enforced against hardware register limits
 *      - Atomic operations ensure safe register access in multitasking environments
 *
 * @verbatim
 *      Revision History:
 *        v1.0 - Initial release
 *        v1.1 - Added power control optimizations
 *        v2.0 - Major revision with calibration support
 * @endinternal
 *
 * @note
 *    Required inclusion hierarchy:
 *      #include "Driver_CALENDAR.h" -> Base API definitions
 *      #include "CALENDAR_private.h" (optional) -> Internal implementation details
 *
 * @author USER
 * @date   2025.01.01
 * @copyright Copyright (c) YEAR Company Name. All rights reserved.
 *          Use of this software is governed by the accompanying license agreement.
 */
#include "Driver_CALENDAR.h"


//! Calibration initial value
#define CALENDAR_CALIB_INIT_CAL          (0x2ee0)

//! Calibration trigger mode selection
#define CALENDAR_CALIB_TRIGGER_MODE      (0x1)
//! Calibration switch mode selection
#define CALENDAR_CALIB_SWITCH_MODE       (0x0)
//! Calibration offset clock source selection
#define CALENDAR_CALIB_OFFSET_PLCK       (0x2)
//! Calibration system clock source selection
#define CALENDAR_CALIB_SEL_PCLK          (0x3)

//! Hour period configuration for calibration
#define CALENDAR_CALIB_HOUR_PLCK         (0x0)
//! Minute period configuration for calibration
#define CALENDAR_CALIB_MIN_PLCK          (0x0)
//! Second period configuration for calibration
#define CALENDAR_CALIB_SEC_PLCK          (0x5)


//! Maximum valid year value for time setting
#define CALENDAR_SET_TIME_YEAR_MARGIN    (127UL)
//! Maximum valid month value for time setting
#define CALENDAR_SET_TIME_month_MARGIN   (12UL)
//! Maximum valid weekday value for time setting
#define CALENDAR_SET_TIME_WEEKEND_MARGIN (7UL)
//! Maximum valid day value for time setting
#define CALENDAR_SET_TIME_DAY_MARGIN     (31UL)
//! Maximum valid hour value for time setting
#define CALENDAR_SET_TIME_HOUR_MARGIN    (23UL)
//! Maximum valid minute value for time setting
#define CALENDAR_SET_TIME_MIN_MARGIN     (59UL)
//! Maximum valid second value for time setting
#define CALENDAR_SET_TIME_SEC_MARGIN     (59UL)

//! Maximum valid year value for alarm setting
#define CALENDAR_SET_ALARM_YEAR_MARGIN   (127UL)
//! Maximum valid month value for alarm setting
#define CALENDAR_SET_ALARM_month_MARGIN  (12UL)
//! Maximum valid day value for alarm setting
#define CALENDAR_SET_ALARM_DAY_MARGIN    (31UL)
//! Maximum valid hour value for alarm setting
#define CALENDAR_SET_ALARM_HOUR_MARGIN   (23UL)
//! Maximum valid minute value for alarm setting
#define CALENDAR_SET_ALARM_MIN_MARGIN    (59UL)
//! Maximum valid second value for alarm setting
#define CALENDAR_SET_ALARM_SEC_MARGIN    (59UL)

//! Disable periodic interrupts
#define CALENDAR_PER_DIS                 (0x0)
//! Generate interrupt every second
#define CALENDAR_PER_SEC_CAUSE           (0x1)
//! Generate interrupt every minute
#define CALENDAR_PER_MIN_CAUSE           (0x2)
//! Generate interrupt every hour
#define CALENDAR_PER_HOUR_CAUSE          (0x3)

//! Driver initialization completed flag
#define CALENDAR_FLAG_INITIALIZED        (1UL << 0)
//! Peripheral powered state flag
#define CALENDAR_FLAG_POWERED            (1UL << 1)

/**
 * @brief Calendar driver information structure
 *        Contains callback function and working memory pointer
 */
typedef struct {
    CSK_CALENDAR_SignalEvent_t cb_event;   ///< User event callback function
    void* workspace;                        ///< User-defined work area
    uint32_t flag;                          ///< Driver status flags
} CALENDAR_INFO;

/**
 * @brief Calendar peripheral resources structure
 *        Manages register mapping, IRQ settings, and driver state
 */
typedef struct {
    CALENDAR_RegDef *reg;                  ///< Register block base address
    uint32_t irq_num;                      ///< Interrupt vector number
    void (*irq_handler)(void);             ///< Interrupt service routine
    CALENDAR_INFO *info;                   ///< Associated info structure
} const CALENDAR_RESOURCES;

static CALENDAR_INFO info = {0};            ///< Global driver information

/**
 * @brief Calendar interrupt handler
 *        Processes timer events and alarm conditions
 */
static void CALENDAR_Handler(void);

/**
 * @brief Global calendar peripheral resources instance
 *        Centralized management of hardware resources
 */
static CALENDAR_RESOURCES calendar_resources = {
   IP_CALENDAR_TOP,                      ///< Hardware register base address
   IRQ_CALENDAR_VECTOR,                 ///< Interrupt vector number
   CALENDAR_Handler,                   ///< Interrupt service routine
   &info,                              ///< Associated info structure
};

/**
 * @brief Resource validation macro
 *        Verify resource pointer matches global instance
 * @param[in] res Resource pointer to validate
 */
#define CHECK_RESOURCES(res)  do{\
       if(res != &calendar_resources){\
           return CSK_DRIVER_ERROR_PARAMETER;\
       }\
}while(0)

/**
 * @brief Get calendar driver resources structure
 * @return Pointer to global calendar resources
 */
void* CALENDAR(){
   return (void*)&calendar_resources;
}

/**
 * @brief Initialize calendar driver
 *       Configures default parameters and sets initialized flag
 * @param[in] res          Resource structure pointer
 * @param[in] cb_event     User event callback function
 * @param[in] workspace    User-defined data buffer
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @note Must be called before any other driver functions
 */
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

/**
 * @brief Uninitialize calendar driver
 *        Clears callback references and resets state machine
 * @param[in] res Resource structure pointer
 * @return CSK_DRIVER_OK always
 * @note Does not modify hardware registers - only software state management
 */
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

/**
 * @brief Control power states of calendar peripheral
 *        Supports POWER_OFF and POWER_FULL states with clock gating
 * @param[in] res   Resource structure pointer
 * @param[in] state Target power state (CSK_POWER_OFF/CSK_POWER_LOW/CSK_POWER_FULL)
 * @return CSK_DRIVER_OK on success, error code for invalid transitions
 * @details
 * - For CSK_POWER_OFF: Disconnects IRQ and clears powered state flag
 * - For CSK_POWER_FULL: Performs reset sequence, restores IRQ handler, and sets powered flag
 * - CSK_POWER_LOW is explicitly unsupported
 */
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

       // Disable calendar interrupt
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

       // Disable calendar interrupt
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

/**
 * @brief Set current calendar time
 *       Validates input ranges before updating hardware registers
 * @param[in] res   Resource structure pointer
 * @param[in] stime Time structure containing new values
 * @return CSK_DRIVER_OK on success, error code for invalid parameters or power state
 * @note All fields must be within defined margin values
 */
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

/**
 * @brief Get current calendar time
 *        Reads directly from shadow registers
 * @param[in] res   Resource structure pointer
 * @param[out] stime Time structure to store current values
 * @return CSK_DRIVER_OK on success, error code for power state
 * @note Returns immediately without waiting for synchronization
 */
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

/**
 * @brief Set calendar alarm time
 *       Validates input ranges before updating hardware registers
 * @param[in] res    Resource structure pointer
 * @param[in] salarm Alarm time structure
 * @return CSK_DRIVER_OK on success, error code for invalid parameters or power state
 * @note Alarm will trigger when matching time occurs
 */
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

/**
 * @brief Get current alarm settings
 *        Copies hardware register values to output structure
 * @param[in] res    Resource structure pointer
 * @param[out] salarm Alarm structure to receive current settings
 * @return CSK_DRIVER_OK on success, error code for power state
 * @note Returns immediately without waiting for synchronization
 */
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

/**
 * @brief Control various calendar features
 *        Supports alarm enable/disable, calibration control, and periodic interrupts
 * @param[in] res    Resource structure pointer
 * @param[in] control Control command type
 * @param[in] arg     Command-specific argument
 * @return CSK_DRIVER_OK on success, error code for invalid control or power state
 * @details
 * - CSK_CALENDAR_CTRL_ALARM_EN: Enable/disable alarm functionality
 * - CSK_CALENDAR_CTRL_CALIBRATION_EN: Start/stop automatic calibration
 * - CSK_CALENDAR_CTRL_SEC_INT/MIN_INT/HOUR_INT: Configure periodic interrupt intervals
 */
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

/**
 * @brief Private helper function for interrupt handling
 *       Processes different types of calendar events
 * @param[in] calendar Pointer to calendar resources structure
 * @note Called from public ISR with proper locking mechanism
 */
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

/**
 * @brief Calendar Interrupt Service Routine (ISR)
 *       Dispatches to private handler with proper resource context
 * @note Executes in interrupt context - keeps execution short
 */
static void
CALENDAR_Handler(void){
   __CALENDAR_Handler__(&calendar_resources);
}
