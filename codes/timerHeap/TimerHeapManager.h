// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include"../base/noncopyable.h"
#include<iostream>
#include<memory.h>
#include<unistd.h>
#include <sys/timerfd.h>
#include<queue>
#include<map>
#include<deque>
#include"../base/TimeSinceGMT.h"
#include"../net/Channel.h"
#include"../net/EventLoop.h"
#include"multimapHelp.h"

using namespace std::placeholders;
using namespace std;

class Connection;

typedef shared_ptr<Connection> timerNode;
class TimerHeapManager
{
public:
	TimerHeapManager(EventLoop * loop,bool excute=false);

	~TimerHeapManager();

	void start(int64_t timeIntervalMicroSeconds);//心搏间隔，在EventLoopThreadPool中启动

	void tick(int64_t receiveTime);

	void addConnection(const timerNode & newNode, int64_t overTime);//overTime表示每个连接从接收开始的存活时间,单位微妙

	void removeConnection(int64_t overTimeSinceGMT, const timerNode & oldNode);

	void modifyConnection(int64_t overTimeSinceGMT, const timerNode & oldNode);

private:
	void stop()
	{
		if (execute_ == false)
			return;
		if (!stoped_)
		{
			channnel_->disableAll();
			channnel_->remove();
			//close(timerfd_);
			stoped_ = true;
		}
	}
	EventLoop * loop_;
	int timerfd_;
	unique_ptr<Channel> channnel_;
	int64_t timeIntervalMicroSeconds_;			//心搏时间间隔
	bool stoped_;
	bool execute_;
};
/*============小根堆废弃代码============*/

