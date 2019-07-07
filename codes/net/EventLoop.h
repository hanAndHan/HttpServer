// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include <vector>
#include"unistd.h"
#include"sys/eventfd.h"
#include<vector>
#include "../base/noncopyable.h"
#include "../base/CurrentThread.h"
#include"../base/Thread.h"
#include"../base/MutexLock.h"
#include<functional>
// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include<map>
#include"../timerHeap/multimapHelp.h"
using namespace std::placeholders;
using namespace std;

//前向声明两个类
class Channel;
class EPoll;
class Connection;
class EventLoop : noncopyable
{
 public:
	typedef function<void()>Functor;
	EventLoop();
	~EventLoop();  
 	void loop();
	void quit();
	int64_t pollReturnTime() const { return pollReturnTime_; }
	void runInLoop(const Functor& cb);			//直接在I/O线程中执行某个回调函数，该函数可以跨线程调用
	void queueInLoop(const Functor& cb);		//一般由其它线程调用
	void updateChannel(Channel* channel);		// 在Poller中添加或者更新通道
	void removeChannel(Channel* channel);		// 从Poller中移除通道
	void assertInLoopThread()
	{
		if (!isInLoopThread())//当前对象所属线程和当前线程不同，则退出
		{
			abortNotInLoopThread();
		}
	}
	bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }//当前线程是创建事件循环的线程，即I/O线程
	static EventLoop* getEventLoopOfCurrentThread();
	multimap<int64_t, shared_ptr<Connection>, compare> & getTimerHeap()
	{
		return timerHeap_;
	}
 private:
	  void abortNotInLoopThread();
	  void handleRead(int64_t receiveTime);
	  void wakeup();							//可实现线程的异步唤醒
	 
	  void doPendingFunctors();
	  typedef std::vector<Channel*> ChannelList;
  
	  bool looping_; /* atomic */				//是否处于循环的状态
	  bool quit_; /* atomic */					//是否退出loop
	  bool eventHandling_; /* atomic */			//当前是否处于事件处理状态
	  bool callingPendingFunctors_;/*atomic*/
	  const int threadId_;						//当前对象所属线程ID
	  int64_t pollReturnTime_;		    		//调用poll函数所返回的时间
	  shared_ptr<EPoll> poller_;				//epoll对象
	  ChannelList activeChannels_;				//Poller返回的活动通道，即事件就绪的的一些通道
	  Channel* currentActiveChannel_;			//当前正在处理的活动通道
	  int wakeupFd_;							//用于eventfd
	  shared_ptr<Channel> wakeupChannel_;		//该通道会纳入epoll来管理
	  vector<Functor>pendingFunctors_;
	  MutexLock mutex_;
	  multimap<int64_t, shared_ptr<Connection>, compare> timerHeap_;	//时间堆
};

