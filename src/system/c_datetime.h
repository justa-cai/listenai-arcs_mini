//
//  c_datetime.h
//  SHCommon
//
//  Created by Joel Pridgen on 3/10/18.
//  Copyright © 2018 Joel Gillette. All rights reserved.
//

#ifndef c_datetime_h
#define c_datetime_h

#include <stdio.h>

static const int BASE_YEAR = 1970;
static const int BEFORE_EPOCH_BASE_YEAR = 1969;
static const int FIRST_LEAP_YEAR = 1972;
static const int LEAP_YEAR_BEFORE_EPOCH = 1968;
static const int UNIX_MIN = 60;
static const int UNIX_HOUR = 3600;
static const int UNIX_DAY = 86400;
static const int UNIX_YEAR = 31536000;
static const int UNIX_LEAP_YEAR = 31622400;
static const int UNIX_LEAP_CYCLE = 126230400;
static const int UNIX_LEAP_YEAR_LAST_SEC = 31622399;
static const int WEEK_START_DAYS_BEFORE = 5;
static const int WEEK_START_DAYS_AFTER = 3;
static const int EPOCH_WEEK_CORRECTION = 4;
static const int TIMESTAMP_BEGIN_1972 = 63072000;
static const int TIMESTAMP_END_1968 = -31536001;
static const int LEAP_CYCLE_YEAR = 4;
static const int OFFSET_LEAP_YEAR = 2;
static const int LEAP_FEB_SUM = 60;
static const int MIN_SEC_LEN = 60;
static const int DAY_HOURS = 24;
static const int EPOCH_NEUTRAL = 1;
static const int MIRROR_BEFORE_EPOCH = -1;
static const int INCLUDE_TODAY = 1;
static const int YEAR_CUSP = 0;
static const int NORMAL_YEAR_DAYS = 365;
static const int LEAP_YEAR_DAYS = 366;
static const long long YEAR_ZERO_FIRST_SEC = -62168515200;

typedef struct {
	long year;
	int month;
	int day;
	int hour;
	int min;
	int sec;
	int timezoneOffset;
} SHDateTime;

int createDateTime(long year, int month, int day, int hour, int minute, int second,
		int timezoneOffset, long *ans);
int createDate(long year, int month, int day, int timezoneOffset, long *ans);
int createTime(int hour, int minute, int second, long *ans);
int timestampToDateObj(long timestamp, int timezoneOffset, SHDateTime *dt);
int dateObjToTimestamp(SHDateTime *dt, long *ans);
int addToTimestamp(long timestamp, long year, int month, int day, long *ans);
int dayStart(long timestamp, int timezoneOffset, long *ans);
int calcWeekdayIdx(long timestamp, int timezoneOffset, int *ans);
int calcDaysBetween(
		long timestampA, int timezoneOffsetA, long timestampB, int timezoneOffsetB, long *ans);

#endif /* c_datetime_h */
