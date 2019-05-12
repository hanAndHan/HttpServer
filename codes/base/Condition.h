// @Author Lin Han
// @Email  hancurrent@foxmail.com
// 条件变量类
#pragma once
#include "noncopyable.h"
#include "MutexLock.h"
#include <pthread.h>
#include <errno.h>
#include <cstdint>
#include <time.h>
#include <sys/time.h>
class Condition: noncopyable
{
public:
    explicit Condition(MutexLock &_mutex):
        mutexLock(_mutex)
    {
        pthread_cond_init(&cond, NULL);
    }
    ~Condition()
    {
        pthread_cond_destroy(&cond);
    }
    void wait()
    {
        pthread_cond_wait(&cond, mutexLock.getMutex());
    }
    void notify()
    {
        pthread_cond_signal(&cond);
    }
    void notifyAll()
    {
        pthread_cond_broadcast(&cond);
    }
    bool waitForSeconds(int seconds)
    {
        struct timespec abstime;//struct timespec有两个成员，一个是秒，一个是纳秒, 所以最高精确度是纳秒
        clock_gettime(CLOCK_REALTIME, &abstime);//系统当前时间，从1970年1.1日算起
        abstime.tv_sec += static_cast<__time_t>(seconds);
        return ETIMEDOUT == pthread_cond_timedwait(&cond, mutexLock.getMutex() , &abstime);
    }
	bool waitForMilliseconds(long millseconds)
	{
		struct timespec abstime;
		struct timeval now;
		long timeout_ms = millseconds; // wait time
		gettimeofday(&now, NULL);
		long nsec = now.tv_usec * 1000 + (timeout_ms % 1000) * 1000000;
		abstime.tv_sec = now.tv_sec + nsec / 1000000000 + timeout_ms / 1000;
		abstime.tv_nsec = nsec % 1000000000;
		return ETIMEDOUT == pthread_cond_timedwait(&cond, mutexLock.getMutex(), &abstime);
	}
private:
    MutexLock &mutexLock;
    pthread_cond_t cond;
};