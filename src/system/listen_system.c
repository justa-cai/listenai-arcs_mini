#define TAG "system"

#include <string.h>
#include "sntp.h"
#include "lisa_mem.h"
#include "lisa_log.h"
#include "c_datetime.h"
#include "lisa_thread.h"
#include "lisa_typedef.h"
#include "listen_wifi.h"
#include "listen_system.h"
#include "Driver_CALENDAR.h"
#include "user_sntp.h"


static void *s_calendar = NULL;
static long s_msec = 0;
static int s_tz = TIMEZONE_SHANGHAI;
static bool s_user_sntp_started = false;
static sntp_synced_callback s_sntp_synced_cb = NULL;

static const uint8_t s_days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

#define IS_LEAP_YEAR(y) ((y%4==0 && y%100!=0) || y%400==0)
#define DAYS_IN_YEAR(y) (IS_LEAP_YEAR(y) ? 366 : 365)
#define DAYS_IN_MONTH(y,m) (m==2 && IS_LEAP_YEAR(y) ? 29 : s_days[m-1])

#define SECONDS_PER_MINUTE (60)
#define SECONDS_PER_HOUR   (60 * SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY    (24 * SECONDS_PER_HOUR)
#define SECONDS_PER_WEEK   (7 * SECONDS_PER_DAY)
#define SECONDS_PER_MONTH(y,m) (DAYS_IN_MONTH(y,m) * SECONDS_PER_DAY)
#define SECONDS_PER_YEAR(y) (DAYS_IN_YEAR(y) * SECONDS_PER_DAY)

#define SECONDS_TIMEZONE_OFFSET(zone) ((zone<0?zone+1:zone) * SECONDS_PER_HOUR)
#define BASE_YEAR (1970)
#define WEEKEND (1)

#define DEFAULT_UNIX_TIMESTAMP 1735660800  /** 2025年1月1日 */

/**
 * \brief UNIX2UTC
 * 
 * \param[in]  tv UNIX time
 * \param[in]  tz timezone
 * \param[out] calendar UTC time
 */
static void time2calendar(struct timeval *tv, int tz, CSK_CALENDAR_TIME *calendar)
{
    long time = tv->tv_sec;
#if TIMEZONE
    time += SECONDS_TIMEZONE_OFFSET(tz);
#endif
#if WEEKEND
    int days = 0;
#endif

    int year = 1970;
    while (true) {
        int seconds_in_year = SECONDS_PER_YEAR(year);
        if (time < seconds_in_year) break;
        time -= seconds_in_year;
#if WEEKEND
        days += DAYS_IN_YEAR(year);
#endif
        year ++;
    }
    calendar->year = year - BASE_YEAR;

    int month = 1;
    while (true) {
        int seconds_in_month = SECONDS_PER_MONTH(year, month);
        if (time < seconds_in_month) break;
        time -= seconds_in_month;
#if WEEKEND
        days += DAYS_IN_MONTH(year, month);
#endif
        month ++;
    }
    calendar->month = month;

    calendar->day = time / SECONDS_PER_DAY + 1;
    time %= SECONDS_PER_DAY;

#if WEEKEND
    days += calendar->day - 1;
    calendar->weekend = (days + 4) % 7;
#endif

    calendar->hour = time / SECONDS_PER_HOUR;
    time %= SECONDS_PER_HOUR;

    calendar->min = time / SECONDS_PER_MINUTE;
    time %= SECONDS_PER_MINUTE;

    calendar->sec = time;
}

/**
 * \brief UTC2UNIX
 *
 * \param[out] tv UNIX time
 * \param[in]  tz timezone
 * \param[in]  calendar UTC time
 */
static void calendar2time(struct timeval *tv, int tz, CSK_CALENDAR_TIME *calendar)
{
    long time = 0;

    int year = 1970, calendar_year = calendar->year + BASE_YEAR;
    while (year < calendar_year) {
        time += SECONDS_PER_YEAR(year);
        year ++;
    }

    int month = 1;
    while (month < calendar->month) {
        time += SECONDS_PER_MONTH(year, month);
        month ++;
    }

    time += (calendar->day - 1) * SECONDS_PER_DAY;
    time += calendar->hour * SECONDS_PER_HOUR;
    time += calendar->min * SECONDS_PER_MINUTE;
    time += calendar->sec;

#if TIMEZONE
    time -= SECONDS_TIMEZONE_OFFSET(tz);
#endif

    tv->tv_sec = time;
    tv->tv_usec = 0;
}

