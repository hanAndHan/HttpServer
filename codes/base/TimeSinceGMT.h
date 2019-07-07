// @Author Liu Han
// @Email  hancurrent@foxmail.com
// TimeSinceGMT类用于获取距离1970年0时0分0秒的微秒数
#pragma once
#include"noncopyable.h"
#include<stdint.h>
#include<string>
#include <sys/time.h>
class TimeSinceGMT 
{
public:
	TimeSinceGMT()
	{
		now();
	}
	std::string toFormattedString() 
	{
		now();
		char buf[32] = { 0 };
		time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
		int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
		struct tm tm_time;
		gmtime_r(&seconds, &tm_time);

		snprintf(buf, sizeof(buf), "%4d%02d%02d|%02d:%02d:%02d.%06d",
			tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
			tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
			microseconds);
		return buf;
	}

	bool valid() const { return microSecondsSinceEpoch_ > 0; }

	int64_t microSecondsSinceEpoch()  //返回距离1970年0时0分0秒的微秒数
	{
		now();
		return microSecondsSinceEpoch_; 
	}
	time_t secondsSinceEpoch() //返回距离1970年0时0分0秒的秒数
	{
		now();
		return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);//1微妙等于百万分之1秒
	}

	int64_t microSecondsSinceEpochOld()  //返回创建对象那一刻距离1970年0时0分0秒的微秒数
	{
		return microSecondsSinceEpoch_;
	}
	time_t secondsSinceEpochOld() //返回创建对象那一刻距离1970年0时0分0秒的秒数
	{
		return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);//1微妙等于百万分之1秒
	}
	static const int kMicroSecondsPerSecond = 1000 * 1000;
private:
	void now()//计算距离1970年0时0分0秒的微秒数
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		int64_t seconds = tv.tv_sec;
		microSecondsSinceEpoch_ = seconds * kMicroSecondsPerSecond + tv.tv_usec;
	}
	int64_t microSecondsSinceEpoch_;//距离1970年0时0分0秒所留过的微妙数
	
};