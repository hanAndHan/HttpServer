// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include"EventLoopThread.h"
#include "EventLoop.h"
EventLoopThread::EventLoopThread(const std::string& name)
	:loop_(NULL),
	exiting_(false),
	thread_(std::bind(&EventLoopThread::threadFunc, this), name),
	mutex_(),
	cond_(mutex_)
{
}
EventLoopThread::~EventLoopThread()
{
	exiting_ = true;
	if (loop_ != NULL)
	{
		loop_->quit();
		thread_.join();
	}
}

EventLoop* EventLoopThread::startLoop()
{
	assert(!thread_.isStarted());
	thread_.start();
	{
		MutexLockGuard lock(mutex_);
		// 一直等到threadFun在Thread里真正跑起来
		while (loop_ == NULL)
			cond_.wait();
	}
	return loop_;
}

void EventLoopThread::threadFunc()
{
	EventLoop loop;

	{
		MutexLockGuard lock(mutex_);
		loop_ = &loop;
		cond_.notify();
	}

	loop.loop();
	loop_ = NULL;
}
