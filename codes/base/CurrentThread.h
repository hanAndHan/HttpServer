// @Author Lin Han
// @Email  hancurrent@foxmail.com
// CurrentThread命名空间中的函数用来方便的获取线程id，名称
#pragma once
#include<unistd.h>
#include<sys/syscall.h>
#include<assert.h>
#include<stdio.h>

namespace CurrentThread
{
	//线程局部全局变量
	extern __thread int t_cachedTid;
	extern __thread char t_tidString[32];
	extern __thread const char* t_threadName;
	inline void cacheTid()//缓存id
	{
		if (t_cachedTid == 0)
		{
			t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));//获取线程唯一id
			snprintf(t_tidString, sizeof t_tidString, "%d ", t_cachedTid);//%5d加空格，长度为6
		}
	}

	inline int tid()//获取线程唯一id
	{
		if (t_cachedTid == 0)//说明该tid没有被缓存过
		{
			cacheTid();//缓存tid
		}
		return t_cachedTid;
	}

	inline const char* tidString() // for logging
	{
		return t_tidString;
	}

	inline const char* name()//返回线程名称
	{
		return t_threadName;
	}

	inline bool isMainThread()//是否是主线程
	{
		return tid() == ::getpid();
	}
};



