// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include "../base/Condition.h"
#include "../base/MutexLock.h"
#include "../base/Thread.h"
#include "../base/noncopyable.h"
#include <assert.h>



class EventLoop;

class EventLoopThread :noncopyable
{
public:
	EventLoopThread(const std::string& name = std::string());
	~EventLoopThread();

	EventLoop* startLoop();

private:
	void threadFunc();

	EventLoop* loop_;
	bool exiting_;
	Thread thread_;
	MutexLock mutex_;
	Condition cond_;
};
