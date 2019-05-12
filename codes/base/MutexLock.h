// @Author Lin Han
// @Email  hancurrent@foxmail.com
// RAII 机制封装MutexLockGuard类
#pragma once
#include"noncopyable.h"
#include"CurrentThread.h"
#include<pthread.h>
class MutexLock:public noncopyable
{
public:
	MutexLock():holder(0)
	{
		pthread_mutex_init(&mutex, NULL);
	}
	~MutexLock()
	{
		assert(holder == 0);
		pthread_mutex_destroy(&mutex);
	}
private:
	void lock()//仅供MutexLockGuard调用，严禁用户代码调用
	{ 
		pthread_mutex_lock(&mutex);//这两行顺序不能反 
		holder = CurrentThread::tid();
	}
	void unlock() //仅供MutexLockGuard调用，严禁用户代码调用
	{ 
		holder = 0;//这两行顺序不能反
		pthread_mutex_unlock(&mutex);
	}
	pthread_mutex_t * getMutex() //仅供Condition调用，严禁用户代码调用
	{
		return &mutex;
	}
private:
	pthread_mutex_t mutex;
	pid_t holder;
	friend class MutexLockGuard;
	friend class Condition;
};
class MutexLockGuard :public noncopyable
{
public:
	explicit MutexLockGuard(MutexLock & _mutexLock):mutexLock(_mutexLock)
	{
		mutexLock.lock();
	}
	~MutexLockGuard()
	{
		mutexLock.unlock();
	}
private:
	MutexLock & mutexLock;//注意传递的是引用
};