static int _system_time_set(struct timeval *tv, int tz)
{
    if (!s_calendar) return -1;

    CSK_CALENDAR_TIME stime;
    time2calendar(tv, tz, &stime);
    CALENDAR_SetTime(s_calendar, &stime);
    // CALENDAR_Control(s_calendar, CSK_CALENDAR_CTRL_SEC_INT, 1);
    CALENDAR_Control(s_calendar, CSK_CALENDAR_CTRL_CALIBRATION_EN, 1);
    s_msec = xTaskGetTickCount();
    return 0;
}

void ls_sys_init(int tz)
{
    if (s_calendar) return;

    LISA_LOGI(TAG, "system init start");
    s_calendar = CALENDAR();
    CALENDAR_Initialize(s_calendar, NULL, NULL);
    CALENDAR_PowerControl(s_calendar, CSK_POWER_FULL);

    struct timeval tv = {
        .tv_sec = DEFAULT_UNIX_TIMESTAMP,
        .tv_usec = 0,
    };

    s_tz = tz;

    _system_time_set(&tv, tz);
    LISA_LOGI(TAG, "system init end");
}

static void _user_sntp_status_callback(SntpSyncStatus_t status)
{
    if (status == SNTP_SYNC_STATUE_SYNCED) {
        if (s_sntp_synced_cb) {
            s_sntp_synced_cb();
            s_sntp_synced_cb = NULL;
        }
    }
}

void ls_sys_sntp_start(sntp_synced_callback cb)
{
#if 0
    // set operating mode
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // get SNTP server via DHCP
    sntp_servermode_dhcp(1);
    // init sntp
    sntp_init();
#endif

    s_sntp_synced_cb = cb;
    LISA_LOGI(TAG, "sntp start");
    user_sntp_start(_user_sntp_status_callback);
}

void ls_sys_set_time(uint32_t sec, uint32_t usec)
{
    if (s_calendar) {
        LISA_LOGV(TAG, "sntp time callback, sec: %u, usec: %u", sec, usec);

        struct timeval tv = {
            .tv_sec = sec,
            .tv_usec = 0,
        };
        _system_time_set(&tv, s_tz);
    }
}

void ls_sys_set_timeval(struct timeval *val)
{
    if (s_calendar) {
        LISA_LOGI(TAG, "sntp timeval callback, sec: %u, usec: %u", val->tv_sec, val->tv_usec);
        _system_time_set(val, s_tz);
    }
}

int ls_sys_get_time(struct timeval *tv)
{
    if (s_calendar) {
        CSK_CALENDAR_TIME stime;
        CALENDAR_GetTime(s_calendar, &stime);
        calendar2time(tv, s_tz, &stime);
        const long now = xTaskGetTickCount() - s_msec;
        tv->tv_usec = ((now * portTICK_PERIOD_MS) % 1000) * 1000;

        return 0;
    }
    return -1;
}

#define SEC_PER_HOUR (3600)
// 中国标准时间较 UTC 偏移秒数, 8hour
#define CST_OFFSET_BY_UTC (8 * SEC_PER_HOUR)
struct tm *ls_sys_get_tmtime(const long int *tv_sec, struct tm *__tm)
{
	if(tv_sec == NULL) return NULL;
	if(__tm == NULL) return NULL;

	SHDateTime shdt;
	timestampToDateObj(*tv_sec + CST_OFFSET_BY_UTC, 0, &shdt);
    __tm->tm_year = shdt.year - 1900;
	__tm->tm_mon = shdt.month - 1;
	__tm->tm_mday = shdt.day;
	__tm->tm_hour = shdt.hour;
	__tm->tm_min = shdt.min;
	__tm->tm_sec = shdt.sec;

	return __tm;
}