//using namespace std::placeholders;
//typedef weak_ptr<Connection> timerNode;
//namespace timerHeap
//{
//	struct timeCompare
//	{
//		bool operator()(const timerNode & left, const timerNode & right)
//		{
//			return left.lock()->getOverTimeSinceGMT() > right.lock()->getOverTimeSinceGMT();
//		}
//	};
//	extern priority_queue<timerNode, deque<timerNode>, timeCompare> timerHeap_;
//}
//class TimerHeapManager:noncopyable
//{
//public:
//	TimerHeapManager(EventLoop * loop) 
//		:loop_(loop),
//		timerfd_(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
//		channnel_(new Channel(loop, timerfd_)),
//		overLastReadTimeThreshold_(15)
//	{
//		channnel_->setReadCallback(std::bind(&TimerHeapManager::tick, this, _1));
//	}
//	~TimerHeapManager()
//	{
//		if (!stoped)
//		{
//			channnel_->disableAll();
//			channnel_->remove();
//			close(timerfd_);
//			stoped = true;
//		}
//	}
//	void start(int64_t timeIntervalSeconds)//心搏间隔
//	{
//		stoped = false;
//		channnel_->enableReading();
//		timeIntervalSeconds_ = timeIntervalSeconds;
//		struct itimerspec howlong;
//		bzero(&howlong, sizeof howlong);
//		howlong.it_value.tv_sec = static_cast<__time_t>(timeIntervalSeconds);
//		howlong.it_interval.tv_sec = static_cast<__time_t>(timeIntervalSeconds);
//		::timerfd_settime(timerfd_, 0, &howlong, NULL);//文件描述符timeIntervalSeconds秒后可读
//	}
//
//	void tick(int64_t receiveTime)
//	{
//		//促发后read一下，否者会一直触发
//		uint64_t howmany;
//		::read(timerfd_, &howmany, sizeof howmany);
//		//cout << "触发" << endl;
//		while (!timerHeap::timerHeap_.empty())
//		{
//			cout << "进来" << endl;
//			timerNode tempNode = timerHeap::timerHeap_.top();
//			if (tempNode.expired())//连接已经被关掉了，这种情况出现只有两种可能，1.客户端关闭连接，服务器read到0之后，服务器主动关闭连接；2.服务器read出错，主动关闭连接
//			{
//				timerHeap::timerHeap_.pop();
//				cout << "已经被关闭" << endl;
//			}
//			else if(tempNode.lock()->getOverTimeSinceGMT() <= receiveTime && tempNode.lock()->readingOrWriting()) //连接还在进行写操作或则读操作,且超时了=》延长超时时间
//			{
//				timerHeap::timerHeap_.pop();
//				tempNode.lock()->setOverTime(tempNode.lock()->getOverTime() * 2);
//				tempNode.lock()->setOverTimeSinceGMT(receiveTime + tempNode.lock()->getOverTime());//超时时间动态增长，减少push频率
//				timerHeap::timerHeap_.push(tempNode);
//				cout <<"正在进行读写操作，延长关闭连接:"<< tempNode.lock()->name()<<"|"<<"从现在开始过"<< tempNode.lock()->getOverTime()<<"s"<<"尝试关闭" << endl;
//			}
//			else if(tempNode.lock()->getOverTimeSinceGMT() <= receiveTime && !tempNode.lock()->readingOrWriting() && (receiveTime - tempNode.lock()->getLastReadTimeSinceGMT() >= overLastReadTimeThreshold_|| tempNode.lock()->getLastReadTimeSinceGMT()==0)) //连接现在没有进行读操作也没有进行写操作,且超时了,且离上次进行读操作过去了15s或者之前一直没有进行读操作=>关闭连接
//			{
//				timerHeap::timerHeap_.pop();
//				tempNode.lock()->shutdown();
//				cout << "已经关闭连接:" << tempNode.lock()->name() <<"|"<<"离上次读操作过去："<< receiveTime - tempNode.lock()->getLastReadTimeSinceGMT() <<"s"<< endl;
//			}
//			else if (tempNode.lock()->getOverTimeSinceGMT() <= receiveTime && !tempNode.lock()->readingOrWriting() && (receiveTime - tempNode.lock()->getLastReadTimeSinceGMT() < overLastReadTimeThreshold_ && tempNode.lock()->getLastReadTimeSinceGMT() != 0)) //连接现在没有进行读操作也没有进行写操作,且超时了,且离上次进行读操作小于15s=>延长超时时间
//			{																												
//				timerHeap::timerHeap_.pop();
//				tempNode.lock()->setOverTime(overLastReadTimeThreshold_ - (receiveTime - tempNode.lock()->getLastReadTimeSinceGMT()));
//				tempNode.lock()->setOverTimeSinceGMT(receiveTime + tempNode.lock()->getOverTime());
//				timerHeap::timerHeap_.push(tempNode);
//				cout << "距离上次读操作不到15s，延长关闭连接:" << tempNode.lock()->name() << "|" << "从现在开始过" << tempNode.lock()->getOverTime() << "s" << "尝试关闭" << endl;
//			}
//			else//没有超时,以剩下的最早超时的连接到现在的超时值设为心搏间隔
//			{
//				timeIntervalSeconds_ = tempNode.lock()->getOverTimeSinceGMT() - receiveTime;
//				cout << "新的心搏间隔" << timeIntervalSeconds_ << endl;
//				stop();
//				start(timeIntervalSeconds_);
//				break;
//			}
//		}
//		if (timerHeap::timerHeap_.empty())
//		{
//			if (timeIntervalSeconds_ != 1)
//			{
//				stop();
//				start(1);
//			}
//		}
//	}
//
//	static void addConnection(const timerNode & newNode,int64_t overTime)//overTime表示每个连接从接收开始的存活时间
//	{
//		cout << "开始add" << endl;
//		newNode.lock()->setOverTime(overTime);
//		int64_t overTimeSinceGMT = static_cast<int64_t>(TimeSinceGMT().secondsSinceEpoch()) + overTime;
//		newNode.lock()->setOverTimeSinceGMT(overTimeSinceGMT);
//		cout << "结束add1" << endl;
//		cout<< newNode.lock()->getOverTimeSinceGMT()<<endl;
//		timerHeap::timerHeap_.push(newNode);
//		cout << "结束add2" << endl;
//	}
//	void setOverLastReadTimeThread(int64_t overLastReadTimeThread)
//	{
//		overLastReadTimeThreshold_ = overLastReadTimeThreshold_;
//	}
//private:
//	void stop()
//	{
//		if (!stoped)
//		{
//			channnel_->disableAll();
//			channnel_->remove();
//			//close(timerfd_);
//			stoped = true;
//		}
//	}
//	EventLoop * loop_;
//	int timerfd_;
//	unique_ptr<Channel> channnel_;
//	int64_t overLastReadTimeThreshold_;			//距离上次读操作的超时阈值
//	int64_t timeIntervalSeconds_;				//心搏时间间隔
//	bool stoped;
//};