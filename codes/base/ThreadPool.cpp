// @Author Lin Han
// @Email  hancurrent@foxmail.com
#include "ThreadPool.h"
ThreadPool::ThreadPool(const string& name)
	: mutex(),
	cond(mutex),
	mutexWait(),
	condWait(mutexWait),
	name(name),
	running(false),
	isBeginWait(false)
{
}

ThreadPool::~ThreadPool()
{
	if (running)
		stop();
}

void ThreadPool::start(int numThreads)//启动线程池
{
	assert(threads.empty());
	running = true;
	threads.reserve(numThreads);
	for (int i = 0; i < numThreads; ++i)
	{
		char id[32];
		snprintf(id, sizeof id, "%d", i);
		threads.push_back(new Thread(bind(&ThreadPool::runInThread, this), name + id));//runInThread一直从任务队列中取任务，有任务就执行
		threads[i]->start();
	}
}

void ThreadPool::wait()//等待正在执行任务的线程把任务执行完，停止线程池
{
	if (queue.empty())
	{
		stop();
	}	
	else
	{
		isBeginWait = true;
		condWait.wait();
		stop();
	}
}

void ThreadPool::stop()//不管任务是否执行完毕，当前任务执行完后，销毁线程池
{
	{
		MutexLockGuard lock(mutex);
		running = false;
		cond.notifyAll();
	}
	for_each(threads.begin(), threads.end(), [](Thread * thread) {thread->join();});
	//for_each(threads_.begin(),threads_.end(),bind(&Thread::join, _1));
	for (auto &it : threads)
		delete it;
}

void ThreadPool::run(const Task& task)//添加任务
{
	if (threads.empty())//线程池中没有线程，主线程直接接执行任务
	{
		task();
	}
	else//向任务队列中添加任务
	{
		MutexLockGuard lock(mutex);
		queue.push_back(task);
		cond.notify();//通知队列中有任务了
	}
}

ThreadPool::Task ThreadPool::take()//取任务
{
	MutexLockGuard lock(mutex);
	while (queue.empty() && running)
	{
		cond.wait();
	}
	Task task; 
	if (!queue.empty())
	{
		task = queue.front();
		queue.pop_front();
	}
	return task;
}

void ThreadPool::runInThread()//每个线程都一直执行这个函数,这个函数的功能就是不停的取任务,执行完就取新任务
{
	while (running)
	{
		Task task = take();
		if (task)
			task();//当前线程把task()这个函数执行完后并没有退出,又回到上面领任务去了
		//下面的代码是为了配合wait()函数使用，通知wait函数，任务队列已经为空，等正在执行的线程执行完毕后可以开始回收了
		if (queue.empty() && isBeginWait)//设置isBeginWait变量，使得只通知一次condWait
		{
			{
				MutexLockGuard lock(mutexWait);
				if (isBeginWait==false)
					return;
				else 
					isBeginWait = false;
			}
			condWait.notify();
		}		
	}
}
