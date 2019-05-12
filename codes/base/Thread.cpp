// @Author Lin Han
// @Email  hancurrent@foxmail.com
#include "Thread.h"
Thread::Thread(const ThreadFunc& func, const string& n)
	: started(false),
	pthreadId(0),
	tid(0),
	func(func),
	name(n)
{
	//numCreated_.increment();//创建的线程个数加1
}

Thread::~Thread()
{
	
}

void Thread::start()
{
	assert(!started);
	started = true;
	int err = pthread_create(&pthreadId, NULL, startThread, this);//不能直接传入runInThread，因为runInThread带有一个隐含的this指针
	if (err != 0)
	{
		fprintf(stderr, "pthread_create error:%s\n", strerror(err));
		exit(-1);
	}
}

int Thread::join()//回收线程
{
	assert(started);
	return pthread_join(pthreadId, NULL);
	return 0;
}

void* Thread::startThread(void* obj)
{
	//startThread是静态函数，不能直接调用runInThread
	Thread* thread = static_cast<Thread*>(obj);
	thread->runInThread();
	//thread->func_();//直接这么写也可以
	return NULL;
}

void Thread::runInThread()
{
	tid = CurrentThread::tid();
	if (name.empty())
		CurrentThread::t_threadName = "empty";
	else
		CurrentThread::t_threadName = name.data();
	func();//调用回调函数
	CurrentThread::t_threadName = "finished";
}
