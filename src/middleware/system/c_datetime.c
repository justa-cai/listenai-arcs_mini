//
//  c_datetime.c
//  SHCommon
//
//  Created by Joel Pridgen on 3/10/18.
//  Copyright © 2018 Joel Gillette. All rights reserved.
//

#include "c_datetime.h"
#include <limits.h>
#include <stdbool.h>

static const int _monthSums[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static const int _backwardMonthSums[12] = {334, 306, 275, 245, 214, 184, 153, 122, 92, 61, 31, 0};
static const int _monthCount[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

typedef struct {
	long totalMins;
	int exSecs;
	long totalHours;
	long totalDays;
} TimeCalcResult;

static int _getMonthFromDaySum(int daySum, int isLeapYear)
{
	if (daySum < 1 || daySum > (366 + (isLeapYear ? 1 : 0))) return -1;
	if (daySum <= 31) return 1;
	if (daySum <= (59 + (isLeapYear ? 1 : 0))) return 2;
	if (daySum <= (90 + (isLeapYear ? 1 : 0))) return 3;
	if (daySum <= (120 + (isLeapYear ? 1 : 0))) return 4;
	if (daySum <= (151 + (isLeapYear ? 1 : 0))) return 5;
	if (daySum <= (181 + (isLeapYear ? 1 : 0))) return 6;
	if (daySum <= (212 + (isLeapYear ? 1 : 0))) return 7;
	if (daySum <= (243 + (isLeapYear ? 1 : 0))) return 8;
	if (daySum <= (273 + (isLeapYear ? 1 : 0))) return 9;
	if (daySum <= (304 + (isLeapYear ? 1 : 0))) return 10;
	if (daySum <= (334 + (isLeapYear ? 1 : 0))) return 11;
	return 12;
}

static int _isTimestampRangeInvalid(long timestamp, int timezoneOffset)
{
	if (timestamp < 0 && (YEAR_ZERO_FIRST_SEC - timestamp) > -1 * timezoneOffset) return -1;
	if (timestamp > 0 && (LONG_MAX - timestamp) < timezoneOffset) return -1;
	return 0;
}

static long _calcNumLeapYears(long year)
{
	long dif = year - FIRST_LEAP_YEAR + (year >= FIRST_LEAP_YEAR ? -1 : 0);
	long mod = dif > 0 ? dif : dif + -1 * dif * 4;  // for neg numbers, factor was abitrary
	long ans = (dif + 4 - (mod % 4)) / 4;
	return ans < 0 ? -1 * ans : ans;
}

static long _calcYears(long seconds)
{
	int beforeEpochOffset = seconds < 0 ? 1 : 0;
	seconds = seconds < 0 ? -1 * seconds : seconds;
	seconds -= beforeEpochOffset;
	int excess = seconds % UNIX_LEAP_CYCLE;
	int years = 0;
	long leapCycles = seconds / UNIX_LEAP_CYCLE;
	if (excess > UNIX_LEAP_YEAR_LAST_SEC) {
		years++;
		excess -= UNIX_LEAP_YEAR;
	} else {
		return (leapCycles * LEAP_CYCLE_YEAR);
	}
	years += (excess / UNIX_YEAR) + (leapCycles * LEAP_CYCLE_YEAR);
	return years;
}

static void _calcTimeFromTimestamp(long timestamp, int minOffset, TimeCalcResult *result)
{
	result->totalMins = timestamp / UNIX_MIN;
	result->exSecs = timestamp % 60;
	result->totalMins += ((result->exSecs + minOffset) / 60);
	result->totalHours = result->totalMins / 60;
	result->totalDays = result->totalHours / 24;
}

int createDateTime(long year, int month, int day, int hour, int minute, int second,
		int timezoneOffset, long *ans)
{
	int isValid = year >= 0;
	isValid &= (hour >= 0 && hour < 24);
	isValid &= (minute >= 0 && minute < 60);
	isValid &= (second >= 0 && second < 60);
	isValid &= month > 0 && month < 13;

	int isLeapYear = !((year - FIRST_LEAP_YEAR) % 4);
	isValid &= (day > 0 && day <= (_monthCount[month - 1] + (isLeapYear && month == 2 ? 1 : 0)));
	if (!isValid) return -1;
	long sum;
	long leapYearCount = _calcNumLeapYears(year);
	if (year >= BASE_YEAR) {
		long span = year - BASE_YEAR;
		int dayTotal = _monthSums[month - 1];
		long yearStart = span * UNIX_DAY * 365 + UNIX_DAY * leapYearCount;
		dayTotal += (day - 1);
		int FEB_DAY_SUM = 59;
		int leapDayOffset =
				isLeapYear && (dayTotal > FEB_DAY_SUM || (month == 3 && dayTotal == FEB_DAY_SUM))
						? 1
						: 0;
		sum = yearStart + dayTotal * UNIX_DAY + leapDayOffset * UNIX_DAY;
		sum += (hour * UNIX_HOUR + minute * UNIX_MIN + second);
	} else {
		long span = year - BASE_YEAR + 1;
		int dayTotal = _backwardMonthSums[month - 1];
		long yearStart = span * UNIX_DAY * 365 - UNIX_DAY * leapYearCount;
		day = day - _monthCount[month - 1] - (isLeapYear && dayTotal >= 306 ? 1 : 0);
		dayTotal = day - dayTotal;
		sum = yearStart + dayTotal * UNIX_DAY;
		sum += ((hour + 1 - 24) * UNIX_HOUR + (minute + 1 - 60) * UNIX_MIN + (second - 60));
	}
	sum -= timezoneOffset;
	*ans = sum;
	return 0;
}

int createDate(long year, int month, int day, int timezoneOffset, long *ans)
{
	return createDateTime(year, month, day, 0, 0, 0, timezoneOffset, ans);
}

int createTime(int hour, int minute, int second, long *ans)
{
	return createDateTime(1970, 1, 1, hour, minute, second, 0, ans);
}

void _filDateTimeObj(long year, int month, int day, int hour, int min, int sec, int timezoneOffset,
		SHDateTime *dt)
{
	dt->year = year;
	dt->month = month;
	dt->day = day;
	dt->hour = hour;
	dt->min = min;
	dt->sec = sec;
	dt->timezoneOffset = timezoneOffset;
}

void _timestampShortToDateObj(int timestamp, SHDateTime *dt)
{
	TimeCalcResult result;
	int isBeforeEpoch = timestamp < 0;
	timestamp += (isBeforeEpoch ? UNIX_YEAR : 0);
	_calcTimeFromTimestamp(timestamp, 0, &result);
	int totalYears = (int)(result.totalDays / NORMAL_YEAR_DAYS);
	int exDays = (int)result.totalDays - totalYears * NORMAL_YEAR_DAYS + INCLUDE_TODAY;
	int month = _getMonthFromDaySum(exDays, false);
	exDays -= _monthSums[month - 1];
	int exHours = result.totalHours % DAY_HOURS;
	int exMin = result.totalMins % MIN_SEC_LEN;
	int year = isBeforeEpoch ? BEFORE_EPOCH_BASE_YEAR : BASE_YEAR + totalYears;
	_filDateTimeObj(year, month, exDays, exHours, exMin, result.exSecs, 0, dt);
}

long _calcShiftedTimestamp(long timestamp, long years, int isBeforeEpoch)
{
	if (years > 1) {
		return (timestamp % (years * UNIX_YEAR));
	} else if (years == 1) {
		return timestamp + (isBeforeEpoch ? UNIX_YEAR : -UNIX_YEAR);
	}
	return timestamp;
}

int timestampToDateObj(long timestamp, int timezoneOffset, SHDateTime *dt)
{
	if (!dt) return -1;
	if (_isTimestampRangeInvalid(timestamp, timezoneOffset)) return -1;
	if (timestamp - timezoneOffset == 0) {
		_filDateTimeObj(BASE_YEAR, 1, 1, 0, 0, 0, timezoneOffset, dt);
		return 0;
	}
	timestamp -= timezoneOffset;
	if (timestamp > TIMESTAMP_END_1968 && timestamp < TIMESTAMP_BEGIN_1972) {
		_timestampShortToDateObj((int)timestamp, dt);
		dt->timezoneOffset = timezoneOffset;
		return 0;
	}
	int isBeforeEpoch = timestamp < 0;
	long leapBasedTimestamp = isBeforeEpoch ? timestamp + UNIX_YEAR : timestamp - 2 * UNIX_YEAR;
	long totalYears = _calcYears(leapBasedTimestamp);
	int isLeapYear = !((totalYears) % LEAP_CYCLE_YEAR);
	long shiftedTimestamp = _calcShiftedTimestamp(leapBasedTimestamp, totalYears, isBeforeEpoch);
	int baseYear = isBeforeEpoch ? LEAP_YEAR_BEFORE_EPOCH : FIRST_LEAP_YEAR;
	if (shiftedTimestamp == YEAR_CUSP) {
		totalYears *= (isBeforeEpoch ? MIRROR_BEFORE_EPOCH : EPOCH_NEUTRAL);
		_filDateTimeObj(baseYear + totalYears, 1, 1, 0, 0, 0, timezoneOffset, dt);
		return 0;
	}
	totalYears *= (isBeforeEpoch ? MIRROR_BEFORE_EPOCH : EPOCH_NEUTRAL);
	long leapCount = _calcNumLeapYears(totalYears + baseYear);
	if (isBeforeEpoch) {
		long complimentYear = (isLeapYear ? UNIX_LEAP_YEAR : UNIX_YEAR) + leapCount * UNIX_DAY;
		shiftedTimestamp = (complimentYear + shiftedTimestamp);
	}
	TimeCalcResult result;
	_calcTimeFromTimestamp(shiftedTimestamp, 0, &result);
	int exDays = (int)result.totalDays + INCLUDE_TODAY;
	exDays -= (!isBeforeEpoch ? leapCount : 0);
	int month = _getMonthFromDaySum(exDays, isLeapYear);
	int currentLeapOffset = (isLeapYear && exDays > LEAP_FEB_SUM ? 1 : 0);
	exDays -= currentLeapOffset;
	exDays -= _monthSums[month - 1];
	int exHours = result.totalHours % DAY_HOURS;
	int exMins = result.totalMins % MIN_SEC_LEN;

	_filDateTimeObj(totalYears + baseYear, month, exDays, exHours, exMins, result.exSecs,
			timezoneOffset, dt);
	return 0;
}

int dateObjToTimestamp(SHDateTime *dt, long *ans)
{
	if (!dt) return -1;
	return createDateTime(
			dt->year, dt->month, dt->day, dt->hour, dt->min, dt->sec, dt->timezoneOffset, ans);
}

int addToTimestamp(long timestamp, long year, int month, int days, long *ans)
{
	SHDateTime dt;
	if (timestampToDateObj(timestamp, 0, &dt)) return -1;
	dt.year += year;
	dt.month += month;
	dt.day += days;
	if (dateObjToTimestamp(&dt, ans)) return -1;
	return 0;
}

int dayStart(long timestamp, int timezoneOffset, long *ans)
{
	SHDateTime dt;
	if (timestampToDateObj(timestamp, timezoneOffset, &dt)) return -1;
	dt.hour = 0;
	dt.min = 0;
	dt.sec = 0;
	dateObjToTimestamp(&dt, ans);
	return 0;
}

int calcWeekdayIdx(long timestamp, int timezoneOffset, int *ans)
{
	if (_isTimestampRangeInvalid(timestamp, timezoneOffset)) return -1;
	timestamp -= timezoneOffset;
	long totalDays = timestamp / UNIX_DAY;
	if (totalDays > -1 * WEEK_START_DAYS_BEFORE && totalDays < WEEK_START_DAYS_AFTER) {
		return (int)totalDays + EPOCH_WEEK_CORRECTION;
	}
	totalDays = totalDays > 0 ? totalDays - WEEK_START_DAYS_AFTER
							  : -1 * totalDays - WEEK_START_DAYS_BEFORE;
	*ans = totalDays % 7;
	return 0;
}

int calcDaysBetween(
		long timestampA, int timezoneOffsetA, long timestampB, int timezoneOffsetB, long *ans)
{
	if (_isTimestampRangeInvalid(timestampA, timezoneOffsetA)) return -1;
	if (_isTimestampRangeInvalid(timestampB, timezoneOffsetB)) return -1;
	timestampA -= timezoneOffsetA;
	timestampB -= timezoneOffsetB;
	long totalDaysA = timestampA / UNIX_DAY;
	long totalDaysB = timestampB / UNIX_DAY;
	*ans = totalDaysA - totalDaysB;
	return 0;
}
