// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include "EventLoop.h"
#include"../base/Logger.h" 
#include"Channel.h"
#include"EPoll.h"
#include<memory.h>
#include"../TCP/Connection.h"
namespace
{
// 当前线程EventLoop对象指针
// 线程局部存储
// 每一个线程都有这么一个指针对象
__thread EventLoop* t_loopInThisThread = nullptr;
int createEventfd()
{
	int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (evtfd < 0) {
		LogFatal("LOG_SYSERR: Failed in eventfd");
	}
	return evtfd;
}
const int kPollTimeMs = 10000;
}

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),//false表示当前还未处于循环状态
    quit_(false),
    eventHandling_(false),
	callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),//当前线程的id赋值给threadId_
	poller_(make_shared<EPoll>(this)),//默认使用的时epoll
	currentActiveChannel_(NULL),
	wakeupFd_(createEventfd()),
	wakeupChannel_(make_shared<Channel>(this, wakeupFd_))
{
  // 如果当前线程已经创建了别的EventLoop对象，终止(LOG_FATAL)
  if (t_loopInThisThread)
  {
	  char msg[128];
	  sprintf(msg, "Another EventLoop %d exists in this thread %d", t_loopInThisThread, threadId_);
	  LogFatal(msg);
  }
  else//t_loopInThisThread==nullptr,表示当前线程还没有创建eventloop对象
  {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadCallback(bind(&EventLoop::handleRead, this, _1));//文件描述符可读后要把数据读走，否则由于是lt模式，将一直触发
  wakeupChannel_->enableReading();//注册该channel到epoll上，监听读事件
}

void EventLoop::handleRead(int64_t receiveTime)
{
	uint64_t one = 1;
	ssize_t n = ::read(wakeupFd_, &one, sizeof one);
	if (n != sizeof one) {
		char msg[128];
		sprintf(msg, "LOG_ERROR: EventLoop::handleRead() reads %ld bytes instead of 8", n);
		LogFatal(msg);
	}
}

EventLoop::~EventLoop()
{
  t_loopInThisThread = NULL;//置空，当前线程没有eventloop了
}

// 事件循环，该函数不能跨线程调用
// 只能在创建该对象的线程中调用
void EventLoop::loop()
{
  //断言还没开始循环
  assert(!looping_);
  //断言当前处于创建该对象的线程中
  assertInLoopThread();
  looping_ = true;
  //开始事件循环
  while (!quit_)
  {
    activeChannels_.clear();//首先将活动通道中上一次的记录清除清除
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);//kPollTimeMs是超时时间,获取活动通道activeChannels_
    eventHandling_ = true;
    for (ChannelList::iterator it = activeChannels_.begin();//遍历这些活动通道进行处理
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;//设置一下当前要处理的通道
      currentActiveChannel_->handleEvent(pollReturnTime_);//调用handleEvent处理这个通道
    }
    currentActiveChannel_ = NULL;//全部处理完后，当前要处理的通道置空
    eventHandling_ = false;
    doPendingFunctors();//让I/O线程也能处理一些计算任务
  }

  //结束事件循环
  looping_ = false;
}
void EventLoop::wakeup()
{
	uint64_t one = 1;//eventfd只有8个字节的缓冲区
	ssize_t n = write(wakeupFd_, (char*)(&one), sizeof one);//往wakeupFd_中写入数据唤醒线程
	if (n != sizeof one)
	{
		char msg[256];
		sprintf(msg, "EventLoop::wakeup() writes %d bytes instead of 8", n);
	}
}
void EventLoop::runInLoop(const Functor& cb)
{
	if (isInLoopThread())
		cb();
	else
		queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(const Functor& cb)
{
	{
		MutexLockGuard lock(mutex_);
		pendingFunctors_.emplace_back(std::move(cb));
	}

	if (!isInLoopThread() || callingPendingFunctors_)
	{
		wakeup();
	}
		
}
void EventLoop::doPendingFunctors() {
	vector<Functor> functors;
	callingPendingFunctors_ = true;

	{
		MutexLockGuard lock(mutex_);
		functors.swap(pendingFunctors_);//这些函数只会被执行一次
	}

	for (size_t i = 0; i < functors.size(); ++i) {
		functors[i]();
	}
	callingPendingFunctors_ = false;
}

//该函数可以跨线程调用
void EventLoop::quit()
{
  quit_ = true;//linux下面对bool类型操作时原子操作，所以这里不需要加锁
  if (!isInLoopThread())//若不是在本线程调用，需要先将事件循环唤醒，因为事件循环可能阻塞在poll上
  {
		wakeup();
  }
}

void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
	char msg[256];
	sprintf(msg, "EventLoop::abortNotInLoopThread - EventLoop %d was created in threadId_ = %d, current thread id =%d", this, threadId_, CurrentThread::tid());
}

