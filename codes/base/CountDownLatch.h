// @Author Lin Han
// @Email  hancurrent@foxmail.com
// CountDownLatch类用于主线程等待子线程完成任务，或者子线程等待主线程发号司令
#pragma once
#include "Condition.h"
#include "MutexLock.h"
#include "noncopyable.h"

class CountDownLatch : noncopyable
{
public:
    explicit CountDownLatch(int count);
    void wait();
    void countDown();
	int getCount()const;

private:
    mutable MutexLock mutex;//顺序很重要，先mutex，后condition
    Condition condition;
    int count;
};