// @Author Lin Han
// @Email  hancurrent@foxmail.com
// 线程池中预留了固定数量的线程队列和一个任务队列，
// 主线程往任务队列中添加任务，
// 线程池的中的每个线程都不停的从任务队列中取任务，执行完一个任务马上就领取下一个任务，没有任务就阻塞在条件变量上
// start()->runInThread()->run()->stop()/wait()
#pragma once
#include<functional>
#include"noncopyable.h"
#include"Thread.h"
#include"MutexLock.h"
#include"Condition.h"
#include<deque>
#include<vector>
#include<algorithm>
class ThreadPool : noncopyable
{
public:
	typedef function<void()> Task;
	explicit ThreadPool(const string& name = string());
	~ThreadPool();
	void start(int numThreads);	//启动线程池，启动线程的个数是固定的
	void stop();			//等待当前线程执行完后，关闭线程池
	void run(const Task& f);	//添加任务
	void wait();			//等待所有任务执行完毕后，关闭线程池
private:
	void runInThread();		//线程池中的线程要执行的函数
	Task take();			//获取任务
	MutexLock mutex;		//和cond配合使用
	Condition cond;			//条件变量，用来唤醒线程池中的线程执行任务
	MutexLock mutexWait;		//和condWait配合使用
	Condition condWait;		//条件变量，等待线程池中的所有线程执行完任务
	string name;			//线程池的名字
	vector<Thread *> threads;	//线程队列，内部存放的是线程指针
	deque<Task> queue;		//任务队列
	bool running;			//线程池是否处于运行状态
	bool isBeginWait;		//已经知道任务队列中所有任务都处理完了
};
