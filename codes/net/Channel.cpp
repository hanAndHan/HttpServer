// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include"EventLoop.h"
#include "Channel.h"
#include <poll.h>
#include"../base/TimeSinceGMT.h"

const int Channel::kNoneEvent = 0;				 //不关注事件
const int Channel::kReadEvent = POLLIN | POLLPRI;//可读/高优先级数据可读(带外数据)
const int Channel::kWriteEvent = POLLOUT;		 //数据可写

Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false)
{
}

Channel::~Channel()
{
  assert(!eventHandling_);
}

void Channel::tie(const shared_ptr<void>& obj)
{
  tie_ = obj;
  tied_ = true;
}

void Channel::update()
{
  loop_->updateChannel(this);
}

// 调用这个函数之前确保调用disableAll，否者会断言失败
void Channel::remove()
{
  assert(isNoneEvent());
  loop_->removeChannel(this);
}

//事件就绪时，调用handelEvent进行处理
void Channel::handleEvent(int64_t receiveTime)
{
 shared_ptr<void> guard;
  if (tied_)
  {
    guard = tie_.lock();
    if (guard)
    {
      handleEventWithGuard(receiveTime);
    }
  }
  else
  {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(int64_t receiveTime)
{ 
  eventHandling_ = true;
  //POLLHUP只会在写时产生，不会在读时产生
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
  {
    if (logHup_)
    {
		LogWarning("Channel::handle_event() POLLHUP");
    }
    if (closeCallback_) closeCallback_();
  }
  //文件描述符没有打开
  if (revents_ & POLLNVAL)
  {
	   LogWarning("Channel::handle_event() POLLNVAL");
  }
  //错误
  if (revents_ & POLLERR)
  {
    if (errorCallback_) errorCallback_();
	LogWarning("Channel::handle_event() POLLERR");
  }
  //普通数据可读或者带外数据可读或者对等方关闭连接，或者关闭半连接
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
  {
    if (readCallback_) readCallback_(receiveTime);
  }
  //可写事件产生
  if (revents_ & POLLOUT)
  {
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}